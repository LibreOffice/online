/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <regex>
#include <vector>
#include <string>

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
#include <Poco/Net/WebSocket.h>
#include <Poco/Path.h>
#include <Poco/StreamCopier.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Thread.h>
#include <Poco/URI.h>
#include <cppunit/extensions/HelperMacros.h>

#include "Common.hpp"
#include "LOOLProtocol.hpp"
#include "Png.hpp"
#include "UserMessages.hpp"
#include "Util.hpp"
#include "countloolkits.hpp"
#include "helpers.hpp"

using namespace helpers;

/// Tests the HTTP WebSocket API of loolwsd. The server has to be started manually before running this test.
class HTTPWSTest : public CPPUNIT_NS::TestFixture
{
    const Poco::URI _uri;
    Poco::Net::HTTPResponse _response;
    static int InitialLoolKitCount;

    CPPUNIT_TEST_SUITE(HTTPWSTest);

    CPPUNIT_TEST(testBadRequest);
    CPPUNIT_TEST(testHandShake);
    CPPUNIT_TEST(testCloseAfterClose);
    CPPUNIT_TEST(testConnectNoLoad);
    CPPUNIT_TEST(testLoad);
    CPPUNIT_TEST(testBadLoad);
    CPPUNIT_TEST(testReload);
    CPPUNIT_TEST(testGetTextSelection);
    CPPUNIT_TEST(testSaveOnDisconnect); // Broken with multiview.
    CPPUNIT_TEST(testReloadWhileDisconnecting);
    CPPUNIT_TEST(testExcelLoad);
    CPPUNIT_TEST(testPaste);
    CPPUNIT_TEST(testLargePaste);
    CPPUNIT_TEST(testRenderingOptions);
    CPPUNIT_TEST(testPasswordProtectedDocumentWithoutPassword);
    CPPUNIT_TEST(testPasswordProtectedDocumentWithWrongPassword);
    CPPUNIT_TEST(testPasswordProtectedDocumentWithCorrectPassword);
    CPPUNIT_TEST(testPasswordProtectedDocumentWithCorrectPasswordAgain);
    CPPUNIT_TEST(testInsertDelete);
    CPPUNIT_TEST(testSlideShow);
    CPPUNIT_TEST(testInactiveClient);
    CPPUNIT_TEST(testMaxColumn);
    CPPUNIT_TEST(testMaxRow);
    CPPUNIT_TEST(testEmptyCellCursor);
    CPPUNIT_TEST(testInsertAnnotationWriter);
    CPPUNIT_TEST(testEditAnnotationWriter);  // Broken with multiview.
    CPPUNIT_TEST(testInsertAnnotationCalc);
    CPPUNIT_TEST(testCalcEditRendering);  // Broken with multiview.
    CPPUNIT_TEST(testFontList);
    CPPUNIT_TEST(testStateUnoCommand);
    CPPUNIT_TEST(testColumnRowResize);
    CPPUNIT_TEST(testOptimalResize);
    CPPUNIT_TEST(testInvalidateViewCursor);
    CPPUNIT_TEST(testViewCursorVisible);
    CPPUNIT_TEST(testCellViewCursor);
    CPPUNIT_TEST(testGraphicViewSelection);
    CPPUNIT_TEST(testGraphicInvalidate);
    CPPUNIT_TEST(testCursorPosition);

    CPPUNIT_TEST_SUITE_END();

    void testCountHowManyLoolkits();
    void testBadRequest();
    void testHandShake();
    void testCloseAfterClose();
    void testConnectNoLoad();
    void testLoad();
    void testBadLoad();
    void testReload();
    void testGetTextSelection();
    void testSaveOnDisconnect();
    void testReloadWhileDisconnecting();
    void testExcelLoad();
    void testPaste();
    void testLargePaste();
    void testRenderingOptions();
    void testPasswordProtectedDocumentWithoutPassword();
    void testPasswordProtectedDocumentWithWrongPassword();
    void testPasswordProtectedDocumentWithCorrectPassword();
    void testPasswordProtectedDocumentWithCorrectPasswordAgain();
    void testInsertDelete();
    void testNoExtraLoolKitsLeft();
    void testSlideShow();
    void testInactiveClient();
    void testMaxColumn();
    void testMaxRow();
    void testEmptyCellCursor();
    void testInsertAnnotationWriter();
    void testEditAnnotationWriter();
    void testInsertAnnotationCalc();
    void testCalcEditRendering();
    void testFontList();
    void testStateUnoCommand();
    void testColumnRowResize();
    void testOptimalResize();
    void testInvalidateViewCursor();
    void testViewCursorVisible();
    void testCellViewCursor();
    void testGraphicViewSelection();
    void testGraphicInvalidate();
    void testCursorPosition();

    void loadDoc(const std::string& documentURL);

    void getPartHashCodes(const std::string response,
                          std::vector<std::string>& parts);

    void getCursor(const std::string& message,
                   int& cursorX,
                   int& cursorY,
                   int& cursorWidth,
                   int& cursorHeight);

    void testLimitCursor( std::function<void(const std::shared_ptr<Poco::Net::WebSocket>& socket,
                                             int cursorX, int cursorY,
                                             int cursorWidth, int cursorHeight,
                                             int docWidth, int docHeight)> keyhandler,
                          std::function<void(int docWidth, int docHeight,
                                             int newWidth, int newHeight)> checkhandler);

    std::string getFontList(const std::string& message);
    void testStateChanged(const std::string& filename, std::vector<std::string>& vecComands);
    double getColRowSize(const std::string& property, const std::string& message, int index);
    double getColRowSize(const std::shared_ptr<Poco::Net::WebSocket>& socket, const std::string& item, int index);
    void testEachView(const std::string& doc, const std::string& type, const std::string& protocol, const std::string& view);

public:
    HTTPWSTest()
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
    ~HTTPWSTest()
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

int HTTPWSTest::InitialLoolKitCount = 1;

void HTTPWSTest::testCountHowManyLoolkits()
{
    InitialLoolKitCount = countLoolKitProcesses(InitialLoolKitCount);
    CPPUNIT_ASSERT(InitialLoolKitCount > 0);
}

void HTTPWSTest::testBadRequest()
{
    try
    {
        // Load a document and get its status.
        const std::string documentURL = "lool/ws/file:///fake.doc";

        Poco::Net::HTTPResponse response;
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        std::unique_ptr<Poco::Net::HTTPClientSession> session(helpers::createSession(_uri));
        // This should result in Bad Request, but results in:
        // WebSocket Exception: Missing Sec-WebSocket-Key in handshake request
        // So Service Unavailable is returned.

        request.set("Connection", "Upgrade");
        request.set("Upgrade", "websocket");
        request.set("Sec-WebSocket-Version", "13");
        request.set("Sec-WebSocket-Key", "");
        request.setChunkedTransferEncoding(false);
        session->setKeepAlive(true);
        session->sendRequest(request);
        session->receiveResponse(response);
        CPPUNIT_ASSERT(response.getStatus() == Poco::Net::HTTPResponse::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testHandShake()
{
    try
    {
        int bytes;
        int flags;
        char buffer[1024] = {0};
        // Load a document and get its status.
        std::string documentPath, documentURL;
        getDocumentPathAndURL("hello.odt", documentPath, documentURL);

        // NOTE: Do not replace with wrappers. This has to be explicit.
        Poco::Net::HTTPResponse response;
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        std::unique_ptr<Poco::Net::HTTPClientSession> session(helpers::createSession(_uri));
        Poco::Net::WebSocket socket(*session, request, response);

        const char* fail = "error:";
        std::string payload("statusindicator: find");

        std::string receive;
        socket.setReceiveTimeout(0);
        bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
        CPPUNIT_ASSERT_EQUAL(std::string(payload), std::string(buffer, bytes));
        CPPUNIT_ASSERT_EQUAL(static_cast<int>(Poco::Net::WebSocket::FRAME_TEXT), flags & Poco::Net::WebSocket::FRAME_TEXT);

        bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
        if (bytes > 0 && !std::strstr(buffer, fail))
        {
            payload = "statusindicator: connect";
            CPPUNIT_ASSERT_EQUAL(payload, std::string(buffer, bytes));
            CPPUNIT_ASSERT_EQUAL(static_cast<int>(Poco::Net::WebSocket::FRAME_TEXT), flags & Poco::Net::WebSocket::FRAME_TEXT);

            bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
            if (!std::strstr(buffer, fail))
            {
                payload = "statusindicator: ready";
                CPPUNIT_ASSERT_EQUAL(payload, std::string(buffer, bytes));
                CPPUNIT_ASSERT_EQUAL(static_cast<int>(Poco::Net::WebSocket::FRAME_TEXT), flags & Poco::Net::WebSocket::FRAME_TEXT);
            }
            else
            {
                // check error message
                CPPUNIT_ASSERT(std::strstr(buffer, SERVICE_UNAVALABLE_INTERNAL_ERROR) != nullptr);
                CPPUNIT_ASSERT_EQUAL(static_cast<int>(Poco::Net::WebSocket::FRAME_TEXT), flags & Poco::Net::WebSocket::FRAME_TEXT);

                // close frame message
                bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
                CPPUNIT_ASSERT(std::strstr(buffer, SERVICE_UNAVALABLE_INTERNAL_ERROR) != nullptr);
                CPPUNIT_ASSERT((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_CLOSE);
            }
        }
        else
        {
            // check error message
            CPPUNIT_ASSERT(std::strstr(buffer, SERVICE_UNAVALABLE_INTERNAL_ERROR) != nullptr);
            CPPUNIT_ASSERT_EQUAL(static_cast<int>(Poco::Net::WebSocket::FRAME_TEXT), flags & Poco::Net::WebSocket::FRAME_TEXT);

            // close frame message
            bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
            CPPUNIT_ASSERT(std::strstr(buffer, SERVICE_UNAVALABLE_INTERNAL_ERROR) != nullptr);
            CPPUNIT_ASSERT((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_CLOSE);
        }
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testCloseAfterClose()
{
    try
    {
        int bytes;
        int flags;
        char buffer[READ_BUFFER_SIZE];

        // Load a document and get its status.
        std::string documentPath, documentURL;
        getDocumentPathAndURL("hello.odt", documentPath, documentURL);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL);
        sendTextFrame(socket, "status");
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket));

        // send normal socket shutdown
        socket.shutdown();

        // 5 seconds timeout
        socket.setReceiveTimeout(5000000);

        // receive close frame handshake
        do
        {
            bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
        }
        while (bytes && (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE);

        // no more messages is received.
        bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
        std::cout << "Received " << bytes << " bytes, flags: "<< std::hex << flags << std::dec << std::endl;
        CPPUNIT_ASSERT_EQUAL(0, bytes);
        CPPUNIT_ASSERT_EQUAL(0, flags);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::loadDoc(const std::string& documentURL)
{
    try
    {
        // Load a document and get its status.
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        auto socket = connectLOKit(_uri, request, _response);
        sendTextFrame(*socket, "load url=" + documentURL);

        SocketProcessor("", socket, [&](const std::string& msg)
                {
                    const std::string prefix = "status: ";
                    if (msg.find(prefix) == 0)
                    {
                        const auto status = msg.substr(prefix.length());
                        // Might be too strict, consider something flexible instread.
                        CPPUNIT_ASSERT_EQUAL(std::string("type=text parts=1 current=0 width=12808 height=16408 viewid=0"), status);
                    }

                    return true;
                });

        socket->shutdown();
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testConnectNoLoad()
{
    std::string documentPath, documentURL;
    getDocumentPathAndURL("hello.odt", documentPath, documentURL);

    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
    auto socket = connectLOKit(_uri, request, _response);
    CPPUNIT_ASSERT_MESSAGE("Failed to connect.", socket);
    socket.reset();

    // Connect and load first view.
    auto socket1 = connectLOKit(_uri, request, _response);
    CPPUNIT_ASSERT_MESSAGE("Failed to connect.", socket1);
    sendTextFrame(socket1, "load url=" + documentURL);
    CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket1));

    // Connect but don't load second view.
    auto socket2 = connectLOKit(_uri, request, _response);
    CPPUNIT_ASSERT_MESSAGE("Failed to connect.", socket2);
    socket2.reset();

    sendTextFrame(socket1, "status");
    assertResponseLine(socket1, "status:");
}

void HTTPWSTest::testLoad()
{
    std::string documentPath, documentURL;
    getDocumentPathAndURL("hello.odt", documentPath, documentURL);
    loadDoc(documentURL);
}

void HTTPWSTest::testBadLoad()
{
    try
    {
        // Load a document and get its status.
        std::string documentPath, documentURL;
        getDocumentPathAndURL("hello.odt", documentPath, documentURL);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        // Before loading request status.
        sendTextFrame(socket, "status");

        int flags;
        int n;
        do
        {
            char buffer[READ_BUFFER_SIZE];
            n = socket.receiveFrame(buffer, sizeof(buffer), flags);
            std::cout << "Got " << n << " bytes, flags: " << std::hex << flags << std::dec << std::endl;
            if (n > 0 && (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE)
            {
                std::cout << "Received message: " << LOOLProtocol::getAbbreviatedMessage(buffer, n) << std::endl;
                const std::string line = LOOLProtocol::getFirstLine(buffer, n);

                // For some reason the server claims a client has the 'edit lock' even if no
                // document has been successfully loaded
                if (LOOLProtocol::getFirstToken(buffer, n) == "statusindicator:")
                    continue;

                CPPUNIT_ASSERT_EQUAL(std::string("error: cmd=status kind=nodocloaded"), line);
                break;
            }
        }
        while (n > 0 && (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testReload()
{
    std::string documentPath, documentURL;
    getDocumentPathAndURL("hello.odt", documentPath, documentURL);
    for (auto i = 0; i < 3; ++i)
    {
        loadDoc(documentURL);
    }
}

void HTTPWSTest::testGetTextSelection()
{
    std::string documentPath, documentURL;
    getDocumentPathAndURL("hello.odt", documentPath, documentURL);

    try
    {
        // Load a document and get its status.
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket));

        Poco::Net::WebSocket socket2 = *connectLOKit(_uri, request, _response);
        sendTextFrame(socket2, "load url=" + documentURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket2, "", true));

        sendTextFrame(socket, "uno .uno:SelectAll");
        sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
        const auto selection = assertResponseLine(socket, "textselectioncontent:");
        CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: Hello world"), selection);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testSaveOnDisconnect()
{
    std::string documentPath, documentURL;
    getDocumentPathAndURL("hello.odt", documentPath, documentURL);

    int kitcount = -1;
    try
    {
        // Load a document and get its status.
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket));

        Poco::Net::WebSocket socket2 = *connectLOKit(_uri, request, _response);
        sendTextFrame(socket2, "load url=" + documentURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket2, "", true));
        sendTextFrame(socket2, "userinactive");

        sendTextFrame(socket, "uno .uno:SelectAll");
        sendTextFrame(socket, "uno .uno:Delete");
        sendTextFrame(socket, "paste mimetype=text/plain;charset=utf-8\naaa bbb ccc");

        // Check if the document contains the pasted text.
        sendTextFrame(socket, "uno .uno:SelectAll");
        sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
        const auto selection = assertResponseLine(socket, "textselectioncontent:");
        CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: aaa bbb ccc"), selection);

        // Closing connection too fast might not flush buffers.
        // Often nothing more than the SelectAll reaches the server before
        // the socket is closed, when the doc is not even modified yet.
        getResponseMessage(socket, "statechanged");
        std::cerr << "Closing connection after pasting." << std::endl;

        kitcount = getLoolKitProcessCount();

        // Shutdown abruptly.
        socket.shutdown();
        socket2.shutdown();
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }

    // Allow time to save and destroy before we connect again.
    testNoExtraLoolKitsLeft();
    std::cerr << "Loading again." << std::endl;
    try
    {
        // Load the same document and check that the last changes (pasted text) is saved.
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL);
        sendTextFrame(socket, "status");
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket));

        // Should have no new instances.
        CPPUNIT_ASSERT_EQUAL(kitcount, countLoolKitProcesses(kitcount));

        // Check if the document contains the pasted text.
        sendTextFrame(socket, "uno .uno:SelectAll");
        sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
        const auto selection = assertResponseLine(socket, "textselectioncontent:");
        CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: aaa bbb ccc"), selection);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testReloadWhileDisconnecting()
{
    const auto testname = "reloadWhileDisconnecting ";

    try
    {
        std::string documentPath, documentURL;
        getDocumentPathAndURL("hello.odt", documentPath, documentURL);

        // Load a document and get its status.
        auto socket = *loadDocAndGetSocket(_uri, documentURL, testname);

        sendTextFrame(socket, "uno .uno:SelectAll");
        sendTextFrame(socket, "uno .uno:Delete");
        sendTextFrame(socket, "paste mimetype=text/plain;charset=utf-8\naaa bbb ccc");

        // Closing connection too fast might not flush buffers.
        // Often nothing more than the SelectAll reaches the server before
        // the socket is closed, when the doc is not even modified yet.
        getResponseMessage(socket, "statechanged");

        const auto kitcount = getLoolKitProcessCount();

        // Shutdown abruptly.
        std::cerr << "Closing connection after pasting." << std::endl;
        socket.shutdown();

        // Load the same document and check that the last changes (pasted text) is saved.
        std::cout << "Loading again." << std::endl;
        socket = *loadDocAndGetSocket(_uri, documentURL, testname);

        // Should have no new instances.
        CPPUNIT_ASSERT_EQUAL(kitcount, countLoolKitProcesses(kitcount));

        // Check if the document contains the pasted text.
        sendTextFrame(socket, "uno .uno:SelectAll");
        sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
        const auto selection = assertResponseLine(socket, "textselectioncontent:");
        CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: aaa bbb ccc"), selection);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testExcelLoad()
{
    try
    {
        // Load a document and make it empty.
        std::string documentPath, documentURL;
        getDocumentPathAndURL("timeline.xlsx", documentPath, documentURL);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket));
        sendTextFrame(socket, "status");
        const auto status = assertResponseLine(socket, "status:");

        // Expected format is something like 'status: type=text parts=2 current=0 width=12808 height=1142'.
        Poco::StringTokenizer tokens(status, " ", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(7), tokens.count());
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testPaste()
{
    try
    {
        // Load a document and make it empty, then paste some text into it.
        std::string documentPath, documentURL;
        getDocumentPathAndURL("hello.odt", documentPath, documentURL);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL);
        sendTextFrame(socket, "status");
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket));

        sendTextFrame(socket, "uno .uno:SelectAll");
        sendTextFrame(socket, "uno .uno:Delete");

        // Paste some text into it.
        sendTextFrame(socket, "paste mimetype=text/plain;charset=utf-8\naaa bbb ccc");

        // Check if the document contains the pasted text.
        sendTextFrame(socket, "uno .uno:SelectAll");
        sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
        const auto selection = assertResponseLine(socket, "textselectioncontent:");
        CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: aaa bbb ccc"), selection);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testLargePaste()
{
    try
    {
        // Load a document and make it empty.
        std::string documentPath, documentURL;
        getDocumentPathAndURL("hello.odt", documentPath, documentURL);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL);
        sendTextFrame(socket, "status");
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket));

        sendTextFrame(socket, "uno .uno:SelectAll");
        sendTextFrame(socket, "uno .uno:Delete");

        // Paste some text into it.
        std::ostringstream oss;
        for (auto i = 0; i < 1000; ++i)
        {
            oss << Util::encodeId(Util::rng::getNext(), 6);
        }

        const auto documentContents = oss.str();
        std::cerr << "Pasting " << documentContents.size() << " characters into document." << std::endl;
        sendTextFrame(socket, "paste mimetype=text/html\n" + documentContents);

        // Check if the server is still alive.
        // This resulted first in a hang, as respose for the message never arrived, then a bit later in a Poco::TimeoutException.
        sendTextFrame(socket, "uno .uno:SelectAll");
        sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
        const auto selection = assertResponseLine(socket, "textselectioncontent:");
        CPPUNIT_ASSERT_MESSAGE("Pasted text was either corrupted or couldn't be read back",
                               "textselectioncontent: " + documentContents == selection);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testRenderingOptions()
{
    try
    {
        // Load a document and get its size.
        std::string documentPath, documentURL;
        getDocumentPathAndURL("hide-whitespace.odt", documentPath, documentURL);

        const std::string options = "{\"rendering\":{\".uno:HideWhitespace\":{\"type\":\"boolean\",\"value\":\"true\"}}}";

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL + " options=" + options);
        sendTextFrame(socket, "status");
        const auto status = assertResponseLine(socket, "status:");

        socket.shutdown();

        // Expected format is something like 'status: type=text parts=2 current=0 width=12808 height=1142'.
        Poco::StringTokenizer tokens(status, " ", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(7), tokens.count());

        const std::string token = tokens[5];
        const std::string prefix = "height=";
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), token.find(prefix));
        const int height = std::stoi(token.substr(prefix.size()));
        // HideWhitespace was ignored, this was 32532, should be around 16706.
        CPPUNIT_ASSERT(height < 20000);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testPasswordProtectedDocumentWithoutPassword()
{
    try
    {
        std::string documentPath, documentURL;
        getDocumentPathAndURL("password-protected.ods", documentPath, documentURL);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        // Send a load request without password first
        sendTextFrame(socket, "load url=" + documentURL);
        std::string response;

        getResponseMessage(socket, "error:", response, true);
        CPPUNIT_ASSERT_MESSAGE("did not receive an error: message as expected", !response.empty());
        {
            Poco::StringTokenizer tokens(response, " ", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
            CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), tokens.count());

            std::string errorCommand;
            std::string errorKind;
            LOOLProtocol::getTokenString(tokens[0], "cmd", errorCommand);
            LOOLProtocol::getTokenString(tokens[1], "kind", errorKind);
            CPPUNIT_ASSERT_EQUAL(std::string("load"), errorCommand);
            CPPUNIT_ASSERT_EQUAL(std::string("passwordrequired:to-view"), errorKind);
        }
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testPasswordProtectedDocumentWithWrongPassword()
{
    try
    {
        std::string documentPath, documentURL;
        getDocumentPathAndURL("password-protected.ods", documentPath, documentURL);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        // Send a load request with incorrect password
        sendTextFrame(socket, "load url=" + documentURL + " password=2");

        std::string response;
        getResponseMessage(socket, "error:", response, true);
        CPPUNIT_ASSERT_MESSAGE("did not receive an error: message as expected", !response.empty());
        {
            Poco::StringTokenizer tokens(response, " ", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
            CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), tokens.count());

            std::string errorCommand;
            std::string errorKind;
            LOOLProtocol::getTokenString(tokens[0], "cmd", errorCommand);
            LOOLProtocol::getTokenString(tokens[1], "kind", errorKind);
            CPPUNIT_ASSERT_EQUAL(std::string("load"), errorCommand);
            CPPUNIT_ASSERT_EQUAL(std::string("wrongpassword"), errorKind);
        }
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testPasswordProtectedDocumentWithCorrectPassword()
{
    try
    {
        const std::string documentPath = Util::getTempFilePath(TDOC, "password-protected.ods");
        const std::string documentURL = "lool/ws/file://" + Poco::Path(documentPath).makeAbsolute().toString();

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        // Send a load request with correct password
        sendTextFrame(socket, "load url=" + documentURL + " password=1");

        CPPUNIT_ASSERT_MESSAGE("cannot load the document with correct password " + documentURL, isDocumentLoaded(socket));
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testPasswordProtectedDocumentWithCorrectPasswordAgain()
{
    testPasswordProtectedDocumentWithCorrectPassword();
}

void HTTPWSTest::testInsertDelete()
{
    try
    {
        std::vector<std::string> parts;
        std::string response;

        // Load a document
        std::string documentPath, documentURL;
        getDocumentPathAndURL("insert-delete.odp", documentPath, documentURL);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket));

        // check total slides 1
        std::cerr << "Expecting 1 slide." << std::endl;
        sendTextFrame(socket, "status");
        getResponseMessage(socket, "status:", response, false);
        CPPUNIT_ASSERT_MESSAGE("did not receive a status: message as expected", !response.empty());
        getPartHashCodes(response, parts);
        CPPUNIT_ASSERT_EQUAL(1, (int)parts.size());

        const auto slide1Hash = parts[0];

        // insert 10 slides
        std::cerr << "Inserting 10 slides." << std::endl;
        for (size_t it = 1; it <= 10; it++)
        {
            sendTextFrame(socket, "uno .uno:InsertPage");
            getResponseMessage(socket, "status:", response, false);
            CPPUNIT_ASSERT_MESSAGE("did not receive a status: message as expected", !response.empty());
            getPartHashCodes(response, parts);
            CPPUNIT_ASSERT_EQUAL(it + 1, parts.size());
        }

        CPPUNIT_ASSERT_MESSAGE("Hash code of slide #1 changed after inserting extra slides.", parts[0] == slide1Hash);
        const std::vector<std::string> parts_after_insert(parts.begin(), parts.end());

        // delete 10 slides
        std::cerr << "Deleting 10 slides." << std::endl;
        for (size_t it = 1; it <= 10; it++)
        {
            sendTextFrame(socket, "uno .uno:DeletePage");
            getResponseMessage(socket, "status:", response, false);
            CPPUNIT_ASSERT_MESSAGE("did not receive a status: message as expected", !response.empty());
            getPartHashCodes(response, parts);
            CPPUNIT_ASSERT_EQUAL(11 - it, parts.size());
        }

        CPPUNIT_ASSERT_MESSAGE("Hash code of slide #1 changed after deleting extra slides.", parts[0] == slide1Hash);

        // undo delete slides
        std::cerr << "Undoing 10 slide deletes." << std::endl;
        for (size_t it = 1; it <= 10; it++)
        {
            sendTextFrame(socket, "uno .uno:Undo");
            getResponseMessage(socket, "status:", response, false);
            CPPUNIT_ASSERT_MESSAGE("did not receive a status: message as expected", !response.empty());
            getPartHashCodes(response, parts);
            CPPUNIT_ASSERT_EQUAL(it + 1, parts.size());
        }

        CPPUNIT_ASSERT_MESSAGE("Hash code of slide #1 changed after undoing slide delete.", parts[0] == slide1Hash);
        const std::vector<std::string> parts_after_undo(parts.begin(), parts.end());
        CPPUNIT_ASSERT_MESSAGE("Hash codes changed between deleting and undo.", parts_after_insert == parts_after_undo);

        // redo inserted slides
        std::cerr << "Redoing 10 slide deletes." << std::endl;
        for (size_t it = 1; it <= 10; it++)
        {
            sendTextFrame(socket, "uno .uno:Redo");
            getResponseMessage(socket, "status:", response, false);
            CPPUNIT_ASSERT_MESSAGE("did not receive a status: message as expected", !response.empty());
            getPartHashCodes(response, parts);
            CPPUNIT_ASSERT_EQUAL(11 - it, parts.size());
        }

        CPPUNIT_ASSERT_MESSAGE("Hash code of slide #1 changed after redoing slide delete.", parts[0] == slide1Hash);

        // check total slides 1
        std::cerr << "Expecting 1 slide." << std::endl;
        sendTextFrame(socket, "status");
        getResponseMessage(socket, "status:", response, false);
        CPPUNIT_ASSERT_MESSAGE("did not receive a status: message as expected", !response.empty());
        getPartHashCodes(response, parts);
        CPPUNIT_ASSERT_EQUAL(1, (int)parts.size());
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testSlideShow()
{
    try
    {
        // Load a document
        std::string documentPath, documentURL;
        std::string response;
        getDocumentPathAndURL("setclientpart.odp", documentPath, documentURL);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket));

        // request slide show
        sendTextFrame(socket, "downloadas name=slideshow.svg id=slideshow format=svg options=");
        getResponseMessage(socket, "downloadas:", response, false);
        CPPUNIT_ASSERT_MESSAGE("did not receive a downloadas: message as expected", !response.empty());
        {
            Poco::StringTokenizer tokens(response, " ", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
            // "downloadas: jail= dir= name=slideshow.svg port= id=slideshow"
            const std::string jail = tokens[0].substr(std::string("jail=").size());
            const std::string dir = tokens[1].substr(std::string("dir=").size());
            const std::string name = tokens[2].substr(std::string("name=").size());
            const int port = std::stoi(tokens[3].substr(std::string("port=").size()));
            const std::string id = tokens[4].substr(std::string("id=").size());
            CPPUNIT_ASSERT(!jail.empty());
            CPPUNIT_ASSERT(!dir.empty());
            CPPUNIT_ASSERT_EQUAL(std::string("slideshow.svg"), name);
            CPPUNIT_ASSERT_EQUAL(static_cast<int>(_uri.getPort()), port);
            CPPUNIT_ASSERT_EQUAL(std::string("slideshow"), id);

            const std::string path = "/lool/" + jail + "/" + dir + "/" + name + "?mime_type=image/svg%2Bxml";
            std::unique_ptr<Poco::Net::HTTPClientSession> session(helpers::createSession(_uri));
            Poco::Net::HTTPRequest requestSVG(Poco::Net::HTTPRequest::HTTP_GET, path);
            session->sendRequest(requestSVG);

            Poco::Net::HTTPResponse responseSVG;
            session->receiveResponse(responseSVG);
            CPPUNIT_ASSERT_EQUAL(Poco::Net::HTTPResponse::HTTP_OK, responseSVG.getStatus());
            CPPUNIT_ASSERT_EQUAL(std::string("image/svg+xml"), responseSVG.getContentType());
        }
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testInactiveClient()
{
    try
    {
        std::string documentPath, documentURL;
        getDocumentPathAndURL("hello.odt", documentPath, documentURL);

        auto socket1 = loadDocAndGetSocket(_uri, documentURL, "inactiveClient-1 ");

        // Connect another and go inactive.
        std::cerr << "Connecting second client." << std::endl;
        auto socket2 = loadDocAndGetSocket(_uri, documentURL, "inactiveClient-2 ", true);
        sendTextFrame(socket2, "userinactive", "inactiveClient-2 ");

        // While second is inactive, make some changes.
        sendTextFrame(socket1, "uno .uno:SelectAll", "inactiveClient-1 ");
        sendTextFrame(socket1, "uno .uno:Delete", "inactiveClient-1 ");

        // Activate second.
        sendTextFrame(socket2, "useractive", "inactiveClient-2 ");
        SocketProcessor("Second ", socket2, [&](const std::string& msg)
                {
                    const auto token = LOOLProtocol::getFirstToken(msg);
                    CPPUNIT_ASSERT_MESSAGE("unexpected message: " + msg,
                                            token == "cursorvisible:" ||
                                            token == "graphicselection:" ||
                                            token == "graphicviewselection:" ||
                                            token == "invalidatecursor:" ||
                                            token == "invalidatetiles:" ||
                                            token == "invalidateviewcursor:" ||
                                            token == "setpart:" ||
                                            token == "statechanged:" ||
                                            token == "textselection:" ||
                                            token == "textselectionend:" ||
                                            token == "textselectionstart:" ||
                                            token == "textviewselection:" ||
                                            token == "viewcursorvisible:" ||
                                            token == "viewinfo:");

                    // End when we get state changed.
                    return (token != "statechanged:");
                });

        std::cerr << "Second client finished." << std::endl;
        socket1->shutdown();
        socket2->shutdown();
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testMaxColumn()
{
    try
    {
        testLimitCursor(
            // move cursor to last column
            [](const std::shared_ptr<Poco::Net::WebSocket>& socket,
               int cursorX, int cursorY, int cursorWidth, int cursorHeight,
               int docWidth, int docHeight)
            {
                CPPUNIT_ASSERT(cursorX >= 0);
                CPPUNIT_ASSERT(cursorY >= 0);
                CPPUNIT_ASSERT(cursorWidth >= 0);
                CPPUNIT_ASSERT(cursorHeight >= 0);
                CPPUNIT_ASSERT(docWidth >= 0);
                CPPUNIT_ASSERT(docHeight >= 0);

                const std::string text = "key type=input char=0 key=1027";
                while ( cursorX <= docWidth )
                {
                    sendTextFrame(socket, text);
                    cursorX += cursorWidth;
                }
            },
            // check new document width
            [](int docWidth, int docHeight, int newWidth, int newHeight)
            {
                CPPUNIT_ASSERT_EQUAL(docHeight, newHeight);
                CPPUNIT_ASSERT(newWidth > docWidth);
            }

        );
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testMaxRow()
{
    try
    {
        testLimitCursor(
            // move cursor to last row
            [](const std::shared_ptr<Poco::Net::WebSocket>& socket,
               int cursorX, int cursorY, int cursorWidth, int cursorHeight,
               int docWidth, int docHeight)
            {
                CPPUNIT_ASSERT(cursorX >= 0);
                CPPUNIT_ASSERT(cursorY >= 0);
                CPPUNIT_ASSERT(cursorWidth >= 0);
                CPPUNIT_ASSERT(cursorHeight >= 0);
                CPPUNIT_ASSERT(docWidth >= 0);
                CPPUNIT_ASSERT(docHeight >= 0);

                const std::string text = "key type=input char=0 key=1024";
                while ( cursorY <= docHeight )
                {
                    sendTextFrame(socket, text);
                    cursorY += cursorHeight;
                }
            },
            // check new document height
            [](int docWidth, int docHeight, int newWidth, int newHeight)
            {
                CPPUNIT_ASSERT_EQUAL(docWidth, newWidth);
                CPPUNIT_ASSERT(newHeight > docHeight);
            }

        );
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testEmptyCellCursor()
{
    // Load a document, and make sure a cell cursor shows up.
    std::string docPath;
    std::string docURL;
    getDocumentPathAndURL("setclientpart.ods", docPath, docURL);
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, docURL);
    std::shared_ptr<Poco::Net::WebSocket> socket = loadDocAndGetSocket(_uri, docURL);
    std::string response;
    getResponseMessage(socket, "cellcursor:", response, false);

    // Begin text edit of a cell.
    sendTextFrame(socket, "key type=input char=115 key=97");

    // Make sure the cell cursor is now hidden.
    getResponseMessage(socket, "cellcursor: ", response, false);
    // This failed as response was "", not "EMPTY", because code in
    // Document::ViewCallback() crashed.
    CPPUNIT_ASSERT_EQUAL(std::string("EMPTY"), response);
}

void HTTPWSTest::testNoExtraLoolKitsLeft()
{
    const auto countNow = countLoolKitProcesses(InitialLoolKitCount);
    CPPUNIT_ASSERT_EQUAL(InitialLoolKitCount, countNow);
}

void HTTPWSTest::getPartHashCodes(const std::string status,
                                  std::vector<std::string>& parts)
{
    std::string line;
    std::istringstream istr(status);
    std::getline(istr, line);

    std::cerr << "Reading parts from [" << status << "]." << std::endl;

    // Expected format is something like 'type= parts= current= width= height= viewid='.
    Poco::StringTokenizer tokens(line, " ", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(6), tokens.count());

    const auto type = tokens[0].substr(std::string("type=").size());
    CPPUNIT_ASSERT_MESSAGE("Expected presentation or spreadsheet type to read part names/codes.",
                           type == "presentation" || type == "spreadsheet");

    const int totalParts = std::stoi(tokens[1].substr(std::string("parts=").size()));
    std::cerr << "Status reports " << totalParts << " parts." << std::endl;

    Poco::RegularExpression endLine("[^\n\r]+");
    Poco::RegularExpression number("^[0-9]+$");
    Poco::RegularExpression::MatchVec matches;
    int offset = 0;

    parts.clear();
    while (endLine.match(status, offset, matches) > 0)
    {
        CPPUNIT_ASSERT_EQUAL(1, (int)matches.size());
        const auto str = status.substr(matches[0].offset, matches[0].length);
        if (number.match(str, 0) > 0)
        {
            parts.push_back(str);
        }

        offset = static_cast<int>(matches[0].offset + matches[0].length);
    }

    std::cerr << "Found " << parts.size() << " part names/codes." << std::endl;

    // Validate that Core is internally consistent when emitting status messages.
    CPPUNIT_ASSERT_EQUAL(totalParts, (int)parts.size());
}

void HTTPWSTest::getCursor(const std::string& message,
                           int& cursorX, int& cursorY, int& cursorWidth, int& cursorHeight)
{
    Poco::JSON::Parser parser;
    const auto result = parser.parse(message);
    const auto& command = result.extract<Poco::JSON::Object::Ptr>();
    auto text = command->get("commandName").toString();
    CPPUNIT_ASSERT_EQUAL(std::string(".uno:CellCursor"), text);
    text = command->get("commandValues").toString();
    CPPUNIT_ASSERT(!text.empty());
    Poco::StringTokenizer position(text, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
    cursorX = std::stoi(position[0]);
    cursorY = std::stoi(position[1]);
    cursorWidth = std::stoi(position[2]);
    cursorHeight = std::stoi(position[3]);
    CPPUNIT_ASSERT(cursorX >= 0);
    CPPUNIT_ASSERT(cursorY >= 0);
    CPPUNIT_ASSERT(cursorWidth >= 0);
    CPPUNIT_ASSERT(cursorHeight >= 0);
}

void HTTPWSTest::testLimitCursor( std::function<void(const std::shared_ptr<Poco::Net::WebSocket>& socket,
                                                     int cursorX, int cursorY,
                                                     int cursorWidth, int cursorHeight,
                                                     int docWidth, int docHeight)> keyhandler,
                                  std::function<void(int docWidth, int docHeight,
                                                     int newWidth, int newHeight)> checkhandler)

{
    int docSheet = -1;
    int docSheets = 0;
    int docHeight = 0;
    int docWidth = 0;
    int docViewId = -1;
    int newSheet = -1;
    int newSheets = 0;
    int newHeight = 0;
    int newWidth = 0;
    int cursorX = 0;
    int cursorY = 0;
    int cursorWidth = 0;
    int cursorHeight = 0;

    std::string docPath;
    std::string docURL;
    std::string response;
    std::string text;

    getDocumentPathAndURL("setclientpart.ods", docPath, docURL);
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, docURL);

    auto socket = loadDocAndGetSocket(_uri, docURL, "limitCursor ");
    // check document size
    sendTextFrame(socket, "status");
    getResponseMessage(socket, "status:", response, false);
    CPPUNIT_ASSERT_MESSAGE("did not receive a status: message as expected", !response.empty());
    parseDocSize(response, "spreadsheet", docSheet, docSheets, docWidth, docHeight, docViewId);

    // Send an arrow key to initialize the CellCursor, otherwise we get "EMPTY".
    sendTextFrame(socket, "key type=input char=0 key=1027");

    text.clear();
    Poco::format(text, "commandvalues command=.uno:CellCursor?outputHeight=%d&outputWidth=%d&tileHeight=%d&tileWidth=%d",
        256, 256, 3840, 3840);
    sendTextFrame(socket, text);
    getResponseMessage(socket, "commandvalues:", response, false);
    CPPUNIT_ASSERT_MESSAGE("did not receive a commandvalues: message as expected", !response.empty());
    getCursor(response, cursorX, cursorY, cursorWidth, cursorHeight);

    // move cursor
    keyhandler(socket, cursorX, cursorY, cursorWidth, cursorHeight, docWidth, docHeight);

    // filter messages, and expect to receive new document size
    getResponseMessage(socket, "status:", response, false);
    CPPUNIT_ASSERT_MESSAGE("did not receive a status: message as expected", !response.empty());
    parseDocSize(response, "spreadsheet", newSheet, newSheets, newWidth, newHeight, docViewId);

    CPPUNIT_ASSERT_EQUAL(docSheets, newSheets);
    CPPUNIT_ASSERT_EQUAL(docSheet, newSheet);

    // check new document size
    checkhandler(docWidth, docHeight, newWidth, newHeight);
}

void HTTPWSTest::testInsertAnnotationWriter()
{
    const auto testname = "insertAnnotationWriter ";

    std::string documentPath, documentURL;
    getDocumentPathAndURL("hello.odt", documentPath, documentURL);
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);

    auto socket = loadDocAndGetSocket(_uri, documentURL, testname);

    // Insert comment.
    sendTextFrame(socket, "uno .uno:InsertAnnotation");

    // Paste some text.
    sendTextFrame(socket, "paste mimetype=text/plain;charset=utf-8\nxxx yyy zzzz");

    // Read it back.
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    auto res = getResponseLine(socket, "textselectioncontent:", "insertAnnotationWriter ");
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: xxx yyy zzzz"), res);
    // Can we edit the coment?
    sendTextFrame(socket, "paste mimetype=text/plain;charset=utf-8\naaa bbb ccc");
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", "insertAnnotationWriter ");
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: aaa bbb ccc"), res);

    // Confirm that the text is in the comment and not doc body.
    // Click in the body.
    sendTextFrame(socket, "mouse type=buttondown x=1600 y=1600 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "mouse type=buttonup x=1600 y=1600 count=1 buttons=1 modifier=0");
    // Read body text.
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", "insertAnnotationWriter ");
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: Hello world"), res);

    // Confirm that the comment is still intact.
    sendTextFrame(socket, "mouse type=buttondown x=13855 y=1893 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "mouse type=buttonup x=13855 y=1893 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", "insertAnnotationWriter ");
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: aaa bbb ccc"), res);

    // Can we still edit the coment?
    sendTextFrame(socket, "paste mimetype=text/plain;charset=utf-8\nand now for something completely different");
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", "insertAnnotationWriter ");
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: and now for something completely different"), res);

    // Close and reopen the same document and test again.
    socket->shutdown();
    std::cerr << "Reloading " << std::endl;
    socket = loadDocAndGetSocket(_uri, documentURL, testname);

    // Confirm that the text is in the comment and not doc body.
    // Click in the body.
    sendTextFrame(socket, "mouse type=buttondown x=1600 y=1600 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "mouse type=buttonup x=1600 y=1600 count=1 buttons=1 modifier=0");
    // Read body text.
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", "insertAnnotationWriter ");
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: Hello world"), res);

    // Confirm that the comment is still intact.
    sendTextFrame(socket, "mouse type=buttondown x=13855 y=1893 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "mouse type=buttonup x=13855 y=1893 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", "insertAnnotationWriter ");
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: and now for something completely different"), res);

    // Can we still edit the coment?
    sendTextFrame(socket, "paste mimetype=text/plain;charset=utf-8\nblah blah xyz");
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", "insertAnnotationWriter ");
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: blah blah xyz"), res);
}

void HTTPWSTest::testEditAnnotationWriter()
{
    const auto testname = "editAnnotationWriter ";

    std::string documentPath, documentURL;
    getDocumentPathAndURL("with_comment.odt", documentPath, documentURL);

    auto socket = loadDocAndGetSocket(_uri, documentURL, testname);

    // Click in the body.
    sendTextFrame(socket, "mouse type=buttondown x=1600 y=1600 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "mouse type=buttonup x=1600 y=1600 count=1 buttons=1 modifier=0");
    // Read body text.
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    auto res = getResponseLine(socket, "textselectioncontent:", testname);
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: Hello world"), res);

    // Confirm that the comment is intact.
    sendTextFrame(socket, "mouse type=buttondown x=13855 y=1893 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "mouse type=buttonup x=13855 y=1893 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", testname);
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: blah blah xyz"), res);

    // Can we still edit the coment?
    sendTextFrame(socket, "paste mimetype=text/plain;charset=utf-8\nand now for something completely different");
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", testname);
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: and now for something completely different"), res);

    const auto kitcount = getLoolKitProcessCount();

    // Close and reopen the same document and test again.
    std::cerr << "Closing connection after pasting." << std::endl;
    socket->shutdown();

    std::cerr << "Reloading " << std::endl;
    socket = loadDocAndGetSocket(_uri, documentURL, testname);

    // Should have no new instances.
    CPPUNIT_ASSERT_EQUAL(kitcount, countLoolKitProcesses(kitcount));

    // Confirm that the text is in the comment and not doc body.
    // Click in the body.
    sendTextFrame(socket, "mouse type=buttondown x=1600 y=1600 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "mouse type=buttonup x=1600 y=1600 count=1 buttons=1 modifier=0");
    // Read body text.
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", testname);
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: Hello world"), res);

    // Confirm that the comment is still intact.
    sendTextFrame(socket, "mouse type=buttondown x=13855 y=1893 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "mouse type=buttonup x=13855 y=1893 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", testname);
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: and now for something completely different"), res);

    // Can we still edit the coment?
    sendTextFrame(socket, "paste mimetype=text/plain;charset=utf-8\nnew text different");
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    res = getResponseLine(socket, "textselectioncontent:", testname);
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: new text different"), res);
}

void HTTPWSTest::testInsertAnnotationCalc()
{
    const auto testname = "insertAnnotationCalc ";
    auto socket = loadDocAndGetSocket("setclientpart.ods", _uri, testname);

    // Insert comment.
    sendTextFrame(socket, "uno .uno:InsertAnnotation");

    // Paste some text.
    sendTextFrame(socket, "paste mimetype=text/plain;charset=utf-8\naaa bbb ccc");

    // Read it back.
    sendTextFrame(socket, "uno .uno:SelectAll");
    sendTextFrame(socket, "gettextselection mimetype=text/plain;charset=utf-8");
    auto res = getResponseLine(socket, "textselectioncontent:", testname);
    CPPUNIT_ASSERT_EQUAL(std::string("textselectioncontent: aaa bbb ccc"), res);
}

void HTTPWSTest::testCalcEditRendering()
{
    const auto testname = "calcEditRendering ";
    auto socket = loadDocAndGetSocket("calc_render.xls", _uri, testname);

    sendTextFrame(socket, "mouse type=buttondown x=5000 y=5 count=1 buttons=1 modifier=0");
    sendTextFrame(socket, "key type=input char=97 key=0");
    sendTextFrame(socket, "key type=input char=98 key=0");
    sendTextFrame(socket, "key type=input char=99 key=0");

    assertResponseLine(socket, "cellformula: abc", testname);

    const auto req = "tilecombine part=0 width=512 height=512 tileposx=3840 tileposy=0 tilewidth=7680 tileheight=7680";
    sendTextFrame(socket, req);

    const auto tile = getResponseMessage(socket, "tile:", testname);
    std::cout << "size: " << tile.size() << std::endl;

    // Return early for now when on LO >= 5.2.
    std::string clientVersion = "loolclient 0.1";
    sendTextFrame(socket, clientVersion);
    std::vector<char> loVersion = getResponseMessage(socket, "lokitversion");
    std::string line = LOOLProtocol::getFirstLine(loVersion.data(), loVersion.size());
    line = line.substr(strlen("lokitversion "));
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var loVersionVar = parser.parse(line);
    const Poco::SharedPtr<Poco::JSON::Object>& loVersionObject = loVersionVar.extract<Poco::JSON::Object::Ptr>();
    std::string loProductVersion = loVersionObject->get("ProductVersion").toString();
    std::istringstream stream(loProductVersion);
    int major = 0;
    stream >> major;
    assert(stream.get() == '.');
    int minor = 0;
    stream >> minor;
    if (major > 5 || (major == 5 && minor >= 2))
        return;

    const std::string firstLine = LOOLProtocol::getFirstLine(tile);
    std::vector<char> res(tile.begin() + firstLine.size() + 1, tile.end());
    std::stringstream streamRes;
    std::copy(res.begin(), res.end(), std::ostream_iterator<char>(streamRes));

    std::fstream outStream("/tmp/res.png", std::ios::out);
    outStream.write(res.data(), res.size());
    outStream.close();

    png_uint_32 height = 0;
    png_uint_32 width = 0;
    png_uint_32 rowBytes = 0;
    auto rows = png::decodePNG(streamRes, height, width, rowBytes);

    const std::vector<char> exp = readDataFromFile("calc_render_0_512x512.3840,0.7680x7680.png");
    std::stringstream streamExp;
    std::copy(exp.begin(), exp.end(), std::ostream_iterator<char>(streamExp));

    png_uint_32 heightExp = 0;
    png_uint_32 widthExp = 0;
    png_uint_32 rowBytesExp = 0;
    auto rowsExp = png::decodePNG(streamExp, heightExp, widthExp, rowBytesExp);

    CPPUNIT_ASSERT_EQUAL(heightExp, height);
    CPPUNIT_ASSERT_EQUAL(widthExp, width);
    CPPUNIT_ASSERT_EQUAL(rowBytesExp, rowBytes);

    for (png_uint_32 itRow = 0; itRow < height; ++itRow)
    {
        const bool eq = std::equal(rowsExp[itRow], rowsExp[itRow] + rowBytes, rows[itRow]);
        CPPUNIT_ASSERT_MESSAGE("Tile not rendered as expected @ row #" + std::to_string(itRow), eq);
    }
}

std::string HTTPWSTest::getFontList(const std::string& message)
{
    Poco::JSON::Parser parser;
    const auto result = parser.parse(message);
    const auto& command = result.extract<Poco::JSON::Object::Ptr>();
    auto text = command->get("commandName").toString();
    CPPUNIT_ASSERT_EQUAL(std::string(".uno:CharFontName"), text);
    text = command->get("commandValues").toString();
    return text;
}

void HTTPWSTest::testFontList()
{
    try
    {
        // Load a document
        std::string documentPath, documentURL;
        std::string text;
        std::vector<char> response;

        getDocumentPathAndURL("setclientpart.odp", documentPath, documentURL);
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + documentURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL, isDocumentLoaded(socket));

        sendTextFrame(socket, "commandvalues command=.uno:CharFontName");
        response = getResponseMessage(socket, "commandvalues:", "testFontList ");
        CPPUNIT_ASSERT_MESSAGE("did not receive a commandvalues: message as expected", !response.empty());

        std::stringstream streamResponse;
        std::copy(response.begin() + std::string("commandvalues:").length() + 1, response.end(), std::ostream_iterator<char>(streamResponse));
        text = getFontList(streamResponse.str());
        CPPUNIT_ASSERT(!text.empty());
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testStateChanged(const std::string& filename, std::vector<std::string>& vecCommands)
{
    std::string docPath;
    std::string docURL;
    std::string response;
    std::string text;

    getDocumentPathAndURL(filename.c_str(), docPath, docURL);
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, docURL);

    auto socket = loadDocAndGetSocket(_uri, docURL, "testCommands ");
    SocketProcessor("", socket,
        [&](const std::string& msg)
        {
            Poco::RegularExpression::MatchVec matches;
            Poco::RegularExpression reUno("\\.[a-zA-Z]*\\:[a-zA-Z]*\\=");

            if (reUno.match(msg, 0, matches) > 0 && matches.size() == 1)
            {
                const auto str = msg.substr(matches[0].offset, matches[0].length);
                auto result = std::find(std::begin(vecCommands), std::end(vecCommands), str);

                if (result != std::end(vecCommands))
                {
                    vecCommands.erase(result);
                }
            }

            if (vecCommands.size() == 0)
                return false;

            return true;
        });

    if (vecCommands.size() > 0 )
    {
        std::ostringstream ostr;

        ostr << filename << " : Missing Uno Commands: " << std::endl;
        for (auto itUno : vecCommands)
            ostr << itUno << std::endl;

        CPPUNIT_FAIL(ostr.str());
    }
}

void HTTPWSTest::testStateUnoCommand()
{
    std::vector<std::string> calcCommands
    {
        ".uno:BackgroundColor=",
        ".uno:Bold=",
        ".uno:CenterPara=",
        ".uno:CharBackColor=",
        ".uno:CharFontName=",
        ".uno:Color=",
        ".uno:FontHeight=",
        ".uno:Italic=",
        ".uno:JustifyPara=",
        ".uno:OutlineFont=",
        ".uno:LeftPara=",
        ".uno:RightPara=",
        ".uno:Shadowed=",
        ".uno:SubScript=",
        ".uno:SuperScript=",
        ".uno:Strikeout=",
        ".uno:StyleApply=",
        ".uno:Underline=",
        ".uno:ModifiedStatus=",
        ".uno:Undo=",
        ".uno:Redo=",
        ".uno:Cut=",
        ".uno:Copy=",
        ".uno:Paste=",
        ".uno:SelectAll=",
        ".uno:InsertAnnotation=",
        ".uno:InsertRowsBefore=",
        ".uno:InsertRowsAfter=",
        ".uno:InsertColumnsBefore=",
        ".uno:InsertColumnsAfter=",
        ".uno:DeleteRows=",
        ".uno:DeleteColumns=",
        ".uno:MergeCells=",
        ".uno:StatusDocPos=",
        ".uno:RowColSelCount=",
        ".uno:StatusPageStyle=",
        ".uno:InsertMode=",
        ".uno:StatusSelectionMode=",
        ".uno:StateTableCell=",
        ".uno:StatusBarFunc=",
        ".uno:WrapText=",
        ".uno:ToggleMergeCells=",
        ".uno:NumberFormatCurrency=",
        ".uno:NumberFormatPercent=",
        ".uno:NumberFormatDate="
    };

    std::vector<std::string> writerCommands
    {
        ".uno:BackColor=",
        ".uno:BackgroundColor=",
        ".uno:Bold=",
        ".uno:CenterPara=",
        ".uno:CharBackColor=",
        ".uno:CharBackgroundExt=",
        ".uno:CharFontName=",
        ".uno:Color=",
        ".uno:DefaultBullet=",
        ".uno:DefaultNumbering=",
        ".uno:FontColor=",
        ".uno:FontHeight=",
        ".uno:Italic=",
        ".uno:JustifyPara=",
        ".uno:OutlineFont=",
        ".uno:LeftPara=",
        ".uno:RightPara=",
        ".uno:Shadowed=",
        ".uno:SubScript=",
        ".uno:SuperScript=",
        ".uno:Strikeout=",
        ".uno:StyleApply=",
        ".uno:Underline=",
        ".uno:ModifiedStatus=",
        ".uno:Undo=",
        ".uno:Redo=",
        ".uno:Cut=",
        ".uno:Copy=",
        ".uno:Paste=",
        ".uno:SelectAll=",
        ".uno:InsertAnnotation=",
        ".uno:InsertRowsBefore=",
        ".uno:InsertRowsAfter=",
        ".uno:InsertColumnsBefore=",
        ".uno:InsertColumnsAfter=",
        ".uno:DeleteRows=",
        ".uno:DeleteColumns=",
        ".uno:DeleteTable=",
        ".uno:SelectTable=",
        ".uno:EntireRow=",
        ".uno:EntireColumn=",
        ".uno:EntireCell=",
        ".uno:MergeCells=",
        ".uno:InsertMode=",
        ".uno:StateTableCell=",
        ".uno:StatePageNumber=",
        ".uno:StateWordCount=",
        ".uno:SelectionMode=",
        ".uno:NumberFormatCurrency=",
        ".uno:NumberFormatPercent=",
        ".uno:NumberFormatDate="
    };

    std::vector<std::string> impressCommands
    {
        ".uno:Bold=",
        ".uno:CenterPara=",
        ".uno:CharBackColor=",
        ".uno:CharFontName=",
        ".uno:Color=",
        ".uno:DefaultBullet=",
        ".uno:DefaultNumbering=",
        ".uno:FontHeight=",
        ".uno:Italic=",
        ".uno:JustifyPara=",
        ".uno:OutlineFont=",
        ".uno:LeftPara=",
        ".uno:RightPara=",
        ".uno:Shadowed=",
        ".uno:SubScript=",
        ".uno:SuperScript=",
        ".uno:Strikeout=",
        ".uno:StyleApply=",
        ".uno:Underline=",
        ".uno:ModifiedStatus=",
        ".uno:Undo=",
        ".uno:Redo=",
        ".uno:InsertPage=",
        ".uno:DeletePage=",
        ".uno:DuplicatePage=",
        ".uno:Cut=",
        ".uno:Copy=",
        ".uno:Paste=",
        ".uno:SelectAll=",
        ".uno:InsertAnnotation=",
        ".uno:InsertRowsBefore=",
        ".uno:InsertRowsAfter=",
        ".uno:InsertColumnsBefore=",
        ".uno:InsertColumnsAfter=",
        ".uno:DeleteRows=",
        ".uno:DeleteColumns=",
        ".uno:SelectTable=",
        ".uno:EntireRow=",
        ".uno:EntireColumn=",
        ".uno:MergeCells=",
        ".uno:AssignLayout=",
        ".uno:PageStatus=",
        ".uno:LayoutStatus=",
        ".uno:Context=",
    };

    try
    {
        testStateChanged(std::string("setclientpart.ods"), calcCommands);
        testStateChanged(std::string("hello.odt"), writerCommands);
        testStateChanged(std::string("setclientpart.odp"), impressCommands);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

double HTTPWSTest::getColRowSize(const std::string& property, const std::string& message, int index)
{
    Poco::JSON::Parser parser;
    const auto result = parser.parse(message);
    const auto& command = result.extract<Poco::JSON::Object::Ptr>();
    auto text = command->get("commandName").toString();

    CPPUNIT_ASSERT_EQUAL(std::string(".uno:ViewRowColumnHeaders"), text);
    CPPUNIT_ASSERT(command->isArray(property));

    auto array = command->getArray(property);

    CPPUNIT_ASSERT(array->isObject(index));

    auto item = array->getObject(index);

    CPPUNIT_ASSERT(item->has("size"));

    return item->getValue<double>("size");
}

double HTTPWSTest::getColRowSize(const std::shared_ptr<Poco::Net::WebSocket>& socket, const std::string& item, int index)
{
    std::vector<char> response;
    response = getResponseMessage(socket, "commandvalues:", "testColumnRowResize ");
    CPPUNIT_ASSERT_MESSAGE("did not receive a commandvalues: message as expected", !response.empty());
    std::vector<char> json(response.begin() + std::string("commandvalues:").length(), response.end());
    json.push_back(0);
    return getColRowSize(item, json.data(), index);
}

void HTTPWSTest::testColumnRowResize()
{
    try
    {
        std::vector<char> response;
        std::string documentPath, documentURL;
        double oldHeight, oldWidth;

        getDocumentPathAndURL("setclientpart.ods", documentPath, documentURL);
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);

        auto socket = loadDocAndGetSocket(_uri, documentURL, "testColumnRowResize ");

        const std::string commandValues = "commandvalues command=.uno:ViewRowColumnHeaders";
        sendTextFrame(socket, commandValues);
        response = getResponseMessage(socket, "commandvalues:", "testColumnRowResize ");
        CPPUNIT_ASSERT_MESSAGE("did not receive a commandvalues: message as expected", !response.empty());
        {
            std::vector<char> json(response.begin() + std::string("commandvalues:").length(), response.end());
            json.push_back(0);

            // get column 2
            oldHeight = getColRowSize("rows", json.data(), 1);
            // get row 2
            oldWidth = getColRowSize("columns", json.data(), 1);
        }

        // send column width
        {
            std::ostringstream oss;
            Poco::JSON::Object objJSON, objColumn, objWidth;
            double newWidth;

            // change column 2
            objColumn.set("type", "unsigned short");
            objColumn.set("value", 2);

            objWidth.set("type", "unsigned short");
            objWidth.set("value", oldWidth + 100);

            objJSON.set("Column", objColumn);
            objJSON.set("Width", objWidth);

            Poco::JSON::Stringifier::stringify(objJSON, oss);
            sendTextFrame(socket, "uno .uno:ColumnWidth " + oss.str());
            sendTextFrame(socket, commandValues);
            response = getResponseMessage(socket, "commandvalues:", "testColumnRowResize ");
            CPPUNIT_ASSERT_MESSAGE("did not receive a commandvalues: message as expected", !response.empty());
            std::vector<char> json(response.begin() + std::string("commandvalues:").length(), response.end());
            json.push_back(0);
            newWidth = getColRowSize("columns", json.data(), 1);
            CPPUNIT_ASSERT(newWidth > oldWidth);
        }

        // send row height
        {
            std::ostringstream oss;
            Poco::JSON::Object objJSON, objRow, objHeight;
            double newHeight;

            // change row 2
            objRow.set("type", "unsigned short");
            objRow.set("value", 2);

            objHeight.set("type", "unsigned short");
            objHeight.set("value", oldHeight + 100);

            objJSON.set("Row", objRow);
            objJSON.set("Height", objHeight);

            Poco::JSON::Stringifier::stringify(objJSON, oss);
            sendTextFrame(socket, "uno .uno:RowHeight " + oss.str());
            sendTextFrame(socket, commandValues);
            response = getResponseMessage(socket, "commandvalues:", "testColumnRowResize ");
            CPPUNIT_ASSERT_MESSAGE("did not receive a commandvalues: message as expected", !response.empty());
            std::vector<char> json(response.begin() + std::string("commandvalues:").length(), response.end());
            json.push_back(0);
            newHeight = getColRowSize("rows", json.data(), 1);
            CPPUNIT_ASSERT(newHeight > oldHeight);
        }
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testOptimalResize()
{
    try
    {
        //std::vector<char> response;
        std::string documentPath, documentURL;
        double newWidth, newHeight;
        Poco::JSON::Object objIndex, objSize, objModifier;

        // row/column index 0
        objIndex.set("type", "unsigned short");
        objIndex.set("value", 1);

        // size in twips
        objSize.set("type", "unsigned short");
        objSize.set("value", 3840);

        // keyboard modifier
        objModifier.set("type", "unsigned short");
        objModifier.set("value", 0);

        getDocumentPathAndURL("empty.ods", documentPath, documentURL);
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        auto socket = loadDocAndGetSocket(_uri, documentURL, "testOptimalResize ");

        const std::string commandValues = "commandvalues command=.uno:ViewRowColumnHeaders";
        // send new column width
        {
            std::ostringstream oss;
            Poco::JSON::Object objJSON;

            objJSON.set("Column", objIndex);
            objJSON.set("Width", objSize);

            Poco::JSON::Stringifier::stringify(objJSON, oss);
            sendTextFrame(socket, "uno .uno:ColumnWidth " + oss.str());
            sendTextFrame(socket, commandValues);
            newWidth = getColRowSize(socket, "columns", 0);
        }
        // send new row height
        {
            std::ostringstream oss;
            Poco::JSON::Object objJSON;

            objJSON.set("Row", objIndex);
            objJSON.set("Height", objSize);

            Poco::JSON::Stringifier::stringify(objJSON, oss);
            sendTextFrame(socket, "uno .uno:RowHeight " + oss.str());
            sendTextFrame(socket, commandValues);
            newHeight = getColRowSize(socket, "rows", 0);
        }

        objIndex.set("value", 0);

        // send optimal column width
        {
            std::ostringstream oss;
            Poco::JSON::Object objJSON;
            double optimalWidth;

            objJSON.set("Col", objIndex);
            objJSON.set("Modifier", objModifier);

            Poco::JSON::Stringifier::stringify(objJSON, oss);
            sendTextFrame(socket, "uno .uno:SelectColumn " + oss.str());
            sendTextFrame(socket, "uno .uno:SetOptimalColumnWidthDirect");
            sendTextFrame(socket, commandValues);
            optimalWidth = getColRowSize(socket, "columns", 0);
            CPPUNIT_ASSERT(optimalWidth < newWidth);
        }

        // send optimal row height
        {
            std::ostringstream oss;
            Poco::JSON::Object objSelect, objOptHeight, objExtra;
            double optimalHeight;

            objSelect.set("Row", objIndex);
            objSelect.set("Modifier", objModifier);

            objExtra.set("type", "unsigned short");
            objExtra.set("value", 0);

            objOptHeight.set("aExtraHeight", objExtra);

            Poco::JSON::Stringifier::stringify(objSelect, oss);
            sendTextFrame(socket, "uno .uno:SelectRow " + oss.str());
            oss.str(""); oss.clear();

            Poco::JSON::Stringifier::stringify(objOptHeight, oss);
            sendTextFrame(socket, "uno .uno:SetOptimalRowHeight " + oss.str());

            sendTextFrame(socket, commandValues);
            optimalHeight = getColRowSize(socket, "rows", 0);
            CPPUNIT_ASSERT(optimalHeight < newHeight);
        }
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testEachView(const std::string& doc, const std::string& type, const std::string& protocol, const std::string& protocolView)
{
    int docPart = -1;
    int docParts = 0;
    int docHeight = 0;
    int docWidth = 0;
    int docViewId = -1;
    int itView = 0;

    // 0..N Views
    std::vector<std::shared_ptr<Poco::Net::WebSocket>> views;
    const std::string view = "view %d -> ";
    const std::string load = "view %d, cannot load the document ";
    const std::string error = "view %d, did not receive a %s message as expected";

    // Load a document
    std::string documentPath, documentURL, response, text;
    getDocumentPathAndURL(doc, documentPath, documentURL);

    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
    Poco::Net::WebSocket socket0 = *connectLOKit(_uri, request, _response);

    sendTextFrame(socket0, "load url=" + documentURL);
    CPPUNIT_ASSERT_MESSAGE(Poco::format(load, itView) + documentURL, isDocumentLoaded(socket0, Poco::format(view, itView)));

    // Check document size
    sendTextFrame(socket0, "status");
    getResponseMessage(socket0, "status:", response, false, Poco::format(view, itView));
    CPPUNIT_ASSERT_MESSAGE(Poco::format(error, itView, std::string("status:")), !response.empty());
    parseDocSize(response, type, docPart, docParts, docWidth, docHeight, docViewId);

    // Send click message
    Poco::format(text, "mouse type=%s x=%d y=%d count=1 buttons=1 modifier=0", std::string("buttondown"), docWidth/2, docHeight/6);
    sendTextFrame(socket0, text); text.clear();
    Poco::format(text, "mouse type=%s x=%d y=%d count=1 buttons=1 modifier=0", std::string("buttonup"), docWidth/2, docHeight/6);
    sendTextFrame(socket0, text);
    getResponseMessage(socket0, protocol, response, false, Poco::format(view, itView));
    CPPUNIT_ASSERT_MESSAGE(Poco::format(error, itView, protocol), !response.empty());

    // Connect 0..N Views, where N=10
    for (itView = 0; itView < 10; ++itView)
    {
        views.emplace_back(connectLOKit(_uri, request, _response));
    }

    itView = 1;
    // Load 0..N view
    for (auto socketView : views)
    {
        sendTextFrame(*socketView, "load url=" + documentURL);
        CPPUNIT_ASSERT_MESSAGE(Poco::format(load, itView) + documentURL, isDocumentLoaded(*socketView, Poco::format(view, itView), true));

        // Expected to receive response each view
        getResponseMessage(*socketView, protocolView, response, false, Poco::format(view, itView));
        CPPUNIT_ASSERT_MESSAGE(Poco::format(error, itView, protocolView), !response.empty());
        ++itView;
    }

    itView = 0;
    // main view should receive response each view
    for (auto socketView : views)
    {
        getResponseMessage(socket0, protocolView, response, false, Poco::format(view, itView));
        CPPUNIT_ASSERT_MESSAGE(Poco::format(error, itView, protocolView), !response.empty());
    }
}

void HTTPWSTest::testInvalidateViewCursor()
{
    try
    {
        testEachView("viewcursor.odp", "presentation", "invalidatecursor:", "invalidateviewcursor:");
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testViewCursorVisible()
{
    try
    {
        testEachView("viewcursor.odp", "presentation", "cursorvisible:", "viewcursorvisible:");

    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testCellViewCursor()
{
    try
    {
        testEachView("empty.ods", "spreadsheet", "cellcursor:", "cellviewcursor:");
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testGraphicViewSelection()
{
    try
    {
        testEachView("graphicviewselection.odp", "presentation", "graphicselection:", "graphicviewselection:");
        testEachView("graphicviewselection.odt", "text", "graphicselection:", "graphicviewselection:");
        testEachView("graphicviewselection.ods", "spreadsheet", "graphicselection:", "graphicviewselection:");
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testGraphicInvalidate()
{
    try
    {
        // Load a document.
        std::string docPath;
        std::string docURL;
        std::string response;
        std::vector<std::string> responses;

        getDocumentPathAndURL("shape.ods", docPath, docURL);
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, docURL);
        Poco::Net::WebSocket socket = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket, "load url=" + docURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + docURL, isDocumentLoaded(socket));

        // Send click message
        sendTextFrame(socket, "mouse type=buttondown x=1035 y=400 count=1 buttons=1 modifier=0");
        sendTextFrame(socket, "mouse type=buttonup x=1035 y=400 count=1 buttons=1 modifier=0");
        getResponseMessage(socket, "graphicselection:", response, false, "testGraphicInvalidate ");

        // Drag & drop graphic
        sendTextFrame(socket, "mouse type=buttondown x=1035 y=400 count=1 buttons=1 modifier=0");
        sendTextFrame(socket, "mouse type=move x=1035 y=450 count=1 buttons=1 modifier=0");
        sendTextFrame(socket, "mouse type=buttonup x=1035 y=450 count=1 buttons=1 modifier=0");

        collectMessages(socket, "invalidatetiles:", responses, 3, "testGraphicInvalidate ");
        CPPUNIT_ASSERT_MESSAGE("No invalidatetiles: received", responses.size() > 0);
        for (auto message : responses)
        {
            CPPUNIT_ASSERT_MESSAGE("Drag & Drop graphic invalidate all tiles", message.find("EMPTY") == std::string::npos);
        }
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

void HTTPWSTest::testCursorPosition()
{
    try
    {
         // Load a document.
        std::string docPath;
        std::string docURL;
        std::string response;
        std::vector<std::string> responses;

        getDocumentPathAndURL("Example.odt", docPath, docURL);
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, docURL);
        Poco::Net::WebSocket socket0 = *connectLOKit(_uri, request, _response);

        sendTextFrame(socket0, "load url=" + docURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + docURL, isDocumentLoaded(socket0));

        // receive cursor position
        getResponseMessage(socket0, "invalidatecursor:", response, false);
        CPPUNIT_ASSERT_MESSAGE("did not receive a invalidatecursor: message as expected", !response.empty());
        Poco::StringTokenizer cursorTokens(response, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), cursorTokens.count());

        // Create second view
        Poco::Net::WebSocket socket1 = *connectLOKit(_uri, request, _response);
        sendTextFrame(socket1, "load url=" + docURL);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + docURL, isDocumentLoaded(socket1));

        //receive view cursor position
        getResponseMessage(socket0, "invalidateviewcursor:", response, false);
        CPPUNIT_ASSERT_MESSAGE("did not receive a invalidateviewcursor: message as expected", !response.empty());

        Poco::JSON::Parser parser;
        const auto result = parser.parse(response);
        const auto& command = result.extract<Poco::JSON::Object::Ptr>();
        CPPUNIT_ASSERT_MESSAGE("missing property rectangle", command->has("rectangle"));

        Poco::StringTokenizer viewTokens(command->get("rectangle").toString(), ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), viewTokens.count());

        // check both cursor should be equal (but tolerate at most 2px -> 30 twips differences)
        const int tolerance = 30;
        CPPUNIT_ASSERT(std::abs(std::stoi(cursorTokens[0]) - std::stoi(viewTokens[0])) < tolerance);
        CPPUNIT_ASSERT(std::abs(std::stoi(cursorTokens[1]) - std::stoi(viewTokens[1])) < tolerance);
        CPPUNIT_ASSERT(std::abs(std::stoi(cursorTokens[2]) - std::stoi(viewTokens[2])) < tolerance);
        CPPUNIT_ASSERT(std::abs(std::stoi(cursorTokens[3]) - std::stoi(viewTokens[3])) < tolerance);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
}

CPPUNIT_TEST_SUITE_REGISTRATION(HTTPWSTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
