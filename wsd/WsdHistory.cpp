/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "WsdHistory.h"
#include "AdminModel.hpp"

#include <cstddef>
#include <sstream>
#include <Poco/StringTokenizer.h>
#include <Poco/URI.h>

using Poco::StringTokenizer;

WsdStats::WsdHistory::WsdHistory() :
        _events(std::make_shared<TimeString_MapType>()),
        _allSnapshotsMap(std::make_shared<AllSnapshots_MapType>())
{
}

WsdStats::WsdHistory::~WsdHistory()
{
}

std::time_t WsdStats::WsdHistory::collectEvent(const std::string& message)
{
    if(message.find("adddoc") == 0
    || message.find("resetidle") == 0
    || message.find("rmdoc") == 0)
    {
        const std::time_t now = std::time(nullptr);
        {
            StringTokenizer mTkns(message, " ", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);
            if (mTkns.count() == 5)
            {
                _events->emplace(std::make_pair(now, mTkns[0]+' '+mTkns[1]+' '+mTkns[2]+' '+mTkns[3] ));
                return now;
            }
            else if (mTkns.count() == 3)
            {
                _events->emplace(std::make_pair(now, mTkns[0]+' '+mTkns[1]+" \t "+mTkns[2] ));
                return now;
            }
            else if (mTkns.count() == 2)
            {
                _events->emplace(std::make_pair(now, mTkns[0]+' '+mTkns[1]+" \t  \t " ));
                return now;
            }
        }
    }
    return 0;
}