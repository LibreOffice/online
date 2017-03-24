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
#include <ctime>

#include <Poco/Process.h>

#include "Log.hpp"
#include "net/WebSocketHandler.hpp"
#include "Util.hpp"

class DocumentSnapshot;

/// A client view in Admin controller.
class View
{
    friend class DocumentSnapshot;
public:
    View(const std::string& sessionId) :
        _sessionId(sessionId),
        _start(std::time(nullptr))
    {
    }

    void expire() { _end = std::time(nullptr); }
    bool isExpired() const { return _end != 0 && std::time(nullptr) >= _end; }

private:
    const std::string _sessionId;
    const std::time_t _start;
    std::time_t _end = 0;
};

/// A document in Admin controller.
class Document
{
    friend class DocumentSnapshot;

public:
    typedef typename std::set<std::shared_ptr<DocumentSnapshot>> DocumentSnapshots_SetType;

    Document(const std::string& docKey,
             Poco::Process::PID pid,
             const std::string& filename)
        : _docKey(docKey),
          _pid(pid),
          _filename(filename),
          _memoryDirty(0),
          _start(std::time(nullptr)),
          _lastActivity(_start),
          _snapshots(DocumentSnapshots_SetType())
    {
    }

    Poco::Process::PID getPid() const { return _pid; }

    std::string getFilename() const { return _filename; }

    bool isExpired() const { return _end != 0 && std::time(nullptr) >= _end; }

    std::time_t getElapsedTime() const { return std::time(nullptr) - _start; }

    std::time_t getIdleTime() const { return std::time(nullptr) - _lastActivity; }

    void addView(const std::string& sessionId);

    int expireView(const std::string& sessionId);

    unsigned getActiveViews() const { return _activeViews; }

    const std::map<std::string, View>& getViews() const { return _views; }

    void updateLastActivityTime() { _lastActivity = std::time(nullptr); }
    bool updateMemoryDirty(int dirty);
    int getMemoryDirty() const { return _memoryDirty; }

    void takeSnapshot();

    //const std::map<std::time_t,std::shared_ptr<DocumentSnapshot>> getHistory() const;
    const DocumentSnapshots_SetType& getHistory() const{ return _snapshots; }

    std::string to_string() const;

private:
    const std::string _docKey;
    /// SessionId mapping to View object
    const Poco::Process::PID _pid;
    /// Total number of active views
    std::map<std::string, View> _views;
    /// Total number of expired views ( map<sid, timestamp> )
    std::map<std::string, std::time_t> _expiredViews;
    unsigned _activeViews = 0;
    /// Hosted filename
    std::string _filename;
    /// The dirty (ie. un-shared) memory of the document's Kit process.
    int _memoryDirty;

    std::time_t _start;
    std::time_t _lastActivity;
    std::time_t _end = 0;

    //std::map<std::time_t,std::shared_ptr<DocumentSnapshot>> _snapshots;
    DocumentSnapshots_SetType _snapshots;
};

class DocumentSnapshot : Document
{
public:
    DocumentSnapshot(const Document& doc) :
        Document(doc),
        _timestamp(std::time(nullptr))
    {
        Document::_snapshots.clear();
    }

    const std::time_t* getCreationTime() const { return &_timestamp; }

    using Document::getPid;
    using Document::getFilename;
    using Document::isExpired;
    std::time_t getElapsedTime() const { return (_timestamp - _start); }
    std::time_t getIdleTime() const {return (_timestamp - _lastActivity); }
    using Document::getActiveViews;
    //const std::map<std::string, View>& getViews() const { return _expiredViews; }
    const std::map<std::string, std::time_t> getViews() const { return _expiredViews; }
    using Document::getMemoryDirty;
    std::string to_string() const;

private:
    // I want to inhibit someone calls these methods on superclass
    using Document::getElapsedTime;
    using Document::getIdleTime;
    using Document::addView;
    using Document::expireView;
    using Document::getViews;
    using Document::updateLastActivityTime;
    using Document::updateMemoryDirty;
    using Document::takeSnapshot;
    using Document::getHistory;
    using Document::to_string;

    const std::time_t _timestamp;
};

/// An Admin session subscriber.
class Subscriber
{
public:
    Subscriber(int sessionId, const std::weak_ptr<WebSocketHandler>& ws)
        : _sessionId(sessionId),
          _ws(ws),
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
    /// Admin session Id
    int _sessionId;

    /// The underlying AdminRequestHandler
    std::weak_ptr<WebSocketHandler> _ws;

    std::set<std::string> _subscriptions;

    std::time_t _start;
    std::time_t _end = 0;
};

/// The Admin controller implementation.
class AdminModel
{
public:
    AdminModel();

    ~AdminModel();

    std::string query(const std::string& command);

    /// Returns memory consumed by all active loolkit processes
    unsigned getKitsMemoryUsage();

    void subscribe(int sessionId, const std::weak_ptr<WebSocketHandler>& ws);
    void subscribe(int sessionId, const std::string& command);

    void unsubscribe(int sessionId, const std::string& command);

    void clearMemStats() { _memStats.clear(); }

    void clearCpuStats() { _cpuStats.clear(); }

    void addMemStats(unsigned memUsage);

    void addCpuStats(unsigned cpuUsage);

    void setCpuStatsSize(unsigned size);

    void setMemStatsSize(unsigned size);

    void notify(const std::string& message);

    void addDocument(const std::string& docKey, Poco::Process::PID pid, const std::string& filename, const std::string& sessionId);

    void removeDocument(const std::string& docKey, const std::string& sessionId);
    void removeDocument(const std::string& docKey);

    void updateLastActivityTime(const std::string& docKey);
    void updateMemoryDirty(const std::string& docKey, int dirty);

private:
    std::string getMemStats();

    std::string getCpuStats();

    unsigned getTotalActiveViews();

    std::string getDocuments() const;

private:
    std::map<int, Subscriber> _subscribers;
    std::map<std::string,Document> _documents;
    std::map<std::string,Document::DocumentSnapshots_SetType> _expiredDocuments;

    /// The last N total memory Dirty size.
    std::list<unsigned> _memStats;
    unsigned _memStatsSize = 100;

    std::list<unsigned> _cpuStats;
    unsigned _cpuStatsSize = 100;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
