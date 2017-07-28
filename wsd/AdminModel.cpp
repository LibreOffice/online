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

void Document::addView(const std::string& sessionId, const std::string& userName, const std::string& userId)
{
    const auto ret = _views.emplace(sessionId, View(sessionId, userName, userId));
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

void AdminModel::assertCorrectThread() const
{
    // FIXME: share this code [!]
    const bool sameThread = std::this_thread::get_id() == _owner;
    if (!sameThread)
        LOG_ERR("Admin command invoked from foreign thread. Expected: 0x" << std::hex <<
        _owner << " but called from 0x" << std::this_thread::get_id() << " (" <<
        std::dec << Util::getThreadId() << ").");

    assert(sameThread);
}

std::string AdminModel::query(const std::string& command)
{
    assertCorrectThread();

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
    assertCorrectThread();

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
    assertCorrectThread();

    const auto ret = _subscribers.emplace(sessionId, Subscriber(sessionId, ws));
    if (!ret.second)
    {
        LOG_WRN("Subscriber already exists");
    }
}

void AdminModel::subscribe(int sessionId, const std::string& command)
{
    assertCorrectThread();

    auto subscriber = _subscribers.find(sessionId);
    if (subscriber != _subscribers.end())
    {
        subscriber->second.subscribe(command);
    }
}

void AdminModel::unsubscribe(int sessionId, const std::string& command)
{
    assertCorrectThread();

    auto subscriber = _subscribers.find(sessionId);
    if (subscriber != _subscribers.end())
        subscriber->second.unsubscribe(command);
}

void AdminModel::addMemStats(unsigned memUsage)
{
    assertCorrectThread();

    _memStats.push_back(memUsage);
    if (_memStats.size() > _memStatsSize)
        _memStats.pop_front();

    notify("mem_stats " + std::to_string(memUsage));
}

void AdminModel::addCpuStats(unsigned cpuUsage)
{
    assertCorrectThread();

    _cpuStats.push_back(cpuUsage);
    if (_cpuStats.size() > _cpuStatsSize)
        _cpuStats.pop_front();

    notify("cpu_stats " + std::to_string(cpuUsage));
}

void AdminModel::setCpuStatsSize(unsigned size)
{
    assertCorrectThread();

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
    assertCorrectThread();

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
    assertCorrectThread();

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

void AdminModel::modificationAlert(const std::string& docKey, Poco::Process::PID pid, bool value)
{
    assertCorrectThread();

    auto doc = _documents.find(docKey);
    if(doc != _documents.end())
    {
        doc->second.setModified(value);
    }

    std::ostringstream oss;
    oss << "modifications "
        << pid << ' '
        << (value?"Yes":"No");

    notify(oss.str());
}

void AdminModel::addDocument(const std::string& docKey, Poco::Process::PID pid,
                             const std::string& filename, const std::string& sessionId,
                             const std::string& userName, const std::string& userId)
{
    assertCorrectThread();

    const auto ret = _documents.emplace(docKey, Document(docKey, pid, filename));
    ret.first->second.addView(sessionId, userName, userId);
    LOG_DBG("Added admin document [" << docKey << "].");

    std::string encodedUsername;
    std::string encodedFilename;
    std::string encodedUserId;
    Poco::URI::encode(userId, " ", encodedUserId);
    Poco::URI::encode(filename, " ", encodedFilename);
    Poco::URI::encode(userName, " ", encodedUsername);

    // Notify the subscribers
    std::ostringstream oss;
    oss << "adddoc "
        << pid << ' '
        << encodedFilename << ' '
        << sessionId << ' '
        << encodedUsername << ' '
        << encodedUserId << ' ';

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
    assertCorrectThread();

    auto docIt = _documents.find(docKey);
    if (docIt != _documents.end() && !docIt->second.isExpired())
    {
        // Notify the subscribers
        std::ostringstream oss;
        oss << "rmdoc "
            << docIt->second.getPid() << ' '
            << sessionId;
        notify(oss.str());

        // TODO: The idea is to only expire the document and keep the history
        // of documents open and close, to be able to give a detailed summary
        // to the admin console with views. For now, just remove the document.
        if (docIt->second.expireView(sessionId) == 0)
        {
            _documents.erase(docIt);
        }
    }
}

void AdminModel::removeDocument(const std::string& docKey)
{
    assertCorrectThread();

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
        }

        LOG_DBG("Removed admin document [" << docKey << "].");
        _documents.erase(docIt);
    }
}

std::string AdminModel::getMemStats()
{
    assertCorrectThread();

    std::ostringstream oss;
    for (const auto& i: _memStats)
    {
        oss << i << ',';
    }

    return oss.str();
}

std::string AdminModel::getCpuStats()
{
    assertCorrectThread();

    std::ostringstream oss;
    for (const auto& i: _cpuStats)
    {
        oss << i << ',';
    }

    return oss.str();
}

unsigned AdminModel::getTotalActiveViews()
{
    assertCorrectThread();

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
    assertCorrectThread();

    std::ostringstream oss;
    std::map<std::string, View> viewers;
    oss << '{' << "\"documents\"" << ':' << '[';
    std::string separator1 = "";
    for (const auto& it: _documents)
    {
        if (!it.second.isExpired())
        {
            std::string encodedFilename;
            Poco::URI::encode(it.second.getFilename(), " ", encodedFilename);
            oss << separator1 << '{' << ' '
                << "\"pid\"" << ':' << it.second.getPid() << ','
                << "\"fileName\"" << ':' << '"' << encodedFilename << '"' << ','
                << "\"activeViews\"" << ':' << it.second.getActiveViews() << ','
                << "\"memory\"" << ':' << it.second.getMemoryDirty() << ','
                << "\"elapsedTime\"" << ':' << it.second.getElapsedTime() << ','
                << "\"idleTime\"" << ':' << it.second.getIdleTime() << ','
                << "\"modified\"" << ':' << '"' << (it.second.getModifiedStatus() ? "Yes" : "No") << '"' << ','
                << "\"views\"" << ':' << '[';
            viewers = it.second.getViews();
            std::string separator = "";
            for(const auto& viewIt: viewers)
            {
                if(!viewIt.second.isExpired()) {
                    oss << separator << '{'
                        << "\"userName\"" << ':' << '"' << viewIt.second.getUserName() << '"' << ','
                        << "\"userId\"" << ':' << '"' << viewIt.second.getUserId() << '"' << ','
                        << "\"sessionid\"" << ':' << '"' << viewIt.second.getSessionId() << '"' << '}';
                        separator = ',';
                }
            }
            oss << "]"
                << "}";
            separator1 = ',';
        }
    }
    oss << "]" << "}";

    return oss.str();
}

void AdminModel::updateLastActivityTime(const std::string& docKey)
{
    assertCorrectThread();

    auto docIt = _documents.find(docKey);
    if (docIt != _documents.end())
    {
        if (docIt->second.getIdleTime() >= 10)
        {
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
    assertCorrectThread();

    auto docIt = _documents.find(docKey);
    if (docIt != _documents.end() &&
        docIt->second.updateMemoryDirty(dirty))
    {
        notify("propchange " + std::to_string(docIt->second.getPid()) +
               " mem " + std::to_string(dirty));
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
