#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run a pinned gsutil."""

import argparse
import base64
import contextlib
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request

import gclient_utils

GSUTIL_URL = 'https://storage.googleapis.com/pub/'
API_URL = 'https://www.googleapis.com/storage/v1/b/pub/o/'

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BIN_DIR = os.path.join(THIS_DIR, 'external_bin', 'gsutil')

IS_WINDOWS = os.name == 'nt'

VERSION = '4.68'

# Google OAuth Context required by gsutil.
LUCI_AUTH_SCOPES = [
    'https://www.googleapis.com/auth/devstorage.full_control',
    'https://www.googleapis.com/auth/userinfo.email',
]

# Prefer LUCI auth mechanism over .boto config.
PREFER_LUCI_AUTH = True

# Platforms unsupported by luci-auth.
LUCI_AUTH_UNSUPPORTED_PLATFORMS = ['aix', 'zos']


class InvalidGsutilError(Exception):
    pass


def download_gsutil(version, target_dir):
    """Downloads gsutil into the target_dir."""
    filename = 'gsutil_%s.zip' % version
    target_filename = os.path.join(target_dir, filename)

    # Get md5 hash of the remote file from the metadata.
    metadata_url = '%s%s' % (API_URL, filename)
    metadata = json.load(urllib.request.urlopen(metadata_url))
    remote_md5 = base64.b64decode(metadata['md5Hash'])

    # Calculate the md5 hash of the local file.
    def calc_local_md5():
        assert os.path.exists(target_filename)
        md5 = hashlib.md5()
        with open(target_filename, 'rb') as f:
            while chunk := f.read(1024 * 1024):
                md5.update(chunk)
        return md5.digest()

    # Use the existing file if it has the correct md5 hash.
    if os.path.exists(target_filename):
        if calc_local_md5() == remote_md5:
            return target_filename
        os.remove(target_filename)

    # Download the file.
    url = '%s%s' % (GSUTIL_URL, filename)
    urllib.request.urlretrieve(url, target_filename)

    # Check if the file was downloaded correctly.
    if calc_local_md5() != remote_md5:
        raise InvalidGsutilError(f'Downloaded gsutil from {url} has wrong md5')

    return target_filename


@contextlib.contextmanager
def temporary_directory(base):
    tmpdir = tempfile.mkdtemp(prefix='t', dir=base)
    try:
        yield tmpdir
    finally:
        if os.path.isdir(tmpdir):
            shutil.rmtree(tmpdir)


def ensure_gsutil(version, target, clean):
    bin_dir = os.path.join(target, 'gsutil_%s' % version)
    gsutil_bin = os.path.join(bin_dir, 'gsutil', 'gsutil')
    gsutil_flag = os.path.join(bin_dir, 'gsutil', 'install.flag')
    # We assume that if gsutil_flag exists, then we have a good version
    # of the gsutil package.
    if not clean and os.path.isfile(gsutil_flag):
        # Everything is awesome! we're all done here.
        return gsutil_bin

    if not os.path.exists(target):
        os.makedirs(target, exist_ok=True)

    import lockfile
    import zipfile
    with lockfile.lock(bin_dir, timeout=30):
        # Check if gsutil is ready (another process may have had lock).
        if not clean and os.path.isfile(gsutil_flag):
            return gsutil_bin

        with temporary_directory(target) as instance_dir:
            download_dir = os.path.join(instance_dir, 'd')
            target_zip_filename = gclient_utils.exponential_backoff_retry(
                lambda: download_gsutil(version, instance_dir),
                name='download_gsutil')
            with zipfile.ZipFile(target_zip_filename, 'r') as target_zip:
                target_zip.extractall(download_dir)

            # Clean up if we're redownloading a corrupted gsutil.
            cleanup_path = os.path.join(instance_dir, 'clean')
            try:
                os.rename(bin_dir, cleanup_path)
            except (OSError, IOError):
                cleanup_path = None
            if cleanup_path:
                shutil.rmtree(cleanup_path)

            shutil.move(download_dir, bin_dir)
            # Final check that the gsutil bin exists.  This should never fail.
            if not os.path.isfile(gsutil_bin):
                raise InvalidGsutilError()
            # Drop a flag file.
            with open(gsutil_flag, 'w') as f:
                f.write('This flag file is dropped by gsutil.py')

    return gsutil_bin


def _is_luci_context():
    """Returns True if the script is run within luci-context"""
    if os.getenv('SWARMING_HEADLESS') == '1':
        return True

    luci_context_env = os.getenv('LUCI_CONTEXT')
    if not luci_context_env:
        return False

    try:
        with open(luci_context_env) as f:
            luci_context_json = json.load(f)
            return 'local_auth' in luci_context_json
    except (ValueError, FileNotFoundError):
        return False


def _is_luci_auth_supported_platform():
    """Returns True if luci-auth is supported in the current platform."""
    return not any(map(sys.platform.startswith,
                       LUCI_AUTH_UNSUPPORTED_PLATFORMS))


def luci_context(cmd, fallback=True):
    """Helper to call`luci-auth context`."""
    p = _luci_auth_cmd('context', wrapped_cmds=cmd)

    # If luci-auth is not logged in, fallback to normal execution.
    luci_not_logged_in = b'Not logged in.' in p.stderr
    if luci_not_logged_in and fallback:
        return _run_subprocess(cmd, interactive=True)

    if not luci_not_logged_in:
        _print_subprocess_result(p)

    return p


def luci_login():
    """Helper to run `luci-auth login`."""
    # luci-auth requires interactive shell.
    return _luci_auth_cmd('login', interactive=True)


def _luci_auth_cmd(luci_cmd, wrapped_cmds=None, interactive=False):
    """Helper to call luci-auth command."""
    cmd = ['luci-auth', luci_cmd, '-scopes', ' '.join(LUCI_AUTH_SCOPES)]
    if wrapped_cmds:
        cmd += ['--'] + wrapped_cmds

    return _run_subprocess(cmd, interactive)


def _run_subprocess(cmd, interactive=False, env=None):
    """Wrapper to run the given command within a subprocess."""
    kwargs = {'shell': IS_WINDOWS}

    if env:
        kwargs['env'] = dict(os.environ, **env)

    if not interactive:
        kwargs['stdout'] = subprocess.PIPE
        kwargs['stderr'] = subprocess.PIPE

    return subprocess.run(cmd, **kwargs)


def _print_subprocess_result(p):
    """Prints the subprocess result to stdout & stderr."""
    if p.stdout:
        sys.stdout.buffer.write(p.stdout)

    if p.stderr:
        sys.stderr.buffer.write(p.stderr)


def get_boto_path():
    """Returns the path to a .boto file if it's present in the default path."""
    env_var = os.getenv('BOTO_CONFIG') or os.getenv('AWS_CREDENTIAL_FILE')
    if env_var:
        return env_var

    home_boto = os.path.join(os.path.expanduser('~'), '.boto')
    if os.path.isfile(home_boto):
        return home_boto

    return ""


def run_gsutil(target, args, clean=False):
    # Redirect gsutil config calls to luci-auth.
    if 'config' in args:
        return luci_login().returncode

    gsutil_bin = ensure_gsutil(VERSION, target, clean)
    args_opt = ['-o', 'GSUtil:software_update_check_period=0']

    if sys.platform == 'darwin':
        # We are experiencing problems with multiprocessing on MacOS where
        # gsutil.py may hang. This behavior is documented in gsutil codebase,
        # and recommendation is to set GSUtil:parallel_process_count=1.
        # https://github.com/GoogleCloudPlatform/gsutil/blob/06efc9dc23719fab4fd5fadb506d252bbd3fe0dd/gslib/command.py#L1331
        # https://github.com/GoogleCloudPlatform/gsutil/issues/1100
        args_opt.extend(['-o', 'GSUtil:parallel_process_count=1'])
    if sys.platform == 'cygwin':
        # This script requires Windows Python, so invoke with depot_tools'
        # Python.
        def winpath(path):
            stdout = subprocess.check_output(['cygpath', '-w', path])
            return stdout.strip().decode('utf-8', 'replace')

        cmd = ['python.bat', winpath(__file__)]
        cmd.extend(args)
        sys.exit(subprocess.call(cmd))
    assert sys.platform != 'cygwin'

    cmd = [
        'vpython3', '-vpython-spec',
        os.path.join(THIS_DIR, 'gsutil.vpython3'), '--', gsutil_bin
    ] + args_opt + args

    boto_path = get_boto_path()

    # Try luci-auth early even if a boto file exists.
    if PREFER_LUCI_AUTH and boto_path:
        # Skip wrapping commands if luci-auth is already being used or if the
        # platform is unsupported by luci-auth.
        if _is_luci_context() or not _is_luci_auth_supported_platform():
            return _run_subprocess(cmd, interactive=True).returncode

        # Wrap gsutil with luci-auth context. If not logged in and boto is
        # present don't fallback to normal execution, fallback to normal
        # flow below.
        p = luci_context(cmd, fallback=False).returncode
        if not p:
            return p


    # When .boto is present, try without additional wrappers and handle specific
    # errors.
    if boto_path:
        # Display a warning about using .boto files.
        if PREFER_LUCI_AUTH:
            separator = '*' * 80
            print(
                '\n' + separator + '\n' +
                'Warning: You are using a .boto file for authentication, '
                'this method is deprecated.\n' + f'({boto_path})\n\n' +
                'Next time please run `gsutil.py config` to use luci-auth.\n\n'
                + 'Falling back to .boto authentication method.\n' + separator +
                '\n',
                file=sys.stderr)

        p = _run_subprocess(cmd)

        # Notify user that their .boto file might be outdated.
        if b'Your credentials are invalid.' in p.stderr:
            # Make sure this error message is visible when invoked by gclient
            # runhooks
            separator = '*' * 80
            print(
                '\n' + separator + '\n' +
                'Warning: You might have an outdated .boto file. If this issue '
                'persists after running `gsutil.py config`, try removing your '
                '.boto, usually located in your home directory.\n' + separator +
                '\n',
                file=sys.stderr)

        _print_subprocess_result(p)
        return p.returncode

    # Skip wrapping commands if luci-auth is already being used or if the
    # platform is unsupported by luci-auth.
    if _is_luci_context() or not _is_luci_auth_supported_platform():
        return _run_subprocess(cmd, interactive=True).returncode

    # Wrap gsutil with luci-auth context.
    return luci_context(cmd).returncode


def parse_args():
    bin_dir = os.environ.get('DEPOT_TOOLS_GSUTIL_BIN_DIR', DEFAULT_BIN_DIR)

    # Help is disabled as it conflicts with gsutil -h, which controls headers.
    parser = argparse.ArgumentParser(add_help=False)

    parser.add_argument(
        '--clean',
        action='store_true',
        help='Clear any existing gsutil package, forcing a new download.')
    parser.add_argument(
        '--target',
        default=bin_dir,
        help='The target directory to download/store a gsutil version in. '
        '(default is %(default)s).')

    # These two args exist for backwards-compatibility but are no-ops.
    parser.add_argument('--force-version',
                        default=VERSION,
                        help='(deprecated, this flag has no effect)')
    parser.add_argument('--fallback',
                        help='(deprecated, this flag has no effect)')

    parser.add_argument('args', nargs=argparse.REMAINDER)

    args, extras = parser.parse_known_args()
    if args.args and args.args[0] == '--':
        args.args.pop(0)
    if extras:
        args.args = extras + args.args
    return args


def main():
    args = parse_args()
    return run_gsutil(args.target, args.args, clean=args.clean)


if __name__ == '__main__':
    sys.exit(main())
