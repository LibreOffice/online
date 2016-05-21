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

#include <Poco/FileStream.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/URI.h>
#include <Poco/URIStreamOpener.h>

#include "Common.hpp"
#include "IoUtil.hpp"
#include "LOOLProtocol.hpp"
#include "LOOLSession.hpp"
#include "LOOLWSD.hpp"
#include "Log.hpp"
#include "PrisonerSession.hpp"
#include "Rectangle.hpp"
#include "Storage.hpp"
#include "TileCache.hpp"
#include "Util.hpp"

using namespace LOOLProtocol;

using Poco::StringTokenizer;

ClientSession::ClientSession(const std::string& id,
                             std::shared_ptr<Poco::Net::WebSocket> ws,
                             std::shared_ptr<DocumentBroker> docBroker,
                             std::shared_ptr<BasicTileQueue> queue) :
    LOOLSession(id, Kind::ToClient, ws),
    _docBroker(docBroker),
    _queue(queue),
    _haveEditLock(std::getenv("LOK_VIEW_CALLBACK")),
    _loadFailed(false),
    _loadPart(-1)
{
    Log::info("ClientSession ctor [" + getName() + "].");
}

ClientSession::~ClientSession()
{
    Log::info("~ClientSession dtor [" + getName() + "].");

    // Release the save-as queue.
    _saveAsQueue.put("");
}

bool ClientSession::_handleInput(const char *buffer, int length)
{
    const std::string firstLine = getFirstLine(buffer, length);
    StringTokenizer tokens(firstLine, " ", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);
    Log::trace(getName() + ": handling [" + firstLine + "].");

    if (LOOLProtocol::tokenIndicatesUserInteraction(tokens[0]))
    {
        // Keep track of timestamps of incoming client messages that indicate user activity.
        updateLastActivityTime();
    }

    if (tokens[0] == "loolclient")
    {
        const auto versionTuple = ParseVersion(tokens[1]);
        if (std::get<0>(versionTuple) != ProtocolMajorVersionNumber ||
            std::get<1>(versionTuple) != ProtocolMinorVersionNumber)
        {
            sendTextFrame("error: cmd=loolclient kind=badversion");
            return false;
        }

        return sendTextFrame("loolserver " + GetProtocolVersion());
    }

    if (tokens[0] == "takeedit")
    {
        _docBroker->takeEditLock(getId());
        return true;
    }
    else if (tokens[0] == "load")
    {
        if (_docURL != "")
        {
            sendTextFrame("error: cmd=load kind=docalreadyloaded");
            return false;
        }
        return loadDocument(buffer, length, tokens);
    }
    else if (tokens[0] != "canceltiles" &&
             tokens[0] != "clientzoom" &&
             tokens[0] != "clientvisiblearea" &&
             tokens[0] != "commandvalues" &&
             tokens[0] != "downloadas" &&
             tokens[0] != "getchildid" &&
             tokens[0] != "gettextselection" &&
             tokens[0] != "paste" &&
             tokens[0] != "insertfile" &&
             tokens[0] != "key" &&
             tokens[0] != "mouse" &&
             tokens[0] != "partpagerectangles" &&
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
        if (!_peer.expired())
        {
            return forwardToPeer(_peer, buffer, length);
        }
    }
    else if (tokens[0] == "commandvalues")
    {
        return getCommandValues(buffer, length, tokens);
    }
    else if (tokens[0] == "partpagerectangles")
    {
        return getPartPageRectangles(buffer, length);
    }
    else if (tokens[0] == "renderfont")
    {
        return sendFontRendering(buffer, length, tokens);
    }
    else if (tokens[0] == "status")
    {
        return getStatus(buffer, length);
    }
    else if (tokens[0] == "tile")
    {
        return sendTile(buffer, length, tokens);
    }
    else if (tokens[0] == "tilecombine")
    {
        return sendCombinedTiles(buffer, length, tokens);
    }
    else
    {
        // All other commands are such that they always require a
        // LibreOfficeKitDocument session, i.e. need to be handled in
        // a child process.
        if (_peer.expired())
        {
            Log::error(getName() + " has no peer to handle [" + tokens[0] + "].");
            return false;
        }

        // Allow 'downloadas' for all kinds of views irrespective of editlock
        if (!isEditLocked() && tokens[0] != "downloadas" &&
            tokens[0] != "userinactive" && tokens[0] != "useractive")
        {
            std::string dummyFrame = "dummymsg";
            return forwardToPeer(_peer, dummyFrame.c_str(), dummyFrame.size());
        }
        else if (tokens[0] != "requestloksession")
        {
            return forwardToPeer(_peer, buffer, length);
        }
        else
        {
            assert(tokens[0] == "requestloksession");
            return true;
        }
    }

    return false;
}

bool ClientSession::loadDocument(const char* /*buffer*/, int /*length*/, StringTokenizer& tokens)
{
    if (tokens.count() < 2)
    {
        // Failed loading ends connection.
        sendTextFrame("error: cmd=load kind=syntax");
        return false;
    }

    Log::info("Requesting document load from child.");
    try
    {
        std::string timestamp;
        parseDocOptions(tokens, _loadPart, timestamp);

        std::ostringstream oss;
        oss << "load";
        oss << " url=" << _docBroker->getPublicUri().toString();
        oss << " jail=" << _docBroker->getJailedUri().toString();

        if (_loadPart >= 0)
            oss << " part=" + std::to_string(_loadPart);

        if (_haveDocPassword)
            oss << " password=" << _docPassword;

        if (!_docOptions.empty())
            oss << " options=" << _docOptions;

        const auto loadRequest = oss.str();
        return forwardToPeer(_peer, loadRequest.c_str(), loadRequest.size());
    }
    catch (const Poco::SyntaxException&)
    {
        sendTextFrame("error: cmd=load kind=uriinvalid");
    }

    return false;
}

bool ClientSession::getStatus(const char *buffer, int length)
{
    const std::string status = _docBroker->tileCache().getTextFile("status.txt");
    if (!status.empty())
    {
        sendTextFrame(status);

        // And let clients know if they hold the edit lock.
        const auto msg = "editlock: " + std::to_string(isEditLocked());
        Log::debug("Returning [" + msg + "] in response to status.");
        return sendTextFrame(msg);
    }

    return forwardToPeer(_peer, buffer, length);
}

bool ClientSession::setEditLock(const bool value)
{
    // Update the sate and forward to child.
    markEditLock(value);
    const auto msg = "editlock: " + std::to_string(isEditLocked());
    const auto mv = std::getenv("LOK_VIEW_CALLBACK") ? "1" : "0";
    Log::debug("Forwarding [" + msg + "] to set editlock to " + std::to_string(value) + ". MultiView: " + mv);
    return forwardToPeer(_peer, msg.data(), msg.size());
}

bool ClientSession::getCommandValues(const char *buffer, int length, StringTokenizer& tokens)
{
    std::string command;
    if (tokens.count() != 2 || !getTokenString(tokens[1], "command", command))
    {
        return sendTextFrame("error: cmd=commandvalues kind=syntax");
    }

    const std::string cmdValues = _docBroker->tileCache().getTextFile("cmdValues" + command + ".txt");
    if (cmdValues.size() > 0)
    {
        return sendTextFrame(cmdValues);
    }

    return forwardToPeer(_peer, buffer, length);
}

bool ClientSession::getPartPageRectangles(const char *buffer, int length)
{
    const std::string partPageRectangles = _docBroker->tileCache().getTextFile("partpagerectangles.txt");
    if (partPageRectangles.size() > 0)
    {
        return sendTextFrame(partPageRectangles);
    }

    return forwardToPeer(_peer, buffer, length);
}

bool ClientSession::sendFontRendering(const char *buffer, int length, StringTokenizer& tokens)
{
    std::string font;
    if (tokens.count() < 2 ||
        !getTokenString(tokens[1], "font", font))
    {
        return sendTextFrame("error: cmd=renderfont kind=syntax");
    }

    const std::string response = "renderfont: " + Poco::cat(std::string(" "), tokens.begin() + 1, tokens.end()) + "\n";

    std::vector<char> output;
    output.resize(response.size());
    std::memcpy(output.data(), response.data(), response.size());

    std::unique_ptr<std::fstream> cachedRendering = _docBroker->tileCache().lookupRendering(font, "font");
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

    return forwardToPeer(_peer, buffer, length);
}

bool ClientSession::sendTile(const char * /*buffer*/, int /*length*/, StringTokenizer& tokens)
{
    try
    {
        auto tileDesc = TileDesc::parse(tokens);
        _docBroker->handleTileRequest(tileDesc, shared_from_this());
    }
    catch (const std::exception& exc)
    {
        Log::error(std::string("Failed to process tile command: ") + exc.what() + ".");
        return sendTextFrame("error: cmd=tile kind=invalid");
    }

    return true;
}

bool ClientSession::sendCombinedTiles(const char* /*buffer*/, int /*length*/, StringTokenizer& tokens)
{
    int part, pixelWidth, pixelHeight, tileWidth, tileHeight;
    std::string tilePositionsX, tilePositionsY;
    if (tokens.count() < 8 ||
        !getTokenInteger(tokens[1], "part", part) ||
        !getTokenInteger(tokens[2], "width", pixelWidth) ||
        !getTokenInteger(tokens[3], "height", pixelHeight) ||
        !getTokenString (tokens[4], "tileposx", tilePositionsX) ||
        !getTokenString (tokens[5], "tileposy", tilePositionsY) ||
        !getTokenInteger(tokens[6], "tilewidth", tileWidth) ||
        !getTokenInteger(tokens[7], "tileheight", tileHeight))
    {
        return sendTextFrame("error: cmd=tilecombine kind=syntax");
    }

    if (part < 0 || pixelWidth <= 0 || pixelHeight <= 0 ||
        tileWidth <= 0 || tileHeight <= 0 ||
        tilePositionsX.empty() || tilePositionsY.empty())
    {
        return sendTextFrame("error: cmd=tilecombine kind=invalid");
    }

    std::string reqTimestamp;
    size_t index = 8;
    if (tokens.count() > index && tokens[index].find("timestamp") == 0)
    {
        getTokenString(tokens[index], "timestamp", reqTimestamp);
        ++index;
    }

    int id = -1;
    if (tokens.count() > index && tokens[index].find("id") == 0)
    {
        getTokenInteger(tokens[index], "id", id);
        ++index;
    }

    StringTokenizer positionXtokens(tilePositionsX, ",", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);
    StringTokenizer positionYtokens(tilePositionsY, ",", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);

    size_t numberOfPositions = positionYtokens.count();

    // check that number of positions for X and Y is the same
    if (numberOfPositions != positionXtokens.count())
    {
        return sendTextFrame("error: cmd=tilecombine kind=invalid");
    }

    for (size_t i = 0; i < numberOfPositions; ++i)
    {
        int x = 0;
        if (!stringToInteger(positionXtokens[i], x))
        {
            return sendTextFrame("error: cmd=tilecombine kind=syntax");
        }

        int y = 0;
        if (!stringToInteger(positionYtokens[i], y))
        {
            return sendTextFrame("error: cmd=tilecombine kind=syntax");
        }

        const TileDesc tile(part, pixelWidth, pixelHeight, x, y, tileWidth, tileHeight);
        _docBroker->handleTileRequest(tile, shared_from_this());
    }

    return true;
}

bool ClientSession::shutdownPeer(Poco::UInt16 statusCode, const std::string& message)
{
    auto peer = _peer.lock();
    if (peer && !peer->isCloseFrame())
    {
        peer->shutdown(statusCode, message);
    }
    return peer != nullptr;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
