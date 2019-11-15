/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SESSION_HPP
#define INCLUDED_SESSION_HPP

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <ostream>

#include <Poco/Buffer.h>
#include <Poco/Path.h>
#include <Poco/Process.h>
#include <Poco/Types.h>

#include "Protocol.hpp"
#include "Log.hpp"
#include "MessageQueue.hpp"
#include "Message.hpp"
#include "TileCache.hpp"
#include "WebSocketHandler.hpp"

/// Base class of a WebSocket session.
class Session : public WebSocketHandler
{
public:
    const std::string& getId() const { return _id; }
    const std::string& getName() const { return _name; }
    bool isDisconnected() const { return _disconnected; }

    virtual void setReadOnly() { _isReadOnly = true; }
    bool isReadOnly() const { return _isReadOnly; }

    virtual bool sendBinaryFrame(const char* buffer, int length);
    virtual bool sendTextFrame(const char* buffer, const int length);
    bool sendTextFrame(const std::string& text)
    {
        return sendTextFrame(text.data(), text.size());
    }

    template <std::size_t N>
    bool sendTextFrame(const char (&buffer)[N])
    {
        return (buffer != nullptr && N > 0 ? sendTextFrame(buffer, N) : false);
    }

    bool sendTextFrame(const char* buffer)
    {
        return (buffer != nullptr ? sendTextFrame(buffer, std::strlen(buffer)) : false);
    }

    virtual void handleMessage(bool fin, WSOpCode code, std::vector<char> &data) override;

    /// Invoked when we want to disconnect a session.
    virtual void disconnect();

    /// Called to handle disconnection command from socket.
    virtual bool handleDisconnect();

    void shutdown(const WebSocketHandler::StatusCodes statusCode = WebSocketHandler::StatusCodes::NORMAL_CLOSE,
                  const std::string& statusMessage = "");

    bool isActive() const { return _isActive; }
    void setIsActive(bool active) { _isActive = active; }

    /// Returns the inactivity time of the client in milliseconds.
    double getInactivityMS() const
    {
        const auto duration = (std::chrono::steady_clock::now() - _lastActivityTime);
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    void closeFrame() { _isCloseFrame = true; };
    bool isCloseFrame() const { return _isCloseFrame; }

    void getIOStats(uint64_t &sent, uint64_t &recv);

    void setUserId(const std::string& userId) { _userId = userId; }

    const std::string& getUserId() const { return _userId; }

    void setWatermarkText(const std::string& watermarkText) { _watermarkText = watermarkText; }

    void setUserExtraInfo(const std::string& userExtraInfo) { _userExtraInfo = userExtraInfo; }

    void setUserName(const std::string& userName) { _userName = userName; }

    const std::string& getUserName() const {return _userName; }

    const std::string& getUserNameAnonym() const { return _userNameAnonym; }

    bool isDocPasswordProtected() const { return _isDocPasswordProtected; }

    const std::string& getDocOptions() const { return _docOptions; }

    bool hasWatermark() const { return !_watermarkText.empty() && _watermarkOpacity > 0.0; }

    const std::string& getWatermarkText() const { return _watermarkText; }

    double getWatermarkOpacity() const { return _watermarkOpacity; }

    const std::string& getLang() const { return _lang; }

    bool getHaveDocPassword() const { return _haveDocPassword; }

    const std::string& getDocPassword() const { return _docPassword; }

    const std::string& getUserExtraInfo() const { return _userExtraInfo; }

    const std::string& getDocURL() const { return  _docURL; }

    const std::string& getJailedFilePath() const { return _jailedFilePath; }

    const std::string& getJailedFilePathAnonym() const { return _jailedFilePathAnonym; }

    int getHash() { return _hash; }

    void setHash(const std::string& text);

    void setHash(const int hash) { _hash = hash; };

protected:
    Session(const std::string& name, const std::string& id, bool readonly);
    virtual ~Session();

    /// Parses the options of the "load" command,
    /// shared between MasterProcessSession::loadDocument() and ChildProcessSession::loadDocument().
    void parseDocOptions(const std::vector<std::string>& tokens, int& part, std::string& timestamp, std::string& doctemplate);

    void updateLastActivityTime()
    {
        _lastActivityTime = std::chrono::steady_clock::now();
    }

    /// Internal lock shared with derived classes.
    std::unique_lock<std::mutex> getLock()
    {
        return std::unique_lock<std::mutex>(_mutex);
    }

    void dumpState(std::ostream& os) override;

private:
    virtual bool _handleInput(const char* buffer, int length) = 0;

    /// A session ID specific to an end-to-end connection (from user to lokit).
    const std::string _id;

    /// A readable name that identifies our peer and ID.
    const std::string _name;

    /// True if we have been disconnected.
    std::atomic<bool> _disconnected;
    /// True if the user is active, otherwise false (switched tabs).
    std::atomic<bool> _isActive;

    /// Time of the last interactive event being received
    std::chrono::steady_clock::time_point _lastActivityTime;

    // Whether websocket received close frame.  Closing Handshake
    std::atomic<bool> _isCloseFrame;

    std::mutex _mutex;

    /// Whether the session is opened as readonly
    bool _isReadOnly;

    /// The actual URL, also in the child, even if the child never accesses that.
    std::string _docURL;

    /// The Jailed document path.
    std::string _jailedFilePath;

    /// The Jailed document path, anonymized for logging.
    std::string _jailedFilePathAnonym;

    /// Password provided, if any, to open the document
    std::string _docPassword;

    /// If password is provided or not
    bool _haveDocPassword;

    /// Whether document is password protected
    bool _isDocPasswordProtected;

    /// Document options: a JSON string, containing options (rendering, also possibly load in the future).
    std::string _docOptions;

    /// Id of the user to whom the session belongs to.
    std::string _userId;

    /// Id of the user to whom the session belongs to, anonymized for logging.
    std::string _userIdAnonym;

    /// Name of the user to whom the session belongs to.
    std::string _userName;

    /// Name of the user to whom the session belongs to, anonymized for logging.
    std::string _userNameAnonym;

    /// Extra info per user, mostly mail, avatar, links, etc.
    std::string _userExtraInfo;

    /// In case a watermark has to be rendered on each tile.
    std::string _watermarkText;

    /// Opacity in case a watermark has to be rendered on each tile.
    double _watermarkOpacity;

    /// Language for the document based on what the user has in the UI.
    std::string _lang;

    /// Hash for normalizedViewId which is basically an identity for the tile to
    /// choose what to render on and send it to its subscribers
    /// it is the close-to-unique integer representation of a string like Watermarks etc.
    int _hash;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
