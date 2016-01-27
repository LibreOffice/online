/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Parts of this file is covered by:

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

 */

#include "config.h"

// This is the main source for the loolwsd program. LOOL uses several loolwsd processes: one main
// parent process that listens on the TCP port and accepts connections from LOOL clients, and a
// number of child processes, each which handles a viewing (editing) session for one document.

#include <errno.h>
#include <locale.h>
#include <unistd.h>

#ifdef __linux
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#endif

#include <ftw.h>
#include <utime.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <mutex>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitInit.h>

#include <Poco/Exception.h>
#include <Poco/File.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/MessageHeader.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/PartHandler.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Path.h>
#include <Poco/Process.h>
#include <Poco/StringTokenizer.h>
#include <Poco/ThreadPool.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionException.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/Mutex.h>
#include <Poco/Net/DialogSocket.h>
#include <Poco/Net/Net.h>
#include <Poco/ThreadLocal.h>
#include <Poco/NamedMutex.h>
#include <Poco/FileStream.h>
#include <Poco/TemporaryFile.h>
#include <Poco/StreamCopier.h>
#include <Poco/URI.h>
#include <Poco/Environment.h>

#include "Common.hpp"
#include "Capabilities.hpp"
#include "LOOLProtocol.hpp"
#include "LOOLSession.hpp"
#include "MasterProcessSession.hpp"
#include "ChildProcessSession.hpp"
#include "LOOLWSD.hpp"
#include "QueueHandler.hpp"
#include "Util.hpp"

using namespace LOOLProtocol;

using Poco::Exception;
using Poco::File;
using Poco::IOException;
using Poco::Net::HTTPClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequestHandlerFactory;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPServer;
using Poco::Net::HTTPServerParams;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;
using Poco::Net::ServerSocket;
using Poco::Net::SocketAddress;
using Poco::Net::WebSocket;
using Poco::Net::WebSocketException;
using Poco::Path;
using Poco::Process;
using Poco::Runnable;
using Poco::StringTokenizer;
using Poco::Thread;
using Poco::ThreadPool;
using Poco::Util::Application;
using Poco::Util::HelpFormatter;
using Poco::Util::IncompatibleOptionsException;
using Poco::Util::MissingOptionException;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::ServerApplication;
using Poco::Net::DialogSocket;
using Poco::FastMutex;
using Poco::Net::Socket;
using Poco::ThreadLocal;
using Poco::Random;
using Poco::NamedMutex;
using Poco::ProcessHandle;
using Poco::URI;

// Document management mutex.
std::mutex DocumentURI::DocumentURIMutex;
std::map<std::string, std::shared_ptr<DocumentURI>> DocumentURI::UriToDocumentURIMap;

/// Handles the filename part of the convert-to POST request payload.
class ConvertToPartHandler : public Poco::Net::PartHandler
{
    std::string& _filename;
public:
    ConvertToPartHandler(std::string& filename)
        : _filename(filename)
    {
    }

    virtual void handlePart(const Poco::Net::MessageHeader& header, std::istream& stream) override
    {
        // Extract filename and put it to a temporary directory.
        std::string disp;
        Poco::Net::NameValueCollection params;
        if (header.has("Content-Disposition"))
        {
            std::string cd = header.get("Content-Disposition");
            Poco::Net::MessageHeader::splitParameters(cd, disp, params);
        }

        if (!params.has("filename"))
            return;

        Path tempPath = Path::forDirectory(Poco::TemporaryFile().tempName() + Path::separator());
        File(tempPath).createDirectories();
        tempPath.setFileName(params.get("filename"));
        _filename = tempPath.toString();

        // Copy the stream to _filename.
        std::ofstream fileStream;
        fileStream.open(_filename);
        Poco::StreamCopier::copyStream(stream, fileStream);
        fileStream.close();
    }
};

// Synchronously process WebSocket requests and dispatch to handler.
// Handler returns false to end.
void SocketProcessor(std::shared_ptr<WebSocket> ws,
                     HTTPServerResponse& response,
                     std::function<bool(const char* data, const int size, const bool singleLine)> handler)
{
    Log::info("Starting Socket Processor.");

    const Poco::Timespan waitTime(POLL_TIMEOUT_MS * 1000);
    try
    {
        int flags = 0;
        int n = 0;
        ws->setReceiveTimeout(0);
        do
        {
            char buffer[200000]; //FIXME: Dynamic?

            if (ws->poll(waitTime, Socket::SELECT_READ))
            {
                n = ws->receiveFrame(buffer, sizeof(buffer), flags);

                if ((flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_PING)
                {
                    // Echo back the ping payload as pong.
                    // Technically, we should send back a PONG control frame.
                    // However Firefox (probably) or Node.js (possibly) doesn't
                    // like that and closes the socket when we do.
                    // Echoing the payload as a normal frame works with Firefox.
                    ws->sendFrame(buffer, n /*, WebSocket::FRAME_OP_PONG*/);
                }
                else if ((flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_PONG)
                {
                    // In case we do send pings in the future.
                }
                else if (n <= 0)
                {
                    // Connection closed.
                    Log::warn() << "Received " << n
                                << " bytes. Connection closed. Flags: "
                                << std::hex << flags << Log::end;
                    break;
                }
                else
                {
                    assert(n > 0);
                    const std::string firstLine = getFirstLine(buffer, n);
                    StringTokenizer tokens(firstLine, " ", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);
                    int size;

                    if (firstLine == "eof")
                    {
                        Log::info("Received EOF. Finishing.");
                        break;
                    }

                    if ((flags & WebSocket::FrameFlags::FRAME_FLAG_FIN) != WebSocket::FrameFlags::FRAME_FLAG_FIN)
                    {
                        // One WS message split into multiple frames.
                        std::vector<char> message(buffer, buffer + n);
                        while (true)
                        {
                            n = ws->receiveFrame(buffer, sizeof(buffer), flags);
                            message.insert(message.end(), buffer, buffer + n);
                            if ((flags & WebSocket::FrameFlags::FRAME_FLAG_FIN) == WebSocket::FrameFlags::FRAME_FLAG_FIN)
                            {
                                // No more frames: invoke the handler. Assume
                                // for now that this is always a multi-line
                                // message.
                                handler(message.data(), message.size(), false);
                                break;
                            }
                        }
                    }
                    else if (tokens.count() == 2 &&
                             tokens[0] == "nextmessage:" && getTokenInteger(tokens[1], "size", size) && size > 0)
                    {
                        // Check if it is a "nextmessage:" and in that case read the large
                        // follow-up message separately, and handle that only.

                        char largeBuffer[size];     //FIXME: Security risk! Flooding may segfault us.

                        n = ws->receiveFrame(largeBuffer, size, flags);
                        if (n > 0 && !handler(largeBuffer, n, false))
                        {
                            Log::info("Socket handler flagged for finishing.");
                            break;
                        }
                    }
                    else if (firstLine.size() == static_cast<std::string::size_type>(n))
                    {
                        handler(firstLine.c_str(), firstLine.size(), true);
                    }
                    else if (!handler(buffer, n, false))
                    {
                        Log::info("Socket handler flagged for finishing.");
                        break;
                    }
                }
            }
        }
        while (!TerminationFlag &&
               (flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE);
        Log::debug() << "Finishing SocketProcessor. TerminationFlag: " << TerminationFlag
                     << ", payload size: " << n
                     << ", flags: " << std::hex << flags << Log::end;
    }
    catch (const WebSocketException& exc)
    {
        Log::error("RequestHandler::handleRequest(), WebSocketException: " + exc.message());
        switch (exc.code())
        {
        case WebSocket::WS_ERR_HANDSHAKE_UNSUPPORTED_VERSION:
            response.set("Sec-WebSocket-Version", WebSocket::WEBSOCKET_VERSION);
            // fallthrough
        case WebSocket::WS_ERR_NO_HANDSHAKE:
        case WebSocket::WS_ERR_HANDSHAKE_NO_VERSION:
        case WebSocket::WS_ERR_HANDSHAKE_NO_KEY:
            response.setStatusAndReason(HTTPResponse::HTTP_BAD_REQUEST);
            response.setContentLength(0);
            response.send();
            break;
        }
    }

    Log::info("Finished Socket Processor.");
}


/// Handle a public connection from a client.
class ClientRequestHandler: public HTTPRequestHandler
{
private:

    void handlePostRequest(HTTPServerRequest& request, HTTPServerResponse& response, const std::string& id)
    {
        Log::info("Post request.");
        StringTokenizer tokens(request.getURI(), "/?");
        if (tokens.count() >= 2 && tokens[1] == "convert-to")
        {
            Log::info("Conversion request.");
            std::string fromPath;
            ConvertToPartHandler handler(fromPath);
            Poco::Net::HTMLForm form(request, request.stream(), handler);
            std::string format;
            if (form.has("format"))
                format = form.get("format");

            bool sent = false;
            if (!fromPath.empty())
            {
                if (!format.empty())
                {
                    // Load the document.
                    std::shared_ptr<WebSocket> ws;
                    const LOOLSession::Kind kind = LOOLSession::Kind::ToClient;
                    auto session = std::make_shared<MasterProcessSession>(id, kind, ws);
                    const std::string filePrefix("file://");
                    std::string encodedFrom;
                    URI::encode(filePrefix + fromPath, std::string(), encodedFrom);
                    const std::string load = "load url=" + encodedFrom;
                    session->handleInput(load.data(), load.size());

                    // Convert it to the requested format.
                    Path toPath(fromPath);
                    toPath.setExtension(format);
                    std::string toJailURL = filePrefix + JailedDocumentRoot + toPath.getFileName();
                    std::string encodedTo;
                    URI::encode(toJailURL, std::string(), encodedTo);
                        std::string saveas = "saveas url=" + encodedTo + " format=" + format + " options=";
                    session->handleInput(saveas.data(), saveas.size());

                    std::string toURL = session->getSaveAs();
                    std::string resultingURL;
                    URI::decode(toURL, resultingURL);

                    // Send it back to the client.
                    if (resultingURL.find(filePrefix) == 0)
                        resultingURL = resultingURL.substr(filePrefix.length());
                    if (!resultingURL.empty())
                    {
                        const std::string mimeType = "application/octet-stream";
                        response.sendFile(resultingURL, mimeType);
                        sent = true;
                    }
                }

                // Clean up the temporary directory the HTMLForm ctor created.
                Path tempDirectory(fromPath);
                tempDirectory.setFileName("");
                Util::removeFile(tempDirectory, /*recursive=*/true);
            }

            if (!sent)
            {
                response.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
                response.setContentLength(0);
                response.send();
            }
        }
        else if (tokens.count() >= 2 && tokens[1] == "insertfile")
        {
            Log::info("Insert file request.");
            response.set("Access-Control-Allow-Origin", "*");
            response.set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            response.set("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");

            std::string tmpPath;
            ConvertToPartHandler handler(tmpPath);
            Poco::Net::HTMLForm form(request, request.stream(), handler);

            bool goodRequest = form.has("childid") && form.has("name");
            std::string formChildid(form.get("childid"));
            std::string formName(form.get("name"));

            // protect against attempts to inject something funny here
            if (goodRequest && formChildid.find('/') != std::string::npos && formName.find('/') != std::string::npos)
                goodRequest = false;

            if (goodRequest)
            {
                try
                {
                    Log::info() << "Perform insertfile: " << formChildid << ", " << formName << Log::end;
                    const std::string dirPath = LOOLWSD::ChildRoot + formChildid
                                              + JailedDocumentRoot + "insertfile";
                    File(dirPath).createDirectories();
                    std::string fileName = dirPath + Path::separator() + form.get("name");
                    File(tmpPath).moveTo(fileName);

                    response.setStatus(HTTPResponse::HTTP_OK);
                    response.send();
                }
                catch (const IOException& exc)
                {
                    Log::info() << "IOException: " << exc.message() << Log::end;
                    response.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
                    response.send();
                }
            }
            else
            {
                response.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
                response.send();
            }
        }
        else if (tokens.count() >= 4)
        {
            Log::info("File download request.");
            // The user might request a file to download
            const std::string dirPath = LOOLWSD::ChildRoot + tokens[1]
                                      + JailedDocumentRoot + tokens[2];
            std::string fileName;
            URI::decode(tokens[3], fileName);
            const std::string filePath = dirPath + Path::separator() + fileName;
            Log::info("HTTP request for: " + filePath);
            File file(filePath);
            if (file.exists())
            {
                response.set("Access-Control-Allow-Origin", "*");
                Poco::Net::HTMLForm form(request);
                std::string mimeType = "application/octet-stream";
                if (form.has("mime_type"))
                    mimeType = form.get("mime_type");
                response.sendFile(filePath, mimeType);
                Util::removeFile(dirPath, true);
            }
            else
            {
                response.setStatus(HTTPResponse::HTTP_NOT_FOUND);
                response.setContentLength(0);
                response.send();
            }
        }
        else
        {
            Log::info("Bad request.");
            response.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            response.setContentLength(0);
            response.send();
        }
    }

    void handleGetRequest(HTTPServerRequest& request, HTTPServerResponse& response, const std::string& id)
    {
        Log::info("Get request.");
        auto ws = std::make_shared<WebSocket>(request, response);
        auto session = std::make_shared<MasterProcessSession>(id, LOOLSession::Kind::ToClient, ws);

        // For ToClient sessions, we store incoming messages in a queue and have a separate
        // thread that handles them. This is so that we can empty the queue when we get a
        // "canceltiles" message.
        BasicTileQueue queue;
        QueueHandler handler(queue, session, "wsd_queue_" + session->getId());

        Thread queueHandlerThread;
        queueHandlerThread.start(handler);

        SocketProcessor(ws, response, [&session, &queue](const char* data, const int size, const bool singleLine)
            {
                // FIXME: There is a race here when a request A gets in the queue and
                // is processed _after_ a later request B, because B gets processed
                // synchronously and A is waiting in the queue thread.
                // The fix is to push everything into the queue
                // (i.e. change MessageQueue to vector<char>).
                const std::string firstLine = getFirstLine(data, size);
                if (singleLine || firstLine.find("paste") == 0)
                {
                    queue.put(std::string(data, size));
                    return true;
                }
                else
                {
                    return session->handleInput(data, size);
                }
            });

        queue.clear();
        queue.put("eof");
        queueHandlerThread.join();
    }

public:

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
    {
        const auto id = LOOLWSD::GenSessionId();
        const std::string thread_name = "client_ws_" + id;

#ifdef __linux
        if (prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(thread_name.c_str()), 0, 0, 0) != 0)
            Log::error("Cannot set thread name to " + thread_name + ".");
#endif
        Log::debug("Thread [" + thread_name + "] started.");

        try
        {
            if (!(request.find("Upgrade") != request.end() && Poco::icompare(request["Upgrade"], "websocket") == 0))
            {
                handlePostRequest(request, response, id);
            }
            else
            {
                handleGetRequest(request, response, id);
            }
        }
        catch (const Exception& exc)
        {
            Log::error() << "Error: " << exc.displayText()
                         << (exc.nested() ? " (" + exc.nested()->displayText() + ")" : "")
                         << Log::end;
        }
        catch (const std::exception& exc)
        {
            Log::error(std::string("Exception: ") + exc.what());
        }
        catch (...)
        {
            Log::error("Unexpected Exception.");
        }

        Log::debug("Thread [" + thread_name + "] finished.");
    }
};

/// Handle requests from prisoners (internal).
class PrisonerRequestHandler: public HTTPRequestHandler
{
public:

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
    {
        assert(request.serverAddress().port() == MASTER_PORT_NUMBER);
        assert(request.getURI().find(LOOLWSD::CHILD_URI) == 0);

        std::string thread_name = "prison_ws_";
        try
        {
            const auto index = request.getURI().find_last_of('/');
            const auto id = request.getURI().substr(index + 1);

            thread_name += id;

#ifdef __linux
            if (prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(thread_name.c_str()), 0, 0, 0) != 0)
                Log::error("Cannot set thread name to " + thread_name + ".");
#endif
            Log::debug("Thread [" + thread_name + "] started.");

            auto ws = std::make_shared<WebSocket>(request, response);
            auto session = std::make_shared<MasterProcessSession>(id, LOOLSession::Kind::ToPrisoner, ws);

            SocketProcessor(ws, response, [&session](const char* data, const int size, bool)
                {
                    return session->handleInput(data, size);
                });
        }
        catch (const Exception& exc)
        {
            Log::error() << "Error: " << exc.displayText()
                         << (exc.nested() ? " (" + exc.nested()->displayText() + ")" : "")
                         << Log::end;
        }
        catch (const std::exception& exc)
        {
            Log::error(std::string("Exception: ") + exc.what());
        }
        catch (...)
        {
            Log::error("Unexpected Exception.");
        }

        Log::debug("Thread [" + thread_name + "] finished.");
    }
};

template <class RequestHandler>
class RequestHandlerFactory: public HTTPRequestHandlerFactory
{
public:
    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request) override
    {
#ifdef __linux
        if (prctl(PR_SET_NAME, reinterpret_cast<unsigned long>("request_handler"), 0, 0, 0) != 0)
            Log::error("Cannot set thread name to request_handler.");
#endif

        auto logger = Log::info();
        logger << "Request from " << request.clientAddress().toString() << ": "
               << request.getMethod() << " " << request.getURI() << " "
               << request.getVersion();

        for (HTTPServerRequest::ConstIterator it = request.begin(); it != request.end(); ++it)
        {
            logger << " / " << it->first << ": " << it->second;
        }

        logger << Log::end;
        return new RequestHandler();
    }
};

class TestOutput : public Runnable
{
public:
    TestOutput(WebSocket& ws) :
        _ws(ws)
    {
    }

    void run() override
    {
        int flags;
        int n;
        _ws.setReceiveTimeout(0);
        try
        {
            do
            {
                char buffer[200000];
                n = _ws.receiveFrame(buffer, sizeof(buffer), flags);
                if (n > 0)
                {
                    Log::trace() << "Client got " << n << " bytes: "
                                 << getAbbreviatedMessage(buffer, n) << Log::end;
                }
            }
            while (n > 0 && (flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE);
        }
        catch (const WebSocketException& exc)
        {
            Log::error("TestOutput::run(), WebSocketException: " + exc.message());
            _ws.close();
        }
    }

private:
    WebSocket& _ws;
};

class TestInput : public Runnable
{
public:
    TestInput(ServerApplication& main, ServerSocket& svs, HTTPServer& srv) :
        _main(main),
        _svs(svs),
        _srv(srv)
    {
    }

    void run() override
    {
        HTTPClientSession cs("127.0.0.1", _svs.address().port());
        HTTPRequest request(HTTPRequest::HTTP_GET, "/ws");
        HTTPResponse response;
        WebSocket ws(cs, request, response);

        Thread thread;
        TestOutput output(ws);
        thread.start(output);

        if (isatty(0))
        {
            std::cout << std::endl;
            std::cout << "Enter LOOL WS requests, one per line. Enter EOF to finish." << std::endl;
        }

        while (!std::cin.eof())
        {
            std::string line;
            std::getline(std::cin, line);
            ws.sendFrame(line.c_str(), line.size());
        }
        thread.join();
        _srv.stopAll();
        _main.terminate();
    }

private:
    ServerApplication& _main;
    ServerSocket& _svs;
    HTTPServer& _srv;
};

std::atomic<unsigned> LOOLWSD::NextSessionId;
int LOOLWSD::BrokerWritePipe = -1;
std::string LOOLWSD::Cache = LOOLWSD_CACHEDIR;
std::string LOOLWSD::SysTemplate;
std::string LOOLWSD::LoTemplate;
std::string LOOLWSD::ChildRoot;
std::string LOOLWSD::JailId;
std::string LOOLWSD::LoSubPath = "lo";

int LOOLWSD::NumPreSpawnedChildren = 10;
bool LOOLWSD::DoTest = false;
const std::string LOOLWSD::CHILD_URI = "/loolws/child/";
const std::string LOOLWSD::PIDLOG = "/tmp/loolwsd.pid";
const std::string LOOLWSD::LOKIT_PIDLOG = "/tmp/lokit.pid";
const std::string LOOLWSD::FIFO_FILE = "/tmp/loolwsdfifo";

LOOLWSD::LOOLWSD()
{
}

LOOLWSD::~LOOLWSD()
{
}

void LOOLWSD::initialize(Application& self)
{
    ServerApplication::initialize(self);
}

void LOOLWSD::uninitialize()
{
    ServerApplication::uninitialize();
}

void LOOLWSD::defineOptions(OptionSet& optionSet)
{
    ServerApplication::defineOptions(optionSet);

    optionSet.addOption(Option("help", "", "Display help information on command line arguments.")
                        .required(false)
                        .repeatable(false));

    optionSet.addOption(Option("version", "", "Display version information.")
                        .required(false)
                        .repeatable(false));

    optionSet.addOption(Option("port", "", "Port number to listen to (default: " + std::to_string(DEFAULT_CLIENT_PORT_NUMBER) + "),"
                             " must not be " + std::to_string(MASTER_PORT_NUMBER) + ".")
                        .required(false)
                        .repeatable(false)
                        .argument("port number"));

    optionSet.addOption(Option("cache", "", "Path to a directory where to keep the persistent tile cache (default: " + std::string(LOOLWSD_CACHEDIR) + ").")
                        .required(false)
                        .repeatable(false)
                        .argument("directory"));

    optionSet.addOption(Option("systemplate", "", "Path to a template tree with shared libraries etc to be used as source for chroot jails for child processes.")
                        .required(false)
                        .repeatable(false)
                        .argument("directory"));

    optionSet.addOption(Option("lotemplate", "", "Path to a LibreOffice installation tree to be copied (linked) into the jails for child processes. Should be on the same file system as systemplate.")
                        .required(false)
                        .repeatable(false)
                        .argument("directory"));

    optionSet.addOption(Option("childroot", "", "Path to the directory under which the chroot jails for the child processes will be created. Should be on the same file system as systemplate and lotemplate.")
                        .required(false)
                        .repeatable(false)
                        .argument("directory"));

    optionSet.addOption(Option("losubpath", "", "Relative path where the LibreOffice installation will be copied inside a jail (default: '" + LoSubPath + "').")
                        .required(false)
                        .repeatable(false)
                        .argument("relative path"));

    optionSet.addOption(Option("numprespawns", "", "Number of child processes to keep started in advance and waiting for new clients.")
                        .required(false)
                        .repeatable(false)
                        .argument("number"));

    optionSet.addOption(Option("test", "", "Interactive testing.")
                        .required(false)
                        .repeatable(false));

#if ENABLE_DEBUG
    optionSet.addOption(Option("uid", "", "Uid to assume if running under sudo for debugging purposes.")
                        .required(false)
                        .repeatable(false)
                        .argument("uid"));
#endif
}

void LOOLWSD::handleOption(const std::string& optionName, const std::string& value)
{
    ServerApplication::handleOption(optionName, value);

    if (optionName == "help")
    {
        displayHelp();
        exit(Application::EXIT_OK);
    }
    else if (optionName == "version")
    {
        displayVersion();
        exit(Application::EXIT_OK);
    }
    else if (optionName == "port")
        ClientPortNumber = std::stoi(value);
    else if (optionName == "cache")
        Cache = value;
    else if (optionName == "systemplate")
        SysTemplate = value;
    else if (optionName == "lotemplate")
        LoTemplate = value;
    else if (optionName == "childroot")
        ChildRoot = value;
    else if (optionName == "losubpath")
        LoSubPath = value;
    else if (optionName == "numprespawns")
        NumPreSpawnedChildren = std::stoi(value);
    else if (optionName == "test")
        LOOLWSD::DoTest = true;
#if ENABLE_DEBUG
    else if (optionName == "uid")
        uid = std::stoull(value);
#endif
}

void LOOLWSD::displayHelp()
{
    HelpFormatter helpFormatter(options());
    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("OPTIONS");
    helpFormatter.setHeader("LibreOffice On-Line WebSocket server.");
    helpFormatter.format(std::cout);
}

void LOOLWSD::displayVersion()
{
    std::cout << LOOLWSD_VERSION << std::endl;
}

Poco::Process::PID LOOLWSD::createBroker(const std::string& rJailId)
{
    Process::Args args;

    args.push_back("--losubpath=" + LOOLWSD::LoSubPath);
    args.push_back("--systemplate=" + SysTemplate);
    args.push_back("--lotemplate=" + LoTemplate);
    args.push_back("--childroot=" + ChildRoot);
    args.push_back("--jailid=" + rJailId);
    args.push_back("--numprespawns=" + std::to_string(NumPreSpawnedChildren));
    args.push_back("--clientport=" + std::to_string(ClientPortNumber));

    const std::string brokerPath = Path(Application::instance().commandPath()).parent().toString() + "loolbroker";

    Log::info("Launching Broker #1: " + brokerPath + " " +
              Poco::cat(std::string(" "), args.begin(), args.end()));

    ProcessHandle child = Process::launch(brokerPath, args);

    return child.id();
}

int LOOLWSD::main(const std::vector<std::string>& /*args*/)
{
    Log::initialize("wsd");

    Poco::Environment::set("LD_BIND_NOW", "1");
    //Poco::Environment::set("LOK_VIEW_CALLBACK", "1");

#ifdef __linux
    char *locale = setlocale(LC_ALL, nullptr);
    if (locale == nullptr || std::strcmp(locale, "C") == 0)
        setlocale(LC_ALL, "en_US.utf8");
#endif

    Util::setSignals(false);

    if (access(Cache.c_str(), R_OK | W_OK | X_OK) != 0)
    {
        Log::error("Unable to access cache [" + Cache +
                   "] please make sure it exists, and has write permission for this user.");
        return Application::EXIT_UNAVAILABLE;
    }

    // We use the same option set for both parent and child loolwsd,
    // so must check options required in the parent (but not in the
    // child) separately now. Also check for options that are
    // meaningless for the parent.
    if (SysTemplate.empty())
        throw MissingOptionException("systemplate");
    if (LoTemplate.empty())
        throw MissingOptionException("lotemplate");

    if (ChildRoot.empty())
        throw MissingOptionException("childroot");
    else if (ChildRoot[ChildRoot.size() - 1] != Path::separator())
        ChildRoot += Path::separator();

    if (ClientPortNumber == MASTER_PORT_NUMBER)
        throw IncompatibleOptionsException("port");

    if (LOOLWSD::DoTest)
        NumPreSpawnedChildren = 1;

    // log pid information
    {
        Poco::FileOutputStream filePID(LOOLWSD::PIDLOG);
        if (filePID.good())
            filePID << Process::id();
    }

    if (!File(FIFO_FILE).exists() && mkfifo(FIFO_FILE.c_str(), 0666) == -1)
    {
        Log::error("Error: Failed to create pipe FIFO [" + FIFO_FILE + "].");
        return Application::EXIT_UNAVAILABLE;
    }

    JailId = Util::createRandomDir(ChildRoot);
    const Poco::Process::PID pidBroker = createBroker(JailId);
    if (pidBroker < 0)
    {
        Log::error("Failed to spawn loolBroker.");
        return Application::EXIT_UNAVAILABLE;
    }

#ifdef __linux
    dropCapability(CAP_SYS_CHROOT);
    dropCapability(CAP_MKNOD);
    dropCapability(CAP_FOWNER);
#else
    dropCapability();
#endif

    // Start a server listening on the port for clients
    ServerSocket svs(ClientPortNumber, NumPreSpawnedChildren*10);
    ThreadPool threadPool(NumPreSpawnedChildren*2, NumPreSpawnedChildren*5);
    HTTPServer srv(new RequestHandlerFactory<ClientRequestHandler>(), threadPool, svs, new HTTPServerParams);

    srv.start();

    // And one on the port for child processes
    SocketAddress addr2("127.0.0.1", MASTER_PORT_NUMBER);
    ServerSocket svs2(addr2, NumPreSpawnedChildren);
    ThreadPool threadPool2(NumPreSpawnedChildren*2, NumPreSpawnedChildren*5);
    HTTPServer srv2(new RequestHandlerFactory<PrisonerRequestHandler>(), threadPool2, svs2, new HTTPServerParams);

    srv2.start();

    if ( (BrokerWritePipe = open(FIFO_FILE.c_str(), O_WRONLY) ) < 0 )
    {
        Log::error("Error: failed to open pipe [" + FIFO_FILE + "] write only.");
        return Application::EXIT_UNAVAILABLE;
    }

    TestInput input(*this, svs, srv);
    Thread inputThread;
    if (LOOLWSD::DoTest)
    {
        inputThread.start(input);
        waitForTerminationRequest();
    }

    int status = 0;
    unsigned timeoutCounter = 0;
    std::chrono::steady_clock::time_point lastPoolTime = std::chrono::steady_clock::now();

    while (!TerminationFlag && !LOOLWSD::DoTest)
    {
        const auto duration = (std::chrono::steady_clock::now() - lastPoolTime);
        if (duration >= std::chrono::seconds(10))
        {
            if (threadPool.available() ==  0)
                Log::warn("The thread pool is full, no more connections are accepted.");

            lastPoolTime = std::chrono::steady_clock::now();
        }

        const pid_t pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
        if (pid > 0)
        {
            if (pidBroker == pid)
            {
                if (WIFEXITED(status))
                {
                    Log::info() << "Child process [" << pid << "] exited with code: "
                                << WEXITSTATUS(status) << "." << Log::end;

                    break;
                }
                else
                if (WIFSIGNALED(status))
                {
                    std::string fate = "died";
#ifdef WCOREDUMP
                    if (WCOREDUMP(status))
                        fate = "core-dumped";
#endif
                    Log::error() << "Child process [" << pid << "] " << fate
                                 << " with " << Util::signalName(WTERMSIG(status))
                                 << " signal. " << Log::end;

                    break;
                }
                else if (WIFSTOPPED(status))
                {
                    Log::info() << "Child process [" << pid << "] stopped with "
                                << Util::signalName(WSTOPSIG(status))
                                << " signal. " << Log::end;
                }
                else if (WIFCONTINUED(status))
                {
                    Log::info() << "Child process [" << pid << "] resumed with SIGCONT."
                                << Log::end;
                }
                else
                {
                    Log::warn() << "Unknown status returned by waitpid: "
                                << std::hex << status << "." << Log::end;
                }
            }
            else
            {
                Log::error("None of our known child processes died. PID: " + std::to_string(pid));
            }
        }
        else if (pid < 0)
            Log::error("Error: waitpid failed.");

        if (timeoutCounter++ == INTERVAL_PROBES)
        {
            timeoutCounter = 0;
            sleep(MAINTENANCE_INTERVAL*2);
        }
    }

    if (LOOLWSD::DoTest)
        inputThread.join();

    // stop the service, no more request
    srv.stop();
    srv2.stop();

    // close all websockets
    threadPool.joinAll();
    threadPool2.joinAll();

    // Terminate child processes
    Util::writeFIFO(LOOLWSD::BrokerWritePipe, "eof\r\n");
    Log::info("Requesting child process " + std::to_string(pidBroker) + " to terminate");
    Process::requestTermination(pidBroker);

    // wait broker process finish
    waitpid(-1, &status, WUNTRACED);

    close(BrokerWritePipe);

    Log::info("Cleaning up childroot directory [" + ChildRoot + "].");
    std::vector<std::string> jails;
    File(ChildRoot).list(jails);
    for (auto& jail : jails)
    {
        const auto path = ChildRoot + jail;
        Log::info("Removing jail [" + path + "].");
        Util::removeFile(path, true);
    }

    Log::info("Process [loolwsd] finished.");
    return Application::EXIT_OK;
}

POCO_SERVER_MAIN(LOOLWSD)

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
