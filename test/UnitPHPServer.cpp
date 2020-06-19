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

#define _PORT_ 9979

int global_ListenerSocket = -1;
int loolWSDPort = 9980;
std::shared_ptr<SocketPoll> _poll;

class PHPClientRequestHandler: public SimpleSocketHandler
{
private:
    std::weak_ptr<StreamSocket> _socket;
    std::string _id;

public:
    PHPClientRequestHandler()
    {
    }
    int _peerID = -1; // Loolsocket will write the response into this socket.

private:
    void onConnect(const std::shared_ptr<StreamSocket>& socket) override
    {
        _id = global_ListenerSocket;
        _socket = socket;
    }

    std::shared_ptr<StreamSocket> connectToLoolWSD(std::shared_ptr<StreamSocket> socket_)
    {
        int sock = 0;
        struct sockaddr_in serv_addr;
        sock = socket(AF_INET, SOCK_STREAM, 0);
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(loolWSDPort);

        // Convert IPv4 and IPv6 addresses from text to binary form
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
        connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

        std::shared_ptr<PHPClientRequestHandler> handler = std::make_shared<PHPClientRequestHandler>();
        handler->_peerID = socket_->getFD();
        std::shared_ptr<StreamSocket> loolSocket = StreamSocket::create<StreamSocket>(sock, false, handler);

        return loolSocket;
    }

    void handleIncomingMessage(SocketDisposition &disposition) override
    {
        disposition.getSocket(); // For unused variable error.
        std::shared_ptr<StreamSocket> socket = _socket.lock();
        std::string res(&socket->getInBuffer()[0], socket->getInBuffer().size());

        if (_peerID == -1)
        {
            // This is a cypress socket. We will add a lool socket and send the message that we received from cypress.
            res.replace(res.find("Host: localhost:9979"), (std::string("Host: localhost:9979")).size(), "Host: localhost:9980");

            std::shared_ptr<StreamSocket> loolSocket = connectToLoolWSD(socket);
            _poll->insertNewSocket(loolSocket);
            loolSocket->send(res);
            //std::cout << "\ngokay - " << res << "\n";
        }
        else
        {
            // This is a lool socket. We will send the response we got from the loolwsd to cypress socket (peer).
            write(_peerID, res.c_str(), std::strlen(res.c_str()));
            std::cout << "\ngokay - " << res << "\n";
        }
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
        int fd = physicalFd;
        std::shared_ptr<Socket> socket = StreamSocket::create<StreamSocket>(fd, false, std::make_shared<PHPClientRequestHandler>());

        return socket;
    }
};

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
            global_ListenerSocket = _serverSocket->getFD();
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
