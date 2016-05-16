/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include <Poco/FileStream.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/URI.h>
#include <Poco/URIStreamOpener.h>

#include "Common.hpp"
#include "LOOLProtocol.hpp"
#include "LOOLSession.hpp"
#include "LOOLWSD.hpp"
#include "MasterProcessSession.hpp"
#include "Rectangle.hpp"
#include "Storage.hpp"
#include "TileCache.hpp"
#include "IoUtil.hpp"
#include "Util.hpp"

using namespace LOOLProtocol;

using Poco::Path;
using Poco::StringTokenizer;

MasterProcessSession::MasterProcessSession(const std::string& id,
                                           const Kind kind,
                                           std::shared_ptr<Poco::Net::WebSocket> ws,
                                           std::shared_ptr<DocumentBroker> docBroker,
                                           std::shared_ptr<BasicTileQueue> queue) :
    LOOLSession(id, kind, ws),
    _curPart(0),
    _loadPart(-1),
    _docBroker(docBroker),
    _queue(queue)
{
    Log::info("MasterProcessSession ctor [" + getName() + "].");
}

MasterProcessSession::~MasterProcessSession()
{
}

bool MasterProcessSession::loadDocument(const char* /*buffer*/, int /*length*/, StringTokenizer& tokens)
{
    if (tokens.count() < 2)
    {
        sendTextFrame("error: cmd=load kind=syntax");
        return false;
    }

    try
    {
        std::string timestamp;
        parseDocOptions(tokens, _loadPart, timestamp);

        // Finally, wait for the Child to connect to Master,
        // link the document in jail and dispatch load to child.
        Log::trace("Dispatching child to handle [load].");
        dispatchChild();

        return true;
    }
    catch (const Poco::SyntaxException&)
    {
        sendTextFrame("error: cmd=load kind=uriinvalid");
    }

    return false;
}

bool MasterProcessSession::getStatus(const char *buffer, int length)
{
    const std::string status = _docBroker->tileCache().getTextFile("status.txt");
    if (!status.empty())
    {
        sendTextFrame(status);

        // And let clients know if they hold the edit lock.
        std::string message = "editlock: ";
        message += std::to_string(isEditLocked());
        Log::debug("Forwarding [" + message + "] in response to status.");
        sendTextFrame(message);

        return true;
    }

    if (_peer.expired())
    {
        Log::trace("Dispatching child to handle [getStatus].");
        dispatchChild();
    }

    forwardToPeer(buffer, length);
    return true;
}

void MasterProcessSession::setEditLock(const bool value)
{
    // Update the sate and forward to child.
    _bEditLock = value;
    const auto msg = std::string("editlock: ") + (value ? "1" : "0");
    forwardToPeer(msg.data(), msg.size());
}

bool MasterProcessSession::getCommandValues(const char *buffer, int length, StringTokenizer& tokens)
{
    std::string command;
    if (tokens.count() != 2 || !getTokenString(tokens[1], "command", command))
    {
        sendTextFrame("error: cmd=commandvalues kind=syntax");
        return false;
    }

    const std::string cmdValues = _docBroker->tileCache().getTextFile("cmdValues" + command + ".txt");
    if (cmdValues.size() > 0)
    {
        sendTextFrame(cmdValues);
        return true;
    }

    if (_peer.expired())
        dispatchChild();
    forwardToPeer(buffer, length);
    return true;
}

bool MasterProcessSession::getPartPageRectangles(const char *buffer, int length)
{
    const std::string partPageRectangles = _docBroker->tileCache().getTextFile("partpagerectangles.txt");
    if (partPageRectangles.size() > 0)
    {
        sendTextFrame(partPageRectangles);
        return true;
    }

    if (_peer.expired())
        dispatchChild();
    forwardToPeer(buffer, length);
    return true;
}

void MasterProcessSession::sendFontRendering(const char *buffer, int length, StringTokenizer& tokens)
{
    std::string font;
    if (tokens.count() < 2 ||
        !getTokenString(tokens[1], "font", font))
    {
        sendTextFrame("error: cmd=renderfont kind=syntax");
        return;
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

        sendBinaryFrame(output.data(), output.size());
        return;
    }

    if (_peer.expired())
        dispatchChild();
    forwardToPeer(buffer, length);
}

void MasterProcessSession::sendTile(const char * /*buffer*/, int /*length*/, StringTokenizer& tokens)
{
    try
    {
        auto tileDesc = TileDesc::parse(tokens);
        _docBroker->handleTileRequest(tileDesc, shared_from_this());
    }
    catch (const std::exception& exc)
    {
        Log::error(std::string("Failed to process tile command: ") + exc.what() + ".");
        sendTextFrame("error: cmd=tile kind=invalid");
    }
}

void MasterProcessSession::sendCombinedTiles(const char* /*buffer*/, int /*length*/, StringTokenizer& tokens)
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
        sendTextFrame("error: cmd=tilecombine kind=syntax");
        return;
    }

    if (part < 0 || pixelWidth <= 0 || pixelHeight <= 0 ||
        tileWidth <= 0 || tileHeight <= 0 ||
        tilePositionsX.empty() || tilePositionsY.empty())
    {
        sendTextFrame("error: cmd=tilecombine kind=invalid");
        return;
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
        sendTextFrame("error: cmd=tilecombine kind=invalid");
        return;
    }

    for (size_t i = 0; i < numberOfPositions; ++i)
    {
        int x = 0;
        if (!stringToInteger(positionXtokens[i], x))
        {
            sendTextFrame("error: cmd=tilecombine kind=syntax");
            return;
        }

        int y = 0;
        if (!stringToInteger(positionYtokens[i], y))
        {
            sendTextFrame("error: cmd=tilecombine kind=syntax");
            return;
        }

        const TileDesc tile(part, pixelWidth, pixelHeight, x, y, tileWidth, tileHeight);
        _docBroker->handleTileRequest(tile, shared_from_this());
    }
}

void MasterProcessSession::dispatchChild()
{
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
    forwardToPeer(loadRequest.c_str(), loadRequest.size());
}

void MasterProcessSession::forwardToPeer(const char *buffer, int length)
{
    const auto message = getAbbreviatedMessage(buffer, length);

    auto peer = _peer.lock();
    if (!peer)
    {
        throw Poco::ProtocolException(getName() + ": no peer to forward to: [" + message + "].");
    }
    else if (peer->isCloseFrame())
    {
        Log::trace(getName() + ": peer began the closing handshake. Dropping forward message [" + message + "].");
        return;
    }

    Log::trace(getName() + " -> " + peer->getName() + ": " + message);
    peer->sendBinaryFrame(buffer, length);
}

bool MasterProcessSession::shutdownPeer(Poco::UInt16 statusCode, const std::string& message)
{
    auto peer = _peer.lock();
    if (peer && !peer->isCloseFrame())
    {
        peer->_ws->shutdown(statusCode, message);
    }
    return peer != nullptr;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
