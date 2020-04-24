/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "config.h"

#if MOBILEAPP

#include <LibreOfficeKit/LibreOfficeKit.hxx>

#ifdef IOS
#import "CODocument.h"
#endif

// On iOS at least we want to be able to have several documents open
// in the same app process.

// It is somewhat complicated to make sure we access the same
// LibreOfficeKit object for the document in both the iOS-specific
// Objective-C++ code and in the mostly generic Online C++ code.

// We pass around a numeric ever-increasing document identifier that
// gets biumped for each document the system asks the app to open.

// For iOS, it is the static std::atomic<unsigned> appDocIdCounter(1)
// in CODocument.mm.

// In practice it will probably be equivalent to the
// DocumentBroker::DocBrokerId or the, but there might be situations
// where multi-threading and opening of several documents in sequence
// very quickly might cause discrepancies, so it is better to usea
// different counter to be sure.

struct DocumentData
{
    lok::Document *loKitDocument;
#ifdef IOS
    CODocument *coDocument;
#endif

    DocumentData() :
        loKitDocument(nullptr)
#ifdef IOS
        , coDocument(nil)
#endif
    {
    }
};

DocumentData &allocateDocumentDataForMobileAppDocId(unsigned docId);
DocumentData &getDocumentDataForMobileAppDocId(unsigned docId);
void deallocateDocumentDataForMobileAppDocId(unsigned docId);

#endif
