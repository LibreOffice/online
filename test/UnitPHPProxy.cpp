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
#include <LOOLWSD.hpp>

#define _PORT_ 9979
const int bufferSize = 16 * 1024;

class SocketThread
{
public:
    void addProxyHeader(std::vector<char>& message)
    {
        const std::string pp = "ProxyPrefix: http://localhost:" + std::to_string(_PORT_) + "\n";
        std::vector<char>::iterator it = std::find(message.begin(), message.end(), '\n');

        // Found the first line break. We will paste the prefix on the second line.
        if (it == message.end())
        {
            message.insert(it, pp.data(), &pp.data()[pp.size()]);
        }
        else
        {
            message.insert(it + 1, pp.data(), &pp.data()[pp.size()]);
        }
    }
    bool isWebSocket(std::string& request)
    {
        return Util::startsWith(request, "GET /lool");
    }
    bool sendMessage(int socketFD, std::vector<char>& message)
    {
        int res;
        std::size_t wroteLen = 0;
        do
        {
            res = send(socketFD, &message[wroteLen], (wroteLen + bufferSize < message.size() ? bufferSize: message.size() - wroteLen), MSG_NOSIGNAL);
            wroteLen += bufferSize;
        }
        while (wroteLen < message.size() && res > 0);
        return res > 0;
    }
    bool readMessage(int socketFD, std::vector<char>& inBuffer)
    {
        char buf[16 * 1024];
        ssize_t len;
        do
        {
            do
            {
                len = recv(socketFD, buf, sizeof(buf), 0);
            }
            while (len < 0 && errno == EINTR);

            if (len > 0)
            {
                inBuffer.insert(inBuffer.end(), &buf[0], &buf[len]);
            }
        }
        while (len == (sizeof(buf)));
        return len > 0;
    }
    void handleRegularSocket(std::shared_ptr<StreamSocket> socket)
    {
        socket->setThreadOwner(std::this_thread::get_id());

        addProxyHeader(socket->getInBuffer());

        int loolSocket = helpers::connectToLocalServer(LOOLWSD::getClientPortNumber(), 1000, true); // Create a socket for loolwsd.
        if (loolSocket > 0)
        {
            sendMessage(loolSocket, socket->getInBuffer());
            std::vector<char> buffer;
            while(readMessage(loolSocket, buffer)){};
            socket->send(buffer.data(), buffer.size()); // Send the response to client.
            close(loolSocket);
        }
        socket->closeConnection(); // Close client socket.
    }
    void handleWebSocket(std::shared_ptr<StreamSocket> socket)
    {
        socket->setThreadOwner(std::this_thread::get_id());

        // With this type of communication, sockets are not closed. When there is no data to read, "read/recv" function may hang up.
        // So we will set a timeout to client socket. lool socket already has a timeout.
        helpers::setSocketTimeOut(socket->getFD(), 100);

        addProxyHeader(socket->getInBuffer());

        int loolSocket = helpers::connectToLocalServer(LOOLWSD::getClientPortNumber(), 100, true); // Create a socket for loolwsd.
        if (loolSocket > 0)
        {
            bool closed = false; // Check if one of the sockets closed.
            std::vector<char> buffer;
            while(closed == false)
            {
                closed = socket->getOutBuffer().size() > 0; // If socket could not write data, this will be above zero.
                if (!closed && socket->getInBuffer().size() > 0) // Does client socket have data.
                {
                    closed = !sendMessage(loolSocket, socket->getInBuffer());
                    socket->getInBuffer().clear(); // Clear client socket buffer.
                }
                readMessage(loolSocket, buffer);
                if (!closed && !buffer.empty()) // Does lool have something to say.
                {
                    socket->send(buffer.data(), buffer.size()); // We send the data to client.
                    buffer.clear(); // Clear lool socket buffer.
                }
                socket->readIncomingData(); // Does client have something new to say.
            }
            close(loolSocket);
        }
        socket->closeConnection();
    }
    static void startThread(std::shared_ptr<StreamSocket> socket)
    {
        SocketThread worker;
        // Set socket's option to blocking mode.
        helpers::setSocketBlockingMode(socket->getFD(), true);

        std::string request(socket->getInBuffer().begin(), socket->getInBuffer().end());
        bool webSocket = worker.isWebSocket(request);
        request.clear();

        if (webSocket == true)
        {
            std::thread webSocketThread(&SocketThread::handleWebSocket, worker, socket);
            webSocketThread.detach(); // This prevents thread from stopping when variable goes out of scope.
        }
        else
        {
            std::thread regularSocketThread(&SocketThread::handleRegularSocket, worker, socket);
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
            _serverSocket->listen(10);
            _poll->insertNewSocket(_serverSocket);

            while (true)
            {

            }

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