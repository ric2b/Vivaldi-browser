# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import sys
import signal
import subprocess

import gclient_paths

# Keeps track of whether our SIGINT handler was called.
_is_canceled = False


def _register_build_id(local_dev_server_path, build_id, out_dir):
    subprocess.run([
        local_dev_server_path, '--register-build-id', build_id, '--builder-pid',
        str(os.getpid()), '--output-directory',
        os.path.abspath(out_dir)
    ])


def _print_status(local_dev_server_path, build_id):
    subprocess.run([local_dev_server_path, '--print-status', build_id])


def _get_server_path():
    src_dir = gclient_paths.GetPrimarySolutionPath()
    return os.path.join(src_dir, 'build/android/fast_local_dev_server.py')


def _set_signal_handler(local_dev_server_path, build_id):
    original_sigint_handler = signal.getsignal(signal.SIGINT)

    def _kill_handler(signum, frame):
        global _is_canceled
        _is_canceled = True
        # Cancel the pending build tasks if user CTRL+c early.
        print('🛑 Canceling pending build_server tasks', file=sys.stderr)
        subprocess.run([local_dev_server_path, '--cancel-build', build_id])
        original_sigint_handler(signum, frame)

    signal.signal(signal.SIGINT, _kill_handler)


def _start_server(local_dev_server_path):
    subprocess.Popen([local_dev_server_path, '--exit-on-idle', '--quiet'],
                     start_new_session=True)


def _set_tty_env():
    # If the caller explicitly set this environment variable, just use it
    # instead of failing.
    if "AUTONINJA_STDOUT_NAME" in os.environ:
        return True
    stdout_name = os.readlink('/proc/self/fd/1')
    # Anonymous pipes can't be opened. These look like "pipe:[394765110]".
    ret = os.path.exists(stdout_name)
    if ret:
        os.environ.setdefault("AUTONINJA_STDOUT_NAME", stdout_name)
    return ret


@contextlib.contextmanager
def build_server_context(build_id, out_dir, use_android_build_server=False):
    if not use_android_build_server or not _set_tty_env():
        yield
        return
    server_path = _get_server_path()
    _start_server(server_path)
    # Tell the build server about us.
    _register_build_id(server_path, build_id, out_dir)
    _set_signal_handler(server_path, build_id)
    yield
    # No need to print status if we CTRL+Ced out.
    if (not _is_canceled):
        _print_status(server_path, build_id)
