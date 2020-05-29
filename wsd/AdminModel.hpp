/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <memory>
#include <set>
#include <string>
#include <cmath>
#include <list>

#include "Log.hpp"
#include "net/WebSocketHandler.hpp"
#include "Util.hpp"

struct DocumentAggregateStats;

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
    // cf. FILE* member.
    Document(const Document &) = delete;
    Document& operator = (const Document &) = delete;

public:
    Document(const std::string& docKey,
             pid_t pid,
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
          _procSMaps(nullptr),
          _lastTimeSMapsRead(0),
          _isModified(false),
          _hasMemDirtyChanged(true)
    {
    }

    ~Document()
    {
        if (_procSMaps)
            fclose(_procSMaps);
    }

    const std::string getDocKey() const { return _docKey; }

    pid_t getPid() const { return _pid; }

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
    void updateMemoryDirty();
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

    std::time_t getOpenTime() const { return isExpired() ? _end - _start : getElapsedTime(); }
    uint64_t getSentBytes() const { return _sentBytes; }
    uint64_t getRecvBytes() const { return _recvBytes; }
    void setViewLoadDuration(const std::string& sessionId, std::chrono::milliseconds viewLoadDuration);
    void setWopiDownloadDuration(std::chrono::milliseconds wopiDownloadDuration) { _wopiDownloadDuration = wopiDownloadDuration; }
    std::chrono::milliseconds getWopiDownloadDuration() const { return _wopiDownloadDuration; }
    void setWopiUploadDuration(const std::chrono::milliseconds wopiUploadDuration) { _wopiUploadDuration = wopiUploadDuration; }
    std::chrono::milliseconds getWopiUploadDuration() const { return _wopiUploadDuration; }
    void setProcSMapsFD(const int smapsFD) { _procSMaps = fdopen(smapsFD, "r"); }
    bool hasMemDirtyChanged() const { return _hasMemDirtyChanged; }
    void setMemDirtyChanged(bool changeStatus) { _hasMemDirtyChanged = changeStatus; }

    std::string to_string() const;

private:
    const std::string _docKey;
    const pid_t _pid;
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

    FILE* _procSMaps;
    std::time_t _lastTimeSMapsRead;

    /// Per-doc kit process settings.
    DocProcSettings _docProcSettings;
    bool _isModified;
    bool _hasMemDirtyChanged;
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

/// The Admin controller implementation.
class AdminModel
{
    AdminModel(const AdminModel &) = delete;
    AdminModel& operator = (const AdminModel &) = delete;
public:
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

    void modificationAlert(const std::string& docKey, pid_t pid, bool value);

    void clearMemStats() { _memStats.clear(); }

    void clearCpuStats() { _cpuStats.clear(); }

    void addMemStats(unsigned memUsage);

    void addCpuStats(unsigned cpuUsage);

    void addSentStats(uint64_t sent);

    void addRecvStats(uint64_t recv);

    void setCpuStatsSize(unsigned size);

    void setMemStatsSize(unsigned size);

    void notify(const std::string& message);

    void addDocument(const std::string& docKey, pid_t pid, const std::string& filename,
                     const std::string& sessionId, const std::string& userName, const std::string& userId,
                     const int smapsFD);

    void removeDocument(const std::string& docKey, const std::string& sessionId);
    void removeDocument(const std::string& docKey);

    void updateLastActivityTime(const std::string& docKey);

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
    void setForKitPid(pid_t pid) { _forKitPid = pid; }

    void getMetrics(std::ostringstream &oss);

    std::set<pid_t> getDocumentPids() const;
    void UpdateMemoryDirty();
    void notifyDocsMemDirtyChanged();

    static int getPidsFromProcName(const std::regex& procNameRegEx, std::vector<int> *pids);

private:
    void doRemove(std::map<std::string, std::unique_ptr<Document>>::iterator &docIt);

    std::string getMemStats();

    std::string getSentActivity();

    std::string getRecvActivity();

    std::string getCpuStats();

    unsigned getTotalActiveViews();

    std::string getDocuments() const;

    void CalcDocAggregateStats(DocumentAggregateStats& stats);

private:
    std::map<int, Subscriber> _subscribers;
    std::map<std::string, std::unique_ptr<Document>> _documents;
    std::map<std::string, std::unique_ptr<Document>> _expiredDocuments;

    /// The last N total memory Dirty size.
    std::list<unsigned> _memStats;
    unsigned _memStatsSize = 100;

    std::list<unsigned> _cpuStats;
    unsigned _cpuStatsSize = 100;

    std::list<unsigned> _sentStats;
    unsigned _sentStatsSize = 200;

    std::list<unsigned> _recvStats;
    unsigned _recvStatsSize = 200;

    uint64_t _sentBytesTotal = 0;
    uint64_t _recvBytesTotal = 0;

    uint64_t _segFaultCount = 0;

    pid_t _forKitPid = 0;

    /// We check the owner even in the release builds, needs to be always correct.
    std::thread::id _owner;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
