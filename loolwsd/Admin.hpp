/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ADMIN_HPP
#define INCLUDED_ADMIN_HPP

#include <Poco/Types.h>
#include <Poco/Net/HTTPServer.h>

#include "AdminModel.hpp"

const std::string FIFO_NOTIFY = "loolnotify.fifo";

using Poco::Runnable;

/// An admin command processor.
class Admin : public Runnable
{
public:
    Admin(const int brokerPipe, const int notifyPipe);

    ~Admin();

    static int getBrokerPid() { return Admin::BrokerPipe; }

    std::string getDocuments();

    void handleInput(std::string& message);

    void run() override;
    
    void updateModel();

private:
    Poco::Net::HTTPServer _srv;
    static int BrokerPipe;
    static int NotifyPipe;
    AdminModel model;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
