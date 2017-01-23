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

#include "Session.hpp"

class DocumentBroker;
class ClientSession;

/// Represents an internal session to a Kit process, in the WSD process.
/// This doesn't really have a direct connection to any Kit process, rather
/// all communication to said Kit process is really handled by DocumentBroker.
class PrisonerSession final : public Session
{
public:
    PrisonerSession(ClientSession& clientSession,
                    std::shared_ptr<DocumentBroker> docBroker);

    virtual ~PrisonerSession();

    /// Returns true if we've got status message.
    bool gotStatus() const { return _gotStatus; }

private:
    /// Handle messages from the Kit to the client.
    virtual bool _handleInput(const char* buffer, int length) override;

    bool forwardToPeer(const std::shared_ptr<Message>& payload);

private:
    std::shared_ptr<DocumentBroker> _docBroker;
    ClientSession& _peer;
    int _curPart;
    bool _gotStatus;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
