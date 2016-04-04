/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
 * This is a trivial helper to allow bind mounting.
 */

#include <sys/mount.h>

int main(int argc, char **argv)
{
    if (argc < 3)
        return 1;

    int retval = mount (argv[1], argv[2], 0, MS_BIND, 0);
    if (retval)
        return retval;

    // apparently this has to be done in a 2nd pass.
    return mount(argv[1], argv[2], 0,
                 (MS_BIND | MS_REMOUNT | MS_NOATIME | MS_NODEV |
                  MS_NOSUID | MS_RDONLY  | MS_SILENT), 0);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
