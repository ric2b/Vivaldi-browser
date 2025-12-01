# Copyright (c) 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Defines common conditions for the new auth stack migration."""

from __future__ import annotations

import os
import sys

import scm


def Enabled() -> bool:
    """Returns True if new auth stack is enabled."""
    if not SwitchedOn():
        return False
    if _HasGitcookies():
        _PrintGitcookiesWarning()
        return False
    return True


def SwitchedOn() -> bool:
    """Returns True if new auth stack is "switched on".

    Note that this does not necessarily mean that new auth is enabled.
    In particular, we still disable new auth if a .gitcookies file is
    present, to protect bots that haven't been migrated yet.
    """
    if Default():
        return not ExplicitlyDisabled()
    return ExplicitlyEnabled()


def Default() -> bool:
    "Returns default enablement status for new auth stack."
    return True


def _HasGitcookies() -> bool:
    """Returns True if user has gitcookies file."""
    return os.path.exists(os.path.expanduser('~/.gitcookies'))


_warning_printed = False


def _PrintGitcookiesWarning() -> None:
    global _warning_printed
    if _warning_printed:
        return
    _warning_printed = True
    sys.stderr.write('''
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Warning !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
depot_tools will soon stop using the .gitcookies file for authentication.

To silence this warning, please run `git cl creds-check` which will help you fix this.

If you encounter any issues, please report them using:
https://issues.chromium.org/issues/new?component=1456702&template=2076315
--------------------------------------------------------------------------------

''')


def ExplicitlyEnabled() -> bool:
    """Returns True if new auth stack is explicitly enabled.

    Directly checks config and doesn't do gitcookie check.
    """
    return scm.GIT.GetConfig(os.getcwd(),
                             'depot-tools.usenewauthstack') in ('yes', 'on',
                                                                'true', '1')


def ExplicitlyDisabled() -> bool:
    """Returns True if new auth stack is explicitly disabled."""
    return scm.GIT.GetConfig(os.getcwd(),
                             'depot-tools.usenewauthstack') in ('no', 'off',
                                                                'false', '0')
