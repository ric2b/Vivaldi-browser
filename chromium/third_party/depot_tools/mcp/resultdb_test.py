# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for resultdb tools."""

import json
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
import requests

import resultdb


class GetNonExoneratedUnexpectedResultsFromBuildTest(
        unittest.IsolatedAsyncioTestCase):

    def setUp(self):
        self.mock_context = mock.AsyncMock()
        self.mock_context.info = mock.AsyncMock()

    @mock.patch('subprocess.run')
    async def test_get_non_exonerated_unexpected_results_from_build_success(
            self, mock_subprocess_run):
        build_id = '12345'
        expected_output = '{"testResults": []}'
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout=expected_output,
            stderr='',
        )

        output = await (
            resultdb.get_non_exonerated_unexpected_results_from_build(
                self.mock_context,
                build_id,
            ))

        self.assertEqual(output, expected_output)
        expected_command = [
            'prpc',
            'call',
            'results.api.luci.app',
            'luci.resultdb.v1.ResultDB.QueryTestResults',
        ]
        expected_request = {
            'invocations': [
                'invocations/build-12345',
            ],
            'predicate': {
                'expectancy': 'VARIANTS_WITH_UNEXPECTED_RESULTS',
                'exclude_exonerated': True,
            },
            'read_mask': ('name,resultId,variant,status,statusV2,duration,'
                          'failureReason,summary_html'),
        }
        mock_subprocess_run.assert_called_once_with(
            expected_command,
            capture_output=True,
            input=json.dumps(expected_request),
            check=True,
            text=True,
        )

    @mock.patch('subprocess.run')
    async def test_get_non_exonerated_unexpected_results_from_build_exception(
            self, mock_subprocess_run):
        build_id = '12345'
        mock_subprocess_run.side_effect = Exception('PRPC call failed')

        with self.assertRaisesRegex(Exception, 'PRPC call failed'):
            await resultdb.get_non_exonerated_unexpected_results_from_build(
                self.mock_context, build_id)

    async def test_get_non_exonerated_unexpected_results_from_build_invalid_id(
            self):
        with self.assertRaisesRegex(
                ValueError,
                'Provided build_id b-12345 contains non-numeric characters'):
            await resultdb.get_non_exonerated_unexpected_results_from_build(
                self.mock_context, 'b-12345')


class ExpandSummaryHtmlTest(unittest.IsolatedAsyncioTestCase):

    def setUp(self):
        self.mock_context = mock.AsyncMock()

    @mock.patch('resultdb.get_test_level_text_artifact',
                new_callable=mock.AsyncMock)
    async def test_expand_summary_html_success(self, mock_get_artifact):
        result_name = 'invocations/build-123/tests/test_id/results/result_id'
        summary_html = ('<p>Failure</p><text-artifact '
                        'artifact-id="artifact1"></text-artifact>')
        artifact_content = 'Detailed failure log.'
        mock_get_artifact.return_value = artifact_content

        expanded_html = await resultdb.expand_summary_html(
            self.mock_context, result_name, summary_html)

        mock_get_artifact.assert_called_once_with(self.mock_context,
                                                  result_name, 'artifact1')
        self.assertEqual(expanded_html, f'<p>Failure</p>{artifact_content}')

    async def test_expand_summary_html_no_artifacts(self):
        result_name = 'invocations/build-123/tests/test_id/results/result_id'
        summary_html = '<p>Just a regular summary.</p>'

        expanded_html = await resultdb.expand_summary_html(
            self.mock_context, result_name, summary_html)

        self.assertEqual(expanded_html, summary_html)

    @mock.patch('resultdb.get_test_level_text_artifact',
                new_callable=mock.AsyncMock)
    async def test_expand_summary_html_missing_artifact_id(
            self, mock_get_artifact):
        result_name = 'invocations/build-123/tests/test_id/results/result_id'
        summary_html = '<p>Failure</p><text-artifact></text-artifact>'

        expanded_html = await resultdb.expand_summary_html(
            self.mock_context, result_name, summary_html)

        mock_get_artifact.assert_not_called()
        self.assertEqual(expanded_html,
                         '<p>Failure</p><text-artifact></text-artifact>')


class GetTestLevelTextArtifactTest(unittest.IsolatedAsyncioTestCase):

    def setUp(self):
        self.mock_context = mock.AsyncMock()
        self.mock_context.info = mock.AsyncMock()

    @mock.patch('requests.get')
    @mock.patch('subprocess.run')
    async def test_get_test_level_text_artifact_success(self,
                                                        mock_subprocess_run,
                                                        mock_requests_get):
        result_name = 'invocations/build-123/tests/test_id/results/result_id'
        artifact_id = 'artifact1'
        fetch_url = 'https://example.com/artifact1'
        artifact_content = 'This is the artifact content.'

        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout=json.dumps({
                'fetchUrl': fetch_url,
                'contentType': 'text/plain'
            }),
            stderr='',
        )
        mock_response = mock.MagicMock()
        mock_response.text = artifact_content
        mock_response.raise_for_status = mock.MagicMock()
        mock_requests_get.return_value = mock_response

        content = await resultdb.get_test_level_text_artifact(
            self.mock_context, result_name, artifact_id)

        self.assertEqual(content, artifact_content)
        expected_command = [
            'prpc',
            'call',
            'results.api.luci.app',
            'luci.resultdb.v1.ResultDB.GetArtifact',
        ]
        expected_artifact_name = (
            'invocations/build-123/tests/test_id/results/result_id/artifacts/'
            'artifact1')
        expected_request = {'name': expected_artifact_name}
        mock_subprocess_run.assert_called_once_with(
            expected_command,
            capture_output=True,
            input=json.dumps(expected_request),
            check=True,
            text=True,
        )
        mock_requests_get.assert_called_once_with(fetch_url)
        mock_response.raise_for_status.assert_called_once()

    @mock.patch('subprocess.run')
    async def test_get_test_level_text_artifact_prpc_exception(
            self, mock_subprocess_run):
        result_name = 'invocations/build-123/tests/test_id/results/result_id'
        artifact_id = 'artifact1'
        mock_subprocess_run.side_effect = Exception('PRPC call failed')

        with self.assertRaisesRegex(Exception, 'PRPC call failed'):
            await resultdb.get_test_level_text_artifact(self.mock_context,
                                                        result_name,
                                                        artifact_id)

    @mock.patch('subprocess.run')
    async def test_get_test_level_text_artifact_wrong_content_type(
            self, mock_subprocess_run):
        result_name = 'invocations/build-123/tests/test_id/results/result_id'
        artifact_id = 'artifact1'
        fetch_url = 'https://example.com/artifact1'

        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout=json.dumps({
                'fetchUrl': fetch_url,
                'contentType': 'application/octet-stream'
            }),
            stderr='',
        )

        with self.assertRaisesRegex(ValueError,
                                    ('Expected text artifact, got content type '
                                     'application/octet-stream')):
            await resultdb.get_test_level_text_artifact(self.mock_context,
                                                        result_name,
                                                        artifact_id)

    @mock.patch('requests.get')
    @mock.patch('subprocess.run')
    async def test_get_test_level_text_artifact_fetch_fails(
            self, mock_subprocess_run, mock_requests_get):
        result_name = 'invocations/build-123/tests/test_id/results/result_id'
        artifact_id = 'artifact1'
        fetch_url = 'https://example.com/artifact1'

        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout=json.dumps({
                'fetchUrl': fetch_url,
                'contentType': 'text/plain'
            }),
            stderr='',
        )
        mock_response = mock.MagicMock()
        mock_response.raise_for_status.side_effect = (
            requests.exceptions.HTTPError('404 Not Found'))
        mock_requests_get.return_value = mock_response

        with self.assertRaises(requests.exceptions.HTTPError):
            await resultdb.get_test_level_text_artifact(self.mock_context,
                                                        result_name,
                                                        artifact_id)


if __name__ == '__main__':
    unittest.main()
