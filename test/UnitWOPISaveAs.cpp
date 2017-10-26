/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include "WopiTestServer.hpp"
#include "Log.hpp"
#include "Unit.hpp"
#include "UnitHTTP.hpp"
#include "helpers.hpp"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Util/LayeredConfiguration.h>

class UnitWOPISaveAs : public WopiTestServer
{
    enum class Phase
    {
        LoadAndSaveAs,
        Polling
    } _phase;

public:
    UnitWOPISaveAs() :
        _phase(Phase::LoadAndSaveAs)
    {
    }

    void assertPutRelativeFileRequest(const Poco::Net::HTTPRequest& request) override
    {
        // spec says UTF-7...
        CPPUNIT_ASSERT_EQUAL(std::string("/jan/hole+AWE-ovsk+AP0-/hello world.txt"), request.get("X-WOPI-SuggestedTarget"));
    }

    bool filterSendMessage(const char* data, const size_t len, const WSOpCode /* code */, const bool /* flush */, int& /*unitReturn*/) override
    {
        std::string message(data, len);
        if (message == "saveas: url=" + helpers::getTestServerURI() + "/something%20wopi/files/1?access_token=anything filename=hello%20world.txt")
        {
            // successfully exit the test if we also got the outgoing message
            // notifying about saving the file
            exitTest(TestResult::Ok);
        }

        return false;
    }

    void invokeTest() override
    {
        constexpr char testName[] = "UnitWOPISaveAs";

        switch (_phase)
        {
            case Phase::LoadAndSaveAs:
            {
                initWebsocket("/wopi/files/0?access_token=anything");

                helpers::sendTextFrame(*_ws->getLOOLWebSocket(), "load url=" + _wopiSrc, testName);
                helpers::sendTextFrame(*_ws->getLOOLWebSocket(), "saveas url=wopi:///jan/hole%C5%A1ovsk%C3%BD/hello%20world.txt", testName);
                SocketPoll::wakeupWorld();

                _phase = Phase::Polling;
                break;
            }
            case Phase::Polling:
            {
                // just wait for the results
                break;
            }
        }
    }
};

UnitBase *unit_create_wsd(void)
{
    return new UnitWOPISaveAs();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
