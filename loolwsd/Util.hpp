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

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>

#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/Process.h>
#include <Poco/RegularExpression.h>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

namespace Util
{
    namespace rng
    {
        void reseed();
        unsigned getNext();

        /// Generates a random string suitable for
        /// file/directory names.
        std::string getFilename(const size_t length);
    }

    /// Encode an integral ID into a string, with padding support.
    std::string encodeId(const unsigned number, const int padding = 5);
    /// Decode an integral ID from a string.
    unsigned decodeId(const std::string& str);

    bool windowingAvailable();

#ifndef BUILDING_TESTS

    /// Send a message to all clients.
    void alertAllUsers(const std::string& msg);

    /// Send a 'error:' message with the specified cmd and kind parameters to all connected
    /// clients. This function can be called either in loolwsd or loolkit processes, even if only
    /// loolwsd obviously has contact with the actual clients; in loolkit it will be forwarded to
    /// loolwsd for redistribution. (This function must be implemented separately in each program
    /// that uses it, it is not in Util.cpp.)
    void alertAllUsers(const std::string& cmd, const std::string& kind);
#else

    /// No-op implementation in the test programs
    inline void alertAllUsers(const std::string&)
    {
    }

    /// No-op implementation in the test programs
    inline void alertAllUsers(const std::string&, const std::string&)
    {
    }
#endif

    /// Assert that a lock is already taken.
    template <typename T>
    void assertIsLocked(const T& lock)
    {
        assert(lock.owns_lock());
    }

    inline void assertIsLocked(std::mutex& mtx)
    {
        assert(!mtx.try_lock());
    }

    int getMemoryUsage(const Poco::Process::PID nPid);

    std::string replace(const std::string& s, const std::string& a, const std::string& b);

    std::string formatLinesForLog(const std::string& s);

    void setThreadName(const std::string& s);

    /// Get version information
    void getVersionInfo(std::string& version, std::string& hash);

    /// Return a string that is unique across processes and calls.
    std::string UniqueId();

    /// Trim spaces from the left. Just spaces.
    inline std::string& ltrim(std::string& s)
    {
        const auto pos = s.find_first_not_of(' ');
        if (pos != std::string::npos)
        {
            s = s.substr(pos);
        }

        return s;
    }

    /// Trim spaces from the left and copy. Just spaces.
    inline std::string ltrimmed(const std::string& s)
    {
        const auto pos = s.find_first_not_of(' ');
        if (pos != std::string::npos)
        {
            return s.substr(pos);
        }

        return s;
    }

    /// Given one or more patterns to allow, and one or more to deny,
    /// the match member will return true if, and only if, the subject
    /// matches the allowed list, but not the deny.
    /// By default, everything is denied.
    class RegexListMatcher
    {
    public:
        RegexListMatcher() :
            _allowByDefault(false)
        {
        }

        RegexListMatcher(const bool allowByDefault) :
            _allowByDefault(allowByDefault)
        {
        }

        RegexListMatcher(std::initializer_list<std::string> allowed) :
            _allowByDefault(false),
            _allowed(allowed)
        {
        }

        RegexListMatcher(std::initializer_list<std::string> allowed,
                         std::initializer_list<std::string> denied) :
            _allowByDefault(false),
            _allowed(allowed),
            _denied(denied)
        {
        }

        RegexListMatcher(const bool allowByDefault,
                         std::initializer_list<std::string> denied) :
            _allowByDefault(allowByDefault),
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
            return (_allowByDefault || match(_allowed, subject)) && !match(_denied, subject);
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
        const bool _allowByDefault;
        std::set<std::string> _allowed;
        std::set<std::string> _denied;
    };

    /// A logical constant that is allowed to initialize
    /// exactly once and checks usage before initialization.
    template <typename T>
    class RuntimeConstant
    {
        T _value;
        std::atomic<bool> _initialized;

    public:
        RuntimeConstant()
            : _value()
            , _initialized(false)
        {
        }

        /// Use a compile-time const instead.
        RuntimeConstant(const T& value) = delete;

        const T& get()
        {
            if (_initialized)
            {
                return _value;
            }

            throw std::runtime_error("RuntimeConstant instance read before being initialized.");
        }

        void set(const T& value)
        {
            assert(!_initialized);

            _initialized = true;
            _value = value;
        }
    };
} // end namespace Util

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
