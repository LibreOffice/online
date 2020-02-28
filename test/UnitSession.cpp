/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <memory>
#include <string>

#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/Document.h>
#include <Poco/DOM/Node.h>
#include <Poco/DOM/NodeFilter.h>
#include <Poco/DOM/NodeIterator.h>
#include <Poco/SAX/InputSource.h>
#include <Poco/URI.h>
#include <cppunit/TestAssert.h>

#include <Unit.hpp>
#include <UserMessages.hpp>
#include <Util.hpp>
#include <helpers.hpp>

class LOOLWebSocket;

namespace
{
int findInDOM(Poco::XML::Document* doc, const char* string, bool checkName,
              unsigned long nodeFilter = Poco::XML::NodeFilter::SHOW_ALL)
{
    int count = 0;
    Poco::XML::NodeIterator itCode(doc, nodeFilter);
    while (Poco::XML::Node* pNode = itCode.nextNode())
    {
        if (checkName)
        {
            if (pNode->nodeName() == string)
                count++;
        }
        else
        {
            if (pNode->getNodeValue().find(string) != std::string::npos)
                count++;
        }
    }
    return count;
}
}

/// Test suite that uses a HTTP session (and not just a socket) directly.
class UnitSession : public UnitWSD
{
    TestResult testBadRequest();
    TestResult testHandshake();
    TestResult testSlideShow();

public:
    void invokeTest() override;
};

UnitBase::TestResult UnitSession::testBadRequest()
{
    try
    {
        // Try to load a bogus url.
        const std::string documentURL = "/lol/file%3A%2F%2F%2Ffake.doc";

        Poco::Net::HTTPResponse response;
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        std::unique_ptr<Poco::Net::HTTPClientSession> session(
            helpers::createSession(Poco::URI(helpers::getTestServerURI())));

        request.set("Connection", "Upgrade");
        request.set("Upgrade", "websocket");
        request.set("Sec-WebSocket-Version", "13");
        request.set("Sec-WebSocket-Key", "");
        request.setChunkedTransferEncoding(false);
        session->setKeepAlive(true);
        session->sendRequest(request);
        session->receiveResponse(response);
        CPPUNIT_ASSERT_EQUAL(Poco::Net::HTTPResponse::HTTPResponse::HTTP_BAD_REQUEST,
                             response.getStatus());
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
    return TestResult::Ok;
}

UnitBase::TestResult UnitSession::testHandshake()
{
    const char* testname = "handshake ";
    try
    {
        std::string documentPath, documentURL;
        helpers::getDocumentPathAndURL("hello.odt", documentPath, documentURL, testname);

        // NOTE: Do not replace with wrappers. This has to be explicit.
        Poco::Net::HTTPResponse response;
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        std::unique_ptr<Poco::Net::HTTPClientSession> session(
            helpers::createSession(Poco::URI(helpers::getTestServerURI())));
        LOOLWebSocket socket(*session, request, response);
        socket.setReceiveTimeout(0);

        int flags = 0;
        char buffer[1024] = { 0 };
        int bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
        TST_LOG("Got " << LOOLProtocol::getAbbreviatedFrameDump(buffer, bytes, flags));
        CPPUNIT_ASSERT_EQUAL(std::string("statusindicator: find"), std::string(buffer, bytes));

        bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
        TST_LOG("Got " << LOOLProtocol::getAbbreviatedFrameDump(buffer, bytes, flags));
        if (bytes > 0 && !std::strstr(buffer, "error:"))
        {
            CPPUNIT_ASSERT_EQUAL(std::string("statusindicator: connect"),
                                 std::string(buffer, bytes));

            bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
            TST_LOG("Got " << LOOLProtocol::getAbbreviatedFrameDump(buffer, bytes, flags));
            if (!std::strstr(buffer, "error:"))
            {
                CPPUNIT_ASSERT_EQUAL(std::string("statusindicator: ready"),
                                     std::string(buffer, bytes));
            }
            else
            {
                // check error message
                CPPUNIT_ASSERT(std::strstr(SERVICE_UNAVAILABLE_INTERNAL_ERROR, buffer) != nullptr);

                // close frame message
                bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
                TST_LOG("Got " << LOOLProtocol::getAbbreviatedFrameDump(buffer, bytes, flags));
                CPPUNIT_ASSERT((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK)
                               == Poco::Net::WebSocket::FRAME_OP_CLOSE);
            }
        }
        else
        {
            // check error message
            CPPUNIT_ASSERT(std::strstr(SERVICE_UNAVAILABLE_INTERNAL_ERROR, buffer) != nullptr);

            // close frame message
            bytes = socket.receiveFrame(buffer, sizeof(buffer), flags);
            TST_LOG("Got " << LOOLProtocol::getAbbreviatedFrameDump(buffer, bytes, flags));
            CPPUNIT_ASSERT((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK)
                           == Poco::Net::WebSocket::FRAME_OP_CLOSE);
        }
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
    return TestResult::Ok;
}

UnitBase::TestResult UnitSession::testSlideShow()
{
    const char* testname = "slideshow ";
    try
    {
        // Load a document
        std::string documentPath, documentURL;
        std::string response;
        helpers::getDocumentPathAndURL("setclientpart.odp", documentPath, documentURL, testname);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, documentURL);
        Poco::Net::HTTPResponse httpResponse;
        std::shared_ptr<LOOLWebSocket> socket = helpers::connectLOKit(
            Poco::URI(helpers::getTestServerURI()), request, httpResponse, testname);

        helpers::sendTextFrame(socket, "load url=" + documentURL, testname);
        CPPUNIT_ASSERT_MESSAGE("cannot load the document " + documentURL,
                               helpers::isDocumentLoaded(socket, testname));

        // request slide show
        helpers::sendTextFrame(
            socket, "downloadas name=slideshow.svg id=slideshow format=svg options=", testname);
        response = helpers::getResponseString(socket, "downloadas:", testname);
        CPPUNIT_ASSERT_MESSAGE("did not receive a downloadas: message as expected",
                               !response.empty());

        StringVector tokens(LOOLProtocol::tokenize(response.substr(11), ' '));
        // "downloadas: jail= dir= name=slideshow.svg port= id=slideshow"
        const std::string jail = tokens[0].substr(std::string("jail=").size());
        const std::string dir = tokens[1].substr(std::string("dir=").size());
        const std::string name = tokens[2].substr(std::string("name=").size());
        const int port = std::stoi(tokens[3].substr(std::string("port=").size()));
        const std::string id = tokens[4].substr(std::string("id=").size());
        CPPUNIT_ASSERT(!jail.empty());
        CPPUNIT_ASSERT(!dir.empty());
        CPPUNIT_ASSERT_EQUAL(std::string("slideshow.svg"), name);
        CPPUNIT_ASSERT_EQUAL(static_cast<int>(Poco::URI(helpers::getTestServerURI()).getPort()),
                             port);
        CPPUNIT_ASSERT_EQUAL(std::string("slideshow"), id);

        std::string encodedDoc;
        Poco::URI::encode(documentPath, ":/?", encodedDoc);
        const std::string path = "/lool/" + encodedDoc + "/" + jail + "/" + dir + "/" + name;
        std::unique_ptr<Poco::Net::HTTPClientSession> session(
            helpers::createSession(Poco::URI(helpers::getTestServerURI())));
        Poco::Net::HTTPRequest requestSVG(Poco::Net::HTTPRequest::HTTP_GET, path);
        TST_LOG("Requesting SVG from " << path);
        session->sendRequest(requestSVG);

        Poco::Net::HTTPResponse responseSVG;
        std::istream& rs = session->receiveResponse(responseSVG);
        CPPUNIT_ASSERT_EQUAL(Poco::Net::HTTPResponse::HTTP_OK /* 200 */, responseSVG.getStatus());
        CPPUNIT_ASSERT_EQUAL(std::string("image/svg+xml"), responseSVG.getContentType());
        TST_LOG("SVG file size: " << responseSVG.getContentLength());

        //        std::ofstream ofs("/tmp/slide.svg");
        //        Poco::StreamCopier::copyStream(rs, ofs);
        //        ofs.close();

        // Asserting on the size of the stream is really unhelpful;
        // lets checkout the contents instead ...
        Poco::XML::DOMParser parser;
        Poco::XML::InputSource svgSrc(rs);
        Poco::AutoPtr<Poco::XML::Document> doc = parser.parse(&svgSrc);

        // Do we have our automation / scripting
        CPPUNIT_ASSERT(
            findInDOM(doc, "jessyinkstart", false, Poco::XML::NodeFilter::SHOW_CDATA_SECTION));
        CPPUNIT_ASSERT(
            findInDOM(doc, "jessyinkend", false, Poco::XML::NodeFilter::SHOW_CDATA_SECTION));
        CPPUNIT_ASSERT(
            findInDOM(doc, "libreofficestart", false, Poco::XML::NodeFilter::SHOW_CDATA_SECTION));
        CPPUNIT_ASSERT(
            findInDOM(doc, "libreofficeend", false, Poco::XML::NodeFilter::SHOW_CDATA_SECTION));

        // Do we have plausible content ?
        int countText = findInDOM(doc, "text", true, Poco::XML::NodeFilter::SHOW_ELEMENT);
        CPPUNIT_ASSERT_EQUAL(countText, 93);
    }
    catch (const Poco::Exception& exc)
    {
        CPPUNIT_FAIL(exc.displayText());
    }
    return TestResult::Ok;
}

void UnitSession::invokeTest()
{
    UnitBase::TestResult result = testBadRequest();
    if (result != TestResult::Ok)
        exitTest(result);

    result = testHandshake();
    if (result != TestResult::Ok)
        exitTest(result);

    result = testSlideShow();
    if (result != TestResult::Ok)
        exitTest(result);

    exitTest(TestResult::Ok);
}

UnitBase* unit_create_wsd(void) { return new UnitSession(); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
