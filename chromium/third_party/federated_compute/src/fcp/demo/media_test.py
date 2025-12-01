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
"""Tests for media."""

import http
from unittest import mock
import uuid

from absl.testing import absltest

from fcp.demo import http_actions
from fcp.demo import media
from fcp.protos.federatedcompute import common_pb2


class MediaTest(absltest.TestCase):

  @mock.patch.object(uuid, 'uuid4', return_value=uuid.uuid4(), autospec=True)
  def test_create_download_group(self, mock_uuid):
    forwarding_info = common_pb2.ForwardingInfo(
        target_uri_prefix='https://media.example/')
    service = media.Service(lambda: forwarding_info)
    with service.create_download_group() as group:
      self.assertEqual(group.prefix,
                       f'https://media.example/data/{mock_uuid.return_value}/')
      name = 'file-name'
      self.assertEqual(group.add(name, b'data'), group.prefix + name)

  def test_download(self):
    service = media.Service(common_pb2.ForwardingInfo)
    with service.create_download_group() as group:
      data = b'data'
      url = group.add('name', data)
      self.assertEqual(
          service.download(b'',
                           *url.split('/')[-2:]),
          http_actions.HttpResponse(
              body=data,
              headers={
                  'Content-Length': len(data),
                  'Content-Type': 'application/octet-stream',
              }))

  def test_download_with_content_type(self):
    service = media.Service(common_pb2.ForwardingInfo)
    with service.create_download_group() as group:
      data = b'data'
      content_type = 'application/x-test'
      url = group.add('name', data, content_type=content_type)
      self.assertEqual(
          service.download(b'',
                           *url.split('/')[-2:]),
          http_actions.HttpResponse(
              body=data,
              headers={
                  'Content-Length': len(data),
                  'Content-Type': content_type,
              }))

  def test_download_multiple_files(self):
    service = media.Service(common_pb2.ForwardingInfo)
    with service.create_download_group() as group:
      data1 = b'data1'
      data2 = b'data2'
      url1 = group.add('file1', data1)
      url2 = group.add('file2', data2)
      self.assertEqual(service.download(b'', *url1.split('/')[-2:]).body, data1)
      self.assertEqual(service.download(b'', *url2.split('/')[-2:]).body, data2)

  def test_download_multiple_groups(self):
    service = media.Service(common_pb2.ForwardingInfo)
    with service.create_download_group() as group1, (
        service.create_download_group()) as group2:
      self.assertNotEqual(group1.prefix, group2.prefix)
      data1 = b'data1'
      data2 = b'data2'
      url1 = group1.add('name', data1)
      url2 = group2.add('name', data2)
      self.assertEqual(service.download(b'', *url1.split('/')[-2:]).body, data1)
      self.assertEqual(service.download(b'', *url2.split('/')[-2:]).body, data2)

  def test_download_unregistered_group(self):
    service = media.Service(common_pb2.ForwardingInfo)
    with self.assertRaises(http_actions.HttpError) as cm:
      service.download(b'', 'does-not-exist', 'does-not-exist')
    self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)

  def test_download_unregistered_file(self):
    service = media.Service(common_pb2.ForwardingInfo)
    with service.create_download_group() as group:
      url = group.add('name', b'data')
      with self.assertRaises(http_actions.HttpError) as cm:
        service.download(b'', url.split('/')[-2], 'does-not-exist')
      self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)

  def test_download_no_longer_registered(self):
    service = media.Service(common_pb2.ForwardingInfo)
    with service.create_download_group() as group:
      url = group.add('name', b'data')
    with self.assertRaises(http_actions.HttpError) as cm:
      service.download(b'', *url.split('/')[-2:])
    self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)

  def test_register_duplicate_download(self):
    service = media.Service(common_pb2.ForwardingInfo)
    with service.create_download_group() as group:
      data1 = b'data'
      url = group.add('name', data1)
      with self.assertRaises(KeyError):
        group.add('name', b'data2')

      # The original file should still be downloadable.
      self.assertEqual(service.download(b'', *url.split('/')[-2:]).body, data1)

  def test_upload(self):
    service = media.Service(common_pb2.ForwardingInfo)
    name = service.register_upload()
    data = b'data'
    self.assertEqual(
        service.upload(data, name), http_actions.HttpResponse(body=b''))
    self.assertEqual(service.finalize_upload(name), data)

  def test_upload_without_data(self):
    service = media.Service(common_pb2.ForwardingInfo)
    name = service.register_upload()
    self.assertIsNone(service.finalize_upload(name))

  def test_upload_multiple_times(self):
    service = media.Service(common_pb2.ForwardingInfo)
    name = service.register_upload()

    data = b'data1'
    self.assertEqual(
        service.upload(data, name), http_actions.HttpResponse(body=b''))

    with self.assertRaises(http_actions.HttpError) as cm:
      service.upload(b'data2', name)
    self.assertEqual(cm.exception.code, http.HTTPStatus.UNAUTHORIZED)

    self.assertEqual(service.finalize_upload(name), data)

  def test_upload_multiple(self):
    service = media.Service(common_pb2.ForwardingInfo)
    name1 = service.register_upload()
    name2 = service.register_upload()

    # Order shouldn't matter.
    service.upload(b'data2', name2)
    service.upload(b'data1', name1)

    self.assertEqual(service.finalize_upload(name1), b'data1')
    self.assertEqual(service.finalize_upload(name2), b'data2')

  def test_upload_unregistered(self):
    service = media.Service(common_pb2.ForwardingInfo)
    with self.assertRaises(http_actions.HttpError) as cm:
      service.upload(b'data', 'does-not-exist')
    self.assertEqual(cm.exception.code, http.HTTPStatus.UNAUTHORIZED)

    with self.assertRaises(KeyError):
      service.finalize_upload('does-not-exist')

  def test_upload_no_longer_registered(self):
    service = media.Service(common_pb2.ForwardingInfo)
    name = service.register_upload()
    self.assertIsNone(service.finalize_upload(name))

    with self.assertRaises(http_actions.HttpError) as cm:
      service.upload(b'data', name)
    self.assertEqual(cm.exception.code, http.HTTPStatus.UNAUTHORIZED)

    with self.assertRaises(KeyError):
      service.finalize_upload(name)


if __name__ == '__main__':
  absltest.main()
