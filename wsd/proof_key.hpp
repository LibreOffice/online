/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// WOPI proof management
#ifndef INCLUDED_WOPISEC_HPP
#define INCLUDED_WOPISEC_HPP

#include <string>
#include <chrono>
#include <Poco/Net/HTTPRequest.h>

int64_t DotNetTicks(const std::chrono::system_clock::time_point& utc);
std::string GetProof(
    const std::string& access_token, // utf-8
    const std::string& uri, // utf-8
    int64_t ticks); // .Net ticks
std::string SignProof(const std::string proof);
std::string GetProofKey();
std::string GetProofModulus();
std::string GetProofExponent();

#endif // INCLUDED_WOPISEC_HPP

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
