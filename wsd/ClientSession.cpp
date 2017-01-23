/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ClientSession.hpp"
#include "config.h"

#include <fstream>

#include <Poco/URI.h>

#include "Common.hpp"
#include "DocumentBroker.hpp"
#include "LOOLWSD.hpp"
#include "Log.hpp"
#include "PrisonerSession.hpp"
#include "Protocol.hpp"
#include "Session.hpp"
#include "Util.hpp"

using namespace LOOLProtocol;

using Poco::StringTokenizer;

ClientSession::ClientSession(const std::string& id,
                             const std::shared_ptr<LOOLWebSocket>& ws,
                             const std::shared_ptr<DocumentBroker>& docBroker,
                             const Poco::URI& uriPublic,
                             const bool readOnly) :
    Session("ToClient-" + id, id, ws),
    _docBroker(docBroker),
    _uriPublic(uriPublic),
    _isReadOnly(readOnly),
    _isDocumentOwner(false),
    _loadPart(-1),
    _isLoadRequested(false),
    _stop(false)
{
    LOG_INF("ClientSession ctor [" << getName() << "].");

    _senderThread = std::thread([this]{ senderThread(); });
}

ClientSession::~ClientSession()
{
    LOG_INF("~ClientSession dtor [" << getName() << "].");

    stop();
    if (_senderThread.joinable())
    {
        _senderThread.join();
    }
}

bool ClientSession::isLoaded() const
{
    return _isLoadRequested && _peer && _peer->gotStatus();
}

void ClientSession::bridgePrisonerSession()
{
    auto docBroker = getDocumentBroker();
    if (docBroker)
    {
        _peer = std::make_shared<PrisonerSession>(shared_from_this(), docBroker);
    }
    else
    {
        const std::string msg = "No valid DocBroker while bridging Prisoner Session for " + getName();
        LOG_ERR(msg);
        throw std::runtime_error(msg);
    }
}

bool ClientSession::_handleInput(const char *buffer, int length)
{
    LOG_TRC(getName() << ": handling [" << getAbbreviatedMessage(buffer, length) << "].");
    const std::string firstLine = getFirstLine(buffer, length);
    const auto tokens = LOOLProtocol::tokenize(firstLine.data(), firstLine.size());

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
             tokens[0] != "commandvalues" &&
             tokens[0] != "closedocument" &&
             tokens[0] != "downloadas" &&
             tokens[0] != "getchildid" &&
             tokens[0] != "gettextselection" &&
             tokens[0] != "paste" &&
             tokens[0] != "insertfile" &&
             tokens[0] != "key" &&
             tokens[0] != "mouse" &&
             tokens[0] != "partpagerectangles" &&
             tokens[0] != "ping" &&
             tokens[0] != "renderfont" &&
             tokens[0] != "requestloksession" &&
             tokens[0] != "resetselection" &&
             tokens[0] != "saveas" &&
             tokens[0] != "selectgraphic" &&
             tokens[0] != "selecttext" &&
             tokens[0] != "setclientpart" &&
             tokens[0] != "setpage" &&
             tokens[0] != "status" &&
             tokens[0] != "tile" &&
             tokens[0] != "tilecombine" &&
             tokens[0] != "uno" &&
             tokens[0] != "useractive" &&
             tokens[0] != "userinactive")
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

        return true;
    }
    else if (tokens[0] == "partpagerectangles")
    {
        return getPartPageRectangles(buffer, length, docBroker);
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
    else
    {
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
        parseDocOptions(tokens, _loadPart, timestamp);

        std::ostringstream oss;
        oss << "load";
        oss << " url=" << docBroker->getPublicUri().toString();
        oss << " jail=" << docBroker->getJailedUri().toString();

        if (!_userId.empty() && !_userName.empty())
        {
            std::string encodedUserId;
            Poco::URI::encode(_userId, "", encodedUserId);
            oss << " authorid=" + encodedUserId;

            std::string encodedUserName;
            Poco::URI::encode(_userName, "", encodedUserName);
            oss << " author=" + encodedUserName;
        }

        if (_loadPart >= 0)
            oss << " part=" + std::to_string(_loadPart);

        if (_haveDocPassword)
            oss << " password=" << _docPassword;

        if (!_docOptions.empty())
            oss << " options=" << _docOptions;

        const auto loadRequest = oss.str();
        if (forwardToChild(loadRequest, docBroker))
        {
            _isLoadRequested = true;
            return true;
        }
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

bool ClientSession::getPartPageRectangles(const char *buffer, int length,
                                          const std::shared_ptr<DocumentBroker>& docBroker)
{
    std::string partPageRectangles;
    if (docBroker->tileCache().getTextFile("partpagerectangles.txt", partPageRectangles))
    {
        return sendTextFrame(partPageRectangles);
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
    else if (tokens[0] == "gettextselection" || tokens[0] == "paste" || tokens[0] == "insertfile")
    {
        if (_wopiFileInfo && _wopiFileInfo->_disableCopy)
        {
            allowed = false;
            LOG_WRN("WOPI host has disabled copying to/from the document");
        }
    }
    else if (isReadOnly())
    {
        // By default, don't allow anything
        allowed = false;
        if (tokens[0] == "userinactive" || tokens[0] == "useractive")
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
    _isReadOnly = true;
    // Also inform the client
    sendTextFrame("perm: readonly");
}

void ClientSession::senderThread()
{
    LOG_DBG(getName() << " SenderThread started");

    while (!stopping())
    {
        std::shared_ptr<Message> item;
        if (_senderQueue.waitDequeue(item, static_cast<size_t>(COMMAND_TIMEOUT_MS)))
        {
            const std::vector<char>& data = item->data();
            try
            {
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
                LOG_ERR("Failed to send message [" << LOOLProtocol::getAbbreviatedMessage(data) <<
                        "] to " << getName() << ": " << ex.what());
            }
        }
    }

    LOG_DBG(getName() << " SenderThread finished");
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
