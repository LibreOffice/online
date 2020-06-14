/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config.h>

#include "WopiTestServer.hpp"
#include <Log.hpp>
#include <Unit.hpp>
#include <UnitHTTP.hpp>
#include <helpers.hpp>

#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/OAuth20Credentials.h>
#include <Poco/Util/LayeredConfiguration.h>

using Poco::Net::OAuth20Credentials;

class UnitWopiHttpHeaders : public WopiTestServer
{
    enum class Phase
    {
        Load,
        Polling
    } _phase;

protected:
    bool handleHttpRequest(const Poco::Net::HTTPRequest& request, Poco::MemoryInputStream& message,
                           std::shared_ptr<StreamSocket>& socket) override
    {
        Poco::URI uriReq(request.getURI());
        Poco::RegularExpression regInfo("/wopi/files/[0-9]");
        Poco::RegularExpression regContent("/wopi/files/[0-9]/contents");
        LOG_INF("Fake wopi host request: " << uriReq.toString());

        assertHeaders(request);

        return WopiTestServer::handleHttpRequest(request, message, socket);
    }

    void assertHeaders(const Poco::Net::HTTPRequest& request) const
    {
        static const std::map<std::string, std::string> Headers{
            { "Authorization", "Bearer xyz123abc456vwc789z" },
            { "X-Requested-With", "XMLHttpRequest" },
            { "DNT", "1" }
        };

        for (const auto& pair : Headers)
        {
            LOK_ASSERT_EQUAL(pair.second, request[pair.first]);
        }
    }

public:
    UnitWopiHttpHeaders() :
        _phase(Phase::Load)
    {
    }

    void invokeTest() override
    {
        constexpr char testName[] = "UnitWopiHttpHeaders";

        switch (_phase)
        {
            case Phase::Load:
            {
                // Technically, having an empty line in the header
                // is invalid (it signifies the end of headers), but
                // this is to illustrate that we are able to overcome
                // such issues and generate valid headers.
                const std::string params
                    = "access_header=Authorization%3A%2520Bearer%"
                      "2520xyz123abc456vwc789z%250D%250A%250D%250AX-Requested-With%"
                      "3A%2520XMLHttpRequest&reuse_cookies=language%3Den-us%3AK%3DGS1&http_headers="
                      "X-Requested-With:%20XMLHttpRequest%0D%0ADNT:%201&permission=edit";

                initWebsocket("/wopi/files/0?" + params);

                helpers::sendTextFrame(*getWs()->getLOOLWebSocket(), "load url=" + getWopiSrc(), testName);
                SocketPoll::wakeupWorld();

                _phase = Phase::Polling;
                break;
            }
            case Phase::Polling:
            {
                // Just wait for the results.
                break;
            }
        }
    }
};

UnitBase *unit_create_wsd(void)
{
    return new UnitWopiHttpHeaders();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
