/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_UTIL_HPP
#define INCLUDED_UTIL_HPP

#include <string>
#include <sstream>
#include <functional>
#include <memory>
#include <set>

#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/Process.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/RegularExpression.h>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include "Log.hpp"

/// Flag to stop pump loops.
extern volatile bool TerminationFlag;

namespace Util
{
    namespace rng
    {
        void reseed();
        unsigned getNext();
    }

    /// Encode an integral ID into a string, with padding support.
    std::string encodeId(const unsigned number, const int padding = 5);
    /// Decode an integral ID from a string.
    unsigned decodeId(const std::string& str);

    /// Creates a randomly name directory within path and returns the name.
    std::string createRandomDir(const std::string& path);

    bool windowingAvailable();

    // Sadly, older libpng headers don't use const for the pixmap pointer parameter to
    // png_write_row(), so can't use const here for pixmap.
    bool encodeBufferToPNG(unsigned char* pixmap, int width, int height,
                           std::vector<char>& output, LibreOfficeKitTileMode mode);
    bool encodeSubBufferToPNG(unsigned char* pixmap, int startX, int startY, int width, int height,
                              int bufferWidth, int bufferHeight,
                              std::vector<char>& output, LibreOfficeKitTileMode mode);

    /// Assert that a lock is already taken.
    template <typename T>
    void assertIsLocked(T& lock)
    {
        assert(!lock.try_lock());
    }

    /// Safely remove a file or directory.
    /// Supresses exception when the file is already removed.
    /// This can happen when there is a race (unavoidable) or when
    /// we don't care to check before we remove (when no race exists).
    inline
    void removeFile(const std::string& path, const bool recursive = false)
    {
        try
        {
            Poco::File(path).remove(recursive);
        }
        catch (const std::exception&)
        {
            // Already removed or we don't care about failures.
        }
    }

    inline
    void removeFile(const Poco::Path& path, const bool recursive = false)
    {
        removeFile(path.toString(), recursive);
    }

    /// Make a temp copy of a file.
    /// Primarily used by tests to avoid tainting the originals.
    /// srcDir shouldn't end with '/' and srcFilename shouldn't contain '/'.
    /// Returns the created file path.
    inline
    std::string getTempFilePath(const std::string srcDir, const std::string& srcFilename)
    {
        const std::string srcPath = srcDir + '/' + srcFilename;

        std::string dstPath = std::tmpnam(nullptr);
        dstPath += '_' + srcFilename;

        Poco::File(srcPath).copyTo(dstPath);
        return dstPath;
    }

    /// Returns the name of the signal.
    const char *signalName(int signo);

    /// Trap signals to cleanup and exit the process gracefully.
    void setTerminationSignals();
    void setFatalSignals();

    void requestTermination(const Poco::Process::PID& pid);

    int getMemoryUsage(const Poco::Process::PID nPid);

    std::string replace(const std::string& s, const std::string& a, const std::string& b);

    std::string formatLinesForLog(const std::string& s);

    void setThreadName(const std::string& s);

    /// Display version information
    void displayVersionInfo(const char *app);

    /// Return a string that is unique across processes and calls.
    std::string UniqueId();

    /// Given one or more patterns to allow, and one or more to deny,
    /// the match member will return true if, and only if, the subject
    /// matches the allowed list, but not the deny.
    /// By default, everything is denied.
    class RegexListMatcher
    {
    public:
        RegexListMatcher()
        {
        }

        RegexListMatcher(std::initializer_list<std::string> allowed) :
            _allowed(allowed)
        {
        }

        RegexListMatcher(std::initializer_list<std::string> allowed,
                         std::initializer_list<std::string> denied) :
            _allowed(allowed),
            _denied(denied)
        {
        }

        void allow(const std::string& pattern) { _allowed.insert(pattern); }
        void deny(const std::string& pattern)
        {
            _allowed.erase(pattern);
            _denied.insert(pattern);
        }

        void clear()
        {
            _allowed.clear();
            _denied.clear();
        }

        bool match(const std::string& subject) const
        {
            return (match(_allowed, subject) && !match(_denied, subject));
        }

    private:
        bool match(const std::set<std::string>& set, const std::string& subject) const
        {
            if (set.find(subject) != set.end())
            {
                return true;
            }

            // Not a perfect match, try regex.
            for (const auto& value : set)
            {
                try
                {
                    // Not performance critical to warrant caching.
                    Poco::RegularExpression re(value, Poco::RegularExpression::RE_CASELESS);
                    Poco::RegularExpression::Match reMatch;

                    // Must be a full match.
                    if (re.match(subject, reMatch) && reMatch.offset == 0 && reMatch.length == subject.size())
                    {
                        return true;
                    }
                }
                catch (const std::exception& exc)
                {
                    // Nothing to do; skip.
                }
            }

            return false;
        }

    private:
        std::set<std::string> _allowed;
        std::set<std::string> _denied;
    };

};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
