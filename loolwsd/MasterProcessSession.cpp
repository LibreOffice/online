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
                                           std::shared_ptr<Poco::Net::WebSocket> ws) :
    LOOLSession(id, kind, ws)
{
    Log::info("MasterProcessSession ctor [" + getName() + "].");
}

MasterProcessSession::~MasterProcessSession()
{
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
