/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * NB. this file is compiled both standalone, and as part of the LOOLBroker.
 */

#include <sys/prctl.h>
#include <sys/poll.h>
#include <sys/syscall.h>
#include <signal.h>

#include <memory>
#include <iostream>

#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Thread.h>
#include <Poco/ThreadPool.h>
#include <Poco/Runnable.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Exception.h>
#include <Poco/Process.h>
#include <Poco/Environment.h>
#include <Poco/NotificationQueue.h>
#include <Poco/Notification.h>
#include <Poco/Mutex.h>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitInit.h>

#include "MessageQueue.hpp"
#include "Util.hpp"
#include "ChildProcessSession.hpp"
#include "LOOLProtocol.hpp"

using namespace LOOLProtocol;
using Poco::Net::WebSocket;
using Poco::Net::HTTPClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using Poco::Thread;
using Poco::Runnable;
using Poco::StringTokenizer;
using Poco::Exception;
using Poco::Process;
using Poco::Notification;
using Poco::NotificationQueue;
using Poco::FastMutex;

const int MASTER_PORT_NUMBER = 9981;
const std::string CHILD_URI = "/loolws/child/";
const std::string LOKIT_BROKER = "/tmp/loolbroker.fifo";

class CallBackWorker: public Runnable
{
public:
    CallBackWorker(NotificationQueue& queue):
        _queue(queue)
    {
    }

    std::string callbackTypeToString (int nType)
    {
        switch (nType)
        {
        case LOK_CALLBACK_INVALIDATE_TILES:
            return std::string("LOK_CALLBACK_INVALIDATE_TILES");
        case LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR:
            return std::string("LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR");
        case LOK_CALLBACK_TEXT_SELECTION:
            return std::string("LOK_CALLBACK_TEXT_SELECTION");
        case LOK_CALLBACK_TEXT_SELECTION_START:
            return std::string("LOK_CALLBACK_TEXT_SELECTION_START");
        case LOK_CALLBACK_TEXT_SELECTION_END:
            return std::string("LOK_CALLBACK_TEXT_SELECTION_END");
        case LOK_CALLBACK_CURSOR_VISIBLE:
            return std::string("LOK_CALLBACK_CURSOR_VISIBLE");
        case LOK_CALLBACK_GRAPHIC_SELECTION:
            return std::string("LOK_CALLBACK_GRAPHIC_SELECTION");
        case LOK_CALLBACK_HYPERLINK_CLICKED:
            return std::string("LOK_CALLBACK_HYPERLINK_CLICKED");
        case LOK_CALLBACK_STATE_CHANGED:
            return std::string("LOK_CALLBACK_STATE_CHANGED");
        case LOK_CALLBACK_STATUS_INDICATOR_START:
            return std::string("LOK_CALLBACK_STATUS_INDICATOR_START");
        case LOK_CALLBACK_STATUS_INDICATOR_SET_VALUE:
            return std::string("LOK_CALLBACK_STATUS_INDICATOR_SET_VALUE");
        case LOK_CALLBACK_STATUS_INDICATOR_FINISH:
            return std::string("LOK_CALLBACK_STATUS_INDICATOR_FINISH");
        case LOK_CALLBACK_SEARCH_NOT_FOUND:
            return std::string("LOK_CALLBACK_SEARCH_NOT_FOUND");
        case LOK_CALLBACK_DOCUMENT_SIZE_CHANGED:
            return std::string("LOK_CALLBACK_DOCUMENT_SIZE_CHANGED");
        case LOK_CALLBACK_SET_PART:
            return std::string("LOK_CALLBACK_SET_PART");
        }
        return std::string("");
    }

    void callback(int nType, std::string& rPayload, void* pData)
    {
        ChildProcessSession *srv = reinterpret_cast<ChildProcessSession *>(pData);

        switch ((LibreOfficeKitCallbackType) nType)
        {
        case LOK_CALLBACK_INVALIDATE_TILES:
            {
                int curPart = srv->_loKitDocument->pClass->getPart(srv->_loKitDocument);
                srv->sendTextFrame("curpart: part=" + std::to_string(curPart));
                if (srv->_docType == "text")
                {
                    curPart = 0;
                }
                StringTokenizer tokens(rPayload, " ", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);
                if (tokens.count() == 4)
                {
                    int x, y, width, height;

                    try {
                        x = std::stoi(tokens[0]);
                        y = std::stoi(tokens[1]);
                        width = std::stoi(tokens[2]);
                        height = std::stoi(tokens[3]);
                    }
                    catch (std::out_of_range&)
                    {
                        // something went wrong, invalidate everything
                        std::cout << Util::logPrefix() << "Ignoring integer values out of range: " << rPayload << std::endl;
                        x = 0;
                        y = 0;
                        width = INT_MAX;
                        height = INT_MAX;
                    }

                    srv->sendTextFrame("invalidatetiles:"
                                       " part=" + std::to_string(curPart) +
                                       " x=" + std::to_string(x) +
                                       " y=" + std::to_string(y) +
                                       " width=" + std::to_string(width) +
                                       " height=" + std::to_string(height));
                }
                else
                {
                    srv->sendTextFrame("invalidatetiles: " + rPayload);
                }
            }
            break;
        case LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR:
            srv->sendTextFrame("invalidatecursor: " + rPayload);
            break;
        case LOK_CALLBACK_TEXT_SELECTION:
            srv->sendTextFrame("textselection: " + rPayload);
            break;
        case LOK_CALLBACK_TEXT_SELECTION_START:
            srv->sendTextFrame("textselectionstart: " + rPayload);
            break;
        case LOK_CALLBACK_TEXT_SELECTION_END:
            srv->sendTextFrame("textselectionend: " + rPayload);
            break;
        case LOK_CALLBACK_CURSOR_VISIBLE:
            srv->sendTextFrame("cursorvisible: " + rPayload);
            break;
        case LOK_CALLBACK_GRAPHIC_SELECTION:
            srv->sendTextFrame("graphicselection: " + rPayload);
            break;
        case LOK_CALLBACK_CELL_CURSOR:
            srv->sendTextFrame("cellcursor: " + rPayload);
            break;
        case LOK_CALLBACK_CELL_FORMULA:
            srv->sendTextFrame("cellformula: " + rPayload);
            break;
        case LOK_CALLBACK_MOUSE_POINTER:
            srv->sendTextFrame("mousepointer: " + rPayload);
            break;
        case LOK_CALLBACK_HYPERLINK_CLICKED:
            srv->sendTextFrame("hyperlinkclicked: " + rPayload);
            break;
        case LOK_CALLBACK_STATE_CHANGED:
            srv->sendTextFrame("statechanged: " + rPayload);
            break;
        case LOK_CALLBACK_STATUS_INDICATOR_START:
            srv->sendTextFrame("statusindicatorstart:");
            break;
        case LOK_CALLBACK_STATUS_INDICATOR_SET_VALUE:
            srv->sendTextFrame("statusindicatorsetvalue: " + rPayload);
            break;
        case LOK_CALLBACK_STATUS_INDICATOR_FINISH:
            srv->sendTextFrame("statusindicatorfinish:");
            break;
        case LOK_CALLBACK_SEARCH_NOT_FOUND:
            srv->sendTextFrame("searchnotfound: " + rPayload);
            break;
        case LOK_CALLBACK_SEARCH_RESULT_SELECTION:
            srv->sendTextFrame("searchresultselection: " + rPayload);
            break;
        case LOK_CALLBACK_DOCUMENT_SIZE_CHANGED:
            srv->getStatus("", 0);
            srv->getPartPageRectangles("", 0);
            break;
        case LOK_CALLBACK_SET_PART:
            srv->sendTextFrame("setpart: " + rPayload);
            break;
        case LOK_CALLBACK_UNO_COMMAND_RESULT:
            srv->sendTextFrame("unocommandresult: " + rPayload);
            break;
        }
    }

    void run()
    {
        while ( true )
        {
            Notification::Ptr aNotification(_queue.waitDequeueNotification());
            if (aNotification)
            {
                CallBackNotification::Ptr aCallBackNotification = aNotification.cast<CallBackNotification>();
                if (aCallBackNotification)
                {
                    {
                        FastMutex::ScopedLock lock(_mutex);
                        callback(aCallBackNotification->m_nType, aCallBackNotification->m_aPayload, aCallBackNotification->m_pSession);
                    }
                }
            }
            else break;
        }
    }

private:
    NotificationQueue& _queue;
    static FastMutex   _mutex;
};

FastMutex CallBackWorker::_mutex;

class QueueHandler: public Runnable
{
public:
    QueueHandler(MessageQueue& queue):
        _queue(queue)
    {
    }

    void setSession(std::shared_ptr<LOOLSession> session)
    {
        _session = session;
    }

    void run() override
    {
#ifdef __linux
      if (prctl(PR_SET_NAME, reinterpret_cast<unsigned long>("queue_handler"), 0, 0, 0) != 0)
        Log::error(std::string("Cannot set thread name :") + strerror(errno));
#endif
        try
        {
            while (true)
            {
                std::string input = _queue.get();
                if (input == "eof")
                    break;
                if (!_session->handleInput(input.c_str(), input.size()))
                    break;
            }
        }
        catch(std::exception& ex)
        {
            Log::error(std::string("Exception: ") + ex.what());
            raise(SIGABRT);
        }
        catch(...)
        {
            Log::error("Unknown Exception.");
            raise(SIGABRT);
        }
    }

private:
    std::shared_ptr<LOOLSession> _session;
    MessageQueue& _queue;
};

class Connection: public Runnable
{
public:
    Connection(LibreOfficeKit *loKit, LibreOfficeKitDocument *loKitDocument, Poco::UInt64 childId, const std::string& threadId) :
        _loKit(loKit),
        _loKitDocument(loKitDocument),
        _childId(childId),
        _threadId(threadId)
    {
        std::cout << Util::logPrefix() << "New connection in child: " << _childId << ", thread: " << _threadId << std::endl;
    }

    void start()
    {
        _thread.start(*this);
    }

    bool isRunning()
    {
        return _thread.isRunning();
    }

    LibreOfficeKitDocument * getLOKitDocument()
    {
        return (_session ? _session->_loKitDocument : NULL);
    };

    void run() override
    {
#ifdef __linux
        if (prctl(PR_SET_NAME, reinterpret_cast<unsigned long>("lokit_connection"), 0, 0, 0) != 0)
            std::cout << Util::logPrefix() << "Cannot set thread name :" << strerror(errno) << std::endl;
#endif
        try
        {
            // Open websocket connection between the child process and the
            // parent. The parent forwards us requests that it can't handle.

            HTTPClientSession cs("127.0.0.1", MASTER_PORT_NUMBER);
            cs.setTimeout(0);
            HTTPRequest request(HTTPRequest::HTTP_GET, CHILD_URI);
            HTTPResponse response;
            std::shared_ptr<WebSocket> ws(new WebSocket(cs, request, response));

            _session.reset(new ChildProcessSession(ws, _loKit, _loKitDocument, std::to_string(_childId)));
            //std::shared_ptr<ChildProcessSession> session(new ChildProcessSession(ws, _loKit, _loKitDocument));
            ws->setReceiveTimeout(0);

            // child Jail TID PID
            std::string hello("child " + std::to_string(_childId) + " " +
                _threadId + " " + std::to_string(Process::id()));
            _session->sendTextFrame(hello);

            TileQueue queue;
            Thread queueHandlerThread;
            QueueHandler handler(queue);

            handler.setSession(_session);
            queueHandlerThread.start(handler);

            int flags;
            int n;
            do
            {
                char buffer[1024];
                n = ws->receiveFrame(buffer, sizeof(buffer), flags);

                if (n > 0 && (flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE)
                {
                    std::string firstLine = getFirstLine(buffer, n);
                    StringTokenizer tokens(firstLine, " ", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);

                    // The only kind of messages a child process receives are the single-line ones (?)
                    assert(firstLine.size() == static_cast<std::string::size_type>(n));

                    queue.put(firstLine);
                }
            }
            while (n > 0 && (flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE);

            queue.clear();
            queue.put("eof");
            queueHandlerThread.join();
        }
        catch (Exception& exc)
        {
            std::cout << Util::logPrefix() + "Exception: " + exc.what() << std::endl;
        }
        catch (std::exception& exc)
        {
            std::cout << Util::logPrefix() + "Exception: " + exc.what() << std::endl;
        }
    }

    ~Connection()
    {
        std::cout << Util::logPrefix() << "Closing connection in child: " << _childId << ", thread: " << _threadId << std::endl;
        //_thread.stop();
    }

private:
    LibreOfficeKit *_loKit;
    LibreOfficeKitDocument *_loKitDocument;
    Poco::UInt64 _childId;
    std::string _threadId;
    Thread _thread;
    std::shared_ptr<ChildProcessSession> _session;
};

void run_lok_main(const std::string &loSubPath, Poco::UInt64 _childId, const std::string& pipe)
{
    struct pollfd aPoll;
    ssize_t nBytes = -1;
    char  aBuffer[1024*2];
    char* pStart = NULL;
    char* pEnd = NULL;

    std::string aURL;
    std::map<std::string, std::shared_ptr<Connection>> _connections;

    assert (_childId != 0);
    assert (!loSubPath.empty());

#ifdef __linux
    if (prctl(PR_SET_NAME, reinterpret_cast<unsigned long>("libreofficekit"), 0, 0, 0) != 0)
        std::cout << Util::logPrefix() << "Cannot set thread name :" << strerror(errno) << std::endl;
#endif

    if (std::getenv("SLEEPFORDEBUGGER"))
    {
        std::cout << "Sleeping " << std::getenv("SLEEPFORDEBUGGER") << " seconds, attach process "
                  << Process::id() << " in debugger now." << std::endl;
        Thread::sleep(std::stoul(std::getenv("SLEEPFORDEBUGGER")) * 1000);
    }

    try
    {
#ifdef __APPLE__
        LibreOfficeKit *loKit(lok_init_2(("/" + loSubPath + "/Frameworks").c_str(), "file:///user"));
#else
        LibreOfficeKit *loKit(lok_init_2(("/" + loSubPath + "/program").c_str(), "file:///user"));
#endif

        if (!loKit)
        {
            std::cout << Util::logPrefix() + "LibreOfficeKit initialization failed" << std::endl;
            exit(-1);
        }

        int writerBroker;
        int readerBroker;

        if ( (readerBroker = open(pipe.c_str(), O_RDONLY) ) < 0 )
        {
            std::cout << Util::logPrefix() << "open pipe read only: " << strerror(errno) << std::endl;
            exit(-1);
        }

        if ( (writerBroker = open(LOKIT_BROKER.c_str(), O_WRONLY) ) < 0 )
        {
            std::cout << Util::logPrefix() << "open pipe write only: " << strerror(errno) << std::endl;
            exit(-1);
        }

        CallBackWorker callbackWorker(ChildProcessSession::_callbackQueue);
        Poco::ThreadPool::defaultPool().start(callbackWorker);

        std::cout << Util::logPrefix() << "child ready!" << std::endl;

        std::string aResponse;
        std::string aMessage;

        while ( true )
        {
            if ( pStart == pEnd )
            {
                aPoll.fd = readerBroker;
                aPoll.events = POLLIN;
                aPoll.revents = 0;

                (void)poll(&aPoll, 1, -1);

                if( (aPoll.revents & POLLIN) != 0 )
                {
                    nBytes = Util::readFIFO(readerBroker, aBuffer, sizeof(aBuffer));
                    if (nBytes < 0)
                    {
                        pStart = pEnd = NULL;
                        std::cout << Util::logPrefix() << "Error reading message :" << strerror(errno) << std::endl;
                        continue;
                    }
                    pStart = aBuffer;
                    pEnd   = aBuffer + nBytes;
                }
            }

            if ( pStart != pEnd )
            {
                char aChar = *pStart++;
                while (pStart != pEnd && aChar != '\r' && aChar != '\n')
                {
                    aMessage += aChar;
                    aChar = *pStart++;
                }

                if ( aChar == '\r' && *pStart == '\n')
                {
                    pStart++;
                    StringTokenizer tokens(aMessage, " ", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);

                    if (tokens[0] == "search")
                    {
                        if ( !_connections.empty() )
                        {
                            aResponse = std::to_string(Process::id()) + ( aURL == tokens[1] ? " ok \r\n" : " no \r\n");
                            Util::writeFIFO(writerBroker, aResponse.c_str(), aResponse.length() );
                        }
                        else
                        {
                            aURL.clear();
                            aResponse = std::to_string(Process::id()) + " empty \r\n";
                            Util::writeFIFO(writerBroker, aResponse.c_str(), aResponse.length() );
                        }
                    }
                    else if (tokens[0] == "thread")
                    {
                        auto aItem = _connections.find(tokens[1]);
                        if (aItem != _connections.end())
                        { // found item, check if still running
                            std::cout << Util::logPrefix() << "found thread" << std::endl;
                            if ( !aItem->second->isRunning() )
                                std::cout << Util::logPrefix() << "found thread not running!" << std::endl;
                        }
                        else
                        { // new thread id
                            //std::cout << Util::logPrefix() << "new thread starting!" << std::endl;
                            std::shared_ptr<Connection> thread;
                            if ( _connections.empty() )
                            {
                                std::cout << Util::logPrefix() << "create main thread" << std::endl;
                                thread = std::shared_ptr<Connection>(new Connection(loKit, NULL, _childId, tokens[1]));
                            }
                            else
                            {
                                std::cout << Util::logPrefix() << "create view thread" << std::endl;
                                auto aConnection = _connections.begin();
                                thread = std::shared_ptr<Connection>(new Connection(loKit, aConnection->second->getLOKitDocument(), _childId, tokens[1]));
                            }

                            auto aInserted = _connections.insert(
                                std::pair<std::string, std::shared_ptr<Connection>>
                                (
                                    tokens[1],
                                    thread
                                ));

                            if ( aInserted.second )
                                thread->start();
                            else
                                std::cout << Util::logPrefix() << "Connection not created!" << std::endl;

                            std::cout << Util::logPrefix() << "connections: " << Process::id() << " " << _connections.size() << std::endl;
                        }
                    }
                    else if (tokens[0] == "url")
                    {
                        aURL = tokens[1];
                    }
                    else
                    {
                        aResponse = "bad message \r\n";
                        Util::writeFIFO(writerBroker, aResponse.c_str(), aResponse.length() );
                    }
                    aMessage.clear();
                }
            }
        }

        // Destroy LibreOfficeKit
        loKit->pClass->destroy(loKit);

        std::cout << Util::logPrefix() << "loolkit finished OK!" << std::endl;

        pthread_exit(0);
    }
    catch (Exception& exc)
    {
        std::cout << Util::logPrefix() + "Exception: " + exc.what() << std::endl;
    }
    catch (std::exception& exc)
    {
        std::cout << Util::logPrefix() + "Exception: " + exc.what() << std::endl;
    }
}

#ifndef LOOLKIT_NO_MAIN

/// Simple argument parsing wrapper / helper for the above.
int main(int argc, char** argv)
{
    std::string loSubPath;
    Poco::UInt64 _childId = 0;
    std::string _pipe;

    Log::initialize("kit");

    for (int i = 1; i < argc; ++i)
    {
        char *cmd = argv[i];
        char *eq  = NULL;
        if (strstr(cmd, "--losubpath=") == cmd)
        {
            eq = strchrnul(cmd, '=');
            if (*eq)
                loSubPath = std::string(++eq);
        }
        else if (strstr(cmd, "--child=") == cmd)
        {
            eq = strchrnul(cmd, '=');
            if (*eq)
                _childId = std::stoll(std::string(++eq));
        }
        else if (strstr(cmd, "--pipe=") == cmd)
        {
            eq = strchrnul(cmd, '=');
            if (*eq)
                _pipe = std::string(++eq);
        }
    }

    if (loSubPath.empty())
    {
        std::cout << Util::logPrefix() << "--losubpath is empty" << std::endl;
        exit(1);
    }

    if ( !_childId )
    {
        std::cout << Util::logPrefix() << "--child is 0" << std::endl;
        exit(1);
    }

    if ( _pipe.empty() )
    {
        std::cout << Util::logPrefix() << "--pipe is empty" << std::endl;
        exit(1);
    }

    try
    {
        Poco::Environment::get("LD_BIND_NOW");
    }
    catch (Poco::NotFoundException& aError)
    {
        std::cout << Util::logPrefix() << aError.what() << std::endl;
    }

    try
    {
        Poco::Environment::get("LOK_VIEW_CALLBACK");
    }
    catch (Poco::NotFoundException& aError)
    {
        std::cout << Util::logPrefix() << aError.what() << std::endl;
    }

    run_lok_main(loSubPath, _childId, _pipe);

    return 0;
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
