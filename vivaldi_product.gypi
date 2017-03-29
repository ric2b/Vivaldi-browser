# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'variables': {
      'vivaldi_version_py_path': '<(DEPTH)/build/util/version.py',
      'vivaldi_product_kind_file': '<(DEPTH)/../VIVALDI_PRODUCT',
    },
    # Beta # string
    'official_product_kind_string': '<!(python <(vivaldi_version_py_path) -f <(vivaldi_product_kind_file) -t "@VIVALDI_PRODUCT@")',
  },  # variables
}
