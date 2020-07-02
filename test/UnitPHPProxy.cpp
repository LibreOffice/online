/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
    road path:
        * cypress test => php server => loolwsd
        * loolwsd => php server => cypress test
*/

#include <memory>
#include <ostream>
#include <set>
#include <string>

#include <Poco/Exception.h>
#include <Poco/RegularExpression.h>
#include <Poco/URI.h>
#include <test/lokassert.hpp>

#include <Png.hpp>
#include <Unit.hpp>
#include <helpers.hpp>
#include "net/ServerSocket.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <map>
#include <LOOLWSD.hpp>
#include <atomic>
#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>

#define _PORT_ 9979
const int bufferSize = 16 * 1024;

std::atomic<bool> failed(false);
std::mutex clientLock;  // protect client send/read.

// This class has nothing in common with the others here. This class just runs the cypress tests.
class CypressTest
{
public:
    void start()
    {
        // spec is added for testing the unit test. Normally it will run all the tests (hopefully).
        std::string command = "cd cypress_test && make check-mobile running_port=9979 spec=calc/apply_font_spec.js";
        system(command.c_str());
    }
};

class PHPProxy
{
public:
    // $_SERVER['QUERY_STRING']
    static std::string getQueryString(std::string& request)
    {
        std::string queryString = Util::splitStringToVector(request, '\n')[0]; // GET www.example.com?test=1
        queryString = Util::splitStringToVector(queryString, ' ')[1]; // www.example.com?test=1
        if (Util::splitStringToVector(queryString, '?').size() > 1)
        {
            return Util::splitStringToVector(queryString, '?')[1]; // test = 1;
        }
        else
        {
            return "";
        }
    }

    // Get the type of the request and modify the queryString if needed.
    static void controlQueryString(std::string& queryString, bool& statusOnly)
    {
        if (Util::startsWith(queryString, "status"))
        {
            queryString = "";
            statusOnly = true;
        }
        else if (Util::startsWith(queryString, "req="))
        {
            queryString = queryString.substr(0, std::strlen("req="));
            if (queryString.substr(0, 1) != "/")
            {
                LOG_ERR("First ?req= param should be an absolute path.");
                std::cout << "\n" << "First ?req= param should be an absolute path" <<"\n";
            }
        }
        else
        {
            LOG_ERR("The param should be \"status\" or \"req=\".");
            std::cout << "\n" << "The param should be \"status\" or \"req=...\"." << "\n";
        }

        LOG_DBG("Debug URI: " + queryString);

        if (queryString == "" && statusOnly == false)
        {
            LOG_ERR("Missing, required req= parameter.");
        }
    }

    // header('')
    static void header(std::string& result, std::string addition)
    {
        if (!result.empty())
        {
            result += "\n" + addition;
        }
        else
        {
            result = addition;
        }
    }

    static void proxyRequest(std::string& request)
    {
        std::string result = "";
        std::string queryString = PHPProxy::getQueryString(request);
        bool statusOnly = false;
        controlQueryString(queryString, statusOnly);

        if (statusOnly == true)
        {
            PHPProxy::header(result, "Content-type: application/json");
            PHPProxy::header(result, "Cache-Control: no-store");
        }
    }
};

class SocketThread
{
private:
    static void setSocketTimeOut(int socketFD, int timeMS)
    {
        struct timeval tv;
        tv.tv_sec = (float)timeMS / (float)1000;
        tv.tv_usec = timeMS;
        setsockopt(socketFD, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    }
    static std::shared_ptr<StreamSocket> createAndConnectLoolwsdSocket()
    {
        int socketFD = 0;
        struct sockaddr_in serv_addr;

        if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            std::cout << "Server client could not be created.";
            return nullptr;
        }
        else
        {
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(LOOLWSD::getClientPortNumber());
            if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
            {
                std::cout << "Invalid address.";
                close(socketFD);
                return nullptr;
            }
            else
            {
                if (connect(socketFD, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
                {
                    std::cout << "Connection failed.";
                    close(socketFD);
                    return nullptr;
                }
                else
                {
                    SocketThread::setSocketTimeOut(socketFD, 1000);
                    std::shared_ptr<StreamSocket> socket = StreamSocket::create<StreamSocket>(socketFD, false, std::make_shared<WebSocketHandler>());
                    return socket;
                }
            }
        }
    }
    static bool isWebSocket(std::string& request)
    {
        return Util::startsWith(request, "GET /lool");
    }
    static void handleRegularSocket(std::shared_ptr<StreamSocket> socket)
    {
        socket->setThreadOwner(std::this_thread::get_id());
        std::shared_ptr<StreamSocket> loolSocket = createAndConnectLoolwsdSocket(); // Create a socket for loolwsd.
        if (loolSocket)
        {
            loolSocket->send(socket->getInBuffer().data(), socket->getInBuffer().size());
            while(loolSocket->readIncomingData()){}; // Read until socket closes.
            socket->send(loolSocket->getInBuffer().data(), loolSocket->getInBuffer().size()); // Send the response to client.
            loolSocket->closeConnection();
        }
        socket->closeConnection(); // Close client socket.
    }
    static void handleWebSocket(std::shared_ptr<StreamSocket> socket)
    {
        socket->setThreadOwner(std::this_thread::get_id());

        // On this type of communication, sockets are not closed. When there is no data to read, "read/recv" function may hang up.
        // So we will set a timeout to client socket. lool socket already has a timeout.
        SocketThread::setSocketTimeOut(socket->getFD(), 1000);

        std::shared_ptr<StreamSocket> loolSocket = createAndConnectLoolwsdSocket();
        if (loolSocket)
        {
            bool closed = false; // Check if one of the sockets closed.
            while(closed == false)
            {
                closed = socket->getOutBuffer().size() > 0; // If socket could not write data, this will be above zero.
                if (socket->getInBuffer().size() > 0) // Does client socket have data.
                {
                    loolSocket->send(socket->getInBuffer().data(), socket->getInBuffer().size()); // We send the data to lool.
                    socket->getInBuffer().clear(); // Clear client socket buffer.
                }
                closed = loolSocket->getOutBuffer().size() > 0;
                if (closed == false)
                {
                    loolSocket->getInBuffer().clear(); // Clear lool socket buffer.
                    loolSocket->readIncomingData(); // Read reply of lool.
                    if (loolSocket->getInBuffer().size() > 0) // Does lool have something to say.
                    {
                        socket->send(loolSocket->getInBuffer().data(), loolSocket->getInBuffer().size()); // We send the data to client.
                        loolSocket->getInBuffer().clear(); // Clear lool socket buffer.
                    }
                    socket->readIncomingData(); // Does client have something new to say.
                }
            }
            loolSocket->closeConnection();
        }
        socket->closeConnection();
    }
    static void setSocketOpitonToBlockingMode(int socketFD)
    {
        int opts;
        opts = fcntl(socketFD, F_GETFL);
        opts = opts & (~O_NONBLOCK);
        fcntl(socketFD, F_SETFL, opts);
    }
public:
    static void startThread(std::shared_ptr<StreamSocket> socket)
    {
        // Set socket's option to blocking mode.
        SocketThread::setSocketOpitonToBlockingMode(socket->getFD());

        std::string request(socket->getInBuffer().begin(), socket->getInBuffer().end());
        bool webSocket = SocketThread::isWebSocket(request);
        request.clear();
        if (webSocket == true)
        {
            std::thread webSocketThread(&SocketThread::handleWebSocket, socket);
            webSocketThread.detach(); // This prevents thread from stopping when variable goes out of scope.
        }
        else
        {
            std::thread regularSocketThread(&SocketThread::handleRegularSocket, socket);
            regularSocketThread.detach();
        }
    }
};

class PHPClientRequestHandler: public SimpleSocketHandler
{
private:
    std::weak_ptr<StreamSocket> _socket;

public:
    PHPClientRequestHandler()
    {
    }

private:
    void onConnect(const std::shared_ptr<StreamSocket>& socket) override
    {
        _socket = socket;
    }
    int getPollEvents(std::chrono::steady_clock::time_point /* now */, int64_t & /* timeoutMaxMs */) override
    {
        return POLLIN;
    }
    void performWrites() override
    {
    }

    void handleIncomingMessage(SocketDisposition& disposition) override
    {
        std::shared_ptr<StreamSocket> socket = _socket.lock();
        disposition.setMove([=] (const std::shared_ptr<Socket> &moveSocket)
        {
            moveSocket->setThreadOwner(std::thread::id(0));
            SocketThread::startThread(socket);
        });
    }
};

class PHPServerSocketFactory final : public SocketFactory
{
public:
    PHPServerSocketFactory()
    {
    }

    std::shared_ptr<Socket> create(const int physicalFd) override
    {
        // This socket is test's client.
        std::shared_ptr<Socket> socket = StreamSocket::create<StreamSocket>(physicalFd, false, std::make_shared<PHPClientRequestHandler>());

        return socket;
    }
};

class UnitPHPProxy : public UnitWSD
{
private:
    std::shared_ptr<SocketPoll> _poll;

public:
    UnitPHPProxy()
    {
    }

    void configure(Poco::Util::LayeredConfiguration& config) override
    {
        UnitWSD::configure(config);
        config.setBool("ssl.enable", false);
        config.setBool("net.proxy_prefix", true);
    }

    void invokeTest()
    {
        try
        {
            _poll = std::make_shared<SocketPoll>("php client poll");
            _poll->startThread();
            ServerSocket::Type clientPortProto = ServerSocket::Type::Public;
            Socket::Type sType = Socket::Type::IPv4;
            std::shared_ptr<SocketFactory> factory = std::make_shared<PHPServerSocketFactory>();
            std::shared_ptr<ServerSocket> _serverSocket = std::make_shared<ServerSocket>(sType, *_poll, factory);
            _serverSocket->bind(clientPortProto, _PORT_);
            _serverSocket->listen(4);
            _poll->insertNewSocket(_serverSocket);

            while (!LOOLWSD::isSSLEnabled())
            {

            }

            CypressTest test;
            std::thread testThread(&CypressTest::start, test);
            testThread.join();

            exitTest(UnitBase::TestResult::Ok);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            exitTest(UnitBase::TestResult::Failed);
        }
    }
};

UnitBase* unit_create_wsd(void) { return new UnitPHPProxy(); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */