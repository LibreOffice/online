/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_CLIENTSSESSION_HPP
#define INCLUDED_CLIENTSSESSION_HPP

#include "LOOLSession.hpp"
#include "MessageQueue.hpp"

#include <Poco/URI.h>

class DocumentBroker;
class PrisonerSession;

/// Represents a session to a LOOL client, in the WSD process.
class ClientSession final : public LOOLSession, public std::enable_shared_from_this<ClientSession>
{
public:
    ClientSession(const std::string& id,
                  const std::shared_ptr<Poco::Net::WebSocket>& ws,
                  const std::shared_ptr<DocumentBroker>& docBroker,
                  const Poco::URI& uriPublic,
                  const bool isReadOnly = false);

    virtual ~ClientSession();

    void setReadOnly();
    bool isReadOnly() const { return _isReadOnly; }

    void setPeer(const std::shared_ptr<PrisonerSession>& peer) { _peer = peer; }
    std::shared_ptr<PrisonerSession> getPeer() const { return _peer; }
    bool shutdownPeer(Poco::UInt16 statusCode);

    void setUserId(const std::string& userId) { _userId = userId; }
    void setUserName(const std::string& userName) { _userName = userName; }

    /**
     * Return the URL of the saved-as document when it's ready. If called
     * before it's ready, the call blocks till then.
     */
    std::string getSaveAsUrl(const unsigned timeoutMs)
    {
        const auto payload = _saveAsQueue.get(timeoutMs);
        return std::string(payload.data(), payload.size());
    }

    void setSaveAsUrl(const std::string& url)
    {
        _saveAsQueue.put(url);
    }

    std::shared_ptr<DocumentBroker> getDocumentBroker() const { return _docBroker.lock(); }

    /// Exact URI (including query params - access tokens etc.) with which
    /// client made the request to us
    const Poco::URI& getPublicUri() const { return _uriPublic; }

private:
    virtual bool _handleInput(const char* buffer, int length) override;

    bool loadDocument(const char* buffer, int length, Poco::StringTokenizer& tokens,
                      const std::shared_ptr<DocumentBroker>& docBroker);

    bool getStatus(const char* buffer, int length,
                   const std::shared_ptr<DocumentBroker>& docBroker);
    bool getCommandValues(const char* buffer, int length, Poco::StringTokenizer& tokens,
                          const std::shared_ptr<DocumentBroker>& docBroker);
    bool getPartPageRectangles(const char* buffer, int length,
                               const std::shared_ptr<DocumentBroker>& docBroker);

    bool sendTile(const char* buffer, int length, Poco::StringTokenizer& tokens,
                  const std::shared_ptr<DocumentBroker>& docBroker);
    bool sendCombinedTiles(const char* buffer, int length, Poco::StringTokenizer& tokens,
                           const std::shared_ptr<DocumentBroker>& docBroker);

    bool sendFontRendering(const char* buffer, int length, Poco::StringTokenizer& tokens,
                           const std::shared_ptr<DocumentBroker>& docBroker);

    bool forwardToChild(const std::string& message,
                        const std::shared_ptr<DocumentBroker>& docBroker);

private:
    std::weak_ptr<DocumentBroker> _docBroker;

    /// URI with which client made request to us
    const Poco::URI _uriPublic;

    /// Whether the session is opened as readonly
    bool _isReadOnly;

    /// Our peer that connects us to the child.
    std::shared_ptr<PrisonerSession> _peer;

    /// Store URLs of completed 'save as' documents.
    MessageQueue _saveAsQueue;

    int _loadPart;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
