#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script is used to download the clang update script. It runs as a gclient
hook.

It's equivalent to using curl to download the latest update script.
"""

import argparse
import curlish
import sys

SCRIPT_DOWNLOAD_URL = ('https://raw.githubusercontent.com/chromium/'
                       'chromium/{}/tools/clang/scripts/update.py')


def get_url(revision):
    return SCRIPT_DOWNLOAD_URL.format(revision or "main")

def main():
    parser = argparse.ArgumentParser(
        description='download the clang update script')
    parser.add_argument('--output', help='path to file to create/overwrite')
    parser.add_argument('--revision', help='revision to download')
    args = parser.parse_args()

    if not args.output:
        parser.print_help()
        return 1

    return 0 if curlish.curlish(get_url(args.revision), args.output) else 1


if __name__ == '__main__':
    sys.exit(main())
