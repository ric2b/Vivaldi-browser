# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ash/strings
      'target_name': 'ash_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ash/strings',
      },
      'dependencies': [ '<(VIVALDI)/app/vivaldi_resources.gyp:ash_strings' ],
      'actions': [
        {
          'action_name': 'generate_ash_strings',
          'disabled': 1,
          'variables': {
            'grit_grd_file': 'ash_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
  ],
}
