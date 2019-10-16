/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mutex>

#include <LibreOfficeKit/LibreOfficeKit.hxx>

extern int loolwsd_server_socket_fd;
extern lok::Document *lok_document;

extern LibreOfficeKit *lo_kit;

extern std::mutex lokit_main_mutex;

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
