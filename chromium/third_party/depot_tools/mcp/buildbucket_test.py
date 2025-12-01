# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for buildbucket tools."""

import json
import os
import pathlib
import subprocess
import sys
import unittest
from unittest.mock import AsyncMock, patch

sys.path.insert(
    0,
    os.path.abspath(
        pathlib.Path(__file__).resolve().parent.parent.joinpath(
            pathlib.Path('infra_lib'))))
import buildbucket


class BuildbucketTest(unittest.IsolatedAsyncioTestCase):

    def setUp(self):
        self.mock_context = AsyncMock()
        self.mock_context.info = AsyncMock()

    @patch('subprocess.run')
    async def test_get_build_status_success(self, mock_subprocess_run):
        build_id = '12345'
        expected_status = 'SUCCESS'
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout=json.dumps({'status': expected_status}),
            stderr='')

        status = await buildbucket.get_build_status(
            self.mock_context,
            build_id,
        )

        self.assertEqual(status, expected_status)
        expected_command = [
            'prpc', 'call', 'cr-buildbucket.appspot.com',
            'buildbucket.v2.Builds.GetBuildStatus'
        ]
        mock_subprocess_run.assert_called_once_with(
            expected_command,
            capture_output=True,
            input=json.dumps({'id': build_id}),
            check=False,
            text=True,
        )

    @patch('subprocess.run')
    async def test_get_build_status_exception(self, mock_subprocess_run):
        build_id = '12345'
        mock_subprocess_run.side_effect = Exception('PRPC call failed')

        result = await buildbucket.get_build_status(self.mock_context, build_id)

        self.assertIn('Exception calling prpc', result)
        self.assertIn('PRPC call failed', result)

    @patch('subprocess.run')
    async def test_get_build_from_id_success(self, mock_subprocess_run):
        build_id = '12345'
        fields = ['steps', 'tags']
        expected_output = '{"id": "12345", "steps": [], "tags": []}'
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout=expected_output, stderr='')

        output = await buildbucket.get_build_from_id(
            self.mock_context,
            build_id,
            fields,
        )

        self.assertEqual(output, expected_output)
        expected_command = [
            'prpc', 'call', 'cr-buildbucket.appspot.com',
            'buildbucket.v2.Builds.GetBuild'
        ]
        expected_request = {'id': build_id, 'mask': {'fields': 'steps,tags'}}
        mock_subprocess_run.assert_called_once_with(
            expected_command,
            capture_output=True,
            input=json.dumps(expected_request),
            check=False,
            text=True)

    @patch('subprocess.run')
    async def test_get_build_from_id_exception(self, mock_subprocess_run):
        build_id = '12345'
        fields = ['steps']
        mock_subprocess_run.side_effect = Exception('PRPC call failed')

        result = await buildbucket.get_build_from_id(
            self.mock_context,
            build_id,
            fields,
        )

        self.assertIn('Exception calling prpc', result)
        self.assertIn('PRPC call failed', result)

    @patch('subprocess.run')
    async def test_get_build_from_build_number_success(
        self,
        mock_subprocess_run,
    ):
        build_number = 987
        builder_name = 'test_builder'
        builder_bucket = 'try'
        builder_project = 'chromium'
        fields = ['status']
        expected_output = '{"status": "SUCCESS"}'
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout=expected_output, stderr='')

        output = await buildbucket.get_build_from_build_number(
            self.mock_context, build_number, builder_name, builder_bucket,
            builder_project, fields)

        self.assertEqual(output, expected_output)
        expected_command = [
            'prpc', 'call', 'cr-buildbucket.appspot.com',
            'buildbucket.v2.Builds.GetBuild'
        ]
        expected_request = {
            'buildNumber': build_number,
            'builder': {
                'builder': builder_name,
                'bucket': builder_bucket,
                'project': builder_project
            },
            'mask': {
                'fields': 'status'
            }
        }
        mock_subprocess_run.assert_called_once_with(
            expected_command,
            capture_output=True,
            input=json.dumps(expected_request),
            check=False,
            text=True)

    @patch('subprocess.run')
    async def test_get_build_from_build_number_exception(
            self, mock_subprocess_run):
        build_number = 987
        builder_name = 'test_builder'
        builder_bucket = 'try'
        builder_project = 'chromium'
        fields = ['status']
        mock_subprocess_run.side_effect = Exception('PRPC call failed')

        result = await buildbucket.get_build_from_build_number(
            self.mock_context, build_number, builder_name, builder_bucket,
            builder_project, fields)

        self.assertIn('Exception calling prpc', result)
        self.assertIn('PRPC call failed', result)

    @patch('subprocess.run')
    async def test_get_build_success(self, mock_subprocess_run):
        request = {"id": "12345"}
        expected_output = '{"id": "12345"}'
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout=expected_output,
            stderr='',
        )

        output = await buildbucket.get_build(self.mock_context, request)

        self.assertEqual(output, expected_output)
        expected_command = [
            'prpc', 'call', 'cr-buildbucket.appspot.com',
            'buildbucket.v2.Builds.GetBuild'
        ]
        mock_subprocess_run.assert_called_once_with(
            expected_command,
            capture_output=True,
            input=json.dumps(request),
            check=False,
            text=True,
        )

    @patch('subprocess.run')
    async def test_get_build_exception(self, mock_subprocess_run):
        request = {"id": "12345"}
        mock_subprocess_run.side_effect = Exception('PRPC call failed')

        result = await buildbucket.get_build(self.mock_context, request)

        self.assertIn('Exception calling prpc', result)
        self.assertIn('PRPC call failed', result)


if __name__ == '__main__':
    unittest.main()
