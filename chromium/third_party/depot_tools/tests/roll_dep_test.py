#!/usr/bin/env vpython3
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import subprocess
import unittest
from unittest import mock

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import roll_dep
from testing_support import fake_repos

ROLL_DEP = os.path.join(ROOT_DIR, 'roll-dep')
GCLIENT = os.path.join(ROOT_DIR, 'gclient')

# TODO: Should fix these warnings.
# pylint: disable=line-too-long

def create_deps_content(git_base, path_to_revision_map):
    """
    Create a DEPS file content string with the given dependency mappings.

    Args:
        git_base: The base URL for git repositories
        path_to_revision_map: Dictionary mapping dependency paths to their revisions

    Returns:
        String with the complete DEPS file content including standard hooks
    """
    dep_lines = []
    git_base = git_base.replace('\\', '\\\\')
    for path, revision in path_to_revision_map.items():
        dep_lines.append(f' "{path}": "file://{git_base}repo_2@{revision}",')

    # Combine all parts with standard hooks.
    deps_content = [
        'deps = {',
        '\n'.join(dep_lines),
        '}',
        'hooks = [',
        '  {"action": ["foo", "--android", "{checkout_android}"]}',
        ']',
    ]
    return '\n'.join(deps_content)


class FakeRepos(fake_repos.FakeReposBase):
    NB_GIT_REPOS = 2

    def populateGit(self):
        for x in range(1,4):
            self._commit_git('repo_2', {'origin': f'git/repo_2@{x}'})

        # repo_2@1 is the default revision.
        # Anything under 'third_party/not_supported' tests handling unsupported
        # cases.
        repo2_revision = self.git_hashes['repo_2'][1][0]
        self._commit_git(
            'repo_1', {
                'DEPS': create_deps_content(self.git_base, {
                    'src/foo': repo2_revision,
                    'src/third_party/repo_2/src': repo2_revision,
                    'src/third_party/repo_2B/src': repo2_revision,
                    'src/third_party/not_supported/with_divider/src': repo2_revision,
                    'src/third_party/not_supported/multiple_revisions/src': repo2_revision,
                    'src/third_party/not_supported/no_revision/src': repo2_revision
                }),
                'README.chromium': '\n'.join([
                    'Name: test repo',
                    'URL: https://example.com',
                    'Version: 1.0',
                    'Revision: abcabc123123',
                    'License: MIT',
                ]),
                'third_party/repo_2/README.chromium': '\n'.join([
                    'Name: test repo 2',
                    'URL: https://example.com',
                    'Version: 1.0',
                    'Revision: abc1234',
                    'License: MIT',
                ]),
                'third_party/repo_2B/README.chromium': '\n'.join([
                    'Name: Override DEPS value for revision',
                    'URL: https://example.com',
                    'Version: 1.0',
                    'Revision: DEPS',
                    'License: MIT',
                ]),
                'third_party/not_supported/with_divider/README.chromium': '\n'.join([
                    'Name: Deps divider not supported',
                    'URL: https://example.com',
                    'Version: 1.0',
                    'Revision: abc1234',
                    'License: MIT',
                    '-------------------- DEPENDENCY DIVIDER --------------------',
                    'Name: So nothing here should change',
                    'URL: https://example.com',
                    'Version: 1.0',
                    'Revision: abc1234',
                    'License: MIT',
                ]),
                'third_party/not_supported/multiple_revisions/README.chromium': '\n'.join([
                    'Name: Multiple revisions',
                    'URL: https://example.com',
                    'Version: 1.0',
                    'Revision: abc1234',
                    'License: MIT',
                    'Revision: abc1235', # This should not happen.
                ]),
                'third_party/not_supported/no_revision/README.chromium': '\n'.join([
                    'Name: No revision',
                    'URL: https://example.com',
                    'Version: 1.0',
                    'License: MIT',
                ]),
            })


class RollDepTest(fake_repos.FakeReposTestBase):
    FAKE_REPOS_CLASS = FakeRepos

    def setUp(self):
        super(RollDepTest, self).setUp()
        # Make sure it doesn't try to auto update when testing!
        self.env = os.environ.copy()
        self.env['DEPOT_TOOLS_UPDATE'] = '0'
        self.env['DEPOT_TOOLS_METRICS'] = '0'
        # Suppress Python 3 warnings and other test undesirables.
        self.env['GCLIENT_TEST'] = '1'

        self.maxDiff = None

        self.enabled = self.FAKE_REPOS.set_up_git()
        self.src_dir = os.path.join(self.root_dir, 'src')
        self.foo_dir = os.path.join(self.src_dir, 'foo')
        self.all_repos = [
            'src/foo',
            'src/third_party/repo_2/src',
            'src/third_party/repo_2B/src',
            'src/third_party/not_supported/with_divider/src',
            'src/third_party/not_supported/multiple_revisions/src',
            'src/third_party/not_supported/no_revision/src',
        ]
        if self.enabled:
            self.call([
                GCLIENT, 'config', 'file://' + self.git_base + 'repo_1',
                '--name', 'src'
            ],
                      cwd=self.root_dir)
            self.call([GCLIENT, 'sync'], cwd=self.root_dir)

    def call(self, cmd, cwd=None):
        cwd = cwd or self.src_dir
        process = subprocess.Popen(cmd,
                                   cwd=cwd,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE,
                                   env=self.env,
                                   shell=sys.platform.startswith('win'))
        stdout, stderr = process.communicate()
        logging.debug("XXX: %s\n%s\nXXX" % (' '.join(cmd), stdout))
        logging.debug("YYY: %s\n%s\nYYY" % (' '.join(cmd), stderr))
        stdout = stdout.decode('utf-8')
        stderr = stderr.decode('utf-8')
        return (stdout.replace('\r\n',
                               '\n'), stderr.replace('\r\n',
                                                     '\n'), process.returncode)

    def assert_deps_match(self, expected_path_to_revision_map):
        # Assume everything is at the default revision and only update the
        # provided paths.
        default_revision = self.githash('repo_2', 1)
        expected_map = {path: default_revision for path in self.all_repos}
        expected_map.update(expected_path_to_revision_map)

        for path, revision in expected_map.items():
            with self.subTest(path=path):
                path_dir = os.path.join(self.root_dir, path)
                self.assertEqual(self.gitrevparse(path_dir), revision)

        with open(os.path.join(self.src_dir, 'DEPS')) as f:
            actual_content = f.read()
        with self.subTest(path='DEPS'):
            expected_content = create_deps_content(self.git_base,expected_map)
            self.assertEqual(expected_content, actual_content)


    def testRollsDep(self):
        if not self.enabled:
            return
        stdout, stderr, returncode = self.call([ROLL_DEP]+self.all_repos)
        latest_revision = self.githash('repo_2', 3)

        self.assertEqual(stderr, '')
        self.assertEqual(returncode, 0)

        # All deps should be rolled to the latest revision.
        self.assert_deps_match({p: latest_revision for p in self.all_repos})

        commit_message = self.call(['git', 'log', '-n', '1'])[0]

        expected_message = 'Roll src/foo/ %s..%s (2 commits)' % (self.githash(
            'repo_2', 1)[:9], latest_revision[:9])

        self.assertIn(expected_message, stdout)
        self.assertIn(expected_message, commit_message)


    def testRollsDepWithReadme(self):
        """Tests roll-dep when updating README.chromium files."""
        if not self.enabled:
            return
        stdout, stderr, returncode = self.call(
                [ROLL_DEP]+self.all_repos
        )
        latest_revision = self.githash('repo_2', 3)

        # All deps should be rolled to the latest revision (3).
        self.assert_deps_match({p: latest_revision for p in self.all_repos})
        self.assertEqual(stderr, '')
        self.assertEqual(returncode, 0)
        for path in self.all_repos:
            with self.subTest(path=path):
                contents = ''
                readme_path = os.path.join(self.root_dir, path, os.path.pardir, 'README.chromium')
                if os.path.exists(readme_path):
                    with open(readme_path, 'r') as f:
                        contents = f.read()
                if path == 'src/third_party/not_supported/no_revision/src':
                    self.assertIn('README.chromium contains 0 Revision: lines', stdout)
                if 'not_supported' in path:
                    self.assertNotIn(latest_revision, contents)
                    continue
                # Check that the revision was updated.
                self.assertIn(f'Revision: {latest_revision}', contents)
                self.assertNotIn('Revision: abcabc123123', contents)
                self.assertNotIn('No README.chromium found', stdout)

    def testRollsDepReviewers(self):
        if not self.enabled:
            return

        stdout, stderr, returncode = self.call([
            ROLL_DEP, 'src/foo', '-r', 'foo@example.com', '-r',
            'bar@example.com,baz@example.com'
        ])

        self.assertEqual(stderr, '')
        self.assertEqual(returncode, 0)

        expected_message = 'R=foo@example.com,bar@example.com,baz@example.com'

        self.assertIn(expected_message, stdout)

    def testRollsDepToSpecificRevision(self):
        if not self.enabled:
            return
        specified_revision = self.githash('repo_2', 2)
        stdout, stderr, returncode = self.call(
            [ROLL_DEP, 'src/foo',  '--roll-to', specified_revision])

        self.assertEqual(stderr, '')
        self.assertEqual(returncode, 0)

        self.assert_deps_match({
            'src/foo': specified_revision,
        })

        commit_message = self.call(['git', 'log', '-n', '1'])[0]

        expected_message = 'Roll src/foo/ %s..%s (1 commit)' % (self.githash(
            'repo_2', 1)[:9], self.githash('repo_2', 2)[:9])

        self.assertIn(expected_message, stdout)
        self.assertIn(expected_message, commit_message)

    def testRollsDepLogLimit(self):
        if not self.enabled:
            return
        stdout, stderr, returncode = self.call(
            [ROLL_DEP, 'src/foo', '--log-limit', '1'])
        latest_revision = self.githash('repo_2', 3)

        self.assertEqual(stderr, '')
        self.assertEqual(returncode, 0)
        self.assert_deps_match({
            'src/foo':latest_revision,
        })

        commit_message = self.call(['git', 'log', '-n', '1'])[0]

        expected_message = 'Roll src/foo/ %s..%s (2 commits)' % (self.githash(
            'repo_2', 1)[:9], self.githash('repo_2', 3)[:9])

        self.assertIn(expected_message, stdout)
        self.assertIn(expected_message, commit_message)


class CommitMessageTest(unittest.TestCase):

    def setUp(self):
        self.logs = '\n'.join([
            '2024-04-05 alice Goodbye',
            '2024-04-03 bob Hello World',
        ])

        # Mock the `git log` call.
        mock.patch('roll_dep.check_output', return_value=self.logs).start()
        self.addCleanup(mock.patch.stopall)

    def testShowShortLog(self):
        message = roll_dep.generate_commit_message(
            '/path/to/dir', 'dep', 'abc', 'def',
            'https://chromium.googlesource.com', True, 10)

        self.assertIn('Roll dep/ abc..def (2 commits)', message)
        self.assertIn('$ git log', message)
        self.assertIn(self.logs, message)

    def testHideShortLog(self):
        message = roll_dep.generate_commit_message(
            '/path/to/dir', 'dep', 'abc', 'def',
            'https://chromium.googlesource.com', False, 10)

        self.assertNotIn('$ git log', message)
        self.assertNotIn(self.logs, message)

    def testShouldShowLogWithPublicHost(self):
        self.assertTrue(
            roll_dep.should_show_log(
                'https://chromium.googlesource.com/project'))

    def testShouldNotShowLogWithPrivateHost(self):
        self.assertFalse(
            roll_dep.should_show_log(
                'https://private.googlesource.com/project'))


if __name__ == '__main__':
    level = logging.DEBUG if '-v' in sys.argv else logging.FATAL
    logging.basicConfig(level=level,
                        format='%(asctime).19s %(levelname)s %(filename)s:'
                        '%(lineno)s %(message)s')
    unittest.main()
