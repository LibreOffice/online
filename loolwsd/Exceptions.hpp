/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Exception classes to differentiate between the
// different error situations and handling.
#ifndef INCLUDED_EXCEPTIONS_HPP
#define INCLUDED_EXCEPTIONS_HPP

#include <exception>

// Generic LOOL errors and base for others.
class LoolException : public std::runtime_error
{
protected:
    using std::runtime_error::runtime_error;
};

/// A bad-request exception that is meant to signify,
/// and translate into, an HTTP bad request.
class BadRequestException : public LoolException
{
public:
    using LoolException::LoolException;
};

/// A bad-argument exception that is meant to signify,
/// and translate into, an HTTP bad request.
class BadArgumentException : public BadRequestException
{
public:
    using BadRequestException::BadRequestException;
};

/// An authorization exception that is means to signify,
/// and translate into, an HTTP unauthorized error.
class UnauthorizedRequestException : public LoolException
{
public:
    using LoolException::LoolException;
};

/// An generic error-message exception meant to
/// propagate via a valid WebSocket to the client.
/// The contents of what() will be displayed on screen.
class WebSocketErrorMessageException : public LoolException
{
public:
    using LoolException::LoolException;
};


#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
