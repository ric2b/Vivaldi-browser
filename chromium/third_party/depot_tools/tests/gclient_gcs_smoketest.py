#!/usr/bin/env vpython3
# Copyright (c) 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Smoke tests for gclient.py.

Shell out 'gclient' and run gcs tests.
"""

import logging
import os
import sys
import unittest

from unittest import mock
import gclient_smoketest_base
import subprocess2

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class GClientSmokeGcs(gclient_smoketest_base.GClientSmokeBase):

    def setUp(self):
        super(GClientSmokeGcs, self).setUp()
        self.enabled = self.FAKE_REPOS.set_up_git()
        if not self.enabled:
            self.skipTest('git fake repos not available')
        self.env['PATH'] = (os.path.join(ROOT_DIR, 'testing_support') +
                            os.pathsep + self.env['PATH'])

    def testSyncGcs(self):
        self.gclient(['config', self.git_base + 'repo_22', '--name', 'src'])
        self.gclient(['sync'])

        tree = self.mangle_git_tree(('repo_22@1', 'src'))
        tree.update({
            'src/another_gcs_dep/hash':
            'abcd123\n',
            'src/another_gcs_dep/llvmfile.tar.gz':
            'tarfile',
            'src/another_gcs_dep/extracted_dir/extracted_file':
            'extracted text',
            'src/gcs_dep/deadbeef':
            'tarfile',
            'src/gcs_dep/hash':
            'abcd123\n',
            'src/gcs_dep/extracted_dir/extracted_file':
            'extracted text',
            'src/gcs_dep_with_output_file/hash':
            'abcd123\n',
            'src/gcs_dep_with_output_file/clang-format-no-extract':
            'non-extractable file',
        })
        self.assertTree(tree)

    def testConvertGitToGcs(self):
        self.gclient(['config', self.git_base + 'repo_23', '--name', 'src'])

        # repo_13@1 has src/repo12 as a git dependency.
        self.gclient([
            'sync', '-v', '-v', '-v', '--revision',
            self.githash('repo_23', 1)
        ])

        tree = self.mangle_git_tree(('repo_23@1', 'src'),
                                    ('repo_12@1', 'src/repo12'))
        self.assertTree(tree)

        # repo_23@3 has src/repo12 as a gcs dependency.
        self.gclient([
            'sync', '-v', '-v', '-v', '--revision',
            self.githash('repo_23', 3), '--delete_unversioned_trees'
        ])

        tree = self.mangle_git_tree(('repo_23@3', 'src'))
        tree.update({
            'src/repo12/extracted_dir/extracted_file': 'extracted text',
            'src/repo12/hash': 'abcd123\n',
            'src/repo12/path_to_file.tar.gz': 'tarfile',
        })
        self.assertTree(tree)

    def testConvertGcsToGit(self):
        self.gclient(['config', self.git_base + 'repo_23', '--name', 'src'])

        # repo_13@3 has src/repo12 as a cipd dependency.
        self.gclient([
            'sync', '-v', '-v', '-v', '--revision',
            self.githash('repo_23', 3), '--delete_unversioned_trees'
        ])

        tree = self.mangle_git_tree(('repo_23@3', 'src'))
        tree.update({
            'src/repo12/extracted_dir/extracted_file': 'extracted text',
            'src/repo12/hash': 'abcd123\n',
            'src/repo12/path_to_file.tar.gz': 'tarfile',
        })
        self.assertTree(tree)

        # repo_23@1 has src/repo12 as a git dependency.
        self.gclient([
            'sync', '-v', '-v', '-v', '--revision',
            self.githash('repo_23', 1)
        ])

        tree = self.mangle_git_tree(('repo_23@1', 'src'),
                                    ('repo_12@1', 'src/repo12'))
        tree.update({
            'src/repo12/extracted_dir/extracted_file': 'extracted text',
            'src/repo12/hash': 'abcd123\n',
            'src/repo12/path_to_file.tar.gz': 'tarfile',
        })
        self.assertTree(tree)

    def testRevInfo(self):
        self.gclient(['config', self.git_base + 'repo_22', '--name', 'src'])
        self.gclient(['sync'])
        results = self.gclient(['revinfo'])
        out = ('src: %(base)srepo_22\n'
               'src/another_gcs_dep: gs://456bucket/Linux/llvmfile.tar.gz\n'
               'src/gcs_dep: gs://123bucket/deadbeef\n'
               'src/gcs_dep_with_output_file: '
               'gs://789bucket/clang-format-version123\n' % {
                   'base': self.git_base,
               })
        self.check((out, '', 0), results)

    def testRevInfoActual(self):
        self.gclient(['config', self.git_base + 'repo_22', '--name', 'src'])
        self.gclient(['sync'])
        results = self.gclient(['revinfo', '--actual'])
        out = (
            'src: %(base)srepo_22@%(hash1)s\n'
            'src/another_gcs_dep: gs://456bucket/Linux/llvmfile.tar.gz@None\n'
            'src/gcs_dep: gs://123bucket/deadbeef@None\n'
            'src/gcs_dep_with_output_file: '
            'gs://789bucket/clang-format-version123@None\n' % {
                'base': self.git_base,
                'hash1': self.githash('repo_22', 1),
            })
        self.check((out, '', 0), results)


if __name__ == '__main__':
    if '-v' in sys.argv:
        logging.basicConfig(level=logging.DEBUG)
    unittest.main()
