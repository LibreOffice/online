/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_WSDHISTORY_H
#define INCLUDED_WSDHISTORY_H

#include <memory>
#include <list>
#include <set> //#include <unordered_set>
#include <string>

#include <Poco/Process.h>

#include "Log.hpp"
#include "Util.hpp"

class Document;

namespace WsdStats
{
    typedef typename std::shared_ptr<Document> DocumentPtr;

    typedef typename std::map<std::time_t,std::string> TimeString_MapType;

    typedef typename std::shared_ptr<TimeString_MapType> DocSnapshots_MapPtr;
    typedef typename std::shared_ptr<TimeString_MapType> Events_MapPtr;

    typedef typename std::map<std::string,DocSnapshots_MapPtr> AllSnapshots_MapType;
    typedef typename std::shared_ptr<AllSnapshots_MapType> AllSnapshots_MapPtr;

    class WsdHistory
    {
    public:
        WsdHistory();
        ~WsdHistory();
        std::time_t collectEvent(const std::string& message);
        std::string getAllHistory();
        std::string getAllHistoryAsJSON();

    public: // TODO: must be private
      Events_MapPtr _events;
      AllSnapshots_MapPtr _allSnapshotsMap;
    };

}

#endif /* INCLUDED_WSDHISTORY_H */
