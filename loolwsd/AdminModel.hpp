/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ADMIN_MODEL_HPP
#define INCLUDED_ADMIN_MODEL_HPP

#include "config.h"

#include <string>
#include <sys/poll.h>

#include <Poco/Net/WebSocket.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Random.h>
#include <Poco/Path.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/Process.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Runnable.h>

#include "Common.hpp"
#include "Util.hpp"
#include "LOOLWSD.hpp"

using Poco::Runnable;
using Poco::StringTokenizer;
using Poco::Process;
using Poco::Thread;
using Poco::Net::WebSocket;

class AdminModel
{
public:
    AdminModel()
    {
        Log::info("AdminModel ctor.");
    }

    ~AdminModel()
    {
        Log::info("AdminModel dtor.");
    }

    void addDocument(Poco::Process::PID pid, std::string url)
    {
        documents[pid] = url;
    }

    unsigned int getUsers(std::string documentUrl);

    std::string getDocuments()
    {
        std::string response;
        for (const auto& it: documents)
        {
            response += std::to_string(it.first)  + " " + it.second + " <BR/>";
        }
        return response;
    }

    unsigned int getMemory(std::string documentUrl);

    void subscribe(std::shared_ptr<WebSocket>& ws)
    {
        adminConsoles.push_back(ws);
    }

    void notify(std::string& message)
    {
        for ( unsigned i = 0; i < adminConsoles.size(); i++ )
        {
            auto adminConsole = adminConsoles[i].lock();
            if (!adminConsole)
            {
                adminConsoles.erase(adminConsoles.begin() + i);
            }
            else
            {
                adminConsole->sendFrame(message.data(), message.length());
            }
        }
    }

private:
    Poco::Process::PID getPID(std::string url);
    std::map<Poco::Process::PID, std::string> documents;

    std::vector<std::weak_ptr<WebSocket> > adminConsoles;
};

#endif
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
