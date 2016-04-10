/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_DOCUMENTBROKER_HPP
#define INCLUDED_DOCUMENTBROKER_HPP

#include <signal.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <map>

#include <Poco/URI.h>

#include "IoUtil.hpp"
#include "MasterProcessSession.hpp"
#include "Util.hpp"

// Forwards.
class StorageBase;
class TileCache;

/// Represents a new LOK child that is read
/// to host a document.
class ChildProcess
{
public:
    ChildProcess() :
        _pid(-1)
    {
    }

    /// pid is the process ID of the child.
    /// ws is the control WebSocket to the child.
    ChildProcess(const Poco::Process::PID pid, const std::shared_ptr<Poco::Net::WebSocket>& ws) :
        _pid(pid),
        _ws(ws)
    {
        Log::info("ChildProcess ctor [" + std::to_string(_pid) + "].");
    }

    ChildProcess(ChildProcess&& other) :
        _pid(other._pid),
        _ws(other._ws)
    {
        Log::info("ChildProcess move ctor [" + std::to_string(_pid) + "].");
        other._pid = -1;
        other._ws.reset();
    }

    const ChildProcess& operator=(ChildProcess&& other)
    {
        Log::info("ChildProcess assign [" + std::to_string(_pid) + "].");
        _pid = other._pid;
        other._pid = -1;
        _ws = other._ws;
        other._ws.reset();

        return *this;
    }

    ~ChildProcess()
    {
        Log::info("~ChildProcess dtor [" + std::to_string(_pid) + "].");
        close(false);
    }

    void close(const bool rude)
    {
        Log::info("Closing child [" + std::to_string(_pid) + "].");
        _ws.reset();
        if (_pid != -1)
        {
            if (rude && kill(_pid, SIGINT) != 0 && kill(_pid, 0) != 0)
            {
                Log::syserror("Cannot terminate lokit [" + std::to_string(_pid) + "]. Abandoning.");
            }

           _pid = -1;
        }
    }

    Poco::Process::PID getPid() const { return _pid; }
    std::shared_ptr<Poco::Net::WebSocket> getWebSocket() const { return _ws; }

private:
    Poco::Process::PID _pid;
    std::shared_ptr<Poco::Net::WebSocket> _ws;
};

/// DocumentBroker is responsible for setting up a document
/// in jail and brokering loading it from Storage
/// and saving it back.
/// Contains URI, physical path, etc.
class DocumentBroker
{
public:

    static
    Poco::URI sanitizeURI(std::string uri);

    /// Returns a document-specific key based
    /// on the URI of the document.
    static
    std::string getDocKey(const Poco::URI& uri);

    DocumentBroker(const Poco::URI& uriPublic,
                   const std::string& docKey,
                   const std::string& childRoot,
                   std::shared_ptr<ChildProcess> childProcess);

    ~DocumentBroker()
    {
        Log::info() << "~DocumentBroker [" << _uriPublic.toString()
                    << "] destroyed with " << getSessionsCount()
                    << " sessions left." << Log::end;
    }

    void validate(const Poco::URI& uri);

    /// Loads a document from the public URI into the jail.
    bool load(const std::string& jailId);

    bool save();

    void autoSave();

    Poco::URI getPublicUri() const { return _uriPublic; }
    Poco::URI getJailedUri() const { return _uriJailed; }
    const std::string& getJailId() const { return _jailId; }
    const std::string& getDocKey() const { return _docKey; }
    TileCache& tileCache() { return *_tileCache; }
    unsigned getSessionsCount() const
    {
        std::lock_guard<std::mutex> sessionsLock(_wsSessionsMutex);
        return _wsSessions.size();
    }

    /// Returns the time in milliseconds since last save.
    double getTimeSinceLastSaveMs() const
    {
        const auto duration = (std::chrono::steady_clock::now() - _lastSaveTime);
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    std::string getJailRoot() const;

    /// Ignore input events from all web socket sessions
    /// except this one
    void takeEditLock(const std::string id);

    void addWSSession(const std::string id, std::shared_ptr<MasterProcessSession>& ws);

    void removeWSSession(const std::string id);

    unsigned getWSSessionsCount() { return _wsSessions.size(); }

private:
    const Poco::URI _uriPublic;
    const std::string _docKey;
    const std::string _childRoot;
    const std::string _cacheRoot;
    Poco::URI _uriJailed;
    std::string _jailId;
    std::string _filename;
    std::chrono::steady_clock::time_point _lastSaveTime;
    std::map<std::string, std::shared_ptr<MasterProcessSession>> _wsSessions;
    mutable std::mutex _wsSessionsMutex;
    std::unique_ptr<StorageBase> _storage;
    std::unique_ptr<TileCache> _tileCache;
    std::shared_ptr<ChildProcess> _childProcess;
    std::mutex _mutex;

    static constexpr auto IdleSaveDurationMs = 30 * 1000;
    static constexpr auto AutoSaveDurationMs = 300 * 1000;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
