#!/usr/bin/env vpython3
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test gsutil.py."""

import base64
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
import urllib.request
from unittest import mock

# Add depot_tools to path
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
DEPOT_TOOLS_DIR = os.path.dirname(THIS_DIR)
sys.path.append(DEPOT_TOOLS_DIR)

import gsutil


class TestError(Exception):
    pass


class FakeCall(object):
    def __init__(self):
        self.expectations = []

    def add_expectation(self, *args, **kwargs):
        returns = kwargs.pop('_returns', None)
        self.expectations.append((args, kwargs, returns))

    def __call__(self, *args, **kwargs):
        if not self.expectations:
            raise TestError('Got unexpected\n%s\n%s' % (args, kwargs))
        exp_args, exp_kwargs, exp_returns = self.expectations.pop(0)
        if args != exp_args or kwargs != exp_kwargs:
            message = 'Expected:\n  args: %s\n  kwargs: %s\n' % (exp_args,
                                                                 exp_kwargs)
            message += 'Got:\n  args: %s\n  kwargs: %s\n' % (args, kwargs)
            raise TestError(message)
        return exp_returns


class MockResponse(io.BytesIO):

    def info(self):
        # urlretrieve expects info() to return a dictionary.
        return {}


class GsutilUnitTests(unittest.TestCase):
    def setUp(self):
        self.fake = FakeCall()
        self.tempdir = tempfile.mkdtemp()
        self.old_urlopen = getattr(urllib.request, 'urlopen')
        self.old_call = getattr(subprocess, 'call')
        setattr(urllib.request, 'urlopen', self.fake)
        setattr(subprocess, 'call', self.fake)

    def tearDown(self):
        self.assertEqual(self.fake.expectations, [])
        shutil.rmtree(self.tempdir)
        setattr(urllib.request, 'urlopen', self.old_urlopen)
        setattr(subprocess, 'call', self.old_call)

    def add_md5_expectation(self, url, data):
        md5_calc = hashlib.md5()
        md5_calc.update(data)
        b64_md5 = base64.b64encode(md5_calc.digest()).decode('utf-8')
        response_data = json.dumps({'md5Hash': b64_md5}).encode('utf-8')
        self.fake.add_expectation(url, _returns=MockResponse(response_data))

    def add_file_expectation(self, url, data):
        self.fake.add_expectation(url, None, _returns=MockResponse(data))

    def test_download_gsutil(self):
        version = gsutil.VERSION
        filename = 'gsutil_%s.zip' % version
        full_filename = os.path.join(self.tempdir, filename)
        fake_file = b'This is gsutil.zip'
        fake_file2 = b'This is other gsutil.zip'
        metadata_url = gsutil.API_URL + filename
        url = gsutil.GSUTIL_URL + filename

        # The md5 is valid, so download_gsutil should download the file.
        self.add_md5_expectation(metadata_url, fake_file)
        self.add_file_expectation(url, fake_file)
        self.assertEqual(gsutil.download_gsutil(version, self.tempdir),
                         full_filename)
        with open(full_filename, 'rb') as f:
            self.assertEqual(fake_file, f.read())
        self.assertEqual(self.fake.expectations, [])

        # The md5 is valid, so download_gsutil should use the existing file.
        self.add_md5_expectation(metadata_url, fake_file)
        self.assertEqual(gsutil.download_gsutil(version, self.tempdir),
                         full_filename)
        with open(full_filename, 'rb') as f:
            self.assertEqual(fake_file, f.read())
        self.assertEqual(self.fake.expectations, [])

        # The md5 is invalid for a new file, so download_gsutil should raise an
        # error.
        self.add_md5_expectation(metadata_url, b'aaaaaaa')
        self.add_file_expectation(url, fake_file2)
        self.assertRaises(gsutil.InvalidGsutilError, gsutil.download_gsutil,
                          version, self.tempdir)
        self.assertEqual(self.fake.expectations, [])

        # The md5 is valid and the new file is already downloaded, so it should
        # be used without downloading again.
        self.add_md5_expectation(metadata_url, fake_file2)
        self.assertEqual(gsutil.download_gsutil(version, self.tempdir),
                         full_filename)
        with open(full_filename, 'rb') as f:
            self.assertEqual(fake_file2, f.read())
        self.assertEqual(self.fake.expectations, [])

    def test_ensure_gsutil_full(self):
        version = gsutil.VERSION
        gsutil_dir = os.path.join(self.tempdir, 'gsutil_%s' % version, 'gsutil')
        gsutil_bin = os.path.join(gsutil_dir, 'gsutil')
        gsutil_flag = os.path.join(gsutil_dir, 'install.flag')
        os.makedirs(gsutil_dir)

        zip_filename = 'gsutil_%s.zip' % version
        metadata_url = gsutil.API_URL + zip_filename
        url = gsutil.GSUTIL_URL + zip_filename
        _, tempzip = tempfile.mkstemp()
        fake_gsutil = 'Fake gsutil'
        with zipfile.ZipFile(tempzip, 'w') as zf:
            zf.writestr('gsutil/gsutil', fake_gsutil)
        with open(tempzip, 'rb') as f:
            fake_file = f.read()
            self.add_md5_expectation(metadata_url, fake_file)
            self.add_file_expectation(url, fake_file)

        # This should write the gsutil_bin with 'Fake gsutil'
        gsutil.ensure_gsutil(version, self.tempdir, False)
        self.assertTrue(os.path.exists(gsutil_bin))
        with open(gsutil_bin, 'r') as f:
            self.assertEqual(f.read(), fake_gsutil)
        self.assertTrue(os.path.exists(gsutil_flag))
        self.assertEqual(self.fake.expectations, [])

    def test_ensure_gsutil_short(self):
        version = gsutil.VERSION
        gsutil_dir = os.path.join(self.tempdir, 'gsutil_%s' % version, 'gsutil')
        gsutil_bin = os.path.join(gsutil_dir, 'gsutil')
        gsutil_flag = os.path.join(gsutil_dir, 'install.flag')
        os.makedirs(gsutil_dir)

        with open(gsutil_bin, 'w') as f:
            f.write('Foobar')
        with open(gsutil_flag, 'w') as f:
            f.write('Barbaz')
        self.assertEqual(gsutil.ensure_gsutil(version, self.tempdir, False),
                         gsutil_bin)


    @mock.patch('sys.platform', 'linux')
    def test__is_supported_platform_returns_true_for_supported_platform(self):
        self.assertTrue(gsutil._is_luci_auth_supported_platform())

    @mock.patch('sys.platform', 'aix')
    def test__is_supported_platform_returns_false_for_unsupported_platform(
            self):
        self.assertFalse(gsutil._is_luci_auth_supported_platform())


if __name__ == '__main__':
    unittest.main()
