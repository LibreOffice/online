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

#include <memory>
#include <string>

#include <Poco/Net/WebSocket.h>
#include <Poco/Process.h>

#include "Util.hpp"

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
        _documents[pid] = url;
    }

    std::string getDocuments()
    {
        std::string response;
        for (const auto& it: _documents)
        {
            response += std::to_string(it.first)  + " " + it.second + " <BR/>";
        }

        return response;
    }

    void subscribe(std::shared_ptr<Poco::Net::WebSocket>& ws)
    {
        _adminConsoles.push_back(ws);
    }

    void notify(std::string& message)
    {
        for ( unsigned i = 0; i < _adminConsoles.size(); i++ )
        {
            auto adminConsole = _adminConsoles[i].lock();
            if (!adminConsole)
            {
                _adminConsoles.erase(_adminConsoles.begin() + i);
            }
            else
            {
                adminConsole->sendFrame(message.data(), message.length());
            }
        }
    }

private:
    std::vector<std::weak_ptr<Poco::Net::WebSocket> > _adminConsoles;
    std::map<Poco::Process::PID, std::string> _documents;
};

#endif
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
