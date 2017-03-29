# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'build_app_dmg_script_path': 'tools/build/mac/build_app_dmg',
    'pkg_dmg_script_path': 'installer/mac/pkg-dmg',
    'mac_signing_key': '<!(echo $VIVALDI_SIGNING_KEY)',
    'mac_signing_id': '<!(echo $VIVALDI_SIGNING_ID)',

    'conditions': [
      # This duplicates the output path from build_app_dmg.
      ['branding=="Chrome"', {
        'dmg_name': 'GoogleChrome.dmg',
      }, { # else: branding!="Chrome"
        'dmg_name': 'Chromium.dmg',
      }],
      ['branding=="vivaldi"', {
        'dmg_name': 'Vivaldi.<(version_full).dmg',
      }],
    ],
  },
  'conditions': [
    ['mac_signing_key and mac_signing_id', {
      'dependencies': [
        'sign_packaging',
      ],
    }],
  ],
  'actions': [
    {
      'inputs': [
        '<(build_app_dmg_script_path)',
        '<(pkg_dmg_script_path)',
        '<(PRODUCT_DIR)/<(mac_product_name).app',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/<(dmg_name)',
      ],
      'action_name': 'build_app_dmg',
      'action': ['<(build_app_dmg_script_path)', '<@(branding)', '<(version_full)'],
    },
  ],  # 'actions'
}
