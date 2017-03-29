# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'variables': {
      'GLOBAL_BUILDNUM%':10000,
      'vivaldi_version_py_path': '<(DEPTH)/build/util/version.py',
      'vivaldi_version_path': '<(DEPTH)/../VIVALDI_VERSION',
    },
    'vivaldi_version_py_path': '<(vivaldi_version_py_path)',
    'vivaldi_version_path': '<(vivaldi_version_path)',
    'vivaldi_global_build_number': '<(GLOBAL_BUILDNUM)',
    'version_full':
        '<!(python <(vivaldi_version_py_path) -f <(vivaldi_version_path) -f <(version_path) -t "@VIVALDI_MAJOR@.@VIVALDI_MINOR@.@VIVALDI_NIGHTLY@.<(vivaldi_global_build_number)")',
    'version_mac_dylib':
        '<!(python <(vivaldi_version_py_path) -f <(vivaldi_version_path) -f <(version_path) -t "@BUILD@.@PATCH_HI@.@PATCH_LO@" -e "PATCH_HI=int(<(vivaldi_global_build_number))/256" -e "PATCH_LO=int(<(vivaldi_global_build_number))%256")',
  },  # variables
}
