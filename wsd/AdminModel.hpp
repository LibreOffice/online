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

class DocumentSnapshot;

/// A document in Admin controller.
class Document
{
    friend class DocumentSnapshot;
public:
    Document(const std::string& docKey,
             Poco::Process::PID pid,
             const std::string& filename)
        : _docKey(docKey),
          _pid(pid),
          _filename(filename),
          _memoryDirty(0),
          _start(std::time(nullptr)),
          _lastActivity(_start),
          _snapshots(std::map<std::time_t,std::shared_ptr<DocumentSnapshot>>())
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
    const std::map<std::time_t,std::shared_ptr<DocumentSnapshot>> getHistory() const;

    std::string to_string() const;

private:
    const std::string _docKey;
    const Poco::Process::PID _pid;
    /// SessionId mapping to View object
    std::map<std::string, View> _views;
    /// Total number of active views
    unsigned _activeViews = 0;
    /// Hosted filename
    std::string _filename;
    /// The dirty (ie. un-shared) memory of the document's Kit process.
    int _memoryDirty;

    std::time_t _start;
    std::time_t _lastActivity;
    std::time_t _end = 0;

    std::map<std::time_t,std::shared_ptr<DocumentSnapshot>> _snapshots;
};


class DocumentSnapshot : Document
{
public:
    DocumentSnapshot(const Document& doc) : Document(doc)
    {
        Document::_snapshots.clear();
    }

    using Document::getPid;
    using Document::getFilename;
    using Document::isExpired;
    std::time_t getElapsedTime() const { return Document::_end - Document::_start; } //I'm not sure..
    std::time_t getIdleTime() const {return Document::_end - Document::_lastActivity; } //ditto
    using Document::getActiveViews;
    //const std::map<std::string, View>& getViews() const;
    using Document::getMemoryDirty;
    using Document::to_string;

private:
    using Document::getElapsedTime;
    using Document::getIdleTime;
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
    std::map<std::string,std::map<std::time_t,std::shared_ptr<DocumentSnapshot>>> _expiredDocuments;

    /// The last N total memory Dirty size.
    std::list<unsigned> _memStats;
    unsigned _memStatsSize = 100;

    std::list<unsigned> _cpuStats;
    unsigned _cpuStatsSize = 100;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
