#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2024 Bùi Gia Viện (BlissLabs) <shadichy@blisslabs.org>
#   SPDX-License-Identifier: GPL-3.0-or-later
#
#   Calamares is Free Software: see the License-Identifier above.
#

import os

import libcalamares

import gettext

_ = gettext.translation(
    "calamares-python",
    localedir=libcalamares.utils.gettext_path(),
    languages=libcalamares.utils.gettext_languages(),
    fallback=True,
).gettext


def pretty_name():
    return _("Post-config after installing bootloader.")


# This is going to be changed from various methods
status = pretty_name()


def pretty_status_message():
    return status


def mkdir_p(path):
    """Create directory.

    :param path:
    """
    if not os.path.exists(path):
        os.makedirs(path)


def run():
    """
    Post-config after installing bootloader
    """
    root_mount_point = libcalamares.globalstorage.value("rootMountPoint")

    if not root_mount_point:
        libcalamares.utils.warning("No mount point for root partition")
        return (
            _("No mount point for root partition"),
            _('globalstorage does not contain a "rootMountPoint" key.'),
        )
    if not os.path.exists(root_mount_point):
        libcalamares.utils.warning('Bad root mount point "{}"'.format(root_mount_point))
        return (
            _("Bad mount point for root partition"),
            _('rootMountPoint is "{}", which does not exist.'.format(root_mount_point)),
        )

    options = libcalamares.globalstorage.value("options")
    cmdline = open("/cdrom/cmdline.txt", "r").readline() + " " + options
    if "grub/android.cfg" in str(options):
        bootloader = "grub"
        command = ["echo", "Skipping"]
    elif "refind.conf" in str(options):
        bootloader = "refind"
        command = [
            "/usr/share/calamares/scripts/refind-conf",
            root_mount_point,
            cmdline,
        ]
    else:
        libcalamares.utils.warning("Unsupported bootloader: {}".format(bootloader))
        bootloader = "none"
        command = [
            "/usr/share/calamares/scripts/no-bootloader",
            root_mount_point,
            cmdline,
        ]

    # (Optional) Write CMDLINE to text file at $SRC for detection
    with open(os.path.join(root_mount_point, "cmdline.txt"), "w") as cmdlineFile:
        print(cmdline, file=cmdlineFile)

    libcalamares.utils.host_env_process_output(command, None)
    libcalamares.job.setprogress(1.0)
    return None
