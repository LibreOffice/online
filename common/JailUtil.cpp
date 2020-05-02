/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config.h>

#include "FileUtil.hpp"
#include "JailUtil.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux
#include <sys/sysmacros.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Log.hpp"

namespace JailUtil
{
    bool loolmount(const std::string& arg, std::string source, std::string target)
    {
        source = Util::trim(source, '/');
        target = Util::trim(target, '/');
        const std::string cmd = Poco::Path(Util::getApplicationPath(), "loolmount").toString() + ' '
                                + arg + ' ' + source + ' ' + target;
        LOG_TRC("Executing loolmount command: " << cmd);
        return !system(cmd.c_str());
    }

    bool bind(const std::string& source, const std::string& target)
    {
        Poco::File(target).createDirectory();
        const bool res = loolmount("-b", source, target);
        if (res)
            LOG_TRC("Bind-mounted [" << source << "] -> [" << target << "].");
        else
            LOG_ERR("Failed to bind-mount [" << source << "] -> [" << target << "].");
        return res;
    }

    bool remountReadonly(const std::string& source, const std::string& target)
    {
        Poco::File(target).createDirectory();
        const bool res = loolmount("-r", source, target);
        if (res)
            LOG_TRC("Mounted [" << source << "] -> [" << target << "].");
        else
            LOG_ERR("Failed to mount [" << source << "] -> [" << target << "].");
        return res;
    }

    bool unmount(const std::string& target)
    {
        LOG_DBG("Unmounting [" << target << "].");
        const bool res = loolmount("-u", "", target);
        if (res)
            LOG_TRC("Unmounted [" << target << "] successfully.");
        else
            LOG_ERR("Failed to unmount [" << target << "].");
        return res;
    }

    bool safeRemoveDir(const std::string& path)
    {
        unmount(path);

        static const bool bind = std::getenv("LOOL_BIND_MOUNT");

        // We must be empty if we had mounted.
        if (bind && !FileUtil::isEmptyDirectory(path))
        {
            LOG_WRN("Path [" << path << "] is not empty. Will not remove it.");
            return false;
        }

        // Recursively remove if link/copied.
        FileUtil::removeFile(path, !bind);
        return true;
    }

    void removeJail(const std::string& path)
    {
        LOG_INF("Removing jail [" << path << "].");

        // Unmount the tmp directory. Don't care if we fail.
        const std::string tmpPath = Poco::Path(path, "tmp").toString();
        FileUtil::removeFile(tmpPath, true); // Delete tmp contents with prejeduce.
        unmount(tmpPath);

        // Unmount the loTemplate directory.
        unmount(Poco::Path(path, "lo").toString());

        // Unmount the jail (sysTemplate).
        safeRemoveDir(path);
    }

    void setupDevNodes(const std::string& root)
    {
        // Create the urandom and random devices
        Poco::File(Poco::Path(root, "/dev")).createDirectory();
        if (!Poco::File(root + "/dev/random").exists())
        {
            LOG_DBG("Making /dev/random node in [" << root << "/dev].");
            if (mknod((root + "/dev/random").c_str(),
                      S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                      makedev(1, 8))
                != 0)
            {
                LOG_SYS("mknod(" << root << "/dev/random) failed. Mount must not use nodev flag.");
            }
        }

        if (!Poco::File(root + "/dev/urandom").exists())
        {
            LOG_DBG("Making /dev/urandom node in [" << root << "/dev].");
            if (mknod((root + "/dev/urandom").c_str(),
                      S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                      makedev(1, 9))
                != 0)
            {
                LOG_SYS("mknod(" << root << "/dev/urandom) failed. Mount must not use nodev flag.");
            }
        }
    }

    /// This cleans up the jails directories.
    /// Note that we assume the templates are mounted
    /// and we unmount first. This is critical, because
    /// otherwise when mounting is disabled we may
    /// inadvertently delete the contents of the mount-points.
    void cleanupJails(const std::string& root)
    {
        if (!Poco::File(root).isDirectory())
            return;

        LOG_INF("Cleaning up childroot directory [" << root << "].");

        if (FileUtil::pathExists(root + "/lo"))
        {
            // This is a jail.
            removeJail(root);
        }
        else
        {
            // Not a jail, recurse. UnitTest creates sub-directories.
            std::vector<std::string> jails;
            Poco::File(root).list(jails);
            for (const auto& jail : jails)
            {
                const Poco::Path path(root, jail);
                if (jail == "tmp") // Delete tmp with prejeduce.
                    FileUtil::removeFile(path.toString(), true);
                else
                    cleanupJails(path.toString());
            }
        }

        // Remove empty directories.
        if (FileUtil::isEmptyDirectory(root))
            FileUtil::removeFile(root, false);
        else
            LOG_WRN("Jails root directory [" << root << "] is not empty. Will not remove it.");
    }

    void setupJails(bool bindMount, const std::string& jailRoot, const std::string& sysTemplate)
    {
        // Start with a clean slate.
        cleanupJails(jailRoot);
        Poco::File(jailRoot).createDirectories();

        unsetenv("LOOL_BIND_MOUNT"); // Clear to avoid surprises.
        if (bindMount)
        {
            // Test mounting to verify it actually works,
            // as it might not function in some systems.
            const std::string target = Poco::Path(jailRoot, "lool_test_mount").toString();
            if (bind(sysTemplate, target))
            {
                safeRemoveDir(target);
                setenv("LOOL_BIND_MOUNT", "1", 1);
                LOG_INF("Enabling Bind-Mounting of jail contents for better performance per "
                        "mount_jail_tree config in loolwsd.xml.");
            }
            else
                LOG_ERR(
                    "Bind-Mounting fails and will be disabled for this run. To disable permanently "
                    "set mount_jail_tree config entry in loolwsd.xml to false.");
        }
        else
            LOG_INF("Disabling Bind-Mounting of jail contents per "
                    "mount_jail_tree config in loolwsd.xml.");
    }

} // namespace JailUtil

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
