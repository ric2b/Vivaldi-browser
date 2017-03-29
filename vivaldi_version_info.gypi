# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes':    ['chromium/build/util/version.gypi'],
  'variables': {
    'lastchange_path': 'chromium/build/util/LASTCHANGE',
    'lastchange_vivaldi_path': 'chromium/build/util/LASTCHANGE.vivaldi',
  },
  'targets': [
    {
      'target_name': 'vivaldi_version_info',
      'type': 'none',
      'actions': [
        {
          'action_name': 'write_version_info',
          'inputs': [
            '<(vivaldi_version_path)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/vivaldi_version.txt',
          ],
          # Just output the default version info variables.
          'action': [
            'python', '<(vivaldi_version_py_path)',
            '-f', '<(vivaldi_version_path)',
            '-f', '<(version_path)',
            '-f', '<(lastchange_path)',
            '-e', 'VIVALDI_BUILD=<(vivaldi_global_build_number)',
            '--vivaldi',
            '-o', '<@(_outputs)'
          ],
        },
      ],
    },
  ],
}
