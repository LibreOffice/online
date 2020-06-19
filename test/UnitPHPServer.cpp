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

#define _PORT_ 9979

std::shared_ptr<SocketPoll> _poll;

std::atomic<bool> failed(false);

class PHPLoolClientHandler: public SimpleSocketHandler
{
private:
    std::weak_ptr<StreamSocket> _socket;

public:
    std::shared_ptr<StreamSocket> _peer;
    PHPLoolClientHandler()
    {
    }

private:
    void onConnect(const std::shared_ptr<StreamSocket>& socket) override
    {
        _socket = socket;
    }
    void handleIncomingMessage(SocketDisposition&) override
    {
        std::shared_ptr<StreamSocket> socket = _socket.lock();
        std::string response = std::string(&socket->getInBuffer()[0], socket->getInBuffer().size());
        _peer->send(response);
        socket->shutdown();
        _peer->shutdown();
    }
    int getPollEvents(std::chrono::steady_clock::time_point /* now */, int64_t & /* timeoutMaxMs */) override
    {
        return POLLIN;
    }
    void performWrites() override
    {
    }
};

class PHPProxy
{
public:
    // $_SERVER['QUERY_STRING']
    std::string getQueryString(std::string& request)
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
    void controlQueryString(std::string& queryString, bool& statusOnly)
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
    void header(std::string& result, std::string addition)
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

    void getSocketRequest(std::shared_ptr<StreamSocket> socket, std::string& request)
    {
        request = std::string(&socket->getInBuffer()[0], socket->getInBuffer().size());
    }

    void proxyRequest(std::string& request)
    {
        std::string result = "";
        std::string queryString = getQueryString(request);
        bool statusOnly = false;
        controlQueryString(queryString, statusOnly);

        if (statusOnly == true)
        {
            header(result, "Content-type: application/json");
            header(result, "Cache-Control: no-store");
        }
    }

    std::shared_ptr<StreamSocket> getNewWebSocketSync(std::shared_ptr<StreamSocket> peer)
    {
        addrinfo* ainfo = nullptr;
        addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        int rc = getaddrinfo(std::string("127.0.01").c_str(), std::to_string(LOOLWSD::getClientPortNumber()).c_str(), &hints, &ainfo);

        std::string canonicalName;
        bool isSSL = false;

        if (!rc && ainfo)
        {
            for (addrinfo* ai = ainfo; ai; ai = ai->ai_next)
            {
                if (ai->ai_canonname)
                    canonicalName = ai->ai_canonname;

                if (ai->ai_addrlen && ai->ai_addr)
                {
                    int fd = socket(ai->ai_addr->sa_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
                    int res = connect(fd, ai->ai_addr, ai->ai_addrlen);
                    if (fd < 0 || (res < 0 && errno != EINPROGRESS))
                    {
                        std::cout << "Failed to connect.";
                        ::close(fd);
                    }
                    else
                    {
                        std::shared_ptr<StreamSocket> socket_;
    #if ENABLE_SSL
                        if (isSSL)
                            socket_ = StreamSocket::create<SslStreamSocket>(fd, true, std::make_shared<PHPLoolClientHandler>());
    #endif
                        if (!socket_ && !isSSL)
                        {
                            auto handler = std::make_shared<PHPLoolClientHandler>();
                            handler->_peer = peer;
                            socket_ = StreamSocket::create<StreamSocket>(fd, true, handler);
                        }

                        if (socket_)
                        {
                            std::cout << "\n gokay Connected to client websocket " << socket_->getFD();
                            freeaddrinfo(ainfo);
                            return socket_;
                        }
                        else
                        {
                            std::cout << "Failed to allocate socket for client websocket ";
                            ::close(fd);
                        }

                        break;
                    }
                }
            }

            freeaddrinfo(ainfo);
            return nullptr;
        }
        else
        {
            std::cout << "Failed to lookup client websocket host.";
            return nullptr;
        }
    }

    void startProxy(std::shared_ptr<StreamSocket> socket)
    {
        try
        {
            std::string request = "";
            getSocketRequest(socket, request);
            proxyRequest(request);
            std::cout << "\ngokay request is: " << request << "\n";
            std::shared_ptr<StreamSocket> serverConnection = getNewWebSocketSync(socket);
            _poll->insertNewSocket(serverConnection);
            serverConnection->send(request);
        }
        catch(std::exception& ex)
        {
            std::cout << "\nPHPProxy exception: " << ex.what() << "\n";
            failed = true;
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
    void handleIncomingMessage(SocketDisposition&) override
    {
        std::shared_ptr<StreamSocket> socket = _socket.lock(); // Get shared pointer from weak pointer.

        PHPProxy requestHandler;
        // Start thread. This will process the request and send a response to cypress test etc.
        //std::thread phpProxyThread(&PHPProxy::startProxy, requestHandler, socket);
        //phpProxyThread.detach();

        requestHandler.startProxy(socket);
    }
    int getPollEvents(std::chrono::steady_clock::time_point /* now */, int64_t & /* timeoutMaxMs */) override
    {
        return POLLIN;
    }
    void performWrites() override
    {
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

class UnitPHPServer : public UnitWSD
{
private:
    std::shared_ptr<ServerSocket> _serverSocket;

public:
    UnitPHPServer()
    {
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
            _serverSocket = std::make_shared<ServerSocket>(sType, *_poll, factory);
            _serverSocket->bind(clientPortProto, _PORT_);
            _serverSocket->listen();
            _poll->insertNewSocket(_serverSocket);

            while (true)
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

UnitBase* unit_create_wsd(void) { return new UnitPHPServer(); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
