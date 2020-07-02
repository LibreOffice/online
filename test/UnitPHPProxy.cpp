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
#include <atomic>

#define _PORT_ 9979
const int bufferSize = 16 * 1024;

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
public:
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
        helpers::setSocketTimeOut(socket->getFD(), 1000);

        int loolSocket = helpers::connectToLocalServer(LOOLWSD::getClientPortNumber(), 1000, true); // Create a socket for loolwsd.
        if (loolSocket > 0)
        {
            bool closed = false; // Check if one of the sockets closed.
            std::vector<char> buffer;
            while(closed == false)
            {
                closed = socket->getOutBuffer().size() > 0; // If socket could not write data, this will be above zero.
                if (!closed && socket->getInBuffer().size() > 0) // Does client socket have data.
                {
                    sendMessage(loolSocket, socket->getInBuffer());
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

            while (!LOOLWSD::isSSLEnabled())
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