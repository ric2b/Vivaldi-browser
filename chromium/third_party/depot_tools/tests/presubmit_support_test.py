#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import subprocess
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import presubmit_support


class PresubmitSupportTest(unittest.TestCase):
    def test_environ(self):
        self.assertIsNone(os.environ.get('PRESUBMIT_FOO_ENV', None))
        kv = {'PRESUBMIT_FOO_ENV': 'FOOBAR'}
        with presubmit_support.setup_environ(kv):
            self.assertEqual(os.environ.get('PRESUBMIT_FOO_ENV', None),
                             'FOOBAR')
        self.assertIsNone(os.environ.get('PRESUBMIT_FOO_ENV', None))


if __name__ == "__main__":
    unittest.main()
