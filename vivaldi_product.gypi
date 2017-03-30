# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'variables': {
      'variables': {
        'vivaldi_version_py_path%': '<(DEPTH)/build/util/version.py',
        'vivaldi_product_kind_file%': '<(DEPTH)/../VIVALDI_PRODUCT',
        'build_branch%': 'master',
      },
      'vivaldi_product_kind_file%': '<(vivaldi_product_kind_file)',
      'vivaldi_version_py_path%': '<(vivaldi_version_py_path)',
      'build_branch%': '<(build_branch)',
      'release_kind%': '<!(python <(VIVALDI)/scripts/get_preview.py "<(build_branch)")',
    },
    'vivaldi_product_kind_file%': '<(vivaldi_product_kind_file)',
    'vivaldi_version_py_path%': '<(vivaldi_version_py_path)',
    'build_branch%': '<(build_branch)',
    'release_kind%': '<(release_kind)',

    # Beta # string
    'official_product_kind_string': '<!(python <(vivaldi_version_py_path) -f <(vivaldi_product_kind_file) -t "@VIVALDI_PRODUCT@")',

    'conditions': [
      ['release_kind == "final"', {
          'grit_defines': [
            '-D', 'vivaldi_release_final',
          ],
          'VIVALDI_RELEASE_KIND': 'vivaldi_final',
      }, 'buildtype=="Official"', {
          'grit_defines': [
            '-D', 'vivaldi_release_snapshot',
          ],
          'VIVALDI_RELEASE_KIND': 'vivaldi_snapshot',
      }, {
          'grit_defines': [
            '-D', 'vivaldi_release_sopranos',
          ],
          'VIVALDI_RELEASE_KIND': 'vivaldi_sopranos',
      }],
    ],
  },  # variables
}
