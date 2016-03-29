/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_LOOLPROTOCOL_HPP
#define INCLUDED_LOOLPROTOCOL_HPP

#include <map>
#include <string>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

namespace LOOLProtocol
{
    // The frames sent from the client to the server are called
    // "commands" and those sent from the server to the client are
    // called "messages". At least until I come up with a better
    // terminology.

    // I don't want to call the latter "responses"
    // because they are not necessarily responses to some "request" or
    // "command". Also "event" would be misleading because that is
    // typically used for things originated by the user, like key or
    // mouse events. And in fact, those are here part of the
    // "commands".

    // Not sure if these enums will be needed
    enum class Command
    {
        GETTEXTSELECTION,
        KEY,
        LOAD,
        MOUSE,
        RESETSELECTION,
        SAVEAS,
        SELECTGRAPHIC,
        SELECTTEXT,
        STATUS,
        TILE,
        UNO,
    };

    enum class Message
    {
        CHILD,
        CURSOR_VISIBLE,
        ERROR,
        GRAPHIC_SELECTION,
        HYPERLINK_CLICKED,
        INVALIDATE_CURSOR,
        INVALIDATE_TILES,
        STATUS,
        TEXT_SELECTION,
        TEXT_SELECTION_END,
        TEXT_SELECTION_START,
        TILE,
    };

    // Protocol Version Number.
    // See protocol.txt.
    constexpr unsigned ProtocolMajorVersionNumber = 0;
    constexpr unsigned ProtocolMinorVersionNumber = 1;

    inline
    std::string GetProtocolVersion()
    {
        return std::to_string(ProtocolMajorVersionNumber) + '.'
             + std::to_string(ProtocolMinorVersionNumber);
    }

    // Parse a string into a version tuple.
    // Negative numbers for error.
    std::tuple<int, int, std::string> ParseVersion(const std::string& version);

    bool stringToInteger(const std::string& input, int& value);

    bool getTokenInteger(const std::string& token, const std::string& name, int& value);
    bool getTokenString(const std::string& token, const std::string& name, std::string& value);
    bool getTokenKeyword(const std::string& token, const std::string& name, const std::map<std::string, int>& map, int& value);

    // Functions that parse messages. All return false if parsing fails
    bool parseStatus(const std::string& message, LibreOfficeKitDocumentType& type, int& nParts, int& currentPart, int& width, int& height);

    /// Returns the first token of a message given a delimiter.
    inline
    std::string getFirstToken(const char *message, const int length, const char delim = ' ')
    {
        if (message == nullptr || length <= 0)
        {
            return "";
        }

        const char *endOfLine = static_cast<const char *>(std::memchr(message, delim, length));
        const auto size = (endOfLine == nullptr ? length : endOfLine - message);
        return std::string(message, size);
    }

    inline
    std::string getFirstToken(const std::vector<char>& message)
    {
        return getFirstToken(message.data(), message.size());
    }

    /// Returns the first line of a message.
    inline
    std::string getFirstLine(const char *message, const int length)
    {
        return getFirstToken(message, length, '\n');
    }

    inline
    std::string getFirstLine(const std::vector<char>& message)
    {
        return getFirstLine(message.data(), message.size());
    }

    /// Returns an abbereviation of the message (the first line, indicating truncation).
    std::string getAbbreviatedMessage(const char *message, const int length);
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
