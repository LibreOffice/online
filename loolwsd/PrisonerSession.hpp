/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_PRISONERSESSION_HPP
#define INCLUDED_PRISONERSESSION_HPP

#include <time.h>

#include <Poco/Random.h>

#include "MasterProcessSession.hpp"
#include "LOOLSession.hpp"
#include "MessageQueue.hpp"

class DocumentBroker;
class ClientSession;

class PrisonerSession final : public LOOLSession, public std::enable_shared_from_this<PrisonerSession>
{
public:
    PrisonerSession(const std::string& id,
                    std::shared_ptr<Poco::Net::WebSocket> ws,
                    std::shared_ptr<DocumentBroker> docBroker);

    virtual ~PrisonerSession();

    void setPeer(const std::shared_ptr<ClientSession>& peer) { _peer = peer; }
    bool shutdownPeer(Poco::UInt16 statusCode, const std::string& message);

private:

    virtual bool _handleInput(const char *buffer, int length) override;

    void forwardToPeer(const char *buffer, int length);

private:

    std::shared_ptr<DocumentBroker> _docBroker;
    std::weak_ptr<ClientSession> _peer;
    int _curPart;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
