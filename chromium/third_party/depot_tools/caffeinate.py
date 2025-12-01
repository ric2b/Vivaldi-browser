# Copyright 2025 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys

_NO_CAFFEINATE_FLAG = '--no-caffeinate'

_HELP_MESSAGE = f"""\
caffeinate:
  {_NO_CAFFEINATE_FLAG}  do not prepend `caffeinate` to ninja command
"""

def run(cmd, env=None):
    """Runs a command with `caffeinate` if it's on macOS."""
    if sys.platform == 'darwin':
        if '-h' in cmd or '--help' in cmd:
            print(_HELP_MESSAGE, file=sys.stderr)
        if _NO_CAFFEINATE_FLAG in cmd:
            cmd.remove(_NO_CAFFEINATE_FLAG)
        else:
            cmd = ['caffeinate'] + cmd
    return subprocess.call(cmd, env=env)
