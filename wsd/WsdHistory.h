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

//    class MsgEvent
//    {
//    public:
//        MsgEvent ();
//        MsgEvent(const std::string& action, const std::string& pid, const std::string& filename, const std::string& sid);
//        ~MsgEvent ();
//
//        void set_action (std::string new_var)
//        {
//            _action = new_var;
//        }
//
//        std::string get_action ()
//        {
//            return _action;
//        }
//
//        void set_pid (unsigned long new_var)
//        {
//            _pid = new_var;
//        }
//
//        std::string get_pid ()
//        {
//          return _pid;
//        }
//
//        void set_filename (std::string new_var)
//        {
//            _filename = new_var;
//        }
//
//        std::string get_filename ()
//        {
//          return _filename;
//        }
//
//        void set_sessionId (unsigned long new_var)
//        {
//            _sessionId = new_var;
//        }
//
//        std::string get_sessionId ()
//        {
//            return _sessionId;
//        }
//
//    private:
//        std::string _action;
//        std::string _pid;
//        std::string _filename;
//        std::string _sessionId;
//    };

    //typedef typename std::shared_ptr<MsgEvent> MsgEventPtr;
    //typedef typename std::map<long int, MsgEventPtr> EventsMapType;
    //typedef typename std::shared_ptr<EventsMapType> EventsMapPtr;

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
        //void collect(const std::string& message, const std::map<std::string,Document>& documents);
        std::time_t collectEvent(const std::string& message);

    public: // TODO: must be private
      Events_MapPtr _events;
      AllSnapshots_MapPtr _allSnapshotsMap;
    };

}

#endif /* INCLUDED_WSDHISTORY_H */
