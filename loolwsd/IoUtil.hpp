/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_IOUTIL_HPP
#define INCLUDED_IOUTIL_HPP

#include <functional>
#include <string>
#include <memory>

#include <sys/poll.h>

#include <Poco/Net/WebSocket.h>
#include <Poco/Logger.h>

namespace IoUtil
{
    /// Synchronously process WebSocket requests and dispatch to handler.
    //. Handler returns false to end.
    void SocketProcessor(std::shared_ptr<Poco::Net::WebSocket> ws,
                         std::function<bool(const std::vector<char>&)> handler,
                         std::function<bool()> stopPredicate);

    /// Call WebSocket::shutdown() ignoring Poco::IOException.
    void shutdownWebSocket(std::shared_ptr<Poco::Net::WebSocket> ws);

    ssize_t writeFIFO(int pipe, const char* buffer, ssize_t size);
    inline
    ssize_t writeFIFO(int pipe, const std::string& message)
    {
        return writeFIFO(pipe, message.c_str(), message.size());
    }

    ssize_t readFIFO(int pipe, char* buffer, ssize_t size);

    class PipeReader
    {
    public:
        PipeReader(const std::string& name, const int pipe) :
            _name(name),
            _pipe(pipe)
        {
        }

        const std::string& getName() const { return _name; }

        /// Reads a single line from the pipe.
        /// Returns 0 for timeout, <0 for error, and >0 on success.
        /// On success, line will contain the read message.
        int readLine(std::string& line,
                     std::function<bool()> stopPredicate);

    private:
        const std::string _name;
        const int _pipe;
        std::string _data;
    };
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
