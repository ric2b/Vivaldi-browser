#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Update in-tree checkout of Rust toolchain

When run without arguments, it fetches and unzips the Rust toolchain package
specieid by the `RUST_REVISION` and `RUST_SUB_REVISION` along with the clang
version specified in //tools/clang/scripts/update.py.
'''

import argparse
import glob
import os
import re
import shutil
import sys
import time
import urllib

from pathlib import Path

# Add Clang scripts to path so we can import them later (if running within a
# Chromium checkout.)
# Note: Imports cannot be done until after the --print-rust-revision flag
# has been processed, since that needs to work when running this script
# in isolation.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

# These fields are written by //tools/clang/scripts/upload_revision.py, and
# should not be changed manually.
#
# In the case that a Rust roll fails and you want to roll Clang alone, reset
# this back to its previous value _AND_ set `OVERRIDE_CLANG_REVISION` below
# to the `CLANG_REVISION` that was in place before the roll.
RUST_REVISION = '3cf924b934322fd7b514600a7dc84fc517515346'
RUST_SUB_REVISION = 3

# If not None, this overrides the `CLANG_REVISION` in
# //tools/clang/scripts/update.py in order to download a Rust toolchain that
# was built against a different LLVM than the latest Clang package.
OVERRIDE_CLANG_REVISION = None

# Trunk on 2022-10-15.
#
# The revision specified below should typically be the same as the
# `crubit_revision` specified in the //DEPS file.  More details and roll
# instructions can be found in tools/rust/README.md.
#
# TODO(danakj): This should be included in --print-rust-revision when we want
# code to depend on using crubit rs_to_cc_bindings.
CRUBIT_REVISION = 'f5cbdf4b54b0e6b9f63a4464a2c901c82e0f0209'
CRUBIT_SUB_REVISION = 1

# Hash of src/stage0.json, which itself contains the stage0 toolchain hashes.
# We trust the Rust build system checks, but to ensure it is not tampered with
# itself check the hash.
STAGE0_JSON_SHA256 = '01d041997206abf7da640361381d10134b2f6e3f0ca65fce7172a07387e01730'

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..'))
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
RUST_TOOLCHAIN_OUT_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-toolchain')
# Path to the VERSION file stored in the archive.
VERSION_SRC_PATH = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'VERSION')


def GetRustClangRevision():
    from update import CLANG_REVISION
    c = OVERRIDE_CLANG_REVISION if OVERRIDE_CLANG_REVISION else CLANG_REVISION
    return (f'{RUST_REVISION}-{RUST_SUB_REVISION}' f'-{c}')


# Get the version of the toolchain package we already have.
def GetStampVersion():
    if os.path.exists(VERSION_SRC_PATH):
        with open(VERSION_SRC_PATH) as version_file:
            existing_stamp = version_file.readline().rstrip()
        version_re = re.compile(r'rustc [0-9.]+ [0-9a-f]+ \((.+?) chromium\)')
        match = version_re.fullmatch(existing_stamp)
        if match is None:
            return None
        return match.group(1)

    return None


def main():
    parser = argparse.ArgumentParser(description='Update Rust package')
    parser.add_argument(
        '--print-rust-revision',
        action='store_true',
        help='Print Rust revision (without Clang revision) and '
        'quit. Can be run outside of a Chromium checkout.')
    parser.add_argument('--print-package-version',
                        action='store_true',
                        help='Print Rust package version (including both the '
                        'Rust and Clang revisions) and quit.')
    args = parser.parse_args()

    if args.print_rust_revision:
        print(f'{RUST_REVISION}-{RUST_SUB_REVISION}')
        return 0

    if args.print_package_version:
        stamp_version = GetStampVersion()
        if stamp_version != GetRustClangRevision():
            print(f'The expected Rust version is {GetRustClangRevision()} '
                  f'but the actual version is {stamp_version}')
            print('Did you run "gclient sync"?')
            return 1
        print(stamp_version)
        return 0

    from update import (DownloadAndUnpack, GetDefaultHostOs,
                        GetPlatformUrlPrefix)

    platform_prefix = GetPlatformUrlPrefix(GetDefaultHostOs())

    version = GetRustClangRevision()

    # Exit early if the existing package is up-to-date. Note that we cannot
    # simply call DownloadAndUnpack() every time: aside from unnecessarily
    # downloading the toolchain if it hasn't changed, it also leads to multiple
    # versions of the same rustlibs. build/rust/std/find_std_rlibs.py chokes in
    # this case.
    # .*_is_first_class_gcs file is created by first class GCS deps when rust
    # hooks are migrated to be first class deps. In case we need to go back to
    # using a hook, this file will indicate that the previous download was
    # from the first class dep and the dir needs to be cleared.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        if version == GetStampVersion() and not glob.glob(
                os.path.join(RUST_TOOLCHAIN_OUT_DIR, '.*_is_first_class_gcs')):
            return 0

    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

    try:
        url = f'{platform_prefix}rust-toolchain-{version}.tar.xz'
        DownloadAndUnpack(url, RUST_TOOLCHAIN_OUT_DIR)
    except urllib.error.HTTPError as e:
        print(f'error: Failed to download Rust package')
        return 1

    # Ensure the newly extracted package has the correct version.
    assert version == GetStampVersion()


if __name__ == '__main__':
    sys.exit(main())
