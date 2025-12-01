#!/usr/bin/env vpython3
# coding=utf-8
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# [VPYTHON:BEGIN]
# python_version: "3.11"
# wheel: <
#   name: "infra/python/wheels/cffi/${vpython_platform}"
#   version: "version:1.15.1.chromium.2"
# >
# wheel: <
#   name: "infra/python/wheels/cryptography/${vpython_platform}"
#   version: "version:43.0.0"
# >
# wheel: <
#   name: "infra/python/wheels/pycparser-py2_py3"
#   version: "version:2.21"
# >
# wheel: <
#   name: "infra/python/wheels/fido2-py3"
#   version: "version:2.0.0"
# >
# [VPYTHON:END]
"""Unit tests for luci_auth_fido2_plugin.py."""

import logging
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from fido2.webauthn import PublicKeyCredentialDescriptor
from fido2.webauthn import PublicKeyCredentialRequestOptions
from fido2.webauthn import PublicKeyCredentialType
from fido2.webauthn import UserVerificationRequirement

import luci_auth_fido2_plugin as plugin


class TestFido2Plugin(unittest.TestCase):

    def test_parse_plugin_request(self):
        req = b'{"type":"get","origin":"https://accounts.google.com","requestData":{"rpId":"google.com","challenge":"alice-==","timeout":30000,"allowCredentials":[{"type":"public-key","id":"key="}],"userVerification":"preferred","extensions":{"appid":"google.com"}}}'
        got = plugin.parse_plugin_request(req)
        want = plugin.PluginRequest(
            origin='https://accounts.google.com',
            public_key_credential_request=PublicKeyCredentialRequestOptions(
                challenge=b'jX\x9c{',
                timeout=30_000,
                rp_id='google.com',
                allow_credentials=[
                    PublicKeyCredentialDescriptor(
                        type=PublicKeyCredentialType.PUBLIC_KEY,
                        id=b'\x91\xec',
                    )
                ],
                user_verification=UserVerificationRequirement.DISCOURAGED,
            ),
        )
        self.assertEqual(got, want)


if __name__ == '__main__':
    logging.basicConfig(
        level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
    unittest.main()
