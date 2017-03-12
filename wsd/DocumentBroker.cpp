/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include "DocumentBroker.hpp"

#include <cassert>
#include <ctime>
#include <fstream>
#include <sstream>

#include <Poco/JSON/Object.h>
#include <Poco/Path.h>
#include <Poco/SHA1Engine.h>
#include <Poco/DigestStream.h>
#include <Poco/StreamCopier.h>
#include <Poco/StringTokenizer.h>

#include "Admin.hpp"
#include "ClientSession.hpp"
#include "Exceptions.hpp"
#include "Message.hpp"
#include "Protocol.hpp"
#include "LOOLWSD.hpp"
#include "Log.hpp"
#include "Storage.hpp"
#include "TileCache.hpp"
#include "SenderQueue.hpp"
#include "Unit.hpp"

using namespace LOOLProtocol;

using Poco::JSON::Object;

void ChildProcess::setDocumentBroker(const std::shared_ptr<DocumentBroker>& docBroker)
{
    assert(docBroker && "Invalid DocumentBroker instance.");
    _docBroker = docBroker;

    // Add the prisoner socket to the docBroker poll.
    docBroker->addSocketToPoll(_socket);
}

namespace
{

/// Returns the cache path for a given document URI.
std::string getCachePath(const std::string& uri)
{
    Poco::SHA1Engine digestEngine;

    digestEngine.update(uri.c_str(), uri.size());

    return (LOOLWSD::Cache + '/' +
            Poco::DigestEngine::digestToHex(digestEngine.digest()).insert(3, "/").insert(2, "/").insert(1, "/"));
}
}

Poco::URI DocumentBroker::sanitizeURI(const std::string& uri)
{
    // The URI of the document should be url-encoded.
    std::string decodedUri;
    Poco::URI::decode(uri, decodedUri);
    auto uriPublic = Poco::URI(decodedUri);

    if (uriPublic.isRelative() || uriPublic.getScheme() == "file")
    {
        // TODO: Validate and limit access to local paths!
        uriPublic.normalize();
    }

    if (uriPublic.getPath().empty())
    {
        throw std::runtime_error("Invalid URI.");
    }

    // We decoded access token before embedding it in loleaflet.html
    // So, we need to decode it now to get its actual value
    Poco::URI::QueryParameters queryParams = uriPublic.getQueryParameters();
    for (auto& param: queryParams)
    {
        // look for encoded query params (access token as of now)
        if (param.first == "access_token")
        {
            std::string decodedToken;
            Poco::URI::decode(param.second, decodedToken);
            param.second = decodedToken;
        }
    }

    uriPublic.setQueryParameters(queryParams);
    return uriPublic;
}

std::string DocumentBroker::getDocKey(const Poco::URI& uri)
{
    // If multiple host-names are used to access us, then
    // they must be aliases. Permission to access aliased hosts
    // is checked at the point of accepting incoming connections.
    // At this point storing the hostname artificially discriminates
    // between aliases and forces same document (when opened from
    // alias hosts) to load as separate documents and sharing doesn't
    // work. Worse, saving overwrites one another.
    std::string docKey;
    Poco::URI::encode(uri.getPath(), "", docKey);
    return docKey;
}

/// The Document Broker Poll - one of these in a thread per document
class DocumentBroker::DocumentBrokerPoll final : public TerminatingPoll
{
    /// The DocumentBroker owning us.
    DocumentBroker& _docBroker;

public:
    DocumentBrokerPoll(const std::string &threadName, DocumentBroker& docBroker) :
        TerminatingPoll(threadName),
        _docBroker(docBroker)
    {
    }

    virtual void pollingThread()
    {
        // Delegate to the docBroker.
        _docBroker.pollThread();
    }
};

DocumentBroker::DocumentBroker(const std::string& uri,
                               const Poco::URI& uriPublic,
                               const std::string& docKey,
                               const std::string& childRoot) :
    _uriOrig(uri),
    _uriPublic(uriPublic),
    _docKey(docKey),
    _childRoot(childRoot),
    _cacheRoot(getCachePath(uriPublic.toString())),
    _lastSaveTime(std::chrono::steady_clock::now()),
    _markToDestroy(false),
    _lastEditableSession(false),
    _isLoaded(false),
    _isModified(false),
    _cursorPosX(0),
    _cursorPosY(0),
    _cursorWidth(0),
    _cursorHeight(0),
    _poll(new DocumentBrokerPoll("docbrk_poll", *this)),
    _tileVersion(0),
    _debugRenderedTileCount(0)
{
    assert(!_docKey.empty());
    assert(!_childRoot.empty());

    LOG_INF("DocumentBroker [" << _uriPublic.toString() << "] created. DocKey: [" << _docKey << "]");

    _stop = false;
}

// The inner heart of the DocumentBroker - our poll loop.
void DocumentBroker::pollThread()
{
    static std::atomic<unsigned> DocBrokerId(1);
    Util::setThreadName("docbroker_" + Util::encodeId(DocBrokerId++, 3));

    LOG_INF("Starting docBroker polling thread for docKey [" << _docKey << "].");

    _threadStart = std::chrono::steady_clock::now();

    // Request a kit process for this doc.
    _childProcess = getNewChild_Blocks();
    if (!_childProcess)
    {
        // Let the client know we can't serve now.
        LOG_ERR("Failed to get new child.");

        // FIXME: need to notify all clients and shut this down ...
#if 0
        const std::string msg = SERVICE_UNAVAILABLE_INTERNAL_ERROR;
        ws.sendFrame(msg);
        // abnormal close frame handshake
        ws.shutdown(WebSocketHandler::StatusCodes::ENDPOINT_GOING_AWAY);
#endif
        // FIXME: return something good down the websocket ...
        _stop = true;
        return;
    }

    _childProcess->setDocumentBroker(shared_from_this());

    auto last30SecCheckTime = std::chrono::steady_clock::now();

    // Main polling loop goodness.
    while (!_stop && !TerminationFlag && !ShutdownRequestFlag)
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_newSessions.empty())
                break;

            NewSession& newSession = _newSessions.front();
            try
            {
                addSession(newSession._session);

                // now send the queued messages
                for (std::string message : newSession._messages)
                {
                    // provide the jailed document path to the 'load' message
                    assert(!_uriJailed.empty());
                    std::vector<std::string> tokens = LOOLProtocol::tokenize(message);
                    if (tokens.size() > 1 && tokens[1] == "load")
                        message += std::string(" jail=") + _uriJailed.toString();

                    LOG_DBG("Sending a queued message: " + message);
                    _childProcess->sendTextFrame(message);
                }
            }
            catch (const std::exception& exc)
            {
                LOG_ERR("Error while adding new session to doc [" << _docKey << "]: " << exc.what());
            }

            _newSessions.pop_front();
        }

        _poll->poll(SocketPoll::DefaultPollTimeoutMs);

        if (!std::getenv("LOOL_NO_AUTOSAVE") && !_stop &&
            std::chrono::duration_cast<std::chrono::seconds>
            (std::chrono::steady_clock::now() - last30SecCheckTime).count() >= 30)
        {
            LOG_TRC("Trigger an autosave ...");
            autoSave(false);
            last30SecCheckTime = std::chrono::steady_clock::now();
        }
    }

    LOG_INF("Finished docBroker polling thread for docKey [" << _docKey << "].");
}

bool DocumentBroker::isAlive() const
{
    if (!_childProcess)
        return true; // waiting to get a child.
    if (_stop) // we're dead.
        return false;

    return _childProcess->isAlive();
}

DocumentBroker::~DocumentBroker()
{
    Admin::instance().rmDoc(_docKey);

    LOG_INF("~DocumentBroker [" << _uriPublic.toString() <<
            "] destroyed with " << _sessions.size() << " sessions left.");

    if (!_sessions.empty())
    {
        LOG_WRN("DocumentBroker still has unremoved sessions.");
    }

    // Need to first make sure the child exited, socket closed,
    // and thread finished before we are destroyed.
    _childProcess.reset();
}

bool DocumentBroker::load(std::shared_ptr<ClientSession>& session, const std::string& jailId)
{
    Util::assertIsLocked(_mutex);

    const std::string sessionId = session->getId();

    LOG_INF("Loading [" << _docKey << "] for session [" << sessionId << "] and jail [" << jailId << "].");

    {
        bool result;
        if (UnitWSD::get().filterLoad(sessionId, jailId, result))
            return result;
    }

    if (_markToDestroy)
    {
        // Tearing down.
        LOG_WRN("Will not load document marked to destroy. DocKey: [" << _docKey << "].");
        return false;
    }

    const Poco::URI& uriPublic = session->getPublicUri();
    LOG_DBG("Loading from URI: " << uriPublic.toString());

    _jailId = jailId;

    // The URL is the publicly visible one, not visible in the chroot jail.
    // We need to map it to a jailed path and copy the file there.

    // user/doc/jailId
    const auto jailPath = Poco::Path(JAILED_DOCUMENT_ROOT, jailId);
    std::string jailRoot = getJailRoot();
#ifndef KIT_IN_PROCESS
    if (LOOLWSD::NoCapsForKit)
    {
        jailRoot = jailPath.toString() + "/" + getJailRoot();
    }
#endif

    LOG_INF("jailPath: " << jailPath.toString() << ", jailRoot: " << jailRoot);

    bool firstInstance = false;
    if (_storage == nullptr)
    {
        // Pass the public URI to storage as it needs to load using the token
        // and other storage-specific data provided in the URI.
        LOG_DBG("Creating new storage instance for URI [" << uriPublic.toString() << "].");
        _storage = StorageBase::create(uriPublic, jailRoot, jailPath.toString());
        if (_storage == nullptr)
        {
            // We should get an exception, not null.
            LOG_ERR("Failed to create Storage instance for [" << _docKey << "] in " << jailPath.toString());
            return false;
        }
        firstInstance = true;
    }

    assert(_storage != nullptr);

    // Call the storage specific fileinfo functions
    std::string userid, username;
    std::chrono::duration<double> getInfoCallDuration(0);
    WopiStorage* wopiStorage = dynamic_cast<WopiStorage*>(_storage.get());
    if (wopiStorage != nullptr)
    {
        std::unique_ptr<WopiStorage::WOPIFileInfo> wopifileinfo =
                                     wopiStorage->getWOPIFileInfo(uriPublic);
        userid = wopifileinfo->_userid;
        username = wopifileinfo->_username;

        if (!wopifileinfo->_userCanWrite)
        {
            LOG_DBG("Setting the session as readonly");
            session->setReadOnly();
        }

        // Construct a JSON containing relevant WOPI host properties
        Object::Ptr wopiInfo = new Object();
        if (!wopifileinfo->_postMessageOrigin.empty())
        {
            wopiInfo->set("PostMessageOrigin", wopifileinfo->_postMessageOrigin);
        }

        // If print, export are disabled, order client to hide these options in the UI
        if (wopifileinfo->_disablePrint)
            wopifileinfo->_hidePrintOption = true;
        if (wopifileinfo->_disableExport)
            wopifileinfo->_hideExportOption = true;

        wopiInfo->set("HidePrintOption", wopifileinfo->_hidePrintOption);
        wopiInfo->set("HideSaveOption", wopifileinfo->_hideSaveOption);
        wopiInfo->set("HideExportOption", wopifileinfo->_hideExportOption);
        wopiInfo->set("DisablePrint", wopifileinfo->_disablePrint);
        wopiInfo->set("DisableExport", wopifileinfo->_disableExport);
        wopiInfo->set("DisableCopy", wopifileinfo->_disableCopy);

        std::ostringstream ossWopiInfo;
        wopiInfo->stringify(ossWopiInfo);
        session->sendTextFrame("wopi: " + ossWopiInfo.str());

        // Mark the session as 'Document owner' if WOPI hosts supports it
        if (userid == _storage->getFileInfo()._ownerId)
        {
            LOG_DBG("Session [" + sessionId + "] is the document owner");
            session->setDocumentOwner(true);
        }

        getInfoCallDuration = wopifileinfo->_callDuration;

        // Pass the ownership to client session
        session->setWopiFileInfo(wopifileinfo);
    }
    else
    {
        LocalStorage* localStorage = dynamic_cast<LocalStorage*>(_storage.get());
        if (localStorage != nullptr)
        {
            std::unique_ptr<LocalStorage::LocalFileInfo> localfileinfo =
                                          localStorage->getLocalFileInfo(uriPublic);
            userid = localfileinfo->_userid;
            username = localfileinfo->_username;
        }
    }

    LOG_DBG("Setting username [" << username << "] and userId [" << userid << "] for session [" << sessionId << "]");
    session->setUserId(userid);
    session->setUserName(username);

    // Basic file information was stored by the above getWOPIFileInfo() or getLocalFileInfo() calls
    const auto fileInfo = _storage->getFileInfo();
    if (!fileInfo.isValid())
    {
        LOG_ERR("Invalid fileinfo for URI [" << uriPublic.toString() << "].");
        return false;
    }

    if (firstInstance)
    {
        _documentLastModifiedTime = fileInfo._modifiedTime;
        LOG_DBG("Document timestamp: " << Poco::DateTimeFormatter::format(Poco::DateTime(_documentLastModifiedTime),
                                                                          Poco::DateTimeFormat::ISO8601_FORMAT));
    }
    else
    {
        // Check if document has been modified by some external action
        LOG_DBG("Timestamp now: " << Poco::DateTimeFormatter::format(Poco::DateTime(fileInfo._modifiedTime),
                                                                     Poco::DateTimeFormat::ISO8601_FORMAT));
        if (_documentLastModifiedTime != Poco::Timestamp::fromEpochTime(0) &&
            fileInfo._modifiedTime != Poco::Timestamp::fromEpochTime(0) &&
            _documentLastModifiedTime != fileInfo._modifiedTime)
        {
            LOG_ERR("Document has been modified behind our back, URI [" << uriPublic.toString() << "].");
            // What do do?
        }
    }

    // Let's load the document now, if not loaded.
    if (!_storage->isLoaded())
    {
        const auto localPath = _storage->loadStorageFileToLocal();

        std::ifstream istr(localPath, std::ios::binary);
        Poco::SHA1Engine sha1;
        Poco::DigestOutputStream dos(sha1);
        Poco::StreamCopier::copyStream(istr, dos);
        dos.close();
        LOG_INF("SHA1 for DocKey [" << _docKey << "] of [" << localPath << "]: " <<
                Poco::DigestEngine::digestToHex(sha1.digest()));

        _uriJailed = Poco::URI(Poco::URI("file://"), localPath);
        _filename = fileInfo._filename;

        // Use the local temp file's timestamp.
        _lastFileModifiedTime = Poco::File(_storage->getLocalRootPath()).getLastModified();
        _tileCache.reset(new TileCache(uriPublic.toString(), _lastFileModifiedTime, _cacheRoot));
    }

    LOOLWSD::dumpNewSessionTrace(getJailId(), sessionId, _uriOrig, _storage->getRootFilePath());

    // Since document has been loaded, send the stats if its WOPI
    if (wopiStorage != nullptr)
    {
        // Get the time taken to load the file from storage
        auto callDuration = wopiStorage->getWopiLoadDuration();
        // Add the time taken to check file info
        callDuration += getInfoCallDuration;
        const std::string msg = "stats: wopiloadduration " + std::to_string(callDuration.count());
        LOG_TRC("Sending to Client [" << msg << "].");
        session->sendTextFrame(msg);
    }

    return true;
}

bool DocumentBroker::saveToStorage(const std::string& sessionId,
                                   bool success, const std::string& result)
{
    const bool res = saveToStorageInternal(sessionId, success, result);

    // If marked to destroy, then this was the last session.
    // FIXME: If during that last save another client connects
    // to this doc, the _markToDestroy will be reset and we
    // will leak the last session. Need to mark the session as
    // dead and cleanup somehow.
    if (_markToDestroy)
    {
        // We've saved and can safely destroy.
        removeSessionInternal(sessionId);

        // Stop so we get cleaned up and removed.
        _stop = true;
    }

    return res;
}

bool DocumentBroker::saveToStorageInternal(const std::string& sessionId,
                                           bool success, const std::string& result)
{
    assert(_poll->isCorrectThread());

    // If save requested, but core didn't save because document was unmodified
    // notify the waiting thread, if any.
    if (!success && result == "unmodified")
    {
        LOG_DBG("Save skipped as document [" << _docKey << "] was not modified.");
        _lastSaveTime = std::chrono::steady_clock::now();
        _poll->wakeup();
        return true;
    }

    const auto it = _sessions.find(sessionId);
    if (it == _sessions.end())
    {
        LOG_ERR("Session with sessionId [" << sessionId << "] not found while saving docKey [" << _docKey << "].");
        return false;
    }

    const Poco::URI& uriPublic = it->second->getPublicUri();
    const auto uri = uriPublic.toString();

    // If we aren't destroying the last editable session just yet,
    // and the file timestamp hasn't changed, skip saving.
    const auto newFileModifiedTime = Poco::File(_storage->getLocalRootPath()).getLastModified();
    if (!_lastEditableSession && newFileModifiedTime == _lastFileModifiedTime)
    {
        // Nothing to do.
        LOG_DBG("Skipping unnecessary saving to URI [" << uri << "] with docKey [" << _docKey <<
                "]. File last modified " << _lastFileModifiedTime.elapsed() / 1000000 << " seconds ago.");
        _lastSaveTime = std::chrono::steady_clock::now();
        _poll->wakeup();
        return true;
    }

    LOG_DBG("Persisting [" << _docKey << "] after saving to URI [" << uri << "].");

    // FIXME: We should check before persisting the document that it hasn't been updated in its
    // storage behind our backs.

    assert(_storage && _tileCache);
    StorageBase::SaveResult storageSaveResult = _storage->saveLocalFileToStorage(uriPublic);
    if (storageSaveResult == StorageBase::SaveResult::OK)
    {
        _isModified = false;
        _tileCache->setUnsavedChanges(false);
        _lastFileModifiedTime = newFileModifiedTime;
        _tileCache->saveLastModified(_lastFileModifiedTime);
        _lastSaveTime = std::chrono::steady_clock::now();
        _poll->wakeup();

        // Calling getWOPIFileInfo() or getLocalFileInfo() has the side-effect of updating
        // StorageBase::_fileInfo. Get the timestamp of the document as persisted in its storage
        // from there.
        // FIXME: Yes, of course we should turn this stuff into a virtual function and avoid this
        // dynamic_cast dance.
        if (dynamic_cast<WopiStorage*>(_storage.get()) != nullptr)
        {
            auto wopiFileInfo = static_cast<WopiStorage*>(_storage.get())->getWOPIFileInfo(uriPublic);
        }
        else if (dynamic_cast<LocalStorage*>(_storage.get()) != nullptr)
        {
            auto localFileInfo = static_cast<LocalStorage*>(_storage.get())->getLocalFileInfo(uriPublic);
        }
        // So set _documentLastModifiedTime then
        _documentLastModifiedTime = _storage->getFileInfo()._modifiedTime;

        LOG_DBG("Saved docKey [" << _docKey << "] to URI [" << uri << "] and updated tile cache. Document modified timestamp: " <<
                Poco::DateTimeFormatter::format(Poco::DateTime(_documentLastModifiedTime),
                                                               Poco::DateTimeFormat::ISO8601_FORMAT));
        return true;
    }
    else if (storageSaveResult == StorageBase::SaveResult::DISKFULL)
    {
        LOG_WRN("Disk full while saving docKey [" << _docKey << "] to URI [" << uri <<
                "]. Making all sessions on doc read-only and notifying clients.");

        // Make everyone readonly and tell everyone that storage is low on diskspace.
        for (const auto& sessionIt : _sessions)
        {
            sessionIt.second->setReadOnly();
            sessionIt.second->sendTextFrame("error: cmd=storage kind=savediskfull");
        }
    }
    else if (storageSaveResult == StorageBase::SaveResult::FAILED)
    {
        //TODO: Should we notify all clients?
        LOG_ERR("Failed to save docKey [" << _docKey << "] to URI [" << uri << "]. Notifying client.");
        it->second->sendTextFrame("error: cmd=storage kind=savefailed");
    }

    return false;
}

void DocumentBroker::setLoaded()
{
    _isLoaded = true;
    _loadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - _threadStart);
    LOG_TRC("Document loaded in " << _loadDuration.count() << "ms");
}

bool DocumentBroker::autoSave(const bool force)
{
    if (_sessions.empty() || _storage == nullptr || !_isLoaded ||
        !_childProcess->isAlive() || (!_isModified && !force))
    {
        // Nothing to do.
        LOG_TRC("Nothing to autosave [" << _docKey << "].");
        return false;
    }

    // Remember the last save time, since this is the predicate.
    LOG_TRC("Checking to autosave [" << _docKey << "].");

    bool sent = false;
    if (force)
    {
        LOG_TRC("Sending forced save command for [" << _docKey << "].");
        sent = sendUnoSave(true);
    }
    else if (_isModified)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto inactivityTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastActivityTime).count();
        const auto timeSinceLastSaveMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastSaveTime).count();
        LOG_TRC("Time since last save of docKey [" << _docKey << "] is " << timeSinceLastSaveMs <<
                "ms and most recent activity was " << inactivityTimeMs << "ms ago.");

        // Either we've been idle long enough, or it's auto-save time.
        if (inactivityTimeMs >= IdleSaveDurationMs ||
            timeSinceLastSaveMs >= AutoSaveDurationMs)
        {
            LOG_TRC("Sending timed save command for [" << _docKey << "].");
            sent = sendUnoSave(true);
        }
    }

    return sent;
}

bool DocumentBroker::sendUnoSave(const bool dontSaveIfUnmodified)
{
    LOG_INF("Autosave triggered for doc [" << _docKey << "].");

    std::shared_ptr<ClientSession> savingSession;
    for (auto& sessionIt : _sessions)
    {
        // Save the document using first session available ...
        if (!savingSession)
        {
            savingSession = sessionIt.second;
        }

        // or if any of the sessions is document owner, use that.
        if (sessionIt.second->isDocumentOwner())
        {
            savingSession = sessionIt.second;
            break;
        }
    }

    if (savingSession)
    {
        // Invalidate the timestamp to force persisting.
        _lastFileModifiedTime = Poco::Timestamp::fromEpochTime(0);

        // We do not want save to terminate editing mode if we are in edit mode now

        std::ostringstream oss;
        // arguments init
        oss << "{";

        // Mention DontTerminateEdit always
        oss << "\"DontTerminateEdit\":"
            << "{"
            << "\"type\":\"boolean\","
            << "\"value\":true"
            << "}";

        // Mention DontSaveIfUnmodified
        if (dontSaveIfUnmodified)
        {
            oss << ","
                << "\"DontSaveIfUnmodified\":"
                << "{"
                << "\"type\":\"boolean\","
                << "\"value\":true"
                << "}";
        }

        // arguments end
        oss << "}";

        const auto saveArgs = oss.str();
        LOG_TRC(".uno:Save arguments: " << saveArgs);
        const auto command = "uno .uno:Save " + saveArgs;
        forwardToChild(savingSession->getId(), command);
        return true;
    }

    LOG_ERR("Failed to auto-save doc [" << _docKey << "]: No valid sessions.");
    return false;
}

std::string DocumentBroker::getJailRoot() const
{
    assert(!_jailId.empty());
    return Poco::Path(_childRoot, _jailId).toString();
}

size_t DocumentBroker::queueSession(std::shared_ptr<ClientSession>& session)
{
    Util::assertIsLocked(_mutex);

    _newSessions.push_back(NewSession(session));
    _poll->wakeup();

    return _sessions.size() + _newSessions.size();
}

size_t DocumentBroker::addSession(std::shared_ptr<ClientSession>& session)
{
    Util::assertIsLocked(_mutex);

    try
    {
        // First load the document, since this can fail.
        if (!load(session, std::to_string(_childProcess->getPid())))
        {
            const auto msg = "Failed to load document with URI [" + session->getPublicUri().toString() + "].";
            LOG_ERR(msg);
            throw std::runtime_error(msg);
        }
    }
    catch (const StorageSpaceLowException&)
    {
        LOG_ERR("Out of storage while loading document with URI [" << session->getPublicUri().toString() << "].");

        // We use the same message as is sent when some of lool's own locations are full,
        // even if in this case it might be a totally different location (file system, or
        // some other type of storage somewhere). This message is not sent to all clients,
        // though, just to all sessions of this document.
        alertAllUsers("internal", "diskfull");
        throw;
    }

    // Below values are recalculated when startDestroy() is called (before destroying the
    // document). It is safe to reset their values to their defaults whenever a new session is added.
    _lastEditableSession = false;
    _markToDestroy = false;
    _stop = false;

    const auto id = session->getId();
    if (!_sessions.emplace(id, session).second)
    {
        LOG_WRN("DocumentBroker: Trying to add already existing session.");
    }

    const auto count = _sessions.size();

    // Request a new session from the child kit.
    const std::string aMessage = "session " + id + ' ' + _docKey;
    _childProcess->sendTextFrame(aMessage);

    // Tell the admin console about this new doc
    Admin::instance().addDoc(_docKey, getPid(), getFilename(), id);

    LOG_TRC("Added " << (session->isReadOnly() ? "readonly" : "non-readonly") <<
            " session [" << id << "] to docKey [" <<
            _docKey << "] to have " << count << " sessions.");

    return count;
}

size_t DocumentBroker::removeSession(const std::string& id, bool destroyIfLast)
{
    auto guard = getLock();

    if (destroyIfLast)
        destroyIfLastEditor(id);

    try
    {
        LOG_INF("Removing session [" << id << "] on docKey [" << _docKey <<
                "]. Have " << _sessions.size() << " sessions.");

        if (!_lastEditableSession || !autoSave(true))
            return removeSessionInternal(id);
    }
    catch (const std::exception& ex)
    {
        LOG_ERR("Error while removing session [" << id << "]: " << ex.what());
    }

    return _sessions.size();
}

size_t DocumentBroker::removeSessionInternal(const std::string& id)
{
    try
    {
        // remove also from the _newSessions
        _newSessions.erase(std::remove_if(_newSessions.begin(), _newSessions.end(),
                                          [&id](NewSession& newSession) { return newSession._session->getId() == id; }),
                           _newSessions.end());

        Admin::instance().rmDoc(_docKey, id);

        auto it = _sessions.find(id);
        if (it != _sessions.end())
        {
            LOOLWSD::dumpEndSessionTrace(getJailId(), id, _uriOrig);

            const auto readonly = (it->second ? it->second->isReadOnly() : false);
            _sessions.erase(it);

            const auto count = _sessions.size();
            LOG_TRC("Removed " << (readonly ? "readonly" : "non-readonly") <<
                    " session [" << id << "] from docKey [" <<
                    _docKey << "] to have " << count << " sessions.");

            // Let the child know the client has disconnected.
            const std::string msg("child-" + id + " disconnect");
            _childProcess->sendTextFrame(msg);

            return count;
        }
        else
        {
            LOG_TRC("Session [" << id << "] not found to remove from docKey [" <<
                    _docKey << "]. Have " << _sessions.size() << " sessions.");
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERR("Error while removing session [" << id << "]: " << ex.what());
    }

    return _sessions.size();
}

void DocumentBroker::addSocketToPoll(const std::shared_ptr<Socket>& socket)
{
    _poll->insertNewSocket(socket);
}

void DocumentBroker::alertAllUsers(const std::string& msg)
{
    Util::assertIsLocked(_mutex);

    auto payload = std::make_shared<Message>(msg, Message::Dir::Out);

    LOG_DBG("Alerting all users of [" << _docKey << "]: " << msg);
    for (auto& it : _sessions)
    {
        it.second->enqueueSendMessage(payload);
    }
}

/// Handles input from the prisoner / child kit process
bool DocumentBroker::handleInput(const std::vector<char>& payload)
{
    auto message = std::make_shared<Message>(payload.data(), payload.size(), Message::Dir::Out);
    const auto& msg = message->abbr();
    LOG_TRC("DocumentBroker handling child message: [" << msg << "].");

    LOOLWSD::dumpOutgoingTrace(getJailId(), "0", msg);

    if (LOOLProtocol::getFirstToken(message->forwardToken(), '-') == "client")
    {
        forwardToClient(message);
    }
    else
    {
        const auto& command = message->firstToken();
        if (command == "tile:")
        {
            handleTileResponse(payload);
        }
        else if (command == "tilecombine:")
        {
            handleTileCombinedResponse(payload);
        }
        else if (command == "errortoall:")
        {
            LOG_CHECK_RET(message->tokens().size() == 3, false);
            std::string cmd, kind;
            LOOLProtocol::getTokenString((*message)[1], "cmd", cmd);
            LOG_CHECK_RET(cmd != "", false);
            LOOLProtocol::getTokenString((*message)[2], "kind", kind);
            LOG_CHECK_RET(kind != "", false);
            Util::alertAllUsers(cmd, kind);
        }
        else if (command == "procmemstats:")
        {
            int dirty;
            if (message->getTokenInteger("dirty", dirty))
            {
                Admin::instance().updateMemoryDirty(_docKey, dirty);
            }
        }
        else
        {
            LOG_ERR("Unexpected message: [" << msg << "].");
            return false;
        }
    }

    return true;
}

void DocumentBroker::invalidateTiles(const std::string& tiles)
{
    // Remove from cache.
    _tileCache->invalidateTiles(tiles);
}

void DocumentBroker::handleTileRequest(TileDesc& tile,
                                       const std::shared_ptr<ClientSession>& session)
{
    std::unique_lock<std::mutex> lock(_mutex);

    tile.setVersion(++_tileVersion);
    const auto tileMsg = tile.serialize();
    LOG_TRC("Tile request for " << tileMsg);

    std::unique_ptr<std::fstream> cachedTile = _tileCache->lookupTile(tile);
    if (cachedTile)
    {
#if ENABLE_DEBUG
        const std::string response = tile.serialize("tile:") + " renderid=cached\n";
#else
        const std::string response = tile.serialize("tile:") + '\n';
#endif

        std::vector<char> output;
        output.reserve(static_cast<size_t>(4) * tile.getWidth() * tile.getHeight());
        output.resize(response.size());
        std::memcpy(output.data(), response.data(), response.size());

        assert(cachedTile->is_open());
        cachedTile->seekg(0, std::ios_base::end);
        const auto pos = output.size();
        std::streamsize size = cachedTile->tellg();
        output.resize(pos + size);
        cachedTile->seekg(0, std::ios_base::beg);
        cachedTile->read(output.data() + pos, size);
        cachedTile->close();

        session->sendBinaryFrame(output.data(), output.size());
        return;
    }

    if (tile.getBroadcast())
    {
        for (auto& it: _sessions)
        {
            tileCache().subscribeToTileRendering(tile, it.second);
        }
    }
    else
    {
        tileCache().subscribeToTileRendering(tile, session);
    }

    // Forward to child to render.
    LOG_DBG("Sending render request for tile (" << tile.getPart() << ',' <<
            tile.getTilePosX() << ',' << tile.getTilePosY() << ").");
    const std::string request = "tile " + tileMsg;
    _childProcess->sendTextFrame(request);
    _debugRenderedTileCount++;
}

void DocumentBroker::handleTileCombinedRequest(TileCombined& tileCombined,
                                               const std::shared_ptr<ClientSession>& session)
{
    std::unique_lock<std::mutex> lock(_mutex);

    LOG_TRC("TileCombined request for " << tileCombined.serialize());

    // Satisfy as many tiles from the cache.
    std::vector<TileDesc> tiles;
    for (auto& tile : tileCombined.getTiles())
    {
        std::unique_ptr<std::fstream> cachedTile = _tileCache->lookupTile(tile);
        if (cachedTile)
        {
            //TODO: Combine the response to reduce latency.
#if ENABLE_DEBUG
            const std::string response = tile.serialize("tile:") + " renderid=cached\n";
#else
            const std::string response = tile.serialize("tile:") + "\n";
#endif

            std::vector<char> output;
            output.reserve(static_cast<size_t>(4) * tile.getWidth() * tile.getHeight());
            output.resize(response.size());
            std::memcpy(output.data(), response.data(), response.size());

            assert(cachedTile->is_open());
            cachedTile->seekg(0, std::ios_base::end);
            const auto pos = output.size();
            std::streamsize size = cachedTile->tellg();
            output.resize(pos + size);
            cachedTile->seekg(0, std::ios_base::beg);
            cachedTile->read(output.data() + pos, size);
            cachedTile->close();

            session->sendBinaryFrame(output.data(), output.size());
        }
        else
        {
            // Not cached, needs rendering.
            tile.setVersion(++_tileVersion);
            tileCache().subscribeToTileRendering(tile, session);
            tiles.push_back(tile);
            _debugRenderedTileCount++;
        }
    }

    if (!tiles.empty())
    {
        auto newTileCombined = TileCombined::create(tiles);

        // Forward to child to render.
        const auto req = newTileCombined.serialize("tilecombine");
        LOG_DBG("Sending residual tilecombine: " << req);
        _childProcess->sendTextFrame(req);
    }
}

void DocumentBroker::cancelTileRequests(const std::shared_ptr<ClientSession>& session)
{
    std::unique_lock<std::mutex> lock(_mutex);

    const auto canceltiles = tileCache().cancelTiles(session);
    if (!canceltiles.empty())
    {
        LOG_DBG("Forwarding canceltiles request: " << canceltiles);
        _childProcess->sendTextFrame(canceltiles);
    }
}

void DocumentBroker::handleTileResponse(const std::vector<char>& payload)
{
    const std::string firstLine = getFirstLine(payload);
    LOG_DBG("Handling tile: " << firstLine);

    try
    {
        const auto length = payload.size();
        if (firstLine.size() < static_cast<std::string::size_type>(length) - 1)
        {
            const auto tile = TileDesc::parse(firstLine);
            const auto buffer = payload.data();
            const auto offset = firstLine.size() + 1;

            std::unique_lock<std::mutex> lock(_mutex);

            tileCache().saveTileAndNotify(tile, buffer + offset, length - offset);
        }
        else
        {
            LOG_DBG("Render request declined for " << firstLine);
            // They will get re-issued if we don't forget them.
        }
    }
    catch (const std::exception& exc)
    {
        LOG_ERR("Failed to process tile response [" << firstLine << "]: " << exc.what() << ".");
    }
}

void DocumentBroker::handleTileCombinedResponse(const std::vector<char>& payload)
{
    const std::string firstLine = getFirstLine(payload);
    LOG_DBG("Handling tile combined: " << firstLine);

    try
    {
        const auto length = payload.size();
        if (firstLine.size() < static_cast<std::string::size_type>(length) - 1)
        {
            const auto tileCombined = TileCombined::parse(firstLine);
            const auto buffer = payload.data();
            auto offset = firstLine.size() + 1;

            std::unique_lock<std::mutex> lock(_mutex);

            for (const auto& tile : tileCombined.getTiles())
            {
                tileCache().saveTileAndNotify(tile, buffer + offset, tile.getImgSize());
                offset += tile.getImgSize();
            }
        }
        else
        {
            LOG_ERR("Render request declined for " << firstLine);
            // They will get re-issued if we don't forget them.
        }
    }
    catch (const std::exception& exc)
    {
        LOG_ERR("Failed to process tile response [" << firstLine << "]: " << exc.what() << ".");
    }
}

void DocumentBroker::destroyIfLastEditor(const std::string& id)
{
    Util::assertIsLocked(_mutex);

    const auto currentSession = _sessions.find(id);
    if (currentSession == _sessions.end())
    {
        // We could be called before adding any sessions.
        // For example when a socket disconnects before loading.
        return;
    }

    // Check if the session being destroyed is the last non-readonly session or not.
    _lastEditableSession = !currentSession->second->isReadOnly();
    if (_lastEditableSession && !_sessions.empty())
    {
        for (const auto& it : _sessions)
        {
            if (it.second->getId() != id &&
                !it.second->isReadOnly())
            {
                // Found another editable.
                _lastEditableSession = false;
                break;
            }
        }
    }

    // Last view going away, can destroy.
    _markToDestroy = (_sessions.size() <= 1);
    LOG_DBG("startDestroy on session [" << id << "] on docKey [" << _docKey <<
            "], markToDestroy: " << _markToDestroy << ", lastEditableSession: " << _lastEditableSession);
}

void DocumentBroker::setModified(const bool value)
{
    _tileCache->setUnsavedChanges(value);
    _isModified = value;
}

bool DocumentBroker::forwardToChild(const std::string& viewId, const std::string& message)
{
    LOG_TRC("Forwarding payload to child [" << viewId << "]: " << message);

    std::string msg = "child-" + viewId + ' ' + message;

    const auto it = _sessions.find(viewId);
    if (it != _sessions.end())
    {
        assert(!_uriJailed.empty());
        if (LOOLProtocol::getFirstToken(message) == "load")
            msg += " jail=" + _uriJailed.toString();

        _childProcess->sendTextFrame(msg);
        return true;
    }
    else
    {
        // try the not yet created sessions
        const auto n = std::find_if(_newSessions.begin(), _newSessions.end(), [&viewId](NewSession& newSession) { return newSession._session->getId() == viewId; });
        if (n != _newSessions.end())
            n->_messages.push_back(msg);
        else
            LOG_WRN("Child session [" << viewId << "] not found to forward message: " << message);
    }

    return false;
}

bool DocumentBroker::forwardToClient(const std::shared_ptr<Message>& payload)
{
    const std::string& msg = payload->abbr();
    const std::string& prefix = payload->forwardToken();
    LOG_TRC("Forwarding payload to [" << prefix << "]: " << msg);

    std::string name;
    std::string sid;
    if (LOOLProtocol::parseNameValuePair(payload->forwardToken(), name, sid, '-') && name == "client")
    {
        const auto& data = payload->data().data();
        const auto& size = payload->size();

        std::unique_lock<std::mutex> lock(_mutex);

        if (sid == "all")
        {
            // Broadcast to all.
            for (const auto& pair : _sessions)
            {
#if 0 // loolnb
                if (!pair.second->isHeadless() && !pair.second->isCloseFrame())
#endif
                if (!pair.second->isHeadless())
                {
                    pair.second->handleKitToClientMessage(data, size);
                }
            }
        }
        else
        {
            const auto it = _sessions.find(sid);
            if (it != _sessions.end())
            {
                return it->second->handleKitToClientMessage(data, size);
            }
            else
            {
                LOG_WRN("Client session [" << sid << "] not found to forward message: " << msg);
            }
        }
    }
    else
    {
        LOG_ERR("Unexpected prefix of forward-to-client message: " << prefix);
    }

    return false;
}

void DocumentBroker::childSocketTerminated()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_childProcess->isAlive())
    {
        LOG_ERR("Child for doc [" << _docKey << "] terminated prematurely.");
    }

    // We could restore the kit if this was unexpected.
    // For now, close the connections to cleanup.
    for (auto& pair : _sessions)
    {
        try
        {
            pair.second->shutdown(WebSocketHandler::StatusCodes::ENDPOINT_GOING_AWAY, "");
        }
        catch (const std::exception& ex)
        {
            LOG_ERR("Error while terminating client connection [" << pair.first << "]: " << ex.what());
        }
    }
}

void DocumentBroker::terminateChild(std::unique_lock<std::mutex>& lock, const std::string& closeReason)
{
    Util::assertIsLocked(_mutex);
    Util::assertIsLocked(lock);

    LOG_INF("Terminating doc [" << _docKey << "].");

    // Close all running sessions
    for (const auto& pair : _sessions)
    {
        try
        {
            pair.second->shutdown(WebSocketHandler::StatusCodes::ENDPOINT_GOING_AWAY, closeReason);
        }
        catch (const std::exception& ex)
        {
            LOG_ERR("Error while terminating client connection [" << pair.first << "]: " << ex.what());
        }
    }

    if (_childProcess)
    {
        LOG_INF("Terminating child [" << getPid() << "] of doc [" << _docKey << "].");

        // First flag to stop as it might be waiting on our lock
        // to process some incoming message.
        _childProcess->stop();

        // Release the lock and wait for the thread to finish.
        lock.unlock();

        _childProcess->close(false);
    }
}

void DocumentBroker::closeDocument(const std::string& reason)
{
    auto lock = getLock();

    LOG_DBG("Closing DocumentBroker for docKey [" << _docKey << "] with reason: " << reason);
    terminateChild(lock, reason);
}

void DocumentBroker::updateLastActivityTime()
{
    _lastActivityTime = std::chrono::steady_clock::now();
    Admin::instance().updateLastActivityTime(_docKey);
}

void DocumentBroker::dumpState(std::ostream& os)
{
    std::unique_lock<std::mutex> lock(_mutex);

    os << " Broker: " << _filename;
    if (_markToDestroy)
        os << " *** Marked to destroy ***";
    else
        os << " has live sessions";
    if (_isLoaded)
        os << "\n  loaded in: " << _loadDuration.count() << "ms";
    else
        os << "\n  still loading...";
    os << "\n  modified?: " << _isModified;
    os << "\n  jail id: " << _jailId;
    os << "\n  public uri: " << _uriPublic.toString();
    os << "\n  jailed uri: " << _uriJailed.toString();
    os << "\n  doc key: " << _docKey;
    os << "\n  num sessions: " << getSessionsCount();
    os << "\n  new sessions: " << _newSessions.size();
    os << "\n  last editable?: " << _lastEditableSession;
    os << "\n  cursor " << _cursorPosX << ", " << _cursorPosY
      << "( " << _cursorWidth << "," << _cursorHeight << ")\n";

    _poll->dumpState(os);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
