# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckTests(input_api, output_api):
    if input_api.platform.startswith(('cygwin', 'win32')):
        return []
    return input_api.RunTests([
        input_api.Command(name='telemetry',
                          cmd=['vpython3', '-m', 'pytest', '.'],
                          kwargs={'cwd': input_api.PresubmitLocalPath()},
                          message=output_api.PresubmitError)
    ])
