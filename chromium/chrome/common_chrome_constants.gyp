# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'target_defaults': {
    'sources': [
      'common/chrome_constants.cc',
      'common/chrome_constants.h',
    ],
    'toolsets': ['host', 'target'],
    'include_dirs': [
      '..',
    ],
  },

  'targets': [
    {
      'target_name': 'common_chrome_constants',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
    },
  ],
  'conditions': [
    ['OS == "win" and target_arch=="ia32"', {
      'targets': [
	    {
          'target_name': 'common_chrome_constants_win64',
          'type': 'static_library',
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
