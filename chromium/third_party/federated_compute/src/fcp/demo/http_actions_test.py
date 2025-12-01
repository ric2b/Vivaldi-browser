# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Tests for http_actions."""

import gzip
import http
import http.client
import http.server
import socket
import threading
from unittest import mock

from absl.testing import absltest

from fcp.demo import http_actions
from fcp.protos.federatedcompute import common_pb2
from fcp.protos.federatedcompute import eligibility_eval_tasks_pb2


class TestService:

  def __init__(self):
    self.proto_action = mock.Mock()
    self.get_action = mock.Mock()
    self.post_action = mock.Mock()

  @http_actions.proto_action(
      service='google.internal.federatedcompute.v1.EligibilityEvalTasks',
      method='RequestEligibilityEvalTask')
  def handle_proto_action(self, *args, **kwargs):
    return self.proto_action(*args, **kwargs)

  @http_actions.http_action(method='get', pattern='/get/{arg1}/{arg2}')
  def handle_get_action(self, *args, **kwargs):
    return self.get_action(*args, **kwargs)

  @http_actions.http_action(method='post', pattern='/post/{arg1}/{arg2}')
  def handle_post_action(self, *args, **kwargs):
    return self.post_action(*args, **kwargs)


class TestHttpServer(http.server.HTTPServer):
  pass


class HttpActionsTest(absltest.TestCase):

  def setUp(self):
    super().setUp()
    self.service = TestService()
    handler = http_actions.create_handler(self.service)
    self._httpd = TestHttpServer(('localhost', 0), handler)
    self._server_thread = threading.Thread(
        target=self._httpd.serve_forever, daemon=True)
    self._server_thread.start()
    self.conn = http.client.HTTPConnection(
        self._httpd.server_name, port=self._httpd.server_port)

  def tearDown(self):
    self._httpd.shutdown()
    self._server_thread.join()
    self._httpd.server_close()
    super().tearDown()

  def test_not_found(self):
    self.conn.request('GET', '/no-match')
    self.assertEqual(self.conn.getresponse().status, http.HTTPStatus.NOT_FOUND)

  def test_proto_success(self):
    expected_response = (
        eligibility_eval_tasks_pb2.EligibilityEvalTaskResponse(
            session_id='test'))
    self.service.proto_action.return_value = expected_response

    request = eligibility_eval_tasks_pb2.EligibilityEvalTaskRequest(
        client_version=common_pb2.ClientVersion(version_code='test123'))
    self.conn.request(
        'POST',
        '/v1/eligibilityevaltasks/test%2Fpopulation:request?%24alt=proto',
        request.SerializeToString())
    response = self.conn.getresponse()
    self.assertEqual(response.status, http.HTTPStatus.OK)
    response_proto = (
        eligibility_eval_tasks_pb2.EligibilityEvalTaskResponse.FromString(
            response.read()))
    self.assertEqual(response_proto, expected_response)
    # `population_name` should be set from the URL.
    request.population_name = 'test/population'
    self.service.proto_action.assert_called_once_with(request)

  def test_proto_error(self):
    self.service.proto_action.side_effect = http_actions.HttpError(
        code=http.HTTPStatus.UNAUTHORIZED)

    self.conn.request(
        'POST',
        '/v1/eligibilityevaltasks/test%2Fpopulation:request?%24alt=proto', b'')
    response = self.conn.getresponse()
    self.assertEqual(response.status, http.HTTPStatus.UNAUTHORIZED)

  def test_proto_with_invalid_payload(self):
    self.conn.request(
        'POST',
        '/v1/eligibilityevaltasks/test%2Fpopulation:request?%24alt=proto',
        b'invalid')
    response = self.conn.getresponse()
    self.assertEqual(response.status, http.HTTPStatus.BAD_REQUEST)

  def test_proto_with_gzip_encoding(self):
    self.service.proto_action.return_value = (
        eligibility_eval_tasks_pb2.EligibilityEvalTaskResponse())

    request = eligibility_eval_tasks_pb2.EligibilityEvalTaskRequest(
        client_version=common_pb2.ClientVersion(version_code='test123'))
    self.conn.request('POST',
                      '/v1/eligibilityevaltasks/test:request?%24alt=proto',
                      gzip.compress(request.SerializeToString()),
                      {'Content-Encoding': 'gzip'})
    self.assertEqual(self.conn.getresponse().status, http.HTTPStatus.OK)
    request.population_name = 'test'
    self.service.proto_action.assert_called_once_with(request)

  def test_proto_with_invalid_gzip_encoding(self):
    self.conn.request('POST',
                      '/v1/eligibilityevaltasks/test:request?%24alt=proto',
                      b'invalid', {'Content-Encoding': 'gzip'})
    response = self.conn.getresponse()
    self.assertEqual(response.status, http.HTTPStatus.BAD_REQUEST)

  def test_proto_with_unsupport_encoding(self):
    self.conn.request('POST',
                      '/v1/eligibilityevaltasks/test:request?%24alt=proto', b'',
                      {'Content-Encoding': 'compress'})
    self.assertEqual(self.conn.getresponse().status,
                     http.HTTPStatus.BAD_REQUEST)
    self.service.proto_action.assert_not_called()

  def test_get_success(self):
    self.service.get_action.return_value = http_actions.HttpResponse(
        body=b'body',
        headers={
            'Content-Length': 4,
            'Content-Type': 'application/x-test',
        })

    self.conn.request('GET', '/get/foo/bar')
    response = self.conn.getresponse()
    self.assertEqual(response.status, http.HTTPStatus.OK)
    self.assertEqual(response.headers['Content-Length'], '4')
    self.assertEqual(response.headers['Content-Type'], 'application/x-test')
    self.assertEqual(response.read(), b'body')
    self.service.get_action.assert_called_once_with(b'', arg1='foo', arg2='bar')

  def test_get_error(self):
    self.service.get_action.side_effect = http_actions.HttpError(
        code=http.HTTPStatus.UNAUTHORIZED)

    self.conn.request('GET', '/get/foo/bar')
    self.assertEqual(self.conn.getresponse().status,
                     http.HTTPStatus.UNAUTHORIZED)

  def test_post_success(self):
    self.service.post_action.return_value = http_actions.HttpResponse(
        body=b'body',
        headers={
            'Content-Length': 4,
            'Content-Type': 'application/x-test',
        })

    self.conn.request('POST', '/post/foo/bar', b'request-body')
    response = self.conn.getresponse()
    self.assertEqual(response.status, http.HTTPStatus.OK)
    self.assertEqual(response.headers['Content-Length'], '4')
    self.assertEqual(response.headers['Content-Type'], 'application/x-test')
    self.assertEqual(response.read(), b'body')
    self.service.post_action.assert_called_once_with(
        b'request-body', arg1='foo', arg2='bar')

  def test_post_error(self):
    self.service.post_action.side_effect = http_actions.HttpError(
        code=http.HTTPStatus.UNAUTHORIZED)

    self.conn.request('POST', '/post/foo/bar', b'request-body')
    self.assertEqual(self.conn.getresponse().status,
                     http.HTTPStatus.UNAUTHORIZED)

  def test_post_with_gzip_encoding(self):
    self.service.post_action.return_value = http_actions.HttpResponse(body=b'')

    self.conn.request('POST', '/post/foo/bar', gzip.compress(b'request-body'),
                      {'Content-Encoding': 'gzip'})
    self.assertEqual(self.conn.getresponse().status, http.HTTPStatus.OK)
    self.service.post_action.assert_called_once_with(
        b'request-body', arg1='foo', arg2='bar')

  def test_post_with_unsupport_encoding(self):
    self.conn.request('POST', '/post/foo/bar', b'',
                      {'Content-Encoding': 'compress'})
    self.assertEqual(self.conn.getresponse().status,
                     http.HTTPStatus.BAD_REQUEST)
    self.service.post_action.assert_not_called()


if __name__ == '__main__':
  absltest.main()
