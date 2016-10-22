/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "AdminModel.hpp"
#include "config.h"

#include <memory>
#include <set>
#include <sstream>
#include <string>

#include <Poco/Net/WebSocket.h>
#include <Poco/Process.h>
#include <Poco/StringTokenizer.h>
#include <Poco/URI.h>

#include "LOOLProtocol.hpp"
#include "Log.hpp"
#include "Unit.hpp"
#include "Util.hpp"

using Poco::StringTokenizer;

void Document::addView(const std::string& sessionId)
{
    const auto ret = _views.emplace(sessionId, View(sessionId));
    if (!ret.second)
    {
        Log::warn() << "View with SessionID [" + sessionId + "] already exists." << Log::end;
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
    if (_subscriptions.find(LOOLProtocol::getFirstToken(message)) == _subscriptions.end())
    {
        // No subscribers for the given message.
        return true;
    }

    auto webSocket = _ws.lock();
    if (webSocket)
    {
        try
        {
            UnitWSD::get().onAdminNotifyMessage(message);
            webSocket->sendFrame(message.data(), message.length());
            return true;
        }
        catch (const std::exception& ex)
        {
            Log::error() << "Failed to notify Admin subscriber with message ["
                         << message << "] due to [" << ex.what() << "]." << Log::end;
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
unsigned AdminModel::getTotalMemoryUsage()
{
    unsigned totalMem = 0;
    for (auto& it: _documents)
    {
        if (!it.second.isExpired())
        {
            const int mem = Util::getMemoryUsage(it.second.getPid());
            if (mem > 0)
            {
                totalMem += mem;
            }
        }
    }

    return totalMem;
}

void AdminModel::subscribe(int nSessionId, std::shared_ptr<Poco::Net::WebSocket>& ws)
{
    const auto ret = _subscribers.emplace(nSessionId, Subscriber(nSessionId, ws));
    if (!ret.second)
    {
        Log::warn() << "Subscriber already exists" << Log::end;
    }
}

void AdminModel::subscribe(int nSessionId, const std::string& command)
{
    auto subscriber = _subscribers.find(nSessionId);
    if (subscriber != _subscribers.end())
    {
        subscriber->second.subscribe(command);
    }
}

void AdminModel::unsubscribe(int nSessionId, const std::string& command)
{
    auto subscriber = _subscribers.find(nSessionId);
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

    std::ostringstream oss;
    oss << "mem_stats "
        << std::to_string(memUsage);
    notify(oss.str());
}

void AdminModel::addCpuStats(unsigned cpuUsage)
{
    _cpuStats.push_back(cpuUsage);
    if (_cpuStats.size() > _cpuStatsSize)
    {
        _cpuStats.pop_front();
    }

    std::ostringstream oss;
    oss << "cpu_stats "
        << std::to_string(cpuUsage);
    notify(oss.str());
}

void AdminModel::setCpuStatsSize(unsigned size)
{
    int wasteValuesLen = _cpuStats.size() - size;
    while (wasteValuesLen-- > 0)
    {
        _cpuStats.pop_front();
    }
    _cpuStatsSize = size;

    std::ostringstream oss;
    oss << "settings "
        << "cpu_stats_size="
        << std::to_string(_cpuStatsSize);
    notify(oss.str());
}

void AdminModel::setMemStatsSize(unsigned size)
{
    int wasteValuesLen = _memStats.size() - size;
    while (wasteValuesLen-- > 0)
    {
        _memStats.pop_front();
    }
    _memStatsSize = size;

    std::ostringstream oss;
    oss << "settings "
        << "mem_stats_size="
        << std::to_string(_memStatsSize);
    notify(oss.str());
}

void AdminModel::notify(const std::string& message)
{
    Log::debug("Message to admin console: " + message);
    auto it = std::begin(_subscribers);
    while (it != std::end(_subscribers))
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

void AdminModel::addDocument(const std::string& docKey, Poco::Process::PID pid,
                             const std::string& filename, const std::string& sessionId)
{
    const auto ret = _documents.emplace(docKey, Document(docKey, pid, filename));
    ret.first->second.addView(sessionId);

    // Notify the subscribers
    const unsigned memUsage = Util::getMemoryUsage(pid);
    std::ostringstream oss;
    std::string encodedFilename;
    Poco::URI::encode(filename, " ", encodedFilename);
    oss << "adddoc "
        << pid << " "
        << encodedFilename << " "
        << sessionId << " "
        << std::to_string(memUsage);
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
            << docIt->second.getPid() << " "
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
    auto docIt = _documents.find(docKey);
    if (docIt != _documents.end())
    {
        for (const auto& pair : docIt->second.getViews())
        {
            // Notify the subscribers
            std::ostringstream oss;
            oss << "rmdoc "
                << docIt->second.getPid() << " "
                << pair.first;
            notify(oss.str());
        }

        _documents.erase(docIt);
    }
}

std::string AdminModel::getMemStats()
{
    std::ostringstream oss;
    for (auto& i: _memStats)
    {
        oss << i << ',';
    }

    return oss.str();
}

std::string AdminModel::getCpuStats()
{
    std::ostringstream oss;
    for (auto& i: _cpuStats)
    {
        oss << i << ',';
    }

    return oss.str();
}

unsigned AdminModel::getTotalActiveViews()
{
    unsigned nTotalViews = 0;
    for (auto& it: _documents)
    {
        if (!it.second.isExpired())
        {
            nTotalViews += it.second.getActiveViews();
        }
    }

    return nTotalViews;
}

std::string AdminModel::getDocuments()
{
    std::ostringstream oss;
    for (auto& it: _documents)
    {
        if (it.second.isExpired())
            continue;

        const auto sPid = std::to_string(it.second.getPid());
        const auto sFilename = it.second.getFilename();
        const auto sViews = std::to_string(it.second.getActiveViews());
        const auto sMem = std::to_string(Util::getMemoryUsage(it.second.getPid()));
        const auto sElapsed = std::to_string(it.second.getElapsedTime());

        std::string encodedFilename;
        Poco::URI::encode(sFilename, " ", encodedFilename);
        oss << sPid << ' '
            << encodedFilename << ' '
            << sViews << ' '
            << sMem << ' '
            << sElapsed << " \n ";
    }

    return oss.str();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
