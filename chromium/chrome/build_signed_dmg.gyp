# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'includes': [
    '../../get_vivaldi_version.gypi'
  ],
  'variables': {
    'mac_product_name': 'Vivaldi',
    'mac_packaging_dir':
        '<(PRODUCT_DIR)/<(mac_product_name) Packaging',
    # <(PRODUCT_DIR) expands to $(BUILT_PRODUCTS_DIR), which doesn't
    # work properly in a shell script, where ${BUILT_PRODUCTS_DIR} is
    # needed.
    'mac_packaging_sh_dir':
        '${BUILT_PRODUCTS_DIR}/<(mac_product_name) Packaging',
    'mac_signing_key': '<!(echo $VIVALDI_SIGNING_KEY)',
    'mac_signing_id': '<!(echo $VIVALDI_SIGNING_ID)',
  }, # variables
  'targets': [
    {
      'target_name': 'sign_packaging',
      'type': 'none',
      'includes': ['sign_mac_build.gypi']
    },
    {
      # Convenience target to build a disk image.
      'target_name': 'build_app_dmg',
      # Don't place this in the 'all' list; most won't want it.
      # In GYP, booleans are 0/1, not True/False.
      'suppress_wildcard': 1,
      'type': 'none',
      'includes': ['build_mac_dmg.gypi']
    },
  ],
}
