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

#include <cstring>
#include <map>
#include <string>

#include <Poco/StringTokenizer.h>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

namespace LOOLProtocol
{
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
    bool parseNameIntegerPair(const std::string& token, std::string& name, int& value);

    bool getTokenInteger(const std::string& token, const std::string& name, int& value);
    bool getTokenString(const std::string& token, const std::string& name, std::string& value);
    bool getTokenKeyword(const std::string& token, const std::string& name, const std::map<std::string, int>& map, int& value);

    bool getTokenInteger(const Poco::StringTokenizer& tokens, const std::string& name, int& value);
    bool getTokenString(const Poco::StringTokenizer& tokens, const std::string& name, std::string& value);
    bool getTokenKeyword(const Poco::StringTokenizer& tokens, const std::string& name, const std::map<std::string, int>& map, int& value);

    // Functions that parse messages. All return false if parsing fails
    bool parseStatus(const std::string& message, LibreOfficeKitDocumentType& type, int& nParts, int& currentPart, int& width, int& height);

    inline
    std::string getDelimitedInitialSubstring(const char *message, const int length, const char delimiter)
    {
        if (message == nullptr || length <= 0)
        {
            return "";
        }

        const char *foundDelimiter = static_cast<const char *>(std::memchr(message, delimiter, length));
        const auto size = (foundDelimiter == nullptr ? length : foundDelimiter - message);
        return std::string(message, size);
    }

    /// Returns the first token of a message.
    inline
    std::string getFirstToken(const char *message, const int length)
    {
        return getDelimitedInitialSubstring(message, length, ' ');
    }

    template <typename T>
    std::string getFirstToken(const T& message)
    {
        return getFirstToken(message.data(), message.size());
    }

    /// Returns true if the token is a user-interaction token.
    /// Currently this excludes commands sent automatically.
    /// Notice that this doesn't guarantee editing activity,
    /// rather just user interaction with the UI.
    inline
    bool tokenIndicatesUserInteraction(const std::string& token)
    {
        // Exclude tokens that include these keywords,
        // such as canceltiles statusindicator.
        return (token.find("tile") == std::string::npos &&
                token.find("status") == std::string::npos &&
                token.find("state") == std::string::npos);
    }

    /// Returns the first line of a message.
    inline
    std::string getFirstLine(const char *message, const int length)
    {
        return getDelimitedInitialSubstring(message, length, '\n');
    }

    template <typename T>
    std::string getFirstLine(const T& message)
    {
        return getFirstLine(message.data(), message.size());
    }

    /// Returns an abbereviation of the message (the first line, indicating truncation).
    std::string getAbbreviatedMessage(const char *message, const int length);

    inline
    std::string getAbbreviatedMessage(const std::string& message)
    {
        return getAbbreviatedMessage(message.data(), message.size());
    }

    template <typename T>
    std::string getAbbreviatedMessage(const T& message)
    {
        return getAbbreviatedMessage(message.data(), message.size());
    }
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
