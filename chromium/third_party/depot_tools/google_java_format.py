#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Redirects to the version of google-java-format checked into the Chrome tree.

google-java-format executable is pulled down from the cipd storage whenever
you sync Chrome. This script finds and runs the executable.
"""

import gclient_paths
import os
import subprocess
import sys


def FindGoogleJavaFormat():
    """Returns the path to the google-java-format executable."""
    # Allow non-chromium projects to use a custom location.
    primary_solution_path = gclient_paths.GetPrimarySolutionPath()
    if primary_solution_path:
        override = os.environ.get('GOOGLE_JAVA_FORMAT_PATH')
        if override:
            # Make relative to solution root if not an absolute path.
            return os.path.join(primary_solution_path, override)

        path = os.path.join(primary_solution_path, 'third_party',
                            'google-java-format', 'google-java-format')
        # Check that the .jar exists, since it is conditionally downloaded via
        # DEPS conditions.
        if os.path.exists(path) and os.path.exists(path + '.jar'):
            return path
    return None


def main(args):
    google_java_format = FindGoogleJavaFormat()
    if google_java_format is None:
        # Fail silently. It could be we are on an old chromium revision,
        # or that it is a non-chromium project. https://crbug.com/1491627.
        print('google-java-format not found, skipping java formatting.')
        return 0

    # Add some visibility to --help showing where the tool lives, since this
    # redirection can be a little opaque.
    help_syntax = ('-h', '--help', '-help', '-help-list', '--help-list')
    if any(match in args for match in help_syntax):
        print('\nDepot tools redirects you to the google-java-format at:\n' +
              '    %s\n' % google_java_format)

    return subprocess.call([google_java_format] + args)


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        sys.stderr.write('interrupted\n')
        sys.exit(1)
