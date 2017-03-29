# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'vivaldi_version_py_path': '<(DEPTH)/build/util/version.py',
    'vivaldi_version_path': '<(VIVALDI)/out/<(VIVALDI_PROFILE)/vivaldi_version.txt',
    'vivaldi_global_build_number': 
        '<!(python <(vivaldi_version_py_path) -f <(vivaldi_version_path) -t "@VIVALDI_BUILD@")',
    'version_full':
        '<!(python <(vivaldi_version_py_path) -f <(vivaldi_version_path) -t "@VIVALDI_MAJOR@.@VIVALDI_MINOR@.@VIVALDI_NIGHTLY@.@VIVALDI_BUILD@")',
  },  # variables
}