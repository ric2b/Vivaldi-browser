# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Exclusive filelocking for all supported platforms."""

import contextlib
import logging
import os
import sys
import time


class LockError(Exception):
    pass


if sys.platform.startswith('win'):
    # Windows implementation
    import win32imports

    BYTES_TO_LOCK = 1

    def _open_file(lockfile):
        return win32imports.Handle(
            win32imports.CreateFileW(
                lockfile,  # lpFileName
                win32imports.GENERIC_WRITE,  # dwDesiredAccess
                0,  # dwShareMode=prevent others from opening file
                None,  # lpSecurityAttributes
                win32imports.CREATE_ALWAYS,  # dwCreationDisposition
                win32imports.FILE_ATTRIBUTE_NORMAL,  # dwFlagsAndAttributes
                None  # hTemplateFile
            ))

    def _close_file(handle, unlock):
        if unlock:
            # Locks are released *before* the CloseHandle function is finished
            # processing:
            # - https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-unlockfileex#remarks
            pass

        win32imports.CloseHandle(handle)

    def _lock_file(handle):
        ret = win32imports.LockFileEx(
            handle,  # hFile
            win32imports.LOCKFILE_FAIL_IMMEDIATELY
            | win32imports.LOCKFILE_EXCLUSIVE_LOCK,  # dwFlags
            0,  #dwReserved
            BYTES_TO_LOCK,  # nNumberOfBytesToLockLow
            0,  # nNumberOfBytesToLockHigh
            win32imports.Overlapped()  # lpOverlapped
        )
        # LockFileEx returns result as bool, which is converted into an integer
        # (1 == successful; 0 == not successful)
        if ret == 0:
            error_code = win32imports.GetLastError()
            raise OSError('Failed to lock handle (error code: %d).' %
                          error_code)
else:
    # Unix implementation
    import fcntl

    def _open_file(lockfile):
        open_flags = (os.O_CREAT | os.O_WRONLY)
        return os.open(lockfile, open_flags, 0o644)

    def _close_file(fd, unlock):
        # "man 2 fcntl" states that closing any file descriptor referring to
        # the lock file will release all the process locks on the file, but
        # there is no guarantee that the locks will be released atomically
        # before the closure.
        #
        # It's necessary to release the lock before the file close to avoid
        # possible race conditions.
        if unlock:
            fcntl.flock(fd, fcntl.LOCK_UN)
        os.close(fd)

    def _lock_file(fd):
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)


def _try_lock(lockfile):
    f = _open_file(lockfile)
    try:
        _lock_file(f)
    except Exception:
        _close_file(f, unlock=False)
        raise
    return lambda: _close_file(f, unlock=True)


def _lock(path, timeout=0):
    """_lock returns function to release the lock if locking was successful.

    _lock also implements simple retry logic.
    NOTE: timeout value doesn't include time it takes to aquire lock, just
    overall sleep time."""
    elapsed = 0
    sleep_time = 0.1
    while True:
        try:
            return _try_lock(path + '.locked')
        except (OSError, IOError) as e:
            if elapsed < timeout:
                logging.info(
                    'Could not create git cache lockfile; '
                    'will retry after sleep(%d).', sleep_time)
                elapsed += sleep_time
                time.sleep(sleep_time)
                continue
            raise LockError("Error locking %s (err: %s)" % (path, str(e)))


@contextlib.contextmanager
def lock(path, timeout=0):
    """Get exclusive lock to path.

    Usage:
        import lockfile
        with lockfile.lock(path, timeout):
            # Do something
            pass

    """
    release_fn = _lock(path, timeout)
    try:
        yield
    finally:
        release_fn()
