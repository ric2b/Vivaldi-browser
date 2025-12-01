#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import siso
from testing_support import trial_dir


class SisoTest(trial_dir.TestCase):

    def setUp(self):
        super().setUp()
        self.previous_dir = os.getcwd()
        os.chdir(self.root_dir)

    def tearDown(self):
        os.chdir(self.previous_dir)
        super().tearDown()

    def test_load_sisorc_no_file(self):
        global_flags, subcmd_flags = siso.load_sisorc(
            os.path.join('build', 'config', 'siso', '.sisorc'))
        self.assertEqual(global_flags, [])
        self.assertEqual(subcmd_flags, {})

    def test_load_sisorc(self):
        sisorc = os.path.join('build', 'config', 'siso', '.sisorc')
        os.makedirs(os.path.dirname(sisorc))
        with open(sisorc, 'w') as f:
            f.write("""
# comment
-credential_helper=gcloud
ninja --failure_verbose=false -k=0
            """)
        global_flags, subcmd_flags = siso.load_sisorc(sisorc)
        self.assertEqual(global_flags, ['-credential_helper=gcloud'])
        self.assertEqual(subcmd_flags,
                         {'ninja': ['--failure_verbose=false', '-k=0']})

    def test_apply_sisorc_none(self):
        new_args = siso.apply_sisorc([], {}, ['ninja', '-C', 'out/Default'],
                                     'ninja')
        self.assertEqual(new_args, ['ninja', '-C', 'out/Default'])

    def test_apply_sisorc_nosubcmd(self):
        new_args = siso.apply_sisorc([], {'ninja': ['-k=0']}, ['-version'], '')
        self.assertEqual(new_args, ['-version'])

    def test_apply_sisorc(self):
        new_args = siso.apply_sisorc(
            ['-credential_helper=luci-auth'], {'ninja': ['-k=0']},
            ['-log_dir=/tmp', 'ninja', '-C', 'out/Default'], 'ninja')
        self.assertEqual(new_args, [
            '-credential_helper=luci-auth', '-log_dir=/tmp', 'ninja', '-k=0',
            '-C', 'out/Default'
        ])


if __name__ == '__main__':
    unittest.main()
