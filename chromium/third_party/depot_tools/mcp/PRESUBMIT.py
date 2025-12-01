# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for depot_tools/mcp.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckRunUnittests(input_api, output_api):
    tests = input_api.canned_checks.GetUnitTestsInDirectory(
        input_api, output_api, '.', files_to_check=[r'.*test\.py$'])
    return input_api.RunTests(tests)
