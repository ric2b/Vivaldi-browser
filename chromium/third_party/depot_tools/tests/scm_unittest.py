#!/usr/bin/env vpython3
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for scm.py."""

import logging
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from testing_support import fake_repos

import scm
import subprocess
import subprocess2


def callError(code=1, cmd='', cwd='', stdout=b'', stderr=b''):
    return subprocess2.CalledProcessError(code, cmd, cwd, stdout, stderr)


class GitWrapperTestCase(unittest.TestCase):
    def setUp(self):
        super(GitWrapperTestCase, self).setUp()
        self.root_dir = '/foo/bar'

    def testRefToRemoteRef(self):
        remote = 'origin'
        refs = {
            'refs/branch-heads/1234': ('refs/remotes/branch-heads/', '1234'),
            # local refs for upstream branch
            'refs/remotes/%s/foobar' % remote:
            ('refs/remotes/%s/' % remote, 'foobar'),
            '%s/foobar' % remote: ('refs/remotes/%s/' % remote, 'foobar'),
            # upstream ref for branch
            'refs/heads/foobar': ('refs/remotes/%s/' % remote, 'foobar'),
            # could be either local or upstream ref, assumed to refer to
            # upstream, but probably don't want to encourage refs like this.
            'heads/foobar': ('refs/remotes/%s/' % remote, 'foobar'),
            # underspecified, probably intended to refer to a local branch
            'foobar':
            None,
            # tags and other refs
            'refs/tags/TAG':
            None,
            'refs/changes/34/1234':
            None,
        }
        for k, v in refs.items():
            r = scm.GIT.RefToRemoteRef(k, remote)
            self.assertEqual(r, v, msg='%s -> %s, expected %s' % (k, r, v))

    def testRemoteRefToRef(self):
        remote = 'origin'
        refs = {
            'refs/remotes/branch-heads/1234': 'refs/branch-heads/1234',
            # local refs for upstream branch
            'refs/remotes/origin/foobar': 'refs/heads/foobar',
            # tags and other refs
            'refs/tags/TAG': 'refs/tags/TAG',
            'refs/changes/34/1234': 'refs/changes/34/1234',
            # different remote
            'refs/remotes/other-remote/foobar': None,
            # underspecified, probably intended to refer to a local branch
            'heads/foobar': None,
            'origin/foobar': None,
            'foobar': None,
            None: None,
        }
        for k, v in refs.items():
            r = scm.GIT.RemoteRefToRef(k, remote)
            self.assertEqual(r, v, msg='%s -> %s, expected %s' % (k, r, v))

    @mock.patch('scm.GIT.Capture')
    @mock.patch('os.path.exists', lambda _: True)
    def testGetRemoteHeadRefLocal(self, mockCapture):
        mockCapture.side_effect = ['refs/remotes/origin/main']
        self.assertEqual(
            'refs/remotes/origin/main',
            scm.GIT.GetRemoteHeadRef('foo', 'proto://url', 'origin'))
        self.assertEqual(mockCapture.call_count, 1)

    @mock.patch('scm.GIT.Capture')
    @mock.patch('os.path.exists', lambda _: True)
    def testGetRemoteHeadRefLocalUpdateHead(self, mockCapture):
        mockCapture.side_effect = [
            'refs/remotes/origin/master',  # first symbolic-ref call
            'foo',  # set-head call
            'refs/remotes/origin/main',  # second symbolic-ref call
        ]
        self.assertEqual(
            'refs/remotes/origin/main',
            scm.GIT.GetRemoteHeadRef('foo', 'proto://url', 'origin'))
        self.assertEqual(mockCapture.call_count, 3)

    @mock.patch('scm.GIT.Capture')
    @mock.patch('os.path.exists', lambda _: True)
    def testGetRemoteHeadRefRemote(self, mockCapture):
        mockCapture.side_effect = [
            subprocess2.CalledProcessError(1, '', '', '', ''),
            subprocess2.CalledProcessError(1, '', '', '', ''),
            'ref: refs/heads/main\tHEAD\n' +
            '0000000000000000000000000000000000000000\tHEAD',
        ]
        self.assertEqual(
            'refs/remotes/origin/main',
            scm.GIT.GetRemoteHeadRef('foo', 'proto://url', 'origin'))
        self.assertEqual(mockCapture.call_count, 3)

    @mock.patch('scm.GIT.Capture')
    def testIsVersioned(self, mockCapture):
        mockCapture.return_value = (
            '160000 blob 423dc77d2182cb2687c53598a1dcef62ea2804ae   dir')
        actual_state = scm.GIT.IsVersioned('cwd', 'dir')
        self.assertEqual(actual_state, scm.VERSIONED_SUBMODULE)

        mockCapture.return_value = ''
        actual_state = scm.GIT.IsVersioned('cwd', 'dir')
        self.assertEqual(actual_state, scm.VERSIONED_NO)

        mockCapture.return_value = (
            '040000 tree ef016abffb316e47a02af447bc51342dcef6f3ca    dir')
        actual_state = scm.GIT.IsVersioned('cwd', 'dir')
        self.assertEqual(actual_state, scm.VERSIONED_DIR)

    @mock.patch('os.path.exists', return_value=True)
    @mock.patch('scm.GIT.Capture')
    def testListSubmodules(self, mockCapture, *_mock):
        mockCapture.return_value = (
            'submodule.submodulename.path foo/path/script'
            '\nsubmodule.submodule2name.path foo/path/script2')
        actual_list = scm.GIT.ListSubmodules('root')
        if sys.platform.startswith('win'):
            self.assertEqual(actual_list,
                             ['foo\\path\\script', 'foo\\path\\script2'])
        else:
            self.assertEqual(actual_list,
                             ['foo/path/script', 'foo/path/script2'])

    def testListSubmodules_missing(self):
        self.assertEqual(scm.GIT.ListSubmodules('root'), [])


class RealGitTest(fake_repos.FakeReposTestBase):
    def setUp(self):
        super(RealGitTest, self).setUp()
        self.enabled = self.FAKE_REPOS.set_up_git()
        if self.enabled:
            self.cwd = scm.os.path.join(self.FAKE_REPOS.git_base, 'repo_1')
        else:
            self.skipTest('git fake repos not available')

    def testResolveCommit(self):
        with self.assertRaises(Exception):
            scm.GIT.ResolveCommit(self.cwd, 'zebra')
        with self.assertRaises(Exception):
            scm.GIT.ResolveCommit(self.cwd, 'r123456')
        first_rev = self.githash('repo_1', 1)
        self.assertEqual(first_rev, scm.GIT.ResolveCommit(self.cwd, first_rev))
        self.assertEqual(self.githash('repo_1', 2),
                         scm.GIT.ResolveCommit(self.cwd, 'HEAD'))

    def testIsValidRevision(self):
        # Sha1's are [0-9a-z]{32}, so starting with a 'z' or 'r' should always
        # fail.
        self.assertFalse(scm.GIT.IsValidRevision(cwd=self.cwd, rev='zebra'))
        self.assertFalse(scm.GIT.IsValidRevision(cwd=self.cwd, rev='r123456'))
        # Valid cases
        first_rev = self.githash('repo_1', 1)
        self.assertTrue(scm.GIT.IsValidRevision(cwd=self.cwd, rev=first_rev))
        self.assertTrue(scm.GIT.IsValidRevision(cwd=self.cwd, rev='HEAD'))

    def testIsAncestor(self):
        self.assertTrue(
            scm.GIT.IsAncestor(self.githash('repo_1', 1),
                               self.githash('repo_1', 2),
                               cwd=self.cwd))
        self.assertFalse(
            scm.GIT.IsAncestor(self.githash('repo_1', 2),
                               self.githash('repo_1', 1),
                               cwd=self.cwd))
        self.assertFalse(scm.GIT.IsAncestor(self.githash('repo_1', 1), 'zebra'))

    def testGetAllFiles(self):
        self.assertEqual(['DEPS', 'foo bar', 'origin'],
                         scm.GIT.GetAllFiles(self.cwd))

    def testGetSetConfig(self):
        key = 'scm.test-key'

        self.assertIsNone(scm.GIT.GetConfig(self.cwd, key))
        self.assertEqual('default-value',
                         scm.GIT.GetConfig(self.cwd, key, 'default-value'))

        scm.GIT.SetConfig(self.cwd, key, 'set-value')
        self.assertEqual('set-value', scm.GIT.GetConfig(self.cwd, key))
        self.assertEqual('set-value',
                         scm.GIT.GetConfig(self.cwd, key, 'default-value'))

        scm.GIT.SetConfig(self.cwd, key, '')
        self.assertEqual('', scm.GIT.GetConfig(self.cwd, key))
        self.assertEqual('', scm.GIT.GetConfig(self.cwd, key, 'default-value'))

        # Clear the cache because we externally manipulate the git config with
        # the subprocess call.
        scm.GIT.drop_config_cache()
        subprocess.run(['git', 'config', key, 'line 1\nline 2\nline 3'],
                       cwd=self.cwd)
        self.assertEqual('line 1\nline 2\nline 3',
                         scm.GIT.GetConfig(self.cwd, key))
        self.assertEqual('line 1\nline 2\nline 3',
                         scm.GIT.GetConfig(self.cwd, key, 'default-value'))

        scm.GIT.SetConfig(self.cwd, key)
        self.assertIsNone(scm.GIT.GetConfig(self.cwd, key))
        self.assertEqual('default-value',
                         scm.GIT.GetConfig(self.cwd, key, 'default-value'))

    def testGetSetConfigBool(self):
        key = 'scm.test-key'
        self.assertFalse(scm.GIT.GetConfigBool(self.cwd, key))

        scm.GIT.SetConfig(self.cwd, key, 'true')
        self.assertTrue(scm.GIT.GetConfigBool(self.cwd, key))

        scm.GIT.SetConfig(self.cwd, key)
        self.assertFalse(scm.GIT.GetConfigBool(self.cwd, key))

    def testGetSetConfigList(self):
        key = 'scm.test-key'
        self.assertListEqual([], scm.GIT.GetConfigList(self.cwd, key))

        scm.GIT.SetConfig(self.cwd, key, 'foo')
        scm.GIT.Capture(['config', '--add', key, 'bar'], cwd=self.cwd)
        self.assertListEqual(['foo', 'bar'],
                             scm.GIT.GetConfigList(self.cwd, key))

        scm.GIT.SetConfig(self.cwd, key, modify_all=True, value_pattern='^f')
        self.assertListEqual(['bar'], scm.GIT.GetConfigList(self.cwd, key))

        scm.GIT.SetConfig(self.cwd, key)
        self.assertListEqual([], scm.GIT.GetConfigList(self.cwd, key))

    def testYieldConfigRegexp(self):
        key1 = 'scm.aaa'
        key2 = 'scm.aaab'

        config = scm.GIT.YieldConfigRegexp(self.cwd, key1)
        with self.assertRaises(StopIteration):
            next(config)

        scm.GIT.SetConfig(self.cwd, key1, 'foo')
        scm.GIT.SetConfig(self.cwd, key2, 'bar')
        scm.GIT.Capture(['config', '--add', key2, 'baz'], cwd=self.cwd)

        config = scm.GIT.YieldConfigRegexp(self.cwd, '^scm\\.aaa')
        self.assertEqual((key1, 'foo'), next(config))
        self.assertEqual((key2, 'bar'), next(config))
        self.assertEqual((key2, 'baz'), next(config))
        with self.assertRaises(StopIteration):
            next(config)

    def testGetSetBranchConfig(self):
        branch = scm.GIT.GetBranch(self.cwd)
        key = 'scm.test-key'

        self.assertIsNone(scm.GIT.GetBranchConfig(self.cwd, branch, key))
        self.assertEqual(
            'default-value',
            scm.GIT.GetBranchConfig(self.cwd, branch, key, 'default-value'))

        scm.GIT.SetBranchConfig(self.cwd, branch, key, 'set-value')
        self.assertEqual('set-value',
                         scm.GIT.GetBranchConfig(self.cwd, branch, key))
        self.assertEqual(
            'set-value',
            scm.GIT.GetBranchConfig(self.cwd, branch, key, 'default-value'))
        self.assertEqual(
            'set-value',
            scm.GIT.GetConfig(self.cwd, 'branch.%s.%s' % (branch, key)))

        scm.GIT.SetBranchConfig(self.cwd, branch, key)
        self.assertIsNone(scm.GIT.GetBranchConfig(self.cwd, branch, key))

    def testFetchUpstreamTuple_NoUpstreamFound(self):
        self.assertEqual((None, None), scm.GIT.FetchUpstreamTuple(self.cwd))

    @mock.patch('scm.GIT.GetRemoteBranches', return_value=['origin/main'])
    def testFetchUpstreamTuple_GuessOriginMaster(self, _mockGetRemoteBranches):
        self.assertEqual(('origin', 'refs/heads/main'),
                         scm.GIT.FetchUpstreamTuple(self.cwd))

    @mock.patch('scm.GIT.GetRemoteBranches',
                return_value=['origin/master', 'origin/main'])
    def testFetchUpstreamTuple_GuessOriginMain(self, _mockGetRemoteBranches):
        self.assertEqual(('origin', 'refs/heads/main'),
                         scm.GIT.FetchUpstreamTuple(self.cwd))

    def testFetchUpstreamTuple_RietveldUpstreamConfig(self):
        scm.GIT.SetConfig(self.cwd, 'rietveld.upstream-branch',
                          'rietveld-upstream')
        scm.GIT.SetConfig(self.cwd, 'rietveld.upstream-remote',
                          'rietveld-remote')
        self.assertEqual(('rietveld-remote', 'rietveld-upstream'),
                         scm.GIT.FetchUpstreamTuple(self.cwd))
        scm.GIT.SetConfig(self.cwd, 'rietveld.upstream-branch')
        scm.GIT.SetConfig(self.cwd, 'rietveld.upstream-remote')

    @mock.patch('scm.GIT.GetBranch', side_effect=callError())
    def testFetchUpstreamTuple_NotOnBranch(self, _mockGetBranch):
        scm.GIT.SetConfig(self.cwd, 'rietveld.upstream-branch',
                          'rietveld-upstream')
        scm.GIT.SetConfig(self.cwd, 'rietveld.upstream-remote',
                          'rietveld-remote')
        self.assertEqual(('rietveld-remote', 'rietveld-upstream'),
                         scm.GIT.FetchUpstreamTuple(self.cwd))
        scm.GIT.SetConfig(self.cwd, 'rietveld.upstream-branch')
        scm.GIT.SetConfig(self.cwd, 'rietveld.upstream-remote')

    def testFetchUpstreamTuple_BranchConfig(self):
        branch = scm.GIT.GetBranch(self.cwd)
        scm.GIT.SetBranchConfig(self.cwd, branch, 'merge', 'branch-merge')
        scm.GIT.SetBranchConfig(self.cwd, branch, 'remote', 'branch-remote')
        self.assertEqual(('branch-remote', 'branch-merge'),
                         scm.GIT.FetchUpstreamTuple(self.cwd))
        scm.GIT.SetBranchConfig(self.cwd, branch, 'merge')
        scm.GIT.SetBranchConfig(self.cwd, branch, 'remote')

    def testFetchUpstreamTuple_AnotherBranchConfig(self):
        branch = 'scm-test-branch'
        scm.GIT.SetBranchConfig(self.cwd, branch, 'merge', 'other-merge')
        scm.GIT.SetBranchConfig(self.cwd, branch, 'remote', 'other-remote')
        self.assertEqual(('other-remote', 'other-merge'),
                         scm.GIT.FetchUpstreamTuple(self.cwd, branch))
        scm.GIT.SetBranchConfig(self.cwd, branch, 'merge')
        scm.GIT.SetBranchConfig(self.cwd, branch, 'remote')

    def testGetBranchRef(self):
        self.assertEqual('refs/heads/main', scm.GIT.GetBranchRef(self.cwd))
        HEAD = scm.GIT.Capture(['rev-parse', 'HEAD'], cwd=self.cwd)
        scm.GIT.Capture(['checkout', HEAD], cwd=self.cwd)
        self.assertIsNone(scm.GIT.GetBranchRef(self.cwd))
        scm.GIT.Capture(['checkout', 'main'], cwd=self.cwd)

    def testGetBranch(self):
        self.assertEqual('main', scm.GIT.GetBranch(self.cwd))
        HEAD = scm.GIT.Capture(['rev-parse', 'HEAD'], cwd=self.cwd)
        scm.GIT.Capture(['checkout', HEAD], cwd=self.cwd)
        self.assertIsNone(scm.GIT.GetBranchRef(self.cwd))
        scm.GIT.Capture(['checkout', 'main'], cwd=self.cwd)


class DiffTestCase(unittest.TestCase):

    def setUp(self):
        self.root = tempfile.mkdtemp()

        os.makedirs(os.path.join(self.root, "foo", "dir"))
        with open(os.path.join(self.root, "foo", "file.txt"), "w") as f:
            f.write("foo\n")
        with open(os.path.join(self.root, "foo", "dir", "file.txt"), "w") as f:
            f.write("foo dir\n")

        os.makedirs(os.path.join(self.root, "baz_repo"))
        with open(os.path.join(self.root, "baz_repo", "file.txt"), "w") as f:
            f.write("baz\n")

    @mock.patch('scm.GIT.ListSubmodules')
    def testGetAllFiles_ReturnsAllFilesIfNoSubmodules(self, mockListSubmodules):
        mockListSubmodules.return_value = []
        files = scm.DIFF.GetAllFiles(self.root)

        if sys.platform.startswith('win'):
            self.assertCountEqual(
                files,
                ["foo\\file.txt", "foo\\dir\\file.txt", "baz_repo\\file.txt"])
        else:
            self.assertCountEqual(
                files,
                ["foo/file.txt", "foo/dir/file.txt", "baz_repo/file.txt"])

    @mock.patch('scm.GIT.ListSubmodules')
    def testGetAllFiles_IgnoresFilesInSubmodules(self, mockListSubmodules):
        mockListSubmodules.return_value = ['baz_repo']
        files = scm.DIFF.GetAllFiles(self.root)

        if sys.platform.startswith('win'):
            self.assertCountEqual(
                files, ["foo\\file.txt", "foo\\dir\\file.txt", "baz_repo"])
        else:
            self.assertCountEqual(
                files, ["foo/file.txt", "foo/dir/file.txt", "baz_repo"])


if __name__ == '__main__':
    if '-v' in sys.argv:
        logging.basicConfig(level=logging.DEBUG)
    unittest.main()

# vim: ts=2:sw=2:tw=80:et:
