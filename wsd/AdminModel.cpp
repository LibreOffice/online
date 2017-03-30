/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include "AdminModel.hpp"

#include <memory>
#include <set>
#include <sstream>
#include <string>

#include <Poco/Process.h>
#include <Poco/StringTokenizer.h>
#include <Poco/URI.h>

#include "Protocol.hpp"
#include "net/WebSocketHandler.hpp"
#include "Log.hpp"
#include "Unit.hpp"
#include "Util.hpp"

void Document::addView(const std::string& sessionId)
{
    const auto ret = _views.emplace(sessionId, View(sessionId));
    if (!ret.second)
    {
        LOG_WRN("View with SessionID [" << sessionId << "] already exists.");
    }
    else
    {
        ++_activeViews;
    }
}

int Document::expireView(const std::string& sessionId)
{
    auto it = _views.find(sessionId);
    if (it != _views.end())
    {
        it->second.expire();

        // If last view, expire the Document also
        if (--_activeViews == 0)
            _end = std::time(nullptr);
    }

    return _activeViews;
}

const std::shared_ptr<Poco::JSON::Object> Document::getSnapshot() const
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"creationTime\"" << ":" << std::time(nullptr) << ",";
    oss << "\"document\"" << ":{";
    //oss << "\"docKey\"" << ":\"" << this->_docKey << "\",";
    oss << "\"pid\"" << ":" << this->getPid() << ",";
    oss << "\"views\"" << ":[";
    const char* separator = "";
    for (auto view : this->getViews())
    {
        oss << separator << "\"" << view.first << "\"";
        separator = ",";
    }
    oss << "],";
    oss << "\"activeViews\"" << ":" << this->getActiveViews() << ",";
    oss << "\"filename\"" << ":\"" << this->getFilename() << "\",";
    oss << "\"memoryDirty\"" << ":" << this->getMemoryDirty() << ",";
    oss << "\"start\"" << ":" << this->_start << ",";
    oss << "\"lastActivity\"" << ":" << this->_lastActivity << ",";
    oss << "\"end\"" << ":" << this->_end;
    oss << "}}";
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var result = parser.parse(oss.str());
    return std::make_shared<Poco::JSON::Object>(*result.extract<Poco::JSON::Object::Ptr>().get());
}

void Document::takeSnapshot()
{
    auto s = this->getSnapshot();
    const std::time_t creationTime = s->getValue<std::time_t>("creationTime");
    auto insPoint = _snapshots.upper_bound(creationTime);
    _snapshots.insert(insPoint, std::make_pair(creationTime,s));
}

std::string Document::to_string() const
{
    std::ostringstream oss;
    std::string encodedFilename;
    Poco::URI::encode(this->getFilename(), " ", encodedFilename);
    oss << this->getPid() << ' '
        << encodedFilename << ' '
        << this->getActiveViews() << ' '
        << this->getMemoryDirty() << ' '
        << this->getElapsedTime() << ' '
        << this->getIdleTime() << ' ';
    return oss.str();
}

bool Subscriber::notify(const std::string& message)
{
    // If there is no socket, then return false to
    // signify we're disconnected.
    auto webSocket = _ws.lock();
    if (webSocket)
    {
        if (_subscriptions.find(LOOLProtocol::getFirstToken(message)) == _subscriptions.end())
        {
            // No subscribers for the given message.
            return true;
        }

        try
        {
            UnitWSD::get().onAdminNotifyMessage(message);
            webSocket->sendMessage(message);
            return true;
        }
        catch (const std::exception& ex)
        {
            LOG_ERR("Failed to notify Admin subscriber with message [" <<
                    message << "] due to [" << ex.what() << "].");
        }
    }

    return false;
}

bool Subscriber::subscribe(const std::string& command)
{
    auto ret = _subscriptions.insert(command);
    return ret.second;
}

void Subscriber::unsubscribe(const std::string& command)
{
    _subscriptions.erase(command);
}

AdminModel::~AdminModel()
{
    Log::debug("History:\n\n" + *getAllHistory() + "\n");
    Log::info("AdminModel dtor.");
}

std::shared_ptr<const std::string> AdminModel::getAllHistory() const
{
    std::ostringstream oss;
    oss << "{ \"documents\" : [";
    std::string separator1 = "";
    for (auto d : _documents)
    {
        oss << separator1;
        oss << "{ \"docKey\" : \"" << d.first << "\", \"snapshots\" : [ ";
        std::string separator2 = "";
        for (auto ds : d.second.getHistory() )
        {
            oss << separator2;
            ds.second->stringify(oss,1,-1);
            separator2 = ",";
        }
        oss << "]}";
        separator1 = ",";
    }
    oss << "], \"expiredDocuments\" : [";
    separator1 = "";
    for (auto ed : _expiredDocuments)
    {
        oss << separator1;
        oss << "{ \"docKey\" : \"" << ed.first << "\", \"snapshots\" : [ ";
        std::string separator2 = "";
        for (auto eds : ed.second.getHistory() )
        {
            oss << separator2;
            eds.second->stringify(oss,1,-1);
            separator2 = ",";
        }
        oss << "]}";
        separator1 = ",";
    }
    oss << "]}";
    return std::make_shared<const std::string>(oss.str());
}


std::string AdminModel::query(const std::string& command)
{
    const auto token = LOOLProtocol::getFirstToken(command);
    if (token == "documents")
    {
        return getDocuments();
    }
    else if (token == "active_users_count")
    {
        return std::to_string(getTotalActiveViews());
    }
    else if (token == "active_docs_count")
    {
        return std::to_string(_documents.size());
    }
    else if (token == "mem_stats")
    {
        return getMemStats();
    }
    else if (token == "mem_stats_size")
    {
        return std::to_string(_memStatsSize);
    }
    else if (token == "cpu_stats")
    {
        return getCpuStats();
    }
    else if (token == "cpu_stats_size")
    {
        return std::to_string(_cpuStatsSize);
    }

    return std::string("");
}

/// Returns memory consumed by all active loolkit processes
unsigned AdminModel::getKitsMemoryUsage()
{
    unsigned totalMem = 0;
    unsigned docs = 0;
    for (const auto& it : _documents)
    {
        if (!it.second.isExpired())
        {
            const auto bytes = it.second.getMemoryDirty();
            if (bytes > 0)
            {
                totalMem += bytes;
                ++docs;
            }
        }
    }

    if (docs > 0)
    {
        LOG_TRC("Got total Kits memory of " << totalMem << " bytes for " << docs <<
                " docs, avg: " << static_cast<double>(totalMem) / docs << " bytes / doc.");
    }

    return totalMem;
}

void AdminModel::subscribe(int sessionId, const std::weak_ptr<WebSocketHandler>& ws)
{
    const auto ret = _subscribers.emplace(sessionId, Subscriber(sessionId, ws));
    if (!ret.second)
    {
        LOG_WRN("Subscriber already exists");
    }
}

void AdminModel::subscribe(int sessionId, const std::string& command)
{
    auto subscriber = _subscribers.find(sessionId);
    if (subscriber != _subscribers.end())
    {
        subscriber->second.subscribe(command);
    }
}

void AdminModel::unsubscribe(int sessionId, const std::string& command)
{
    auto subscriber = _subscribers.find(sessionId);
    if (subscriber != _subscribers.end())
    {
        subscriber->second.unsubscribe(command);
    }
}

void AdminModel::addMemStats(unsigned memUsage)
{
    _memStats.push_back(memUsage);
    if (_memStats.size() > _memStatsSize)
    {
        _memStats.pop_front();
    }

    notify("mem_stats " + std::to_string(memUsage));
}

void AdminModel::addCpuStats(unsigned cpuUsage)
{
    _cpuStats.push_back(cpuUsage);
    if (_cpuStats.size() > _cpuStatsSize)
    {
        _cpuStats.pop_front();
    }

    notify("cpu_stats " + std::to_string(cpuUsage));
}

void AdminModel::setCpuStatsSize(unsigned size)
{
    int wasteValuesLen = _cpuStats.size() - size;
    while (wasteValuesLen-- > 0)
    {
        _cpuStats.pop_front();
    }
    _cpuStatsSize = size;

    notify("settings cpu_stats_size=" + std::to_string(_cpuStatsSize));
}

void AdminModel::setMemStatsSize(unsigned size)
{
    int wasteValuesLen = _memStats.size() - size;
    while (wasteValuesLen-- > 0)
    {
        _memStats.pop_front();
    }
    _memStatsSize = size;

    notify("settings mem_stats_size=" + std::to_string(_memStatsSize));
}

void AdminModel::notify(const std::string& message)
{
    if (!_subscribers.empty())
    {
        LOG_TRC("Message to admin console: " << message);
        for (auto it = std::begin(_subscribers); it != std::end(_subscribers); )
        {
            if (!it->second.notify(message))
            {
                it = _subscribers.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

void AdminModel::addDocument(const std::string& docKey, Poco::Process::PID pid,
                             const std::string& filename, const std::string& sessionId)
{
    const auto ret = _documents.emplace(docKey, Document(docKey, pid, filename));
    ret.first->second.addView(sessionId);
    ret.first->second.takeSnapshot();
    LOG_DBG("Added admin document [" << docKey << "].");

    std::string encodedFilename;
    Poco::URI::encode(filename, " ", encodedFilename);

    // Notify the subscribers
    std::ostringstream oss;
    oss << "adddoc "
        << pid << ' '
        << encodedFilename << ' '
        << sessionId << ' ';

    // We have to wait until the kit sends us its PSS.
    // Here we guestimate until we get an update.
    if (_documents.size() < 2) // If we aren't the only one.
    {
        if (_memStats.empty())
        {
            oss << 0;
        }
        else
        {
            // Estimate half as much as wsd+forkit.
            oss << _memStats.front() / 2;
        }
    }
    else
    {
        oss << _documents.begin()->second.getMemoryDirty();
    }

    notify(oss.str());
}

void AdminModel::removeDocument(const std::string& docKey, const std::string& sessionId)
{
    auto docIt = _documents.find(docKey);
    if (docIt != _documents.end() && !docIt->second.isExpired())
    {
        // Notify the subscribers
        std::ostringstream oss;
        oss << "rmdoc "
            << docIt->second.getPid() << ' '
            << sessionId;
        notify(oss.str());

        // The idea is to only expire the document and keep the history
        // of documents open and close, to be able to give a detailed summary
        // to the admin console with views.
        docIt->second.takeSnapshot();
        if (docIt->second.expireView(sessionId) == 0)
        {
            _expiredDocuments.emplace(*docIt);
            _documents.erase(docIt);
        }
    }
}

void AdminModel::removeDocument(const std::string& docKey)
{
    auto docIt = _documents.find(docKey);
    if (docIt != _documents.end())
    {
        std::ostringstream oss;
        oss << "rmdoc "
            << docIt->second.getPid() << ' ';
        const std::string msg = oss.str();

        for (const auto& pair : docIt->second.getViews())
        {
            // Notify the subscribers
            notify(msg + pair.first);
            docIt->second.takeSnapshot();
        }

        LOG_DBG("Removed admin document [" << docKey << "].");
        _expiredDocuments.emplace(*docIt);
        _documents.erase(docIt);
    }
}

std::string AdminModel::getMemStats()
{
    std::ostringstream oss;
    for (const auto& i: _memStats)
    {
        oss << i << ',';
    }

    return oss.str();
}

std::string AdminModel::getCpuStats()
{
    std::ostringstream oss;
    for (const auto& i: _cpuStats)
    {
        oss << i << ',';
    }

    return oss.str();
}

unsigned AdminModel::getTotalActiveViews()
{
    unsigned numTotalViews = 0;
    for (const auto& it: _documents)
    {
        if (!it.second.isExpired())
        {
            numTotalViews += it.second.getActiveViews();
        }
    }

    return numTotalViews;
}

std::string AdminModel::getDocuments() const
{
    std::ostringstream oss;
    for (const auto& it: _documents)
    {
        if (!it.second.isExpired())
        {
            oss << it.second.to_string() << "\n ";
        }
    }

    return oss.str();
}

void AdminModel::updateLastActivityTime(const std::string& docKey)
{
    auto docIt = _documents.find(docKey);
    if (docIt != _documents.end())
    {
        if (docIt->second.getIdleTime() >= 10)
        {
            docIt->second.takeSnapshot(); // I would like to keep the idle time
            docIt->second.updateLastActivityTime();
            notify("resetidle " + std::to_string(docIt->second.getPid()));
        }
    }
}

bool Document::updateMemoryDirty(int dirty)
{
    if (_memoryDirty == dirty)
        return false;
    _memoryDirty = dirty;
    return true;
}

void AdminModel::updateMemoryDirty(const std::string& docKey, int dirty)
{
    auto docIt = _documents.find(docKey);
    if (docIt != _documents.end() &&
        docIt->second.updateMemoryDirty(dirty))
    {
        notify("propchange " + std::to_string(docIt->second.getPid()) +
               " mem " + std::to_string(dirty));
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
