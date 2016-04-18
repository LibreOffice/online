/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <dlfcn.h>
#include <ftw.h>
#include <cassert>
#include <iostream>

#include "Common.hpp"
#include "Util.hpp"
#include "Unit.hpp"
#include "Log.hpp"
#include "LOOLProtocol.hpp"

#include <Poco/Timestamp.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Net/HTTPBasicCredentials.h>
#include <Poco/Net/HTTPCookie.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/URI.h>

#define UNIT_URI "/loolwsd/unit-admin"

using Poco::Net::HTTPBasicCredentials;
using Poco::Net::HTTPCookie;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPSClientSession;

// Inside the WSD process
class UnitAdmin : public UnitWSD
{
private:
    unsigned _testCounter = 0;
    std::string _jwtCookie;
    bool _bIsTestRunning = false;
    const Poco::URI _uri;

    typedef bool (UnitAdmin::*AdminTest)(void);

private:
    bool testIncorrectPassword()
    {
        HTTPResponse response;
        std::string path(_uri.getPathAndQuery());
        HTTPRequest request(HTTPRequest::HTTP_GET, path);
        HTTPSClientSession session(_uri.getHost(), _uri.getPort());

        session.sendRequest(request);
        session.receiveResponse(response);
        if (response.getStatus() == HTTPResponse::HTTP_UNAUTHORIZED)
            return true;

        return false;
    }

    bool testCorrectPassword()
    {
        HTTPResponse response;
        std::string path(_uri.getPathAndQuery());
        HTTPRequest request(HTTPRequest::HTTP_GET, path);
        HTTPSClientSession session(_uri.getHost(), _uri.getPort());
        HTTPBasicCredentials credentials("admin", "admin");
        credentials.authenticate(request);

        session.sendRequest(request);
        session.receiveResponse(response);
        std::vector<HTTPCookie> cookies;
        response.getCookies(cookies);

        for (auto& cookie: cookies)
        {
            Log::info("Cookie found: " + cookie.getName());

            if (cookie == "jwt")
            {
                // jwt is present
                return true;
            }
        }

        return false;
    }

private:
    AdminTest _tests[2] =
    {
        &UnitAdmin::testIncorrectPassword,
        &UnitAdmin::testCorrectPassword
        // Subscribe to updates here
/*        &UnitAdmin::testWebSocket,
        // Do variety of add docs (preferrably different docs)
        // Also keep logging mem usage
        &UnitAdmin::testAddDocNotify,
        // Do variety of rm docs
        // keep logging mem usage
        &UnitAdmin::testRmDocNotify,
        // query Admin for active_user_count, active_docs_count
        &UnitAdmin::testDocCount,
        // &UnitAdmin::Test mem usage timer
        &UnitAdmin::testMemUsageTimer,
        // &UnitAdmin::Test settings default value
        &UnitAdmin::testSettings,
        // &UnitAdmin::Test settings update
        &UnitAdmin::testUpdateSettings
*/
    };


public:
    UnitAdmin()
        : _uri("https://127.0.0.1:" + std::to_string(DEFAULT_CLIENT_PORT_NUMBER) + "/loleaflet/dist/admin/admin.html")
    {

    }

    virtual void invokeTest()
    {
        if (!_bIsTestRunning)
        {
            _bIsTestRunning = true;
            AdminTest test = _tests[_testCounter++];
            bool res = ((*this).*(test))();
            if (!res)
            {
                exitTest(TestResult::TEST_FAILED);
                return;
            }

            _bIsTestRunning = false;
            Log::info("Test passed");
            // End this UT when all tests are finished
            if (sizeof _tests == _testCounter)
                exitTest(TestResult::TEST_OK);
        }
    }
};

UnitBase *unit_create_wsd(void)
{
    return new UnitAdmin();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
