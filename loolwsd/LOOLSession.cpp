/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <ftw.h>
#include <utime.h>

#include <cassert>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <set>

#include <Poco/Exception.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Path.h>
#include <Poco/String.h>
#include <Poco/StringTokenizer.h>

#include "Common.hpp"
#include "LOOLProtocol.hpp"
#include "LOOLSession.hpp"
#include "TileCache.hpp"
#include "Util.hpp"

using namespace LOOLProtocol;

using Poco::Exception;
using Poco::IOException;
using Poco::Net::WebSocket;
using Poco::Path;
using Poco::StringTokenizer;

LOOLSession::LOOLSession(const std::string& id, const Kind kind,
                         std::shared_ptr<WebSocket> ws) :
    _kind(kind),
    _kindString(kind == Kind::ToClient ? "ToClient" :
                kind == Kind::ToMaster ? "ToMaster" : "ToPrisoner"),
    _ws(ws),
    _docPassword(""),
    _isDocPasswordProvided(false),
    _isDocLoaded(false),
    _isDocPasswordProtected(false),
    _disconnected(false)
{
    // Only a post request can have a null ws.
    if (_kind != Kind::ToClient)
        assert(_ws);

    setId(id);
}

LOOLSession::~LOOLSession()
{
    Util::shutdownWebSocket(_ws);
}

void LOOLSession::sendTextFrame(const std::string& text)
{
    if (!_ws)
    {
        Log::error("Error: No socket to send " + getAbbreviatedMessage(text.c_str(), text.size()) + " to.");
        return;
    }
    else
        Log::trace(getName() + " Send: " + getAbbreviatedMessage(text.c_str(), text.size()));

    std::unique_lock<std::mutex> lock(_mutex);
    const int length = text.size();
    if ( length > SMALL_MESSAGE_SIZE )
    {
        const std::string nextmessage = "nextmessage: size=" + std::to_string(length);
        _ws->sendFrame(nextmessage.data(), nextmessage.size());
    }

    _ws->sendFrame(text.data(), length);
}

void LOOLSession::sendBinaryFrame(const char *buffer, int length)
{
    if (!_ws)
    {
        Log::error("Error: No socket to send binary frame of " + std::to_string(length) + " bytes to.");
        return;
    }
    else
        Log::trace(getName() + " Send: " + std::to_string(length) + " bytes");

    std::unique_lock<std::mutex> lock(_mutex);

    if ( length > SMALL_MESSAGE_SIZE )
    {
        const std::string nextmessage = "nextmessage: size=" + std::to_string(length);
        _ws->sendFrame(nextmessage.data(), nextmessage.size());
    }

    _ws->sendFrame(buffer, length, WebSocket::FRAME_BINARY);
}

void LOOLSession::parseDocOptions(const StringTokenizer& tokens, int& part, std::string& timestamp)
{
    // First token is the "load" command itself.
    size_t offset = 1;
    if (tokens.count() > 2 && tokens[1].find("part=") == 0)
    {
        getTokenInteger(tokens[1], "part", part);
        ++offset;
    }

    for (size_t i = offset; i < tokens.count(); ++i)
    {
        if (tokens[i].find("url=") == 0)
        {
            _docURL = tokens[i].substr(strlen("url="));
            ++offset;
        }
        else if (tokens[i].find("jail=") == 0)
        {
            _jailedFilePath = tokens[i].substr(strlen("jail="));
            ++offset;
        }
        else if (tokens[i].find("timestamp=") == 0)
        {
            timestamp = tokens[i].substr(strlen("timestamp="));
            ++offset;
        }
        else if (tokens[i].find("password=") == 0)
        {
            _docPassword = tokens[i].substr(strlen("password="));
            _isDocPasswordProvided = true;
            ++offset;
        }
    }

    if (tokens.count() > offset)
    {
        if (getTokenString(tokens[offset], "options", _docOptions))
        {
            if (tokens.count() > offset + 1)
                _docOptions += Poco::cat(std::string(" "), tokens.begin() + offset + 1, tokens.end());
        }
    }
}

void LOOLSession::disconnect(const std::string& reason)
{
    try
    {
        if (!_disconnected)
        {
            if (reason != "")
                sendTextFrame("disconnect " + reason);
            else
                sendTextFrame("disconnect");
            _disconnected = true;
            Util::shutdownWebSocket(_ws);
        }
    }
    catch (const IOException& exc)
    {
        Log::error("LOOLSession::disconnect: Exception: " + exc.displayText() + (exc.nested() ? " (" + exc.nested()->displayText() + ")" : ""));
    }
}

bool LOOLSession::handleDisconnect(StringTokenizer& /*tokens*/)
{
    _disconnected = true;
    Util::shutdownWebSocket(_ws);
    return false;
}

bool LOOLSession::handleInput(const char *buffer, int length)
{
    assert(buffer != nullptr);

    const auto summary = getAbbreviatedMessage(buffer, length);
    try
    {
        Log::trace(getName() + " Recv: " + summary);
        if (TerminationFlag)
        {
            Log::warn("Input while terminating: [" + summary + "].");
        }

        return _handleInput(buffer, length);
    }
    catch (const Exception& exc)
    {
        Log::error() << "LOOLSession::handleInput: Exception while handling [" + summary + "] in "
                     << getName() << ": "
                     << exc.displayText()
                     << (exc.nested() ? " (" + exc.nested()->displayText() + ")" : "")
                     << Log::end;
    }
    catch (const std::exception& exc)
    {
        Log::error("LOOLSession::handleInput: Exception while handling [" + summary + "]: " + exc.what());
    }

    return false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
