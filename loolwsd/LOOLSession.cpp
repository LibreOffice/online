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

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKit.h>
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <Poco/Dynamic/Var.h>
#include <Poco/Exception.h>
#include <Poco/File.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/HTTPStreamFactory.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Path.h>
#include <Poco/Process.h>
#include <Poco/Random.h>
#include <Poco/StreamCopier.h>
#include <Poco/String.h>
#include <Poco/StringTokenizer.h>
#include <Poco/ThreadLocal.h>
#include <Poco/URI.h>
#include <Poco/URIStreamOpener.h>
#include <Poco/Exception.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/DialogSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/FileStream.h>

#include "LOOLProtocol.hpp"
#include "LOOLSession.hpp"
#include "LOOLWSD.hpp"
#include "TileCache.hpp"
#include "Util.hpp"

using namespace LOOLProtocol;

using Poco::Dynamic::Var;
using Poco::File;
using Poco::IOException;
using Poco::JSON::Object;
using Poco::JSON::Parser;
using Poco::Net::HTTPStreamFactory;
using Poco::Net::WebSocket;
using Poco::Path;
using Poco::Process;
using Poco::ProcessHandle;
using Poco::Random;
using Poco::StreamCopier;
using Poco::StringTokenizer;
using Poco::Thread;
using Poco::ThreadLocal;
using Poco::UInt64;
using Poco::URI;
using Poco::URIStreamOpener;
using Poco::Exception;
using Poco::Net::DialogSocket;
using Poco::Net::SocketAddress;
using Poco::Net::WebSocketException;

const std::string LOOLSession::jailDocumentURL = "/user/thedocument";

LOOLSession::LOOLSession(std::shared_ptr<WebSocket> ws, Kind kind) :
    _kind(kind),
    _ws(ws),
    _docURL("")
{
    std::cout << Util::logPrefix() << "LOOLSession ctor this=" << this << " " << _kind << " ws=" << _ws.get() << std::endl;
    if (kind == Kind::ToClient)
    {
        _kindString = "ToClient";
    }
    else if (kind == Kind::ToMaster)
    {
        _kindString = "ToMaster";
    }
    else if (kind == Kind::ToPrisoner)
    {
        _kindString = "ToPrisoner";
    }
}

LOOLSession::~LOOLSession()
{
    std::cout << Util::logPrefix() << "loolsession dtor this=" << this << " " << _kind << std::endl;
    if (_ws)
        Util::shutdownWebSocket(*_ws);
}

void LOOLSession::sendTextFrame(const std::string& text)
{
    if (!_ws)
        Log::error("No socket to send to.");
    else
        Log::debug(_kindString + ",Send," + getAbbreviatedMessage(text.c_str(), text.size()));

    std::unique_lock<std::mutex> lock(_mutex);

    _ws->sendFrame(text.data(), text.size());
}

void LOOLSession::sendBinaryFrame(const char *buffer, int length)
{
    if (!_ws)
        Log::error("No socket to send to.");
    else
        Log::debug(_kindString + ",Send," + std::to_string(length) + " bytes");

    std::unique_lock<std::mutex> lock(_mutex);

    if (length > 1000)
    {
        std::string nextmessage = "nextmessage: size=" + std::to_string(length);
        if (_ws)
            _ws->sendFrame(nextmessage.data(), nextmessage.size());
    }

    if (_ws)
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
        else if (tokens[i].find("timestamp=") == 0)
        {
            timestamp = tokens[i].substr(strlen("timestamp="));
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

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
