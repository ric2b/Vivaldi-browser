# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'actions': [
    {
      # Perform signing of inner bundle
      'action_name': 'sign inner bundle',
      'variables': {
        'sign_version': '<(mac_packaging_dir)/sign_versioned_dir.sh',
      },
      'inputs': [
        '<(mac_packaging_dir)/sign_versioned_dir.sh',
      ],
      'outputs': [
        '<(INTERMEDIATE_DIR)/signed_version.done',
        '<(PRODUCT_DIR)/<(mac_product_name).app/Contents/Versions/<(version_full)/<(mac_product_name) Framework.framework/<(mac_product_name) Framework',
      ],
      'action': [
        '<(sign_version)',
        '<(PRODUCT_DIR)/<(mac_product_name).app',
        '<(mac_signing_key)',
        '<(mac_signing_id)',
      ],
    },
    {
      # Perform signing of package
      'action_name': 'sign package',
      'variables': {
        'sign_app': '<(mac_packaging_dir)/sign_app.sh',
      },
      'inputs': [
        '<(sign_app)',
        '<(PRODUCT_DIR)/<(mac_product_name).app/Contents/Versions/<(version_full)/<(mac_product_name) Framework.framework/<(mac_product_name) Framework',
      ],
      'outputs': [
        '<(INTERMEDIATE_DIR)/signed_package.done',
      ],
      'action': [
        '<(sign_app)',
        '<(PRODUCT_DIR)/<(mac_product_name).app',
        '<(mac_signing_key)',
        '<(mac_signing_id)',
      ],
    },
  ],  # actions
}
