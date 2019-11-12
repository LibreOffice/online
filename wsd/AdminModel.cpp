/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config.h>

#include "AdminModel.hpp"

#include <chrono>
#include <memory>
#include <set>
#include <sstream>
#include <string>

#include <Poco/Process.h>
#include <Poco/StringTokenizer.h>
#include <Poco/URI.h>

#include <Protocol.hpp>
#include <net/WebSocketHandler.hpp>
#include <Log.hpp>
#include <Unit.hpp>
#include <Util.hpp>
#include <wsd/LOOLWSD.hpp>

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
    takeSnapshot();

    return _activeViews;
}

void Document::setViewLoadDuration(const std::string& sessionId, std::chrono::milliseconds viewLoadDuration)
{
    std::map<std::string, View>::iterator it = _views.find(sessionId);
    if (it != _views.end())
    {
        it->second.setLoadDuration(viewLoadDuration);
    }
}

std::pair<std::time_t, std::string> Document::getSnapshot() const
{
    std::time_t ct = std::time(nullptr);
    std::ostringstream oss;
    oss << "{";
    oss << "\"creationTime\"" << ":" << ct << ",";
    oss << "\"memoryDirty\"" << ":" << getMemoryDirty() << ",";
    oss << "\"activeViews\"" << ":" << getActiveViews() << ",";

    oss << "\"views\"" << ":[";
    std::string separator;
    for (const auto& view : getViews())
    {
        oss << separator << "\"";
        if(view.second.isExpired())
        {
            oss << "-";
        }
        oss << view.first << "\"";
        separator = ",";
    }
    oss << "],";

    oss << "\"lastActivity\"" << ":" << _lastActivity;
    oss << "}";
    return std::make_pair(ct, oss.str());
}

const std::string Document::getHistory() const
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"docKey\"" << ":\"" << _docKey << "\",";
    oss << "\"filename\"" << ":\"" << LOOLWSD::anonymizeUrl(getFilename()) << "\",";
    oss << "\"start\"" << ":" << _start << ",";
    oss << "\"end\"" << ":" << _end << ",";
    oss << "\"pid\"" << ":" << getPid() << ",";
    oss << "\"snapshots\"" << ":[";
    std::string separator;
    for (const auto& s : _snapshots)
    {
        oss << separator << s.second;
        separator = ",";
    }
    oss << "]}";
    return oss.str();
}

void Document::takeSnapshot()
{
    std::pair<std::time_t, std::string> p = getSnapshot();
    auto insPoint = _snapshots.upper_bound(p.first);
    _snapshots.insert(insPoint, p);
}

std::string Document::to_string() const
{
    std::ostringstream oss;
    std::string encodedFilename;
    Poco::URI::encode(getFilename(), " ", encodedFilename);
    oss << getPid() << ' '
        << encodedFilename << ' '
        << getActiveViews() << ' '
        << getMemoryDirty() << ' '
        << getElapsedTime() << ' '
        << getIdleTime() << ' ';
    return oss.str();
}

bool Subscriber::notify(const std::string& message)
{
    // If there is no socket, then return false to
    // signify we're disconnected.
    std::shared_ptr<WebSocketHandler> webSocket = _ws.lock();
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
        LOG_ERR("Admin command invoked from foreign thread. Expected: " <<
        Log::to_string(_owner) << " but called from " <<
        std::this_thread::get_id() << " (" << Util::getThreadId() << ").");

    assert(sameThread);
}

AdminModel::~AdminModel()
{
    LOG_TRC("History:\n\n" << getAllHistory() << '\n');
    LOG_INF("AdminModel dtor.");
}

std::string AdminModel::getAllHistory() const
{
    std::ostringstream oss;
    oss << "{ \"documents\" : [";
    std::string separator1;
    for (const auto& d : _documents)
    {
        oss << separator1;
        oss << d.second.getHistory();
        separator1 = ",";
    }
    oss << "], \"expiredDocuments\" : [";
    separator1 = "";
    for (const auto& ed : _expiredDocuments)
    {
        oss << separator1;
        oss << ed.second.getHistory();
        separator1 = ",";
    }
    oss << "]}";
    return oss.str();
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
    else if (token == "sent_activity")
    {
        return getSentActivity();
    }
    else if (token == "recv_activity")
    {
        return getRecvActivity();
    }
    else if (token == "net_stats_size")
    {
        return std::to_string(std::max(_sentStatsSize, _recvStatsSize));
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
            const int bytes = it.second.getMemoryDirty();
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

size_t AdminModel::getKitsJiffies()
{
    assertCorrectThread();

    size_t totalJ = 0;
    for (auto& it : _documents)
    {
        if (!it.second.isExpired())
        {
            const int pid = it.second.getPid();
            if (pid > 0)
            {
                unsigned newJ = Util::getCpuUsage(pid);
                unsigned prevJ = it.second.getLastJiffies();
                if(newJ >= prevJ)
                {
                    totalJ += (newJ - prevJ);
                    it.second.setLastJiffies(newJ);
                }
            }
        }
    }
    return totalJ;
}

void AdminModel::subscribe(int sessionId, const std::weak_ptr<WebSocketHandler>& ws)
{
    assertCorrectThread();

    const auto ret = _subscribers.emplace(sessionId, Subscriber(ws));
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

void AdminModel::addSentStats(uint64_t sent)
{
    assertCorrectThread();

    _sentStats.push_back(sent);
    if (_sentStats.size() > _sentStatsSize)
        _sentStats.pop_front();

    notify("sent_activity " + std::to_string(sent));
}

void AdminModel::addRecvStats(uint64_t recv)
{
    assertCorrectThread();

    _recvStats.push_back(recv);
    if (_recvStats.size() > _recvStatsSize)
        _recvStats.pop_front();

    notify("recv_activity " + std::to_string(recv));
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

void AdminModel::addBytes(const std::string& docKey, uint64_t sent, uint64_t recv)
{
    assertCorrectThread();

    auto doc = _documents.find(docKey);
    if(doc != _documents.end())
        doc->second.addBytes(sent, recv);

    _sentBytesTotal += sent;
    _recvBytesTotal += recv;
}

void AdminModel::modificationAlert(const std::string& docKey, Poco::Process::PID pid, bool value)
{
    assertCorrectThread();

    auto doc = _documents.find(docKey);
    if (doc != _documents.end())
        doc->second.setModified(value);

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
    ret.first->second.takeSnapshot();
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

        // The idea is to only expire the document and keep the history
        // of documents open and close, to be able to give a detailed summary
        // to the admin console with views.
        if (docIt->second.expireView(sessionId) == 0)
        {
            _expiredDocuments.emplace(docIt->first + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()), docIt->second);
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
            docIt->second.expireView(pair.first);
        }

        LOG_DBG("Removed admin document [" << docKey << "].");
        _expiredDocuments.emplace(docIt->first + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()), docIt->second);
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

std::string AdminModel::getSentActivity()
{
    assertCorrectThread();

    std::ostringstream oss;
    for (const auto& i: _sentStats)
    {
        oss << i << ',';
    }

    return oss.str();
}

std::string AdminModel::getRecvActivity()
{
    assertCorrectThread();

    std::ostringstream oss;
    for (const auto& i: _recvStats)
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

std::vector<DocBasicInfo> AdminModel::getDocumentsSortedByIdle() const
{
    std::vector<DocBasicInfo> docs;
    docs.reserve(_documents.size());
    for (const auto& it: _documents)
    {
        docs.emplace_back(it.second.getDocKey(),
                          it.second.getIdleTime(),
                          it.second.getMemoryDirty(),
                          !it.second.getModifiedStatus());
    }

    // Sort the list by idle times;
    std::sort(std::begin(docs), std::end(docs),
              [](const DocBasicInfo& a, const DocBasicInfo& b)
              {
                return a.getIdleTime() >= b.getIdleTime();
              });

    return docs;
}

std::string AdminModel::getDocuments() const
{
    assertCorrectThread();

    std::ostringstream oss;
    std::map<std::string, View> viewers;
    oss << '{' << "\"documents\"" << ':' << '[';
    std::string separator1;
    for (const auto& it: _documents)
    {
        if (!it.second.isExpired())
        {
            std::string encodedFilename;
            Poco::URI::encode(it.second.getFilename(), " ", encodedFilename);
            oss << separator1 << '{' << ' '
                << "\"pid\"" << ':' << it.second.getPid() << ','
                << "\"docKey\"" << ':' << '"' << it.second.getDocKey() << '"' << ','
                << "\"fileName\"" << ':' << '"' << encodedFilename << '"' << ','
                << "\"activeViews\"" << ':' << it.second.getActiveViews() << ','
                << "\"memory\"" << ':' << it.second.getMemoryDirty() << ','
                << "\"elapsedTime\"" << ':' << it.second.getElapsedTime() << ','
                << "\"idleTime\"" << ':' << it.second.getIdleTime() << ','
                << "\"modified\"" << ':' << '"' << (it.second.getModifiedStatus() ? "Yes" : "No") << '"' << ','
                << "\"views\"" << ':' << '[';
            viewers = it.second.getViews();
            std::string separator;
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
    assertCorrectThread();

    auto docIt = _documents.find(docKey);
    if (docIt != _documents.end() &&
        docIt->second.updateMemoryDirty(dirty))
    {
        notify("propchange " + std::to_string(docIt->second.getPid()) +
               " mem " + std::to_string(dirty));
    }
}

double AdminModel::getServerUptime()
{
    auto currentTime = std::chrono::system_clock::now();
    std::chrono::duration<double> uptime = currentTime - LOOLWSD::StartTime;
    return uptime.count();
}

void AdminModel::setViewLoadDuration(const std::string& docKey, const std::string& sessionId, std::chrono::milliseconds viewLoadDuration)
{
    std::map<std::string, Document>::iterator it = _documents.find(docKey);
    if (it != _documents.end())
    {
        it->second.setViewLoadDuration(sessionId, viewLoadDuration);
    }
}

void AdminModel::setDocWopiDownloadDuration(const std::string& docKey, std::chrono::milliseconds wopiDownloadDuration)
{
    std::map<std::string, Document>::iterator it = _documents.find(docKey);
    if (it != _documents.end())
    {
        it->second.setWopiDownloadDuration(wopiDownloadDuration);
    }
}

void AdminModel::setDocWopiUploadDuration(const std::string& docKey, const std::chrono::milliseconds wopiUploadDuration)
{
    std::map<std::string, Document>::iterator it = _documents.find(docKey);
    if (it != _documents.end())
    {
        it->second.setWopiUploadDuration(wopiUploadDuration);
    }
}

void AdminModel::addSegFaultCount(unsigned segFaultCount)
{
    _segFaultCount += segFaultCount;
}

void AdminModel::updateDocumentStats(const Document &d, DocumentStats &stats)
{
    uint64_t usedMemory;
    uint64_t bytesSentToClients, bytesReceivedFromClients;
    uint64_t wopiDownloadDuration, wopiUploadDuration;
    uint64_t viewLoadDuration;

    //Used memory
    usedMemory = d.getMemoryDirty();
    stats.totalUsedMemory += usedMemory;
    if (stats.maxUsedMemory < usedMemory)
    {
        stats.maxUsedMemory = usedMemory;
    }
    if (stats.minUsedMemory > usedMemory)
    {
        stats.minUsedMemory = usedMemory;
    }

    //Sent bytes to clients
    bytesSentToClients = d.getSentBytes();
    stats.totalBytesSentToClients += bytesSentToClients;
    if (stats.maxBytesSentToClients < bytesSentToClients)
    {
        stats.maxBytesSentToClients = bytesSentToClients;
    }
    if (stats.minBytesSentToClients > bytesSentToClients)
    {
        stats.minBytesSentToClients = bytesSentToClients;
    }

    //Received bytes from clients
    bytesReceivedFromClients = d.getRecvBytes();
    stats.totalBytesReceivedFromClients += bytesReceivedFromClients;
    if (stats.maxBytesReceivedFromClients < bytesReceivedFromClients)
    {
        stats.maxBytesReceivedFromClients = bytesReceivedFromClients;
    }
    if (stats.minBytesReceivedFromClients > bytesReceivedFromClients)
    {
        stats.minBytesReceivedFromClients = bytesReceivedFromClients;
    }

    //Wopi download duration
    wopiDownloadDuration = d.getWopiDownloadDuration().count();
    stats.totalWopiDownloadDuration += wopiDownloadDuration;
    if (stats.maxWopiDownloadDuration < wopiDownloadDuration)
    {
        stats.maxWopiDownloadDuration = wopiDownloadDuration;
    }
    if (stats.minWopiDownloadDuration > wopiDownloadDuration)
    {
        stats.minWopiDownloadDuration = wopiDownloadDuration;
    }

    //Wopi upload duration
    wopiUploadDuration = d.getWopiUploadDuration().count();
    if (wopiUploadDuration > 0)
    {
        stats.totalUploadedDocs ++;
        stats.totalWopiUploadDuration += wopiUploadDuration;
        if (stats.maxWopiUploadDuration < wopiUploadDuration)
        {
            stats.maxWopiUploadDuration = wopiUploadDuration;
        }
        if (stats.minWopiUploadDuration > wopiUploadDuration)
        {
            stats.minWopiUploadDuration = wopiUploadDuration;
        }
    }

    //View first load duration
    for (const auto& v : d.getViews())
    {
        viewLoadDuration = v.second.getLoadDuration().count();
        stats.totalViewLoadDuration += viewLoadDuration;
        if (stats.maxViewLoadDuration < viewLoadDuration)
        {
            stats.maxViewLoadDuration = viewLoadDuration;
        }
        if (stats.minViewLoadDuration > viewLoadDuration)
        {
            stats.minViewLoadDuration = viewLoadDuration;
        }
    }
}

#define MIN_ABS_TO_0(val) ((val) == 0xFFFFFFFFFFFFFFFF ? 0 : (val))

void AdminModel::getMetrics(AdminMetrics &metrics)
{
    int activeDocsCount = _documents.size();
    int expiredDocsCount = _expiredDocuments.size();
    int totalDocsCount = activeDocsCount + expiredDocsCount;
    int totalActiveDocAllViews = 0, totalActiveDocActiveViews = 0, maxActiveDocAllViews = 0, maxActiveDocActiveViews = 0;
    int totalActiveDocExpiredViews = 0, maxActiveDocExpiredViews = 0;
    int totalExpiredDocAllViews = 0, maxExpiredDocAllViews = 0;
    int totalAllViews = 0, maxAllViews = 0, views = 0;
    int totalThreadCount = 0, maxThreadCount = 0, threadCount = 0;
    uint64_t totalCpuTime = 0, minCpuTime = 0xFFFFFFFFFFFFFFFF, maxCpuTime = 0, cpuTime = 0xFFFFFFFFFFFFFFFF;
    int totalActiveProcCount = 0;
    DocumentStats activeStats, expiredStats, totalStats;
    uint64_t openTime;

    for (auto& d : _documents)
    {
        //Views count
        views = d.second.getViews().size();
        totalActiveDocAllViews += views;
        if (maxActiveDocAllViews < views)
        {
            maxActiveDocAllViews = views;
        }
        views = d.second.getActiveViews();
        totalActiveDocActiveViews += views;
        if (maxActiveDocActiveViews < views)
        {
            maxActiveDocActiveViews = views;
        }
        views = d.second.getViews().size() - views;
        totalActiveDocExpiredViews += views;
        if (maxActiveDocExpiredViews < views)
        {
            maxActiveDocExpiredViews = views;
        }

        updateDocumentStats(d.second, activeStats);

        //Open time
        openTime = (uint64_t)d.second.getElapsedTime();
        activeStats.totalOpenTime += openTime;
        if (activeStats.maxOpenTime < openTime)
        {
            activeStats.maxOpenTime = openTime;
        }
        if (activeStats.minOpenTime > openTime)
        {
            activeStats.minOpenTime = openTime;
        }
    }
    activeStats.minUsedMemory = MIN_ABS_TO_0(activeStats.minUsedMemory) * 1024;
    activeStats.maxUsedMemory *= 1024;
    activeStats.totalUsedMemory *= 1024;

    for (auto& d : _expiredDocuments)
    {
        //Views
        views = d.second.getViews().size();
        totalExpiredDocAllViews += views;
        if (maxExpiredDocAllViews < views)
        {
            maxExpiredDocAllViews = views;
        }

        updateDocumentStats(d.second, expiredStats);

        //Open time
        openTime = d.second.getOpenTime();
        expiredStats.totalOpenTime += openTime;
        if (expiredStats.maxOpenTime < openTime)
        {
            expiredStats.maxOpenTime = openTime;
        }
        if (expiredStats.minOpenTime > openTime)
        {
            expiredStats.minOpenTime = openTime;
        }
    }
    expiredStats.minUsedMemory = MIN_ABS_TO_0(expiredStats.minUsedMemory) * 1024;
    expiredStats.maxUsedMemory *= 1024;
    expiredStats.totalUsedMemory *= 1024;
    totalAllViews = totalActiveDocAllViews + totalExpiredDocAllViews;
    maxAllViews = (maxActiveDocAllViews < maxExpiredDocAllViews ? maxExpiredDocAllViews : maxActiveDocAllViews);

    //Number of documents
    metrics.global_all_document_count = totalDocsCount;
    metrics.global_active_document_count = activeDocsCount;
    metrics.global_expired_document_count = expiredDocsCount;

    //Total active views count
    metrics.global_all_views_count = totalAllViews;
    metrics.global_active_views_count = totalActiveDocActiveViews;
    metrics.global_expired_views_count = totalActiveDocExpiredViews + totalExpiredDocAllViews;

    //Total bytes sent/received to/from the clients
    metrics.global_bytes_sent_to_clients_bytes = getSentBytesTotal();
    metrics.global_bytes_received_from_clients_bytes = getRecvBytesTotal();

    //loolkit processes
    std::vector<int> childProcs;

    //processes in different states
    int unassignedProcCount = Util::getPidsFromProcName(std::regex("kit_spare_[0-9]*"), &childProcs);
    int assignedProcCount = Util::getPidsFromProcName(std::regex("kitbroker_[0-9]*"), &childProcs);

    //processes count
    metrics.kit_count = totalActiveProcCount = unassignedProcCount + assignedProcCount;

    for (int& pid : childProcs)
    {
        threadCount = Util::getStatFromPid(pid, 19);
        totalThreadCount += threadCount;
        if (maxThreadCount < threadCount)
        {
            maxThreadCount = threadCount;
        }

        cpuTime = Util::getCpuUsage(pid);
        totalCpuTime += cpuTime;
        if (maxCpuTime < cpuTime)
        {
            maxCpuTime = cpuTime;
        }
        if (minCpuTime > cpuTime)
        {
            minCpuTime = cpuTime;
        }
    }

    totalCpuTime /= sysconf (_SC_CLK_TCK);
    maxCpuTime /= sysconf (_SC_CLK_TCK);
    minCpuTime = MIN_ABS_TO_0(minCpuTime) / sysconf (_SC_CLK_TCK);

    totalStats.totalUsedMemory = activeStats.totalUsedMemory + expiredStats.totalUsedMemory;
    totalStats.minUsedMemory = (activeStats.minUsedMemory < expiredStats.minUsedMemory ? activeStats.minUsedMemory : expiredStats.minUsedMemory);
    totalStats.maxUsedMemory = (activeStats.maxUsedMemory > expiredStats.maxUsedMemory ? activeStats.maxUsedMemory : expiredStats.maxUsedMemory);
    totalStats.totalOpenTime = activeStats.totalOpenTime + expiredStats.totalOpenTime;
    totalStats.minOpenTime = (activeStats.minOpenTime < expiredStats.minOpenTime ? activeStats.minOpenTime : expiredStats.minOpenTime);
    totalStats.maxOpenTime = (activeStats.maxOpenTime > expiredStats.maxOpenTime ? activeStats.maxOpenTime : expiredStats.maxOpenTime);
    totalStats.totalBytesSentToClients = activeStats.totalBytesSentToClients + expiredStats.totalBytesSentToClients;
    totalStats.minBytesSentToClients = (activeStats.minBytesSentToClients < expiredStats.minBytesSentToClients ? activeStats.minBytesSentToClients : expiredStats.minBytesSentToClients);
    totalStats.maxBytesSentToClients = (activeStats.maxBytesSentToClients > expiredStats.maxBytesSentToClients ? activeStats.maxBytesSentToClients : expiredStats.maxBytesSentToClients);
    totalStats.totalBytesReceivedFromClients = activeStats.totalBytesReceivedFromClients + expiredStats.totalBytesReceivedFromClients;
    totalStats.minBytesReceivedFromClients = (activeStats.minBytesReceivedFromClients < expiredStats.minBytesReceivedFromClients ? activeStats.minBytesReceivedFromClients : expiredStats.minBytesReceivedFromClients);
    totalStats.maxBytesReceivedFromClients = (activeStats.maxBytesReceivedFromClients > expiredStats.maxBytesReceivedFromClients ? activeStats.maxBytesReceivedFromClients : expiredStats.maxBytesReceivedFromClients);
    totalStats.totalWopiDownloadDuration = activeStats.totalWopiDownloadDuration + expiredStats.totalWopiDownloadDuration;
    totalStats.minWopiDownloadDuration = (activeStats.minWopiDownloadDuration < expiredStats.minWopiDownloadDuration ? activeStats.minWopiDownloadDuration : expiredStats.minWopiDownloadDuration);
    totalStats.maxWopiDownloadDuration = (activeStats.maxWopiDownloadDuration > expiredStats.maxWopiDownloadDuration ? activeStats.maxWopiDownloadDuration : expiredStats.maxWopiDownloadDuration);
    totalStats.totalWopiUploadDuration = activeStats.totalWopiUploadDuration + expiredStats.totalWopiUploadDuration;
    totalStats.minWopiUploadDuration = (activeStats.minWopiUploadDuration < expiredStats.minWopiUploadDuration ? activeStats.minWopiUploadDuration : expiredStats.minWopiUploadDuration);
    totalStats.maxWopiUploadDuration = (activeStats.maxWopiUploadDuration > expiredStats.maxWopiUploadDuration ? activeStats.maxWopiUploadDuration : expiredStats.maxWopiUploadDuration);
    totalStats.totalViewLoadDuration = activeStats.totalViewLoadDuration + expiredStats.totalViewLoadDuration;
    totalStats.minViewLoadDuration = (activeStats.minViewLoadDuration < expiredStats.minViewLoadDuration ? activeStats.minViewLoadDuration : expiredStats.minViewLoadDuration);
    totalStats.maxViewLoadDuration = (activeStats.maxViewLoadDuration > expiredStats.maxViewLoadDuration ? activeStats.maxViewLoadDuration : expiredStats.maxViewLoadDuration);
    totalStats.totalUploadedDocs = activeStats.totalUploadedDocs + expiredStats.totalUploadedDocs;

    metrics.kit_unassigned_count = unassignedProcCount;
    metrics.kit_assigned_count = assignedProcCount;
    metrics.kit_segfaulted_count = _segFaultCount;
    metrics.kit_thread_count_average = (totalActiveProcCount ? totalThreadCount / totalActiveProcCount : 0);
    metrics.kit_thread_count_max = maxThreadCount;
    metrics.kit_memory_used_total_bytes = activeStats.totalUsedMemory;
    metrics.kit_memory_used_average_bytes = (activeDocsCount ? activeStats.totalUsedMemory / activeDocsCount : 0);
    metrics.kit_memory_used_min_bytes = activeStats.minUsedMemory;
    metrics.kit_memory_used_max_bytes = activeStats.maxUsedMemory;
    metrics.kit_cpu_time_total_seconds = totalCpuTime;
    metrics.kit_cpu_time_average_seconds = (totalActiveProcCount ? totalCpuTime / totalActiveProcCount : 0);
    metrics.kit_cpu_time_min_seconds = minCpuTime;
    metrics.kit_cpu_time_max_seconds = maxCpuTime;

    metrics.document_all_views_all_count_average = (totalDocsCount ? totalAllViews / totalDocsCount : 0);
    metrics.document_all_views_all_count_max = maxAllViews;
    metrics.document_active_views_all_count_average = (activeDocsCount ? totalActiveDocAllViews / activeDocsCount : 0);
    metrics.document_active_views_all_count_max = maxActiveDocAllViews;
    metrics.document_active_views_active_count = totalActiveDocActiveViews;
    metrics.document_active_views_active_count_average = (activeDocsCount ? totalActiveDocActiveViews / activeDocsCount : 0);
    metrics.document_active_views_active_count_max = maxActiveDocActiveViews;
    metrics.document_active_views_expired_count = totalActiveDocExpiredViews;
    metrics.document_active_views_expired_count_average = (activeDocsCount ? totalActiveDocExpiredViews / activeDocsCount : 0);
    metrics.document_active_views_expired_count_max = maxActiveDocExpiredViews;
    metrics.document_expired_views_all_count_average = (expiredDocsCount ? totalExpiredDocAllViews / expiredDocsCount : 0);
    metrics.document_expired_views_all_count_max = maxExpiredDocAllViews;

    metrics.document_all_opened_time_average_seconds = (totalDocsCount ? totalStats.totalOpenTime / totalDocsCount : 0);
    metrics.document_all_opened_time_min_seconds = MIN_ABS_TO_0(totalStats.minOpenTime);
    metrics.document_all_opened_time_max_seconds = totalStats.maxOpenTime;
    metrics.document_active_opened_time_average_seconds = (activeDocsCount ? activeStats.totalOpenTime / activeDocsCount : 0);
    metrics.document_active_opened_time_min_seconds = MIN_ABS_TO_0(activeStats.minOpenTime);
    metrics.document_active_opened_time_max_seconds = activeStats.maxOpenTime;
    metrics.document_expired_opened_time_average_seconds = (expiredDocsCount ? expiredStats.totalOpenTime / expiredDocsCount : 0);
    metrics.document_expired_opened_time_min_seconds = MIN_ABS_TO_0(expiredStats.minOpenTime);
    metrics.document_expired_opened_time_max_seconds = expiredStats.maxOpenTime;

    metrics.document_all_sent_to_clients_average_bytes = (totalDocsCount ? totalStats.totalBytesSentToClients / totalDocsCount : 0);
    metrics.document_all_sent_to_clients_min_bytes = MIN_ABS_TO_0(totalStats.minBytesSentToClients);
    metrics.document_all_sent_to_clients_max_bytes = totalStats.maxBytesSentToClients;
    metrics.document_active_sent_to_clients_average_bytes = (activeDocsCount ? activeStats.totalBytesSentToClients / activeDocsCount : 0);
    metrics.document_active_sent_to_clients_min_bytes = MIN_ABS_TO_0(activeStats.minBytesSentToClients);
    metrics.document_active_sent_to_clients_max_bytes = activeStats.maxBytesSentToClients;
    metrics.document_expired_sent_to_clients_average_bytes = (expiredDocsCount ? expiredStats.totalBytesSentToClients / expiredDocsCount : 0);
    metrics.document_expired_sent_to_clients_min_bytes = MIN_ABS_TO_0(expiredStats.minBytesSentToClients);
    metrics.document_expired_sent_to_clients_max_bytes = expiredStats.maxBytesSentToClients;

    metrics.document_all_received_from_clients_average_bytes = (totalDocsCount ? totalStats.totalBytesReceivedFromClients / totalDocsCount : 0);
    metrics.document_all_received_from_clients_min_bytes = MIN_ABS_TO_0(totalStats.minBytesReceivedFromClients);
    metrics.document_all_received_from_clients_max_bytes = totalStats.maxBytesReceivedFromClients;
    metrics.document_active_received_from_clients_average_bytes = (activeDocsCount ? activeStats.totalBytesReceivedFromClients / activeDocsCount : 0);
    metrics.document_active_received_from_clients_min_bytes = MIN_ABS_TO_0(activeStats.minBytesReceivedFromClients);
    metrics.document_active_received_from_clients_max_bytes = activeStats.maxBytesReceivedFromClients;
    metrics.document_expired_received_from_clients_average_bytes = (expiredDocsCount ? expiredStats.totalBytesReceivedFromClients / expiredDocsCount : 0);
    metrics.document_expired_received_from_clients_min_bytes = MIN_ABS_TO_0(expiredStats.minBytesReceivedFromClients);
    metrics.document_expired_received_from_clients_max_bytes = expiredStats.maxBytesReceivedFromClients;

    metrics.document_all_wopi_download_duration_average_seconds = (totalDocsCount ? totalStats.totalWopiDownloadDuration / totalDocsCount : 0) / (double)1000;
    metrics.document_all_wopi_download_duration_min_seconds = MIN_ABS_TO_0(totalStats.minWopiDownloadDuration) / (double)1000;
    metrics.document_all_wopi_download_duration_max_seconds = totalStats.maxWopiDownloadDuration / (double)1000;
    metrics.document_active_wopi_download_duration_average_seconds = (activeDocsCount ? activeStats.totalWopiDownloadDuration / activeDocsCount : 0) / (double)1000;
    metrics.document_active_wopi_download_duration_min_seconds = MIN_ABS_TO_0(activeStats.minWopiDownloadDuration) / (double)1000;
    metrics.document_active_wopi_download_duration_max_seconds = activeStats.maxWopiDownloadDuration / (double)1000;
    metrics.document_expired_wopi_download_duration_average_seconds = (expiredDocsCount ? expiredStats.totalWopiDownloadDuration / expiredDocsCount : 0) / (double)1000;
    metrics.document_expired_wopi_download_duration_min_seconds = MIN_ABS_TO_0(expiredStats.minWopiDownloadDuration) / (double)1000;
    metrics.document_expired_wopi_download_duration_max_seconds = expiredStats.maxWopiDownloadDuration / (double)1000;

    metrics.document_all_wopi_upload_duration_average_seconds = (totalStats.totalUploadedDocs ? totalStats.totalWopiUploadDuration / totalStats.totalUploadedDocs : 0) / (double)1000;
    metrics.document_all_wopi_upload_duration_min_seconds = MIN_ABS_TO_0(totalStats.minWopiUploadDuration) / (double)1000;
    metrics.document_all_wopi_upload_duration_max_seconds = totalStats.maxWopiUploadDuration / (double)1000;
    metrics.document_active_wopi_upload_duration_average_seconds = (activeStats.totalUploadedDocs ? activeStats.totalWopiUploadDuration / activeStats.totalUploadedDocs : 0) / (double)1000;
    metrics.document_active_wopi_upload_duration_min_seconds = MIN_ABS_TO_0(activeStats.minWopiUploadDuration) / (double)1000;
    metrics.document_active_wopi_upload_duration_max_seconds = activeStats.maxWopiUploadDuration / (double)1000;
    metrics.document_expired_wopi_upload_duration_average_seconds = (expiredStats.totalUploadedDocs ? expiredStats.totalWopiUploadDuration / expiredStats.totalUploadedDocs : 0) / (double)1000;
    metrics.document_expired_wopi_upload_duration_min_seconds = MIN_ABS_TO_0(expiredStats.minWopiUploadDuration) / (double)1000;
    metrics.document_expired_wopi_upload_duration_max_seconds = expiredStats.maxWopiUploadDuration / (double)1000;

    metrics.document_all_view_load_duration_average_seconds = (totalAllViews ? totalStats.totalViewLoadDuration / totalAllViews : 0) / (double)1000;
    metrics.document_all_view_load_duration_min_seconds = MIN_ABS_TO_0(totalStats.minViewLoadDuration) / (double)1000;
    metrics.document_all_view_load_duration_max_seconds = totalStats.maxViewLoadDuration / (double)1000;
    metrics.document_active_view_load_duration_average_seconds = (totalActiveDocAllViews ? activeStats.totalViewLoadDuration / totalActiveDocAllViews : 0) / (double)1000;
    metrics.document_active_view_load_duration_min_seconds = MIN_ABS_TO_0(activeStats.minViewLoadDuration) / (double)1000;
    metrics.document_active_view_load_duration_max_seconds = activeStats.maxViewLoadDuration / (double)1000;
    metrics.document_expired_view_load_duration_average_seconds = (totalExpiredDocAllViews ? expiredStats.totalViewLoadDuration / totalExpiredDocAllViews : 0) / (double)1000;
    metrics.document_expired_view_load_duration_min_seconds = MIN_ABS_TO_0(expiredStats.minViewLoadDuration) / (double)1000;
    metrics.document_expired_view_load_duration_max_seconds = expiredStats.maxViewLoadDuration / (double)1000;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
