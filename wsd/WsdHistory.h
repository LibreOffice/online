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

    class MsgEvent
    {
    public:
        MsgEvent ();
        MsgEvent(const std::string& action, const std::string& pid, const std::string& filename, const std::string& sid);
        ~MsgEvent ();

        void set_action (std::string new_var)
        {
            _action = new_var;
        }

        std::string get_action ()
        {
            return _action;
        }

        void set_pid (unsigned long new_var)
        {
            _pid = new_var;
        }

        std::string get_pid ()
        {
          return _pid;
        }

        void set_filename (std::string new_var)
        {
            _filename = new_var;
        }

        std::string get_filename ()
        {
          return _filename;
        }

        void set_sessionId (unsigned long new_var)
        {
            _sessionId = new_var;
        }

        std::string get_sessionId ()
        {
            return _sessionId;
        }

    private:
        std::string _action;
        std::string _pid;
        std::string _filename;
        std::string _sessionId;
    };

    class MsgDocument
    {
        friend std::ostream& WsdStats::operator<<(std::ostream& oss, WsdStats::MsgDocument const& v)
        {
            oss << v._pid << "\t" << v._filename << "\t" << v._numberOfViews << "\t" << v._memoryConsumed << "\t" << v._elapsedTime << "\t" << v._idleTime;
            return oss;
        }

    public:
        MsgDocument ();
        MsgDocument(std::string dockey, std::string pid, std::string fn, std::string numViews, std::string mem, std::string et, std::string it);
        ~MsgDocument ();
        std::string to_string();
        bool operator <( const WsdStats::MsgDocument &rhs ) const { return ( _pid < rhs._pid ); }

    private:
        std::string _docKey;
        std::string _pid;
        std::string _filename;
        std::string _numberOfViews;
        std::string _memoryConsumed;
        std::string _elapsedTime;
        std::string _idleTime;
    };

    typedef typename std::shared_ptr<MsgEvent> MsgEventPtr;
    typedef typename std::map<long int, MsgEventPtr> EventsMapType;
    typedef typename std::shared_ptr<EventsMapType> EventsMapPtr;

    typedef typename std::shared_ptr<MsgDocument> MsgDocumentPtr;
    typedef typename std::set<MsgDocumentPtr> DocumentMessSetType;
    typedef typename std::shared_ptr<DocumentMessSetType> DocumentsSetPtr;

    typedef typename std::list<MsgDocumentPtr> SnapshotListType;
    typedef typename std::shared_ptr<SnapshotListType> SnapshotListPtr;
    typedef typename std::map<long int, SnapshotListPtr> SnapshotsMapType;
    typedef typename std::shared_ptr<SnapshotsMapType> SnapshotsMapPtr;

    class WsdHistory
    {
    public:
        WsdHistory();
        ~WsdHistory();
        void collect(const std::string& message, const std::map<std::string,Document>& documents);

    public: // TODO: must be private
      EventsMapPtr _events;
      DocumentsSetPtr _messages; // will be an unordered_set!!
      SnapshotsMapPtr _documents;
    };

}

#endif /* INCLUDED_WSDHISTORY_H */