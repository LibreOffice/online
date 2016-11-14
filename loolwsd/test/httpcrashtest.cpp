/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include <cstring>

#include <Poco/DirectoryIterator.h>
#include <Poco/Dynamic/Var.h>
#include <Poco/FileStream.h>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/InvalidCertificateHandler.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/PrivateKeyPassphraseHandler.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/Net/Socket.h>
#include <Poco/Path.h>
#include <Poco/StreamCopier.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Thread.h>
#include <Poco/URI.h>
#include <cppunit/extensions/HelperMacros.h>

#include <Common.hpp>
#include <UserMessages.hpp>
#include <Util.hpp>
#include <LOOLProtocol.hpp>
#include <LOOLWebSocket.hpp>
#include "helpers.hpp"
#include "countloolkits.hpp"

using namespace helpers;

/// Tests the HTTP WebSocket API of loolwsd. The server has to be started manually before running this test.
class HTTPCrashTest : public CPPUNIT_NS::TestFixture
{
    const Poco::URI _uri;
    Poco::Net::HTTPResponse _response;
    static int InitialLoolKitCount;

    CPPUNIT_TEST_SUITE(HTTPCrashTest);

    CPPUNIT_TEST(testBarren);
    CPPUNIT_TEST(testCrashKit);
    CPPUNIT_TEST(testCrashForkit);

    CPPUNIT_TEST_SUITE_END();

    void testCountHowManyLoolkits();
    void testBarren();
    void testCrashKit();
    void testCrashForkit();
    void testNoExtraLoolKitsLeft();

    static
    void killLoKitProcesses(const char* exec_filename);

public:
    HTTPCrashTest()
        : _uri(helpers::getTestServerURI())
    {
#if ENABLE_SSL
        Poco::Net::initializeSSL();
        // Just accept the certificate anyway for testing purposes
        Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> invalidCertHandler = new Poco::Net::AcceptCertificateHandler(false);
        Poco::Net::Context::Params sslParams;
        Poco::Net::Context::Ptr sslContext = new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, sslParams);
        Poco::Net::SSLManager::instance().initializeClient(0, invalidCertHandler, sslContext);
#endif
    }

#if ENABLE_SSL
    ~HTTPCrashTest()
    {
        Poco::Net::uninitializeSSL();
    }
#endif

    void setUp()
    {
        testCountHowManyLoolkits();
    }

    void tearDown()
    {
        testNoExtraLoolKitsLeft();
    }
};

int HTTPCrashTest::InitialLoolKitCount = 1;

void HTTPCrashTest::testCountHowManyLoolkits()
{
    InitialLoolKitCount = countLoolKitProcesses(InitialLoolKitCount);
    CPPUNIT_ASSERT(InitialLoolKitCount > 0);
}

void HTTPCrashTest::testNoExtraLoolKitsLeft()
{
    const auto countNow = countLoolKitProcesses(InitialLoolKitCount);

    CPPUNIT_ASSERT_EQUAL(InitialLoolKitCount, countNow);
}

void HTTPCrashTest::testBarren()
{
    // Kill all kit processes and try loading a document.
    const auto testname = "barren ";
    try
    {
        killLoKitProcesses("(loolkit)");
        countLoolKitProcesses(0);

        std::cerr << "Loading after kill." << std::endl;

        // Load a document and get its status.
        auto socket = loadDocAndGetSocket("hello.odt", _uri, testname);

        sendTextFrame(socket, "status", testname);
        assertResponseString(socket, "status:", testname);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPCrashTest::testCrashKit()
{
    const auto testname = "crashKit ";
    try
    {
        auto socket = loadDocAndGetSocket("empty.odt", _uri, testname);

        killLoKitProcesses("(loolkit)");
        countLoolKitProcesses(0);

        // We expect the client connection to close.
        // In the future we might restore the kit, but currently we don't.
        std::cerr << "Reading after kill." << std::endl;

        // Drain the socket.
        getResponseMessage(socket, "", testname, 1000);

        // 5 seconds timeout
        socket->setReceiveTimeout(5000000);

        // receive close frame handshake
        int bytes;
        int flags;
        char buffer[READ_BUFFER_SIZE];
        do
        {
            bytes = socket->receiveFrame(buffer, sizeof(buffer), flags);
            std::cerr << testname << "Got " << LOOLProtocol::getAbbreviatedFrameDump(buffer, bytes, flags) << std::endl;
        }
        while ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE);

        // respond close frame
        socket->shutdown();

        // no more messages is received.
        bytes = socket->receiveFrame(buffer, sizeof(buffer), flags);
        CPPUNIT_ASSERT_MESSAGE("Expected no more data", bytes <= 2); // The 2-byte marker is ok.
        CPPUNIT_ASSERT_EQUAL(0x88, flags);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPCrashTest::testCrashForkit()
{
    const auto testname = "crashForkit ";
    try
    {
        auto socket = loadDocAndGetSocket("empty.odt", _uri, testname);

        std::cerr << "Killing forkit." << std::endl;
        killLoKitProcesses("(loolforkit)");
        std::cerr << "Communicating after kill." << std::endl;

        sendTextFrame(socket, "status", testname);
        assertResponseString(socket, "status:", testname);

        // respond close frame
        socket->shutdown();


        std::cerr << "Killing loolkit." << std::endl;
        killLoKitProcesses("(loolkit)");
        countLoolKitProcesses(0);
        std::cerr << "Communicating after kill." << std::endl;
        loadDocAndGetSocket("empty.odt", _uri, testname);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPCrashTest::killLoKitProcesses(const char* exec_filename)
{
    // Crash all lokit processes.
    for (auto it = Poco::DirectoryIterator(std::string("/proc")); it != Poco::DirectoryIterator(); ++it)
    {
        try
        {
            Poco::Path procEntry = it.path();
            const std::string& fileName = procEntry.getFileName();
            int pid;
            std::size_t endPos = 0;
            try
            {
                pid = std::stoi(fileName, &endPos);
            }
            catch (const std::invalid_argument&)
            {
                pid = 0;
            }

            if (pid > 1 && endPos == fileName.length())
            {
                Poco::FileInputStream stat(procEntry.toString() + "/stat");
                std::string statString;
                Poco::StreamCopier::copyToString(stat, statString);
                Poco::StringTokenizer tokens(statString, " ");
                if (tokens.count() > 3 && tokens[1] == exec_filename)
                {
                    std::cerr << "Killing " << pid << std::endl;
                    if (kill(pid, SIGKILL) == -1)
                    {
                        std::cerr << "kill(" << pid << ", SIGKILL) failed: " << std::strerror(errno) << std::endl;
                    }
                }
            }
        }
        catch (const Poco::Exception&)
        {
        }
    }
}

CPPUNIT_TEST_SUITE_REGISTRATION(HTTPCrashTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
