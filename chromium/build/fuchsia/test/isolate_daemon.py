#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sets up the isolate daemon environment to run test on the bots."""

import os
import sys
import tempfile

from contextlib import AbstractContextManager
from typing import List

from common import catch_sigterm, set_ffx_isolate_dir, start_ffx_daemon, \
                   stop_ffx_daemon, wait_for_sigterm
from ffx_integration import ScopedFfxConfig


class IsolateDaemon(AbstractContextManager):
    """Sets up the environment of an isolate ffx daemon."""
    class IsolateDir(AbstractContextManager):
        """Sets up the ffx isolate dir to a temporary folder."""
        def __init__(self):
            self._temp_dir = tempfile.TemporaryDirectory()

        def __enter__(self):
            set_ffx_isolate_dir(self._temp_dir.__enter__())
            return self

        def __exit__(self, exc_type, exc_value, traceback):
            try:
                self._temp_dir.__exit__(exc_type, exc_value, traceback)
            except OSError:
                # Ignore the errors when cleaning up the temporary folder.
                pass
            return True

        def name(self):
            """Returns the location of the isolate dir."""
            return self._temp_dir.name

    def __init__(self, extra_inits: List[AbstractContextManager] = None):
        # Keep the alphabetical order.
        self._extra_inits = [
            self.IsolateDir(),
            ScopedFfxConfig('ffx.isolated', 'true'),
            ScopedFfxConfig('daemon.autostart', 'false'),
            # fxb/126212: The timeout rate determines the timeout for each file
            # transfer based on the size of the file / this rate (in MB).
            # Decreasing the rate to 1 (from 5) increases the timeout in
            # swarming, where large files can take longer to transfer.
            ScopedFfxConfig('fastboot.flash.timeout_rate', '1'),
            ScopedFfxConfig('fastboot.reboot.reconnect_timeout', '120'),
            ScopedFfxConfig('fastboot.usb.disabled', 'true'),
            ScopedFfxConfig('log.level', 'debug'),
            ScopedFfxConfig('repository.server.listen', '"[::]:0"'),
        ] + (extra_inits or [])

    # Updating configurations to meet the requirement of isolate.
    def __enter__(self):
        # This environment variable needs to be set before stopping ffx daemon
        # to avoid sending unnecessary analytics.
        os.environ['FUCHSIA_ANALYTICS_DISABLED'] = '1'
        stop_ffx_daemon()
        for extra_init in self._extra_inits:
            extra_init.__enter__()
        start_ffx_daemon()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        for extra_init in self._extra_inits:
            extra_init.__exit__(exc_type, exc_value, traceback)
        stop_ffx_daemon()

    def isolate_dir(self):
        """Returns the location of the isolate dir."""
        return self._extra_inits[0].name()


def main():
    """Executes the IsolateDaemon and waits for the sigterm."""
    catch_sigterm()
    with IsolateDaemon() as daemon:
        # Clients can assume the daemon is up and running when the output is
        # captured. Note, the client may rely on the printed isolate_dir.
        print(daemon.isolate_dir(), flush=True)
        wait_for_sigterm('shutting down the daemon.')


if __name__ == '__main__':
    sys.exit(main())
