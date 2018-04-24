/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include "ClientSession.hpp"

#include <fstream>

#include <Poco/Net/HTTPResponse.h>
#include <Poco/StringTokenizer.h>
#include <Poco/URI.h>

#include "DocumentBroker.hpp"
#include "LOOLWSD.hpp"
#include "Storage.hpp"
#include "common/Common.hpp"
#include "common/Log.hpp"
#include "common/Protocol.hpp"
#include "common/Session.hpp"
#include "common/Unit.hpp"
#include "common/Util.hpp"

using namespace LOOLProtocol;

using Poco::Path;
using Poco::StringTokenizer;

ClientSession::ClientSession(const std::string& id,
                             const std::shared_ptr<DocumentBroker>& docBroker,
                             const Poco::URI& uriPublic,
                             const bool readOnly) :
    Session("ToClient-" + id, id, readOnly),
    _docBroker(docBroker),
    _uriPublic(uriPublic),
    _isDocumentOwner(false),
    _isAttached(false),
    _isViewLoaded(false),
    _keyEvents(1)
{
    const size_t curConnections = ++LOOLWSD::NumConnections;
    LOG_INF("ClientSession ctor [" << getName() << "], current number of connections: " << curConnections);
}

ClientSession::~ClientSession()
{
    const size_t curConnections = --LOOLWSD::NumConnections;
    LOG_INF("~ClientSession dtor [" << getName() << "], current number of connections: " << curConnections);
}

void ClientSession::handleIncomingMessage(SocketDisposition &disposition)
{
    if (UnitWSD::get().filterHandleRequest(
            UnitWSD::TestRequest::Client, disposition, *this))
        return;

    Session::handleIncomingMessage(disposition);
}

bool ClientSession::_handleInput(const char *buffer, int length)
{
    LOG_TRC(getName() << ": handling incoming [" << getAbbreviatedMessage(buffer, length) << "].");
    const std::string firstLine = getFirstLine(buffer, length);
    const std::vector<std::string> tokens = LOOLProtocol::tokenize(firstLine.data(), firstLine.size());

    auto docBroker = getDocumentBroker();
    if (!docBroker)
    {
        LOG_ERR("No DocBroker found. Terminating session " << getName());
        return false;
    }

    LOOLWSD::dumpIncomingTrace(docBroker->getJailId(), getId(), firstLine);

    if (LOOLProtocol::tokenIndicatesUserInteraction(tokens[0]))
    {
        // Keep track of timestamps of incoming client messages that indicate user activity.
        updateLastActivityTime();
        docBroker->updateLastActivityTime();
    }
    if (tokens[0] == "loolclient")
    {
        if (tokens.size() < 1)
        {
            sendTextFrame("error: cmd=loolclient kind=badprotocolversion");
            return false;
        }

        const auto versionTuple = ParseVersion(tokens[1]);
        if (std::get<0>(versionTuple) != ProtocolMajorVersionNumber ||
            std::get<1>(versionTuple) != ProtocolMinorVersionNumber)
        {
            sendTextFrame("error: cmd=loolclient kind=badprotocolversion");
            return false;
        }

        // Send LOOL version information
        std::string version, hash;
        Util::getVersionInfo(version, hash);
        std::string versionStr =
            "{ \"Version\":  \"" + version + "\", " +
            "\"Hash\":     \"" + hash + "\", " +
            "\"Protocol\": \"" + GetProtocolVersion() + "\" }";
        sendTextFrame("loolserver " + versionStr);
        // Send LOKit version information
        sendTextFrame("lokitversion " + LOOLWSD::LOKitVersion);

        return true;
    }

    if (tokens[0] == "load")
    {
        if (_docURL != "")
        {
            sendTextFrame("error: cmd=load kind=docalreadyloaded");
            return false;
        }

        return loadDocument(buffer, length, tokens, docBroker);
    }
    else if (tokens[0] != "canceltiles" &&
             tokens[0] != "clientzoom" &&
             tokens[0] != "clientvisiblearea" &&
             tokens[0] != "outlinestate" &&
             tokens[0] != "commandvalues" &&
             tokens[0] != "closedocument" &&
             tokens[0] != "versionrestore" &&
             tokens[0] != "downloadas" &&
             tokens[0] != "getchildid" &&
             tokens[0] != "gettextselection" &&
             tokens[0] != "paste" &&
             tokens[0] != "insertfile" &&
             tokens[0] != "key" &&
             tokens[0] != "textinput" &&
             tokens[0] != "windowkey" &&
             tokens[0] != "mouse" &&
             tokens[0] != "windowmouse" &&
             tokens[0] != "partpagerectangles" &&
             tokens[0] != "ping" &&
             tokens[0] != "renderfont" &&
             tokens[0] != "requestloksession" &&
             tokens[0] != "resetselection" &&
             tokens[0] != "save" &&
             tokens[0] != "saveas" &&
             tokens[0] != "savetostorage" &&
             tokens[0] != "selectgraphic" &&
             tokens[0] != "selecttext" &&
             tokens[0] != "setclientpart" &&
             tokens[0] != "setpage" &&
             tokens[0] != "status" &&
             tokens[0] != "tile" &&
             tokens[0] != "tilecombine" &&
             tokens[0] != "uno" &&
             tokens[0] != "useractive" &&
             tokens[0] != "userinactive" &&
             tokens[0] != "paintwindow" &&
             tokens[0] != "windowcommand")
    {
        sendTextFrame("error: cmd=" + tokens[0] + " kind=unknown");
        return false;
    }
    else if (_docURL == "")
    {
        sendTextFrame("error: cmd=" + tokens[0] + " kind=nodocloaded");
        return false;
    }
    else if (tokens[0] == "canceltiles")
    {
        docBroker->cancelTileRequests(shared_from_this());
        return true;
    }
    else if (tokens[0] == "commandvalues")
    {
        return getCommandValues(buffer, length, tokens, docBroker);
    }
    else if (tokens[0] == "closedocument")
    {
        // If this session is the owner of the file & 'EnableOwnerTermination' feature
        // is turned on by WOPI, let it close all sessions
        if (_isDocumentOwner && _wopiFileInfo && _wopiFileInfo->_enableOwnerTermination)
        {
            LOG_DBG("Session [" << getId() << "] requested owner termination");
            docBroker->closeDocument("ownertermination");
        }
        else if (docBroker->isDocumentChangedInStorage())
        {
            LOG_DBG("Document marked as changed in storage and user ["
                    << getUserId() << ", " << getUserName()
                    << "] wants to refresh the document for all.");
            docBroker->stop("documentconflict " + getUserName());
        }

        return true;
    }
    else if (tokens[0] == "versionrestore") {
        if (tokens[1] == "prerestore") {
            // green signal to WOPI host to restore the version *after* saving
            // any unsaved changes, if any, to the storage
            docBroker->closeDocument("versionrestore: prerestore_ack");
        }
    }
    else if (tokens[0] == "partpagerectangles")
    {
        // We don't support partpagerectangles any more, will be removed in the
        // next version
        sendTextFrame("partpagerectangles: ");
        return true;
    }
    else if (tokens[0] == "ping")
    {
        std::string count = std::to_string(docBroker->getRenderedTileCount());
        sendTextFrame("pong rendercount=" + count);
        return true;
    }
    else if (tokens[0] == "renderfont")
    {
        return sendFontRendering(buffer, length, tokens, docBroker);
    }
    else if (tokens[0] == "status")
    {
        assert(firstLine.size() == static_cast<size_t>(length));
        return forwardToChild(firstLine, docBroker);
    }
    else if (tokens[0] == "tile")
    {
        return sendTile(buffer, length, tokens, docBroker);
    }
    else if (tokens[0] == "tilecombine")
    {
        return sendCombinedTiles(buffer, length, tokens, docBroker);
    }
    else if (tokens[0] == "save")
    {
        int dontTerminateEdit = 1;
        if (tokens.size() > 1)
            getTokenInteger(tokens[1], "dontTerminateEdit", dontTerminateEdit);

        // Don't save unmodified docs by default, or when read-only.
        int dontSaveIfUnmodified = 1;
        if (!isReadOnly() && tokens.size() > 2)
            getTokenInteger(tokens[2], "dontSaveIfUnmodified", dontSaveIfUnmodified);

        docBroker->sendUnoSave(getId(), dontTerminateEdit != 0, dontSaveIfUnmodified != 0);
    }
    else if (tokens[0] == "savetostorage")
    {
        int force = 0;
        if (tokens.size() > 1)
            getTokenInteger(tokens[1], "force", force);

        if (docBroker->saveToStorage(getId(), true, "" /* This is irrelevant when success is true*/, true))
        {
            docBroker->broadcastMessage("commandresult: { \"command\": \"savetostorage\", \"success\": true }");
        }
    }
    else
    {
        if (tokens[0] == "key")
            _keyEvents++;

        if (!filterMessage(firstLine))
        {
            const std::string dummyFrame = "dummymsg";
            return forwardToChild(dummyFrame, docBroker);
        }
        else if (tokens[0] != "requestloksession")
        {
            return forwardToChild(std::string(buffer, length), docBroker);
        }
        else
        {
            assert(tokens[0] == "requestloksession");
            return true;
        }
    }

    return false;
}

bool ClientSession::loadDocument(const char* /*buffer*/, int /*length*/,
                                 const std::vector<std::string>& tokens,
                                 const std::shared_ptr<DocumentBroker>& docBroker)
{
    if (tokens.size() < 2)
    {
        // Failed loading ends connection.
        sendTextFrame("error: cmd=load kind=syntax");
        return false;
    }

    LOG_INF("Requesting document load from child.");
    try
    {
        std::string timestamp;
        int loadPart = -1;
        parseDocOptions(tokens, loadPart, timestamp);

        std::ostringstream oss;
        oss << "load";
        oss << " url=" << docBroker->getPublicUri().toString();

        if (!_userId.empty() && !_userName.empty())
        {
            std::string encodedUserId;
            Poco::URI::encode(_userId, "", encodedUserId);
            oss << " authorid=" << encodedUserId;

            std::string encodedUserName;
            Poco::URI::encode(_userName, "", encodedUserName);
            oss << " author=" << encodedUserName;
        }

        if (!_userExtraInfo.empty())
        {
            std::string encodedUserExtraInfo;
            Poco::URI::encode(_userExtraInfo, "", encodedUserExtraInfo);
            oss << " authorextrainfo=" << encodedUserExtraInfo;
        }

        oss << " readonly=" << isReadOnly();

        if (loadPart >= 0)
        {
            oss << " part=" << loadPart;
        }

        if (_haveDocPassword)
        {
            oss << " password=" << _docPassword;
        }

        if (!_lang.empty())
        {
            oss << " lang=" << _lang;
        }

        if (!_watermarkText.empty())
        {
            std::string encodedWatermarkText;
            Poco::URI::encode(_watermarkText, "", encodedWatermarkText);
            oss << " watermarkText=" << encodedWatermarkText;
        }

        if (!_docOptions.empty())
        {
            oss << " options=" << _docOptions;
        }

        return forwardToChild(oss.str(), docBroker);
    }
    catch (const Poco::SyntaxException&)
    {
        sendTextFrame("error: cmd=load kind=uriinvalid");
    }

    return false;
}

bool ClientSession::getCommandValues(const char *buffer, int length, const std::vector<std::string>& tokens,
                                     const std::shared_ptr<DocumentBroker>& docBroker)
{
    std::string command;
    if (tokens.size() != 2 || !getTokenString(tokens[1], "command", command))
    {
        return sendTextFrame("error: cmd=commandvalues kind=syntax");
    }

    std::string cmdValues;
    if (docBroker->tileCache().getTextFile("cmdValues" + command + ".txt", cmdValues))
    {
        return sendTextFrame(cmdValues);
    }

    return forwardToChild(std::string(buffer, length), docBroker);
}

bool ClientSession::sendFontRendering(const char *buffer, int length, const std::vector<std::string>& tokens,
                                      const std::shared_ptr<DocumentBroker>& docBroker)
{
    std::string font, text;
    if (tokens.size() < 2 ||
        !getTokenString(tokens[1], "font", font))
    {
        return sendTextFrame("error: cmd=renderfont kind=syntax");
    }

    getTokenString(tokens[2], "char", text);
    const std::string response = "renderfont: " + Poco::cat(std::string(" "), tokens.begin() + 1, tokens.end()) + "\n";

    std::vector<char> output;
    output.resize(response.size());
    std::memcpy(output.data(), response.data(), response.size());

    std::unique_ptr<std::fstream> cachedRendering = docBroker->tileCache().lookupCachedFile(font+text, "font");
    if (cachedRendering && cachedRendering->is_open())
    {
        cachedRendering->seekg(0, std::ios_base::end);
        size_t pos = output.size();
        std::streamsize size = cachedRendering->tellg();
        output.resize(pos + size);
        cachedRendering->seekg(0, std::ios_base::beg);
        cachedRendering->read(output.data() + pos, size);
        cachedRendering->close();

        return sendBinaryFrame(output.data(), output.size());
    }

    return forwardToChild(std::string(buffer, length), docBroker);
}

bool ClientSession::sendTile(const char * /*buffer*/, int /*length*/, const std::vector<std::string>& tokens,
                             const std::shared_ptr<DocumentBroker>& docBroker)
{
    try
    {
        auto tileDesc = TileDesc::parse(tokens);
        docBroker->handleTileRequest(tileDesc, shared_from_this());
    }
    catch (const std::exception& exc)
    {
        LOG_ERR("Failed to process tile command: " << exc.what());
        return sendTextFrame("error: cmd=tile kind=invalid");
    }

    return true;
}

bool ClientSession::sendCombinedTiles(const char* /*buffer*/, int /*length*/, const std::vector<std::string>& tokens,
                                      const std::shared_ptr<DocumentBroker>& docBroker)
{
    try
    {
        auto tileCombined = TileCombined::parse(tokens);
        docBroker->handleTileCombinedRequest(tileCombined, shared_from_this());
    }
    catch (const std::exception& exc)
    {
        LOG_ERR("Failed to process tilecombine command: " << exc.what());
        return sendTextFrame("error: cmd=tile kind=invalid");
    }

    return true;
}

bool ClientSession::forwardToChild(const std::string& message,
                                   const std::shared_ptr<DocumentBroker>& docBroker)
{
    return docBroker->forwardToChild(getId(), message);
}

bool ClientSession::filterMessage(const std::string& message) const
{
    bool allowed = true;
    StringTokenizer tokens(message, " ", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);

    // Set allowed flag to false depending on if particular WOPI properties are set
    if (tokens[0] == "downloadas")
    {
        std::string id;
        if (getTokenString(tokens[2], "id", id))
        {
            if (id == "print" && _wopiFileInfo && _wopiFileInfo->_disablePrint)
            {
                allowed = false;
                LOG_WRN("WOPI host has disabled print for this session");
            }
            else if (id == "export" && _wopiFileInfo && _wopiFileInfo->_disableExport)
            {
                allowed = false;
                LOG_WRN("WOPI host has disabled export for this session");
            }
        }
        else
        {
                allowed = false;
                LOG_WRN("No value of id in downloadas message");
        }
    }
    else if (tokens[0] == "gettextselection")
    {
        if (_wopiFileInfo && _wopiFileInfo->_disableCopy)
        {
            allowed = false;
            LOG_WRN("WOPI host has disabled copying from the document");
        }
    }
    else if (isReadOnly())
    {
        // By default, don't allow anything
        allowed = false;
        if (tokens[0] == "userinactive" || tokens[0] == "useractive" || tokens[0] == "saveas")
        {
            allowed = true;
        }
        else if (tokens[0] == "uno")
        {
            if (tokens[1] == ".uno:ExecuteSearch")
            {
                allowed = true;
            }
        }
    }

    return allowed;
}

void ClientSession::setReadOnly()
{
    Session::setReadOnly();
    // Also inform the client
    sendTextFrame("perm: readonly");
}

int ClientSession::getPollEvents(std::chrono::steady_clock::time_point /* now */,
                                 int & /* timeoutMaxMs */)
{
    LOG_TRC(getName() << " ClientSession has " << _senderQueue.size() << " write message(s) queued.");
    int events = POLLIN;
    if (_senderQueue.size())
        events |= POLLOUT;
    return events;
}

void ClientSession::performWrites()
{
    LOG_DBG(getName() << " ClientSession: performing writes");

    std::shared_ptr<Message> item;
    if (_senderQueue.dequeue(item))
    {
        try
        {
            const std::vector<char>& data = item->data();
            if (item->isBinary())
            {
                Session::sendBinaryFrame(data.data(), data.size());
            }
            else
            {
                Session::sendTextFrame(data.data(), data.size());
            }
        }
        catch (const std::exception& ex)
        {
            LOG_ERR("Failed to send message " << item->abbr() <<
                    " to " << getName() << ": " << ex.what());
        }
    }

    LOG_DBG(getName() << " ClientSession: performed write");
}

bool ClientSession::handleKitToClientMessage(const char* buffer, const int length)
{
    const auto payload = std::make_shared<Message>(buffer, length, Message::Dir::Out);

    LOG_TRC(getName() << ": handling kit-to-client [" << payload->abbr() << "].");
    const std::string& firstLine = payload->firstLine();

    const auto docBroker = _docBroker.lock();
    if (!docBroker)
    {
        LOG_ERR("No DocBroker to handle kit-to-client message: " << firstLine);
        return false;
    }

    LOOLWSD::dumpOutgoingTrace(docBroker->getJailId(), getId(), firstLine);

    const auto& tokens = payload->tokens();
    if (tokens[0] == "unocommandresult:")
    {
        const std::string stringMsg(buffer, length);
        LOG_INF(getName() << ": Command: " << stringMsg);
        const auto index = stringMsg.find_first_of('{');
        if (index != std::string::npos)
        {
            const std::string stringJSON = stringMsg.substr(index);
            Poco::JSON::Parser parser;
            const auto parsedJSON = parser.parse(stringJSON);
            const auto& object = parsedJSON.extract<Poco::JSON::Object::Ptr>();
            if (object->get("commandName").toString() == ".uno:Save")
            {
                const bool success = object->get("success").toString() == "true";
                std::string result;
                if (object->has("result"))
                {
                    const auto parsedResultJSON = object->get("result");
                    const auto& resultObj = parsedResultJSON.extract<Poco::JSON::Object::Ptr>();
                    if (resultObj->get("type").toString() == "string")
                        result = resultObj->get("value").toString();
                }

                // Save to Storage and log result.
                docBroker->saveToStorage(getId(), success, result);

                if (!isCloseFrame())
                    forwardToClient(payload);

                return true;
            }
        }
        else
        {
            LOG_WRN("Expected json unocommandresult. Ignoring: " << stringMsg);
        }
    }
    else if (tokens[0] == "error:")
    {
        std::string errorCommand;
        std::string errorKind;
        if (getTokenString(tokens[1], "cmd", errorCommand) &&
            getTokenString(tokens[2], "kind", errorKind) )
        {
            if (errorCommand == "load")
            {
                if (errorKind == "passwordrequired:to-view" ||
                    errorKind == "passwordrequired:to-modify" ||
                    errorKind == "wrongpassword")
                {
                    forwardToClient(payload);
                    LOG_WRN("Document load failed: " << errorKind);
                    return false;
                }
            }
        }
    }
    else if (tokens[0] == "curpart:" && tokens.size() == 2)
    {
        //TODO: Should forward to client?
        int curPart;
        return getTokenInteger(tokens[1], "part", curPart);
    }
    else if (tokens.size() == 3 && tokens[0] == "saveas:")
    {
        bool isConvertTo = static_cast<bool>(_saveAsSocket);

        std::string encodedURL;
        if (!getTokenString(tokens[1], "url", encodedURL))
        {
            LOG_ERR("Bad syntax for: " << firstLine);
            // we must not return early with convert-to so that we clean up
            // the session
            if (!isConvertTo)
            {
                sendTextFrame("error: cmd=saveas kind=syntax");
                return false;
            }
        }

        std::string encodedWopiFilename;
        if (!isConvertTo && !getTokenString(tokens[2], "filename", encodedWopiFilename))
        {
            LOG_ERR("Bad syntax for: " << firstLine);
            sendTextFrame("error: cmd=saveas kind=syntax");
            return false;
        }

        std::string url, wopiFilename;
        Poco::URI::decode(encodedURL, url);
        Poco::URI::decode(encodedWopiFilename, wopiFilename);

        // Save-as completed, inform the ClientSession.
        Poco::URI resultURL(url);
        if (resultURL.getScheme() == "file")
        {
            std::string relative(resultURL.getPath());
            if (relative.size() > 0 && relative[0] == '/')
                relative = relative.substr(1);

            // Rewrite file:// URLs, as they are visible to the outside world.
            const Path path(docBroker->getJailRoot(), relative);
            if (Poco::File(path).exists())
            {
                resultURL.setPath(path.toString());
            }
            else
            {
                // Blank for failure.
                LOG_DBG("SaveAs produced no output in '" << path.toString() << "', producing blank url.");
                resultURL.clear();
            }
        }

        LOG_TRC("Save-as URL: " << resultURL.toString());

        if (!isConvertTo)
        {
            // Normal SaveAs - save to Storage and log result.
            if (resultURL.getScheme() == "file" && !resultURL.getPath().empty())
            {
                // this also sends the saveas: result
                LOG_TRC("Save-as path: " << resultURL.getPath());
                docBroker->saveAsToStorage(getId(), resultURL.getPath(), wopiFilename);
            }
            else
                sendTextFrame("error: cmd=storage kind=savefailed");
        }
        else
        {
            // using the convert-to REST API
            // TODO: Send back error when there is no output.
            if (!resultURL.getPath().empty())
            {
                const std::string mimeType = "application/octet-stream";
                std::string encodedFilePath;
                Poco::URI::encode(resultURL.getPath(), "", encodedFilePath);
                LOG_TRC("Sending file: " << encodedFilePath);

                const std::string fileName = Poco::Path(resultURL.getPath()).getFileName();
                Poco::Net::HTTPResponse response;
                if (!fileName.empty())
                    response.set("Content-Disposition", "attachment; filename=\"" + fileName + "\"");

                HttpHelper::sendFile(_saveAsSocket, encodedFilePath, mimeType, response);
            }

            // Conversion is done, cleanup this fake session.
            LOG_TRC("Removing save-as ClientSession after conversion.");

            // Remove us.
            docBroker->removeSession(getId());

            // Now terminate.
            docBroker->stop("Finished saveas handler.");
        }

        return true;
    }
    else if (tokens.size() == 2 && tokens[0] == "statechanged:")
    {
        StringTokenizer stateTokens(tokens[1], "=", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);
        if (stateTokens.count() == 2 && stateTokens[0] == ".uno:ModifiedStatus")
        {
            docBroker->setModified(stateTokens[1] == "true");
        }
        else
        {
            // Set the initial settings per the user's request.
            const std::pair<std::string, std::string> unoStatePair = LOOLProtocol::split(tokens[1], '=');

            if (!docBroker->isInitialSettingSet(unoStatePair.first))
            {
                docBroker->setInitialSetting(unoStatePair.first);
                if (unoStatePair.first == ".uno:TrackChanges")
                {
                    if ((unoStatePair.second == "true" &&
                         _wopiFileInfo->_disableChangeTrackingRecord == WopiStorage::WOPIFileInfo::TriState::True) ||
                        (unoStatePair.second == "false" &&
                         _wopiFileInfo->_disableChangeTrackingRecord == WopiStorage::WOPIFileInfo::TriState::False))
                    {
                        // Toggle the TrackChanges state.
                        LOG_DBG("Forcing " << unoStatePair.first << " toggle per user settings.");
                        forwardToChild("uno .uno:TrackChanges", docBroker);
                    }
                }
                else if (unoStatePair.first == ".uno:ShowTrackedChanges")
                {
                    if ((unoStatePair.second == "true" &&
                         _wopiFileInfo->_disableChangeTrackingShow == WopiStorage::WOPIFileInfo::TriState::True) ||
                        (unoStatePair.second == "false" &&
                         _wopiFileInfo->_disableChangeTrackingShow == WopiStorage::WOPIFileInfo::TriState::False))
                    {
                        // Toggle the ShowTrackChanges state.
                        LOG_DBG("Forcing " << unoStatePair.first << " toggle per user settings.");
                        forwardToChild("uno .uno:ShowTrackedChanges", docBroker);
                    }
                }
            }
        }
    }

    if (!_isDocPasswordProtected)
    {
        if (tokens[0] == "tile:")
        {
            assert(false && "Tile traffic should go through the DocumentBroker-LoKit WS.");
        }
        else if (tokens[0] == "status:")
        {
            setViewLoaded();
            docBroker->setLoaded();

            // Forward the status response to the client.
            return forwardToClient(payload);
        }
        else if (tokens[0] == "commandvalues:")
        {
            const std::string stringMsg(buffer, length);
            const auto index = stringMsg.find_first_of('{');
            if (index != std::string::npos)
            {
                const std::string stringJSON = stringMsg.substr(index);
                Poco::JSON::Parser parser;
                const auto result = parser.parse(stringJSON);
                const auto& object = result.extract<Poco::JSON::Object::Ptr>();
                const std::string commandName = object->has("commandName") ? object->get("commandName").toString() : "";
                if (commandName == ".uno:CharFontName" ||
                    commandName == ".uno:StyleApply")
                {
                    // other commands should not be cached
                    docBroker->tileCache().saveTextFile(stringMsg, "cmdValues" + commandName + ".txt");
                }
            }
        }
        else if (tokens[0] == "invalidatetiles:")
        {
            assert(firstLine.size() == static_cast<std::string::size_type>(length));
            docBroker->invalidateTiles(firstLine);
        }
        else if (tokens[0] == "invalidatecursor:")
        {
            assert(firstLine.size() == static_cast<std::string::size_type>(length));

            const size_t index = firstLine.find_first_of('{');
            const std::string stringJSON = firstLine.substr(index);
            Poco::JSON::Parser parser;
            const Poco::Dynamic::Var result = parser.parse(stringJSON);
            const auto& object = result.extract<Poco::JSON::Object::Ptr>();
            const std::string rectangle = object->get("rectangle").toString();
            StringTokenizer rectangleTokens(rectangle, ",", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);
            int x = 0, y = 0, w = 0, h = 0;
            if (rectangleTokens.count() > 2 &&
                stringToInteger(rectangleTokens[0], x) &&
                stringToInteger(rectangleTokens[1], y))
            {
                if (rectangleTokens.count() > 3)
                {
                    stringToInteger(rectangleTokens[2], w);
                    stringToInteger(rectangleTokens[3], h);
                }

                docBroker->invalidateCursor(x, y, w, h);
            }
            else
            {
                LOG_ERR("Unable to parse " << firstLine);
            }
        }
        else if (tokens[0] == "renderfont:")
        {
            std::string font, text;
            if (tokens.size() < 3 ||
                !getTokenString(tokens[1], "font", font))
            {
                LOG_ERR("Bad syntax for: " << firstLine);
                return false;
            }

            getTokenString(tokens[2], "char", text);
            assert(firstLine.size() < static_cast<std::string::size_type>(length));
            docBroker->tileCache().saveRendering(font+text, "font", buffer + firstLine.size() + 1, length - firstLine.size() - 1);
            return forwardToClient(payload);
        }
    }
    else
    {
        LOG_INF("Ignoring notification on password protected document: " << firstLine);
    }

    // Forward everything else.
    return forwardToClient(payload);
}

bool ClientSession::forwardToClient(const std::shared_ptr<Message>& payload)
{
    const auto& message = payload->abbr();

    if (isCloseFrame())
    {
        LOG_TRC(getName() << ": peer began the closing handshake. Dropping forward message [" << message << "].");
        return true;
    }

    enqueueSendMessage(payload);
    return true;
}

Authorization ClientSession::getAuthorization() const
{
    Poco::URI::QueryParameters queryParams = _uriPublic.getQueryParameters();

    // prefer the access_token
    for (auto& param: queryParams)
    {
        if (param.first == "access_token")
        {
            std::string decodedToken;
            Poco::URI::decode(param.second, decodedToken);
            return Authorization(Authorization::Type::Token, decodedToken);
        }
    }

    for (auto& param: queryParams)
    {
        if (param.first == "access_header")
        {
            std::string decodedHeader;
            Poco::URI::decode(param.second, decodedHeader);
            return Authorization(Authorization::Type::Header, decodedHeader);
        }
    }

    return Authorization();
}

void ClientSession::onDisconnect()
{
    LOG_INF(getName() << " Disconnected, current number of connections: " << LOOLWSD::NumConnections);

    const auto docBroker = getDocumentBroker();
    LOG_CHECK_RET(docBroker && "Null DocumentBroker instance", );
    docBroker->assertCorrectThread();
    const auto docKey = docBroker->getDocKey();

    try
    {
        // Connection terminated. Destroy session.
        LOG_DBG(getName() << " on docKey [" << docKey << "] terminated. Cleaning up.");

        docBroker->removeSession(getId());
    }
    catch (const UnauthorizedRequestException& exc)
    {
        LOG_ERR("Error in client request handler: " << exc.toString());
        const std::string status = "error: cmd=internal kind=unauthorized";
        LOG_TRC("Sending to Client [" << status << "].");
        sendMessage(status);
    }
    catch (const std::exception& exc)
    {
        LOG_ERR("Error in client request handler: " << exc.what());
    }

    try
    {
        if (isCloseFrame())
        {
            LOG_TRC("Normal close handshake.");
            // Client initiated close handshake
            // respond with close frame
            shutdown();
        }
        else if (!ShutdownRequestFlag)
        {
            // something wrong, with internal exceptions
            LOG_TRC("Abnormal close handshake.");
            closeFrame();
            shutdown(WebSocketHandler::StatusCodes::ENDPOINT_GOING_AWAY);
        }
        else
        {
            LOG_TRC("Server recycling.");
            closeFrame();
            shutdown(WebSocketHandler::StatusCodes::ENDPOINT_GOING_AWAY);
        }
    }
    catch (const std::exception& exc)
    {
        LOG_WRN(getName() << ": Exception while closing socket for docKey [" << docKey << "]: " << exc.what());
    }
}

void ClientSession::dumpState(std::ostream& os)
{
    Session::dumpState(os);

    os << "\t\tisReadOnly: " << isReadOnly()
       << "\n\t\tisDocumentOwner: " << _isDocumentOwner
       << "\n\t\tisAttached: " << _isAttached
       << "\n\t\tkeyEvents: " << _keyEvents;

    std::shared_ptr<StreamSocket> socket = _socket.lock();
    if (socket)
    {
        uint64_t sent, recv;
        socket->getIOStats(sent, recv);
        os << "\n\t\tsent/keystroke: " << (double)sent/_keyEvents << "bytes";
    }

    os << "\n";
    _senderQueue.dumpState(os);

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
