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

WsdStats::MsgEvent::MsgEvent()
{

}

WsdStats::MsgEvent::MsgEvent(const std::string& action, const std::string& pid, const std::string& filename, const std::string& sid)
{
    this->_action = action;
    this->_filename = filename;
    this->_pid = pid;
    this->_sessionId = sid;
}

WsdStats::MsgEvent::~MsgEvent()
{

}

WsdStats::MsgDocument::MsgDocument()
{

}

WsdStats::MsgDocument::MsgDocument(std::string dockey, std::string pid, std::string fn, std::string numViews, std::string mem, std::string et, std::string it)
{
    this->_docKey = dockey;
    this->_pid = pid;
    this->_filename = fn;
    this->_numberOfViews = numViews;
    this->_memoryConsumed = mem;
    this->_elapsedTime = et;
    this->_idleTime = it;
}

std::string WsdStats::MsgDocument::to_string()
{
    std::ostringstream oss;
    oss << this->_pid << "\t" << this->_filename << "\t" << this->_numberOfViews << "\t" << this->_memoryConsumed << "\t" << this->_elapsedTime << "\t" << this->_idleTime;
    return oss.str();
}

WsdStats::MsgDocument::~MsgDocument()
{

}

WsdStats::WsdHistory::WsdHistory() :
        _events(std::make_shared<EventsMapType>()),
        _messages(std::make_shared<DocumentMessSetType>()),
        _documents(std::make_shared<SnapshotsMapType>())
{
}

WsdStats::WsdHistory::~WsdHistory()
{
}

void WsdStats::WsdHistory::collect(const std::string& message, const std::map<std::string,Document>& documents)
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
                _events->emplace(std::make_pair(now, std::make_shared<MsgEvent>(mTkns[0], mTkns[1], mTkns[2], mTkns[3]) ));
            }
            else if (mTkns.count() == 3)
            {
                _events->emplace(std::make_pair(now, std::make_shared<MsgEvent>(mTkns[0], mTkns[1], "\t", mTkns[2]) ));
            }
            else if (mTkns.count() == 2)
            {
                _events->emplace(std::make_pair(now, std::make_shared<MsgEvent>(mTkns[0], mTkns[1], "\t", "\t") ));
            }
        }

        SnapshotListPtr snapshot(std::make_shared<SnapshotListType>());
        for (const auto& it: documents)
        {
            if (!it.second.isExpired())
            {
                std::string encodedFilename;
                Poco::URI::encode(it.second.getFilename(), " ", encodedFilename);
                MsgDocumentPtr msgd(std::make_shared<MsgDocument>(it.first, std::to_string(it.second.getPid()), encodedFilename, std::to_string(it.second.getActiveViews()), std::to_string(Util::getMemoryUsage(it.second.getPid())), std::to_string(it.second.getElapsedTime()), std::to_string(it.second.getIdleTime())));
                auto messIt = _messages->find(msgd);
                if(messIt == _messages->end())
                {
                    // _messages will be an unordered_set
                    std::pair<std::_Rb_tree_const_iterator<MsgDocumentPtr>, bool> outcome = _messages->emplace(std::move(msgd));
                    if(outcome.second)
                    {
                        snapshot->emplace(snapshot->end(), *(outcome.first) );
                    }
                }
                else
                {
                    snapshot->emplace(snapshot->end(), *messIt );
                }
                msgd = nullptr;
            }
        }
        _documents->emplace(std::make_pair(now, std::move(snapshot) ));
        snapshot = nullptr;
    }
}