# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for git_cl tools."""

import os
import pathlib
import subprocess
import sys
import unittest
from unittest import mock

sys.path.insert(
    0,
    os.path.abspath(
        pathlib.Path(__file__).resolve().parent.parent.joinpath(
            pathlib.Path('infra_lib'))))
import git_cl


class GitClTest(unittest.IsolatedAsyncioTestCase):

    def setUp(self):
        self.mock_context = mock.AsyncMock()
        self.mock_context.info = mock.AsyncMock()
        self.checkout = '/path/to/checkout'

    @mock.patch('subprocess.run')
    async def test_try_builder_results_success(self, mock_subprocess_run):
        expected_output = '{"builds": []}'
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout=expected_output, stderr='')

        output = await git_cl.try_builder_results(self.mock_context,
                                                  self.checkout, None)

        self.assertEqual(output, expected_output)
        expected_command = ["git", "cl", "try-results", "--json=-"]
        mock_subprocess_run.assert_called_once_with(expected_command,
                                                    capture_output=True,
                                                    check=True,
                                                    text=True,
                                                    cwd=self.checkout)

    @mock.patch('subprocess.run')
    async def test_try_builder_results_with_issue_success(
            self, mock_subprocess_run):
        expected_output = '{"builds": []}'
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout=expected_output, stderr='')

        output = await git_cl.try_builder_results(self.mock_context,
                                                  self.checkout, 1234)

        self.assertEqual(output, expected_output)
        expected_command = [
            "git", "cl", "try-results", "--json=-", "-i", "1234"
        ]
        mock_subprocess_run.assert_called_once_with(expected_command,
                                                    capture_output=True,
                                                    check=True,
                                                    text=True,
                                                    cwd=self.checkout)

    @mock.patch('subprocess.run')
    async def test_get_current_changes_success(self, mock_subprocess_run):
        expected_output = 'diff --git a/file.txt b/file.txt'
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout=expected_output, stderr='')

        output = await git_cl.get_current_changes(self.mock_context,
                                                  self.checkout)

        self.assertEqual(output, expected_output)
        expected_command = ["git", "cl", "diff"]
        mock_subprocess_run.assert_called_once_with(expected_command,
                                                    capture_output=True,
                                                    check=True,
                                                    text=True,
                                                    cwd=self.checkout)

    @mock.patch('subprocess.run')
    async def test_format_checkout_success(self, mock_subprocess_run):
        expected_output = 'Formatted 1 file'
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout=expected_output, stderr='')

        output = await git_cl.format_checkout(self.mock_context, self.checkout)

        self.assertEqual(output, expected_output)
        expected_command = ["git", "cl", "format"]
        mock_subprocess_run.assert_called_once_with(expected_command,
                                                    capture_output=True,
                                                    check=True,
                                                    text=True,
                                                    cwd=self.checkout)

    @mock.patch('subprocess.run')
    async def test_upload_change_list_success(self, mock_subprocess_run):
        expected_output = 'CL uploaded'
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout=expected_output, stderr='')

        output = await git_cl.upload_change_list(self.mock_context,
                                                 self.checkout)

        self.assertEqual(output, expected_output)
        expected_command = ["git", "cl", "upload", "-f"]
        mock_subprocess_run.assert_called_once_with(expected_command,
                                                    capture_output=True,
                                                    check=True,
                                                    text=True,
                                                    cwd=self.checkout)


if __name__ == '__main__':
    unittest.main()
