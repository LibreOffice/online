/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string>

#include <Poco/File.h>
#include <Poco/Path.h>

namespace JailUtil
{
    /// Bind mount a jail directory.
    bool bind(const std::string& source, const std::string& target);

    /// Remount a bound mount point as readonly.
    bool remountReadonly(const std::string& source, const std::string& target);

    /// Unmount a bind-mounted jail directory.
    bool unmount(const std::string& target);

    /// Remove the jail directory and all its contents.
    void removeJail(const std::string& path);

    /// Remove all the jails given their paths.
    inline void removeJails(const std::vector<std::string>& jails)
    {
        for (const auto& path : jails)
        {
            removeJail(path);
        }
    }

    /// Remove all jails.
    void cleanupJails(const std::string& jailRoot);

    /// Setup /dev/random and /dev/urandom in the given root.
    void setupDevNodes(const std::string& root);

    /// Setup the jails.
    void setupJails(bool bindMount, const std::string& jailRoot, const std::string& sysTemplate);

} // end namespace JailUtil

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
