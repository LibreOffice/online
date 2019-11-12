/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ADMINMODEL_HPP
#define INCLUDED_ADMINMODEL_HPP

#include <memory>
#include <set>
#include <string>

#include <Poco/Process.h>

#include "Log.hpp"
#include "net/WebSocketHandler.hpp"
#include "Util.hpp"

/// A client view in Admin controller.
class View
{
public:
    View(const std::string& sessionId, const std::string& userName, const std::string& userId) :
        _sessionId(sessionId),
        _userName(userName),
        _userId(userId),
        _start(std::time(nullptr)),
        _loadDuration(0)
    {
    }

    void expire() { _end = std::time(nullptr); }
    std::string getUserName() const { return _userName; }
    std::string getUserId() const { return _userId; }
    std::string getSessionId() const { return _sessionId; }
    bool isExpired() const { return _end != 0 && std::time(nullptr) >= _end; }
    std::chrono::milliseconds getLoadDuration() const { return _loadDuration; }
    void setLoadDuration(std::chrono::milliseconds loadDuration) { _loadDuration = loadDuration; }

private:
    const std::string _sessionId;
    const std::string _userName;
    const std::string _userId;
    const std::time_t _start;
    std::time_t _end = 0;
    std::chrono::milliseconds _loadDuration;
};

struct DocProcSettings
{
    void setLimitVirtMemMb(size_t limitVirtMemMb) { _limitVirtMemMb = limitVirtMemMb; }
    size_t getLimitVirtMemMb() const { return _limitVirtMemMb; }
    void setLimitStackMemKb(size_t limitStackMemKb) { _limitStackMemKb = limitStackMemKb; }
    size_t getLimitStackMemKb() const { return _limitStackMemKb; }
    void setLimitFileSizeMb(size_t limitFileSizeMb) { _limitFileSizeMb = limitFileSizeMb; }
    size_t getLimitFileSizeMb() const { return _limitFileSizeMb; }
    void setLimitNumberOpenFiles(size_t limitNumberOpenFiles) { _limitNumberOpenFiles = limitNumberOpenFiles; }
    size_t getLimitNumberOpenFiles() const { return _limitNumberOpenFiles; }

private:
    size_t _limitVirtMemMb;
    size_t _limitStackMemKb;
    size_t _limitFileSizeMb;
    size_t _limitNumberOpenFiles;
};

/// Containing basic information about document
class DocBasicInfo
{
    std::string _docKey;
    std::time_t _idleTime;
    int _mem;
    bool _saved;

public:
    DocBasicInfo(const std::string& docKey, std::time_t idleTime, int mem, bool saved) :
        _docKey(docKey),
        _idleTime(idleTime),
        _mem(mem),
        _saved(saved)
    {
    }

    const std::string& getDocKey() const { return _docKey; }

    std::time_t getIdleTime() const { return _idleTime; }

    int getMem() const { return _mem; }

    bool getSaved() const { return _saved; }
};

/// A document in Admin controller.
class Document
{
public:
    Document(const std::string& docKey,
             Poco::Process::PID pid,
             const std::string& filename)
        : _docKey(docKey),
          _pid(pid),
          _activeViews(0),
          _filename(filename),
          _memoryDirty(0),
          _lastJiffy(0),
          _start(std::time(nullptr)),
          _lastActivity(_start),
          _end(0),
          _sentBytes(0),
          _recvBytes(0),
          _wopiDownloadDuration(0),
          _wopiUploadDuration(0),
          _isModified(false)
    {
    }

    const std::string getDocKey() const { return _docKey; }

    Poco::Process::PID getPid() const { return _pid; }

    std::string getFilename() const { return _filename; }

    bool isExpired() const { return _end != 0 && std::time(nullptr) >= _end; }

    std::time_t getElapsedTime() const { return std::time(nullptr) - _start; }

    std::time_t getIdleTime() const { return std::time(nullptr) - _lastActivity; }

    void addView(const std::string& sessionId, const std::string& userName, const std::string& userId);

    int expireView(const std::string& sessionId);

    unsigned getActiveViews() const { return _activeViews; }

    unsigned getLastJiffies() const { return _lastJiffy; }
    void setLastJiffies(size_t newJ) { _lastJiffy = newJ; }

    const std::map<std::string, View>& getViews() const { return _views; }

    void updateLastActivityTime() { _lastActivity = std::time(nullptr); }
    bool updateMemoryDirty(int dirty);
    int getMemoryDirty() const { return _memoryDirty; }

    std::pair<std::time_t, std::string> getSnapshot() const;
    const std::string getHistory() const;
    void takeSnapshot();

    void setModified(bool value) { _isModified = value; }
    bool getModifiedStatus() const { return _isModified; }

    void addBytes(uint64_t sent, uint64_t recv)
    {
        _sentBytes += sent;
        _recvBytes += recv;
    }

    const DocProcSettings& getDocProcSettings() const { return _docProcSettings; }
    void setDocProcSettings(const DocProcSettings& docProcSettings) { _docProcSettings = docProcSettings; }

    std::time_t getOpenTime(){ return _end - _start; }
    uint64_t getSentBytes() const { return _sentBytes; }
    uint64_t getRecvBytes() const { return _recvBytes; }
    void setViewLoadDuration(const std::string& sessionId, std::chrono::milliseconds viewLoadDuration);
    void setWopiDownloadDuration(std::chrono::milliseconds wopiDownloadDuration) { _wopiDownloadDuration = wopiDownloadDuration; }
    std::chrono::milliseconds getWopiDownloadDuration() const { return _wopiDownloadDuration; }
    void setWopiUploadDuration(const std::chrono::milliseconds wopiUploadDuration) { _wopiUploadDuration = wopiUploadDuration; }
    std::chrono::milliseconds getWopiUploadDuration() const { return _wopiUploadDuration; }

    std::string to_string() const;

private:
    const std::string _docKey;
    const Poco::Process::PID _pid;
    /// SessionId mapping to View object
    std::map<std::string, View> _views;
    /// Total number of active views
    unsigned _activeViews;
    /// Hosted filename
    std::string _filename;
    /// The dirty (ie. un-shared) memory of the document's Kit process.
    int _memoryDirty;
    /// Last noted Jiffy count
    unsigned _lastJiffy;

    std::time_t _start;
    std::time_t _lastActivity;
    std::time_t _end;
    std::map<std::time_t,std::string> _snapshots;

    /// Total bytes sent and recv'd by this document.
    uint64_t _sentBytes, _recvBytes;

    //Download/upload duration from/to storage for this document
    std::chrono::milliseconds _wopiDownloadDuration;
    std::chrono::milliseconds _wopiUploadDuration;

    /// Per-doc kit process settings.
    DocProcSettings _docProcSettings;
    bool _isModified;
};

/// An Admin session subscriber.
class Subscriber
{
public:
    Subscriber(const std::weak_ptr<WebSocketHandler>& ws)
        : _ws(ws),
          _start(std::time(nullptr))
    {
        LOG_INF("Subscriber ctor.");
    }

    ~Subscriber()
    {
        LOG_INF("Subscriber dtor.");
    }

    bool notify(const std::string& message);

    bool subscribe(const std::string& command);

    void unsubscribe(const std::string& command);

    void expire() { _end = std::time(nullptr); }

    bool isExpired() const { return _end != 0 && std::time(nullptr) >= _end; }

private:
    /// The underlying AdminRequestHandler
    std::weak_ptr<WebSocketHandler> _ws;

    std::set<std::string> _subscriptions;

    std::time_t _start;
    std::time_t _end = 0;
};

struct AdminMetrics
{
    AdminMetrics()
    :
    global_host_system_memory_bytes(0),
    global_memory_available_bytes(0),
    global_memory_used_bytes(0),
    global_memory_free_bytes(0),
    loolwsd_count(0),
    loolwsd_thread_count(0),
    loolwsd_cpu_time_seconds(0),
    loolwsd_memory_used_bytes(0),
    forkit_count(0),
    forkit_thread_count(0),
    forkit_cpu_time_seconds(0),
    forkit_memory_used_bytes(0),
    global_all_document_count(0),
    global_active_document_count(0),
    global_idle_document_count(0),
    global_expired_document_count(0),

    global_all_views_count(0),
    global_active_views_count(0),
    global_expired_views_count(0),

    global_bytes_sent_to_clients_bytes(0),
    global_bytes_received_from_clients_bytes(0),

    kit_count(0),
    kit_unassigned_count(0),
    kit_assigned_count(0),
    kit_segfaulted_count(0),
    kit_thread_count_average(0),
    kit_thread_count_max(0),
    kit_memory_used_total_bytes(0),
    kit_memory_used_average_bytes(0),
    kit_memory_used_min_bytes(0),
    kit_memory_used_max_bytes(0),
    kit_cpu_time_total_seconds(0),
    kit_cpu_time_average_seconds(0),
    kit_cpu_time_min_seconds(0),
    kit_cpu_time_max_seconds(0),

    document_all_views_all_count_average(0),
    document_all_views_all_count_max(0),
    document_active_views_all_count_average(0),
    document_active_views_all_count_max(0),
    document_active_views_active_count(0),
    document_active_views_active_count_average(0),
    document_active_views_active_count_max(0),
    document_active_views_expired_count(0),
    document_active_views_expired_count_average(0),
    document_active_views_expired_count_max(0),
    document_expired_views_all_count_average(0),
    document_expired_views_all_count_max(0),

    document_all_opened_time_average_seconds(0),
    document_all_opened_time_min_seconds(0),
    document_all_opened_time_max_seconds(0),
    document_active_opened_time_average_seconds(0),
    document_active_opened_time_min_seconds(0),
    document_active_opened_time_max_seconds(0),
    document_expired_opened_time_average_seconds(0),
    document_expired_opened_time_min_seconds(0),
    document_expired_opened_time_max_seconds(0),

    document_all_sent_to_clients_average_bytes(0),
    document_all_sent_to_clients_min_bytes(0),
    document_all_sent_to_clients_max_bytes(0),
    document_active_sent_to_clients_average_bytes(0),
    document_active_sent_to_clients_min_bytes(0),
    document_active_sent_to_clients_max_bytes(0),
    document_expired_sent_to_clients_average_bytes(0),
    document_expired_sent_to_clients_min_bytes(0),
    document_expired_sent_to_clients_max_bytes(0),

    document_all_received_from_clients_average_bytes(0),
    document_all_received_from_clients_min_bytes(0),
    document_all_received_from_clients_max_bytes(0),
    document_active_received_from_clients_average_bytes(0),
    document_active_received_from_clients_min_bytes(0),
    document_active_received_from_clients_max_bytes(0),
    document_expired_received_from_clients_average_bytes(0),
    document_expired_received_from_clients_min_bytes(0),
    document_expired_received_from_clients_max_bytes(0),

    document_all_wopi_download_duration_average_seconds(0),
    document_all_wopi_download_duration_min_seconds(0),
    document_all_wopi_download_duration_max_seconds(0),
    document_active_wopi_download_duration_average_seconds(0),
    document_active_wopi_download_duration_min_seconds(0),
    document_active_wopi_download_duration_max_seconds(0),
    document_expired_wopi_download_duration_average_seconds(0),
    document_expired_wopi_download_duration_min_seconds(0),
    document_expired_wopi_download_duration_max_seconds(0),

    document_all_wopi_upload_duration_average_seconds(0),
    document_all_wopi_upload_duration_min_seconds(0),
    document_all_wopi_upload_duration_max_seconds(0),
    document_active_wopi_upload_duration_average_seconds(0),
    document_active_wopi_upload_duration_min_seconds(0),
    document_active_wopi_upload_duration_max_seconds(0),
    document_expired_wopi_upload_duration_average_seconds(0),
    document_expired_wopi_upload_duration_min_seconds(0),
    document_expired_wopi_upload_duration_max_seconds(0),

    document_all_view_load_duration_average_seconds(0),
    document_all_view_load_duration_min_seconds(0),
    document_all_view_load_duration_max_seconds(0),
    document_active_view_load_duration_average_seconds(0),
    document_active_view_load_duration_min_seconds(0),
    document_active_view_load_duration_max_seconds(0),
    document_expired_view_load_duration_average_seconds(0),
    document_expired_view_load_duration_min_seconds(0),
    document_expired_view_load_duration_max_seconds(0)

    {}

    uint64_t global_host_system_memory_bytes;
    uint64_t global_memory_available_bytes;
    uint64_t global_memory_used_bytes;
    uint64_t global_memory_free_bytes;

    uint32_t loolwsd_count;
    uint32_t loolwsd_thread_count;
    uint32_t loolwsd_cpu_time_seconds;
    uint64_t loolwsd_memory_used_bytes;

    uint32_t forkit_count;
    uint32_t forkit_thread_count;
    uint32_t forkit_cpu_time_seconds;
    uint64_t forkit_memory_used_bytes;

    uint32_t global_all_document_count;
    uint32_t global_active_document_count;
    uint32_t global_idle_document_count;
    uint32_t global_expired_document_count;

    uint32_t global_all_views_count;
    uint32_t global_active_views_count;
    uint32_t global_expired_views_count;

    uint64_t global_bytes_sent_to_clients_bytes;
    uint64_t global_bytes_received_from_clients_bytes;

    uint32_t kit_count;
    uint32_t kit_unassigned_count;
    uint32_t kit_assigned_count;
    uint32_t kit_segfaulted_count;
    uint32_t kit_thread_count_average;
    uint32_t kit_thread_count_max;
    uint64_t kit_memory_used_total_bytes;
    uint64_t kit_memory_used_average_bytes;
    uint64_t kit_memory_used_min_bytes;
    uint64_t kit_memory_used_max_bytes;
    uint32_t kit_cpu_time_total_seconds;
    uint32_t kit_cpu_time_average_seconds;
    uint32_t kit_cpu_time_min_seconds;
    uint32_t kit_cpu_time_max_seconds;

    uint32_t document_all_views_all_count_average;
    uint32_t document_all_views_all_count_max;
    uint32_t document_active_views_all_count_average;
    uint32_t document_active_views_all_count_max;
    uint32_t document_active_views_active_count;
    uint32_t document_active_views_active_count_average;
    uint32_t document_active_views_active_count_max;
    uint32_t document_active_views_expired_count;
    uint32_t document_active_views_expired_count_average;
    uint32_t document_active_views_expired_count_max;
    uint32_t document_expired_views_all_count_average;
    uint32_t document_expired_views_all_count_max;

    uint32_t document_all_opened_time_average_seconds;
    uint32_t document_all_opened_time_min_seconds;
    uint32_t document_all_opened_time_max_seconds;
    uint32_t document_active_opened_time_average_seconds;
    uint32_t document_active_opened_time_min_seconds;
    uint32_t document_active_opened_time_max_seconds;
    uint32_t document_expired_opened_time_average_seconds;
    uint32_t document_expired_opened_time_min_seconds;
    uint32_t document_expired_opened_time_max_seconds;

    uint64_t document_all_sent_to_clients_average_bytes;
    uint64_t document_all_sent_to_clients_min_bytes;
    uint64_t document_all_sent_to_clients_max_bytes;
    uint64_t document_active_sent_to_clients_average_bytes;
    uint64_t document_active_sent_to_clients_min_bytes;
    uint64_t document_active_sent_to_clients_max_bytes;
    uint64_t document_expired_sent_to_clients_average_bytes;
    uint64_t document_expired_sent_to_clients_min_bytes;
    uint64_t document_expired_sent_to_clients_max_bytes;

    uint64_t document_all_received_from_clients_average_bytes;
    uint64_t document_all_received_from_clients_min_bytes;
    uint64_t document_all_received_from_clients_max_bytes;
    uint64_t document_active_received_from_clients_average_bytes;
    uint64_t document_active_received_from_clients_min_bytes;
    uint64_t document_active_received_from_clients_max_bytes;
    uint64_t document_expired_received_from_clients_average_bytes;
    uint64_t document_expired_received_from_clients_min_bytes;
    uint64_t document_expired_received_from_clients_max_bytes;

    double document_all_wopi_download_duration_average_seconds;
    double document_all_wopi_download_duration_min_seconds;
    double document_all_wopi_download_duration_max_seconds;
    double document_active_wopi_download_duration_average_seconds;
    double document_active_wopi_download_duration_min_seconds;
    double document_active_wopi_download_duration_max_seconds;
    double document_expired_wopi_download_duration_average_seconds;
    double document_expired_wopi_download_duration_min_seconds;
    double document_expired_wopi_download_duration_max_seconds;

    double document_all_wopi_upload_duration_average_seconds;
    double document_all_wopi_upload_duration_min_seconds;
    double document_all_wopi_upload_duration_max_seconds;
    double document_active_wopi_upload_duration_average_seconds;
    double document_active_wopi_upload_duration_min_seconds;
    double document_active_wopi_upload_duration_max_seconds;
    double document_expired_wopi_upload_duration_average_seconds;
    double document_expired_wopi_upload_duration_min_seconds;
    double document_expired_wopi_upload_duration_max_seconds;

    double document_all_view_load_duration_average_seconds;
    double document_all_view_load_duration_min_seconds;
    double document_all_view_load_duration_max_seconds;
    double document_active_view_load_duration_average_seconds;
    double document_active_view_load_duration_min_seconds;
    double document_active_view_load_duration_max_seconds;
    double document_expired_view_load_duration_average_seconds;
    double document_expired_view_load_duration_min_seconds;
    double document_expired_view_load_duration_max_seconds;

    void toString(std::string &metrics);
};

/// The Admin controller implementation.
class AdminModel
{
public:

typedef struct _DocumentStats
    {
        _DocumentStats()
        : totalUsedMemory(0),
        minUsedMemory(0xFFFFFFFFFFFFFFFF),
        maxUsedMemory(0),
        totalOpenTime(0),
        minOpenTime(0xFFFFFFFFFFFFFFFF),
        maxOpenTime(0),
        totalBytesSentToClients(0),
        minBytesSentToClients(0xFFFFFFFFFFFFFFFF),
        maxBytesSentToClients(0),
        totalBytesReceivedFromClients(0),
        minBytesReceivedFromClients(0xFFFFFFFFFFFFFFFF),
        maxBytesReceivedFromClients(0),
        totalWopiDownloadDuration(0),
        minWopiDownloadDuration(0xFFFFFFFFFFFFFFFF),
        maxWopiDownloadDuration(0),
        totalWopiUploadDuration(0),
        minWopiUploadDuration(0xFFFFFFFFFFFFFFFF),
        maxWopiUploadDuration(0),
        totalViewLoadDuration(0),
        minViewLoadDuration(0xFFFFFFFFFFFFFFFF),
        maxViewLoadDuration(0),
        totalUploadedDocs(0)
        {}

        uint64_t totalUsedMemory;
        uint64_t minUsedMemory;
        uint64_t maxUsedMemory;
        uint64_t totalOpenTime;
        uint64_t minOpenTime;
        uint64_t maxOpenTime;
        uint64_t totalBytesSentToClients;
        uint64_t minBytesSentToClients;
        uint64_t maxBytesSentToClients;
        uint64_t totalBytesReceivedFromClients;
        uint64_t minBytesReceivedFromClients;
        uint64_t maxBytesReceivedFromClients;
        uint64_t totalWopiDownloadDuration;
        uint64_t minWopiDownloadDuration;
        uint64_t maxWopiDownloadDuration;
        uint64_t totalWopiUploadDuration;
        uint64_t minWopiUploadDuration;
        uint64_t maxWopiUploadDuration;
        uint64_t totalViewLoadDuration;
        uint64_t minViewLoadDuration;
        uint64_t maxViewLoadDuration;
        int totalUploadedDocs;
    }DocumentStats;

    AdminModel() :
        _segFaultCount(0),
        _owner(std::this_thread::get_id())
    {
        LOG_INF("AdminModel ctor.");
    }

    ~AdminModel();

    /// All methods here must be called from the Admin socket-poll
    void setThreadOwner(const std::thread::id &id) { _owner = id; }

    /// In debug mode check that code is running in the correct thread.
    /// Asserts in the debug builds, otherwise just logs.
    void assertCorrectThread() const;

    std::string query(const std::string& command);
    std::string getAllHistory() const;

    /// Returns memory consumed by all active loolkit processes
    unsigned getKitsMemoryUsage();
    size_t getKitsJiffies();

    void subscribe(int sessionId, const std::weak_ptr<WebSocketHandler>& ws);
    void subscribe(int sessionId, const std::string& command);

    void unsubscribe(int sessionId, const std::string& command);

    void modificationAlert(const std::string& docKey, Poco::Process::PID pid, bool value);

    void clearMemStats() { _memStats.clear(); }

    void clearCpuStats() { _cpuStats.clear(); }

    void addMemStats(unsigned memUsage);

    void addCpuStats(unsigned cpuUsage);

    void addSentStats(uint64_t sent);

    void addRecvStats(uint64_t recv);

    void setCpuStatsSize(unsigned size);

    void setMemStatsSize(unsigned size);

    void notify(const std::string& message);

    void addDocument(const std::string& docKey, Poco::Process::PID pid, const std::string& filename, const std::string& sessionId, const std::string& userName, const std::string& userId);

    void removeDocument(const std::string& docKey, const std::string& sessionId);
    void removeDocument(const std::string& docKey);

    void updateLastActivityTime(const std::string& docKey);
    void updateMemoryDirty(const std::string& docKey, int dirty);

    void addBytes(const std::string& docKey, uint64_t sent, uint64_t recv);

    uint64_t getSentBytesTotal() { return _sentBytesTotal; }
    uint64_t getRecvBytesTotal() { return _recvBytesTotal; }

    double getServerUptime();

    /// Document basic info list sorted by most idle time
    std::vector<DocBasicInfo> getDocumentsSortedByIdle() const;

    void setViewLoadDuration(const std::string& docKey, const std::string& sessionId, std::chrono::milliseconds viewLoadDuration);
    void setDocWopiDownloadDuration(const std::string& docKey, std::chrono::milliseconds wopiDownloadDuration);
    void setDocWopiUploadDuration(const std::string& docKey, const std::chrono::milliseconds wopiUploadDuration);
    void addSegFaultCount(unsigned segFaultCount);

    void updateDocumentStats(const Document &d, DocumentStats &stats);
    void getMetrics(AdminMetrics &metrics);
    void getMetrics(std::string &metrics);

private:
    std::string getMemStats();

    std::string getSentActivity();

    std::string getRecvActivity();

    std::string getCpuStats();

    unsigned getTotalActiveViews();

    std::string getDocuments() const;

private:
    std::map<int, Subscriber> _subscribers;
    std::map<std::string, Document> _documents;
    std::map<std::string, Document> _expiredDocuments;

    /// The last N total memory Dirty size.
    std::list<unsigned> _memStats;
    unsigned _memStatsSize = 100;

    std::list<unsigned> _cpuStats;
    unsigned _cpuStatsSize = 100;

    std::list<unsigned> _sentStats;
    unsigned _sentStatsSize = 100;

    std::list<unsigned> _recvStats;
    unsigned _recvStatsSize = 100;

    uint64_t _sentBytesTotal;
    uint64_t _recvBytesTotal;

    uint64_t _segFaultCount;

    /// We check the owner even in the release builds, needs to be always correct.
    std::thread::id _owner;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
