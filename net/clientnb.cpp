/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <assert.h>

#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/FilePartSource.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Net/KeyConsoleHandler.h>
#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/StreamCopier.h>
#include <Poco/URI.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Runnable.h>
#include <Poco/Thread.h>

using Poco::Net::HTTPClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using Poco::Net::WebSocket;
using Poco::Runnable;
using Poco::Thread;
using Poco::URI;
using Poco::Util::Application;
using Poco::Util::HelpFormatter;
using Poco::Util::Option;
using Poco::Util::OptionSet;

const char *HostName = "127.0.0.1";
constexpr int HttpPortNumber = 9191;
constexpr int SslPortNumber = 9193;

static bool EnableHttps = false;

struct Session
{
    std::string _session_name;
    Poco::Net::HTTPClientSession *_session;

    Session(const char *session_name, bool https = false)
        : _session_name(session_name)
    {
        if (https)
            _session = new Poco::Net::HTTPSClientSession(HostName, SslPortNumber);
        else
            _session = new Poco::Net::HTTPClientSession(HostName, HttpPortNumber);
    }
    ~Session()
    {
        delete _session;
    }

    void sendPing(int i)
    {
        Poco::Net::HTTPRequest request(
            Poco::Net::HTTPRequest::HTTP_POST,
            "/ping/" + _session_name + "/" + std::to_string(i));
        try {
            Poco::Net::HTMLForm form;
            form.setEncoding(Poco::Net::HTMLForm::ENCODING_MULTIPART);
            form.prepareSubmit(request);
            form.write(_session->sendRequest(request));
        }
        catch (const Poco::Exception &e)
        {
            std::cerr << "Failed to write data: " << e.name() <<
                  " " << e.message() << "\n";
            throw;
        }
    }

    std::string getResponseString()
    {
        Poco::Net::HTTPResponse response;
        std::istream& responseStream = _session->receiveResponse(response);
        const std::string result(std::istreambuf_iterator<char>(responseStream), {});
        // std::cerr << "Got response '" << result << "'\n";
        return result;
    }

    int getResponseInt()
    {
        int number = 42;

        try {
//            std::cerr << "try to get response\n";
            const std::string result = getResponseString();
            number = std::stoi(result);
        }
        catch (const Poco::Exception &e)
        {
            std::cerr << "Exception converting: " << e.name() <<
                  " " << e.message() << "\n";
            throw;
        }
        return number;
    }

    std::shared_ptr<WebSocket> getWebSocket()
    {
        _session->setTimeout(Poco::Timespan(10, 0));
        HTTPRequest request(HTTPRequest::HTTP_GET, "/ws");
        HTTPResponse response;
        return std::shared_ptr<WebSocket>(
            new WebSocket(*_session, request, response));
    }
};

struct ThreadWorker : public Runnable
{
    const char *_domain;
    ThreadWorker(const char *domain = nullptr)
        : _domain(domain)
    {
    }
    virtual void run()
    {
        for (int i = 0; i < 100; ++i)
        {
            Session ping(_domain ? _domain : "init", EnableHttps);
            ping.sendPing(i);
            int back = ping.getResponseInt();
            assert(back == i + 1);
        }
    }
};

struct Client : public Poco::Util::Application
{
    void testPing()
    {
        std::cerr << "testPing\n";
        Session first("init", EnableHttps);
        Session second("init", EnableHttps);

        int count = 42, back;
        first.sendPing(count);
        second.sendPing(count + 1);

        back = first.getResponseInt();
        std::cerr << "testPing: " << back << "\n";
        assert (back == count + 1);

        back = second.getResponseInt();
        std::cerr << "testPing: " << back << "\n";
        assert (back == count + 2);
    }

    void testLadder()
    {
        std::cerr << "testLadder\n";
        ThreadWorker ladder;
        Thread thread;
        thread.start(ladder);
        thread.join();
    }

    void testParallel()
    {
        std::cerr << "testParallel\n";
        const int num = 10;
        Thread snakes[num];
        ThreadWorker ladders[num];

        for (size_t i = 0; i < num; i++)
            snakes[i].start(ladders[i]);

        for (int i = 0; i < num; i++)
            snakes[i].join();
    }

    void testWebsocket()
    {
        std::cerr << "testwebsocket\n";
        Session session("ws", EnableHttps);
        std::shared_ptr<WebSocket> ws = session.getWebSocket();

        std::string send = "hello there";
        ws->sendFrame(&send[0], send.length(),
                      WebSocket::SendFlags::FRAME_TEXT);

        for (size_t i = 0; i < 10; i++)
        {
            ws->sendFrame(&i, sizeof(i), WebSocket::SendFlags::FRAME_BINARY);
            size_t back[5];
            int flags = 0;
            int recvd = ws->receiveFrame((void *)back, sizeof(back), flags);
            assert(recvd == sizeof(size_t));
            assert(back[0] == i + 1);
        }
    }

public:
    int main(const std::vector<std::string>& args) override
    {
        EnableHttps = (args.size() > 0 && args[0] == "ssl");
        std::cerr << "Starting " << (EnableHttps ? "HTTPS" : "HTTP") << " client." << std::endl;

        if (EnableHttps)
        {
            Poco::Net::initializeSSL();
            // Just accept the certificate anyway for testing purposes
            Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> invalidCertHandler = new Poco::Net::AcceptCertificateHandler(false);

            Poco::Net::Context::Params sslParams;
            Poco::Net::Context::Ptr sslContext = new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, sslParams);
            Poco::Net::SSLManager::instance().initializeClient(nullptr, invalidCertHandler, sslContext);
        }

        testWebsocket();

        testPing();
        testLadder();
        testParallel();

        return 0;
    }
};

POCO_APP_MAIN(Client)

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
