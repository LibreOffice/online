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
#define _LOOLPORT_ 9980

class FakePHPServer
{
public:
    static std::atomic<bool> _continue;

private:
    int _listener; // Listening socket.
    std::vector<pollfd> _fDescriptors;
    std::vector<int> _createdSockets;
    std::map<int, int> _socketPair; // This will pair the client and the loolwsd server sockets. key: loolsocket, value: clientsocket.

    void initiliazeListenerSocket()
    {
        _listener = socket(AF_INET, SOCK_STREAM, 0);

        int optVal = 1;
        setsockopt(_listener, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optVal, sizeof(optVal));

        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(_PORT_);
        //bind(_listener, (struct sockaddr *)&address, sizeof(address));
        bind(_listener, (sockaddr*)&address, sizeof(address));

        listen(_listener, 5);
    }
    void initiliazeListenerDescriptor()
    {
        _fDescriptors.push_back(pollfd()); // This is for the listener socket..

        _fDescriptors[0].fd = _listener;
        _fDescriptors[0].events = POLLIN; // This means we are ready to read.
    }
    void createSocket()
    {
        sockaddr addr; // This will be filled by the accept function.
        socklen_t addrlen = sizeof addr; // We have to initialize this with the exact size of the addr.
        int newSocket = accept(_listener, &addr, &addrlen); // We accepted the connection and created a socket for it.

        if (newSocket != -1)
        {
            pollfd newFD;
            newFD.fd = newSocket;
            newFD.events = POLLIN; // Wait for information.
            _fDescriptors.push_back(newFD);
            _createdSockets.push_back(newSocket);
        }
    }
    void removeSocket(int i)
    {
        int socketID = _fDescriptors[i].fd;

        close(_fDescriptors[i].fd);
        _fDescriptors.erase(_fDescriptors.begin() + i);
        _createdSockets.erase(_createdSockets.begin() + i);

        // We will also remove its pair.
        if (_socketPair.find(socketID) != _socketPair.end())
        {
            // If this was a loolwsd socket, close corresponding client socket.
            close(_socketPair[socketID]);
            _socketPair.erase(_socketPair.find(socketID));
        }
        else
        {
            // If this was a client socket. Close corresponding loolwsd socket.
            for (auto it = _socketPair.begin(); it != _socketPair.end(); ++it)
            {
                if (it->second == socketID)
                {
                    close(it->first);
                    _socketPair.erase(it);
                    break;
                }
            }
        }
    }
    void pairLoolAndClientSockets(int loolSocket, int clientSocket)
    {
        _socketPair[loolSocket] = clientSocket;
    }
    int createLoolwsdSocket()
    {
        int loolSocket;
        sockaddr_in serverAddr;

        loolSocket = socket(AF_INET, SOCK_STREAM, 0);

        if (loolSocket == -1)
        {
            return -1;
        }
        else
        {
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(_LOOLPORT_);
            int result = inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
            if (result <= 0)
            {
                close(loolSocket);
                return -1;
            }
            else
            {
                result = connect(loolSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
                if (result == -1)
                {
                    close(loolSocket);
                    return -1;
                }
                else
                {
                    _createdSockets.push_back(loolSocket);
                    pollfd lpfd;
                    lpfd.fd = loolSocket;
                    lpfd.events = POLLIN;
                    _fDescriptors.push_back(lpfd);
                    return loolSocket;
                }
            }
        }
    }
    void sendLoolwsdTheRequest(int loolSocket, std::string request)
    {
        send(loolSocket, request.c_str(), strlen(request.c_str()), 0);
    }
    void sendClientTheResponse(int clientSocket, std::string response)
    {
        send(clientSocket, response.c_str(), strlen(response.c_str()), 0);
    }
    std::string proxyRequest(std::string request)
    {
        return request; // It returns the same string for now.
    }
    void handleRequest(int i)
    {
        char buffer[10000];
        int length = recv(_fDescriptors[i].fd, buffer, sizeof buffer, 0);

        if (length <= 0) // There is an error.
        {
            removeSocket(i);
        }
        else
        {
            std::string request(buffer);

            // See if it is a loolwsd socket.
            if (_socketPair.find(_fDescriptors[i].fd) != _socketPair.end())
            {
                // This is a loolwsd socket.
                // We will send the response to the client.
                sendClientTheResponse(_socketPair[_fDescriptors[i].fd], request);
                // Since the circle is done. We will close the socket pair.
                removeSocket(_fDescriptors[i].fd);
            }
            else
            {
                // Here we have a client request.
                /*
                    We will:
                        * Proxy the request.
                        * Create a socket for the loolwsd server.
                        * Pair sockets.
                        * Send the request to the loolwsd server.
                        * Get a response from the loolwsd server.
                        * Send it to the client.
                */
                request = proxyRequest(request);
                int loolSocket = createLoolwsdSocket();
                if (loolSocket != -1)
                {
                    pairLoolAndClientSockets(loolSocket, _fDescriptors[i].fd);
                    sendLoolwsdTheRequest(loolSocket, request);
                }
            }
        }
    }
    void handlePoll()
    {
        poll(_fDescriptors.data(), _fDescriptors.size(), 1000); // We update the requests.
        for(std::size_t i = 0; i < _fDescriptors.size(); i++)
        {
            if (_fDescriptors[i].revents & POLLIN)
            {
                if (_fDescriptors[i].fd == _listener)
                {
                    createSocket(); // Incoming connection.
                }
                else
                {
                    handleRequest(i);
                }
            }
        }
    }
public:
    void start(void)
    {
        initiliazeListenerSocket();
        initiliazeListenerDescriptor();
        while (FakePHPServer::_continue == true)
        {
            handlePoll();
        }
    }

    FakePHPServer()
    {
        FakePHPServer::_continue = true;
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
public:
    void invokeTest()
    {
        // We will first initialize server in another thread.
        // Then we will start cypress_test in another thread too and tell it the server is running on port 9979.
        // They will communicate each other.
        // Then we will join() cypress_test.
        // After joining, we will send stop signal to the server thread by changing _continue variable to false.

        std::stringstream ss;
        std::cout.rdbuf(ss.rdbuf());
        std::stringstream ss2;
        std::cerr.rdbuf(ss2.rdbuf());

        exitTest(UnitBase::TestResult::Ok);

        try
        {
            FakePHPServer server;
            std::thread serverThread(&FakePHPServer::start, server);

            CypressTest test;
            std::thread testThread(&CypressTest::start, test);

            testThread.join();
            FakePHPServer::_continue = false;
            serverThread.join();
            //std::cout << "\n" << ss.str() << "\n";
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            //std::cout << "\n" << ss.str() << "\n";
            exitTest(UnitBase::TestResult::Failed);
        }

        exitTest(UnitBase::TestResult::Ok);
    }
};

UnitBase* unit_create_wsd(void) { return new UnitPHPServer(); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
