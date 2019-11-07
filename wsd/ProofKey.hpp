/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// WOPI proof management
#ifndef INCLUDED_PROOFKEY_HPP
#define INCLUDED_PROOFKEY_HPP

#include <string>
#include <utility>
#include <vector>

std::vector<std::pair<std::string, std::string>> GetProofHeaders(
    const std::string& access_token, // utf-8
    const std::string& uri); // utf-8
std::vector<std::pair<std::string, std::string>> GetProofKeyAttributes();

#endif // INCLUDED_PROOFKEY_HPP

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
