// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*-
//
// This file is part of the LibreOffice project.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstring>

#include "ios.h"

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

extern "C" {
#import <native-code.h>
}

int loolwsd_server_socket_fd = -1;
lok::Document *lok_document = nullptr;
LibreOfficeKit *lo_kit;

// vim:set shiftwidth=4 softtabstop=4 expandtab:
