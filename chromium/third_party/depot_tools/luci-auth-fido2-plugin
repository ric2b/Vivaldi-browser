#!/usr/bin/env vpython3
# Copyright 2025 The ChromiumOS Authors
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

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from contextlib import contextmanager
import dataclasses
import json
import logging
import signal
import sys
from threading import Event
from typing import BinaryIO

from fido2.client import DefaultClientDataCollector
from fido2.client import Fido2Client, UserInteraction, WebAuthnClient
from fido2.hid import CtapHidDevice
from fido2.webauthn import AuthenticationResponse
from fido2.webauthn import PublicKeyCredentialRequestOptions, UserVerificationRequirement

try:
    from fido2.client.windows import WindowsClient
except ImportError:
    WindowsClient = None

_PLUGIN_ENDIANNESS = 'little'
_PLUGIN_HEADER_SIZE = 4

# Exit codes.
_EXIT_NO_FIDO2_DEVICES = 11
_EXIT_ALL_ASSERTIONS_FAILED = 12
_EXIT_NO_MATCHING_CRED = 13


def read_full(r: BinaryIO, size: int) -> bytes:
    """Read an exact amount of data.

    Raises exception on error or EOF.
    """
    b = r.read(size)
    if len(b) != size:
        raise EOFError(f"premature EOF when reading {size} bytes from {r}.")
    return b


def write_full(w: BinaryIO, b: bytes):
    """Write all bytes.

    Raises IOError if the write isn't complete.
    """
    written = w.write(b)
    if written != len(b):
        raise IOError(
            f"failed to write fully, wrote {written} bytes out of {_PLUGIN_HEADER_SIZE} bytes."
        )


def plugin_read(r: BinaryIO) -> bytes:
    """Read a framed WebAuthn plugin message.

    A frame consists of: 4 bytes of little endian uint32 length, plus
    this amount bytes of binary data.
    """
    header = read_full(r, _PLUGIN_HEADER_SIZE)
    length = int.from_bytes(header, _PLUGIN_ENDIANNESS, signed=False)
    return read_full(r, length)


def plugin_write(w: BinaryIO, b: bytes):
    """Write a framed Webauthn plugin message.

    A frame consists of: 4 bytes of little endian uint32 length, plus
    this amount bytes of binary data.
    """
    length = len(b)
    header = length.to_bytes(_PLUGIN_HEADER_SIZE,
                             _PLUGIN_ENDIANNESS,
                             signed=False)
    write_full(w, header)
    write_full(w, b)


@dataclasses.dataclass
class PluginRequest:
    origin: str
    public_key_credential_request: PublicKeyCredentialRequestOptions


def parse_plugin_request(b: bytes) -> PluginRequest:
    """Parse a plugin request JSON string."""
    j = json.loads(b)

    req = PublicKeyCredentialRequestOptions.from_dict(j["requestData"])

    # Apply overrides to certain fields.
    req = PublicKeyCredentialRequestOptions(
        challenge=req.challenge,
        rp_id=req.rp_id,
        allow_credentials=req.allow_credentials,
        hints=req.hints,

        # Default to 30s timeout.
        timeout=req.timeout or 30_000,

        # Discourage UV.
        #
        # ReAuth flow is triggered for user who's already logged in, so
        # there's no need to ask for PIN/password authentication factor.
        #
        # Here we only want to test for user presence and ownership of
        # the private key.
        user_verification=UserVerificationRequirement.DISCOURAGED,

        # Don't support extensions for now.
        extensions=None,
    )

    return PluginRequest(
        origin=j["origin"],
        public_key_credential_request=req,
    )


def encode_plugin_response(a: AuthenticationResponse) -> bytes:
    """Encode a plugin response to JSON."""
    return json.dumps({
        "type": "getResponse",
        "responseData": dict(a),
        "error": None,
    }).encode('utf-8')


class DiscardInteraction(UserInteraction):
    """Handler when user interaction is required.

    This plugin's stdin/stdout talks with git-credential-luci, so we fail
    actions that require user input (this plugin shouldn't set any flag
    that require user interaction).
    """

    def prompt_up(self):
        sys.stderr.write("\nTouch your blinking security key to continue.\n\n")

    def request_pin(self, permissions, rp_id):
        # This plugin shouldn't set assertion flags that will require
        # PIN entry.
        return None

    def request_uv(self, permissions, rp_id):
        # Don't allow user verification (UV), because we don't allow PIN
        # entry, UV will fail.
        return False


def get_clients(origin: str) -> list[tuple[WebAuthnClient, str]]:
    """Return WebAuthn clients.

    The return value is a list of (WebAuthnClient, client description)
    where we can send assertion requests to.

    On Windows, this method returns a client that talks with Win32 API
    if available.
    """
    client_data_collector = DefaultClientDataCollector(origin)

    # Use Windows WebAuthn API if available.
    if WindowsClient and WindowsClient.is_available():
        logging.debug("Using WindowsClient")
        return [(WindowsClient(client_data_collector), "WindowsWebAuthn")]

    user_interaction = DiscardInteraction()
    clients = []
    for dev in CtapHidDevice.list_devices():
        desc = dev.descriptor
        desc_str = (f'CtapHidDevice {desc.product_name}'
                    f' (VID 0x{desc.vid:04x},'
                    f' PID 0x{desc.pid:04x}) at {desc.path}')
        logging.debug("Found %s", desc_str)
        clients.append((
            Fido2Client(
                dev,
                client_data_collector=client_data_collector,
                user_interaction=user_interaction,
            ),
            desc_str,
        ))

    return clients


def assert_on_client(*, client: WebAuthnClient, client_desc: str,
                     request: PublicKeyCredentialRequestOptions, cancel: Event):
    try:
        return client.get_assertion(request, cancel)
    except Exception as e:
        if not cancel.is_set():
            logging.error("Assertion failed on %s: %s", client_desc, e)
        return None


@contextmanager
def set_event_on_signal(signum: int, event: Event):
    """Return a context manager that sets `event` when `signum` is signaled."""
    original_handler = signal.getsignal(signum)

    def handler(signum, _):
        logging.info("Signal %s received.", signal.strsignal(signum))
        event.set()

    signal.signal(signum, handler)
    try:
        yield
    finally:
        signal.signal(signum, original_handler)


def get_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=
        "A LUCI Auth plugin to perform FIDO2 security key assertions", )
    parser.add_argument(
        "-l",
        "--list-devices",
        action="store_true",
        default=False,
        help=
        "If set, detects FIDO devices, then print their information to stderr, "
        "then exit this program. Useful for troubleshoot udev rules and "
        "permission issues on Linux.")
    return parser


def main():
    args = get_parser().parse_args()

    logging.basicConfig(level=logging.INFO)

    # If requested, probe FIDO devices, print their information, then exit.
    if args.list_devices:
        # A "stub" origin to satisfy Fido2Client constructor.
        stub_origin = "chromium.org"
        clients = get_clients(stub_origin)
        if clients:
            logging.info("Found the following FIDO devices:")
            for _, client_desc in clients:
                logging.info("    * %s", client_desc)
        else:
            logging.info("No available FIDO device.")
        sys.exit(0)

    plugin_req = parse_plugin_request(plugin_read(sys.stdin.buffer))

    clients = get_clients(plugin_req.origin)
    if not clients:
        logging.error("No available FIDO device.")
        sys.exit(_EXIT_NO_FIDO2_DEVICES)

    # Race and retrieve the first successful assertion.
    outcome = None
    cancel = Event()
    with set_event_on_signal(signal.SIGINT, cancel), set_event_on_signal(
            signal.SIGTERM,
            cancel), ThreadPoolExecutor(max_workers=len(clients)) as executor:
        futures = [
            executor.submit(assert_on_client,
                            client=client,
                            client_desc=desc,
                            request=plugin_req.public_key_credential_request,
                            cancel=cancel) for client, desc in clients
        ]
        for future in as_completed(futures):
            if result := future.result():
                outcome = result
                cancel.set()
                break

    if not outcome:
        logging.error("All assertions failed or timed out.")
        sys.exit(_EXIT_ALL_ASSERTIONS_FAILED)

    assertions = outcome.get_assertions()
    if not assertions:
        logging.error("No matching credential.")
        sys.exit(_EXIT_NO_MATCHING_CRED)
    elif len(assertions) > 1:
        logging.warning(
            "Multiple assertions returned for rp_id %s, selecting the first one.",
            plugin_req.public_key_credential_request.rp_id)

    # Write the first completed assertion.
    plugin_write(sys.stdout.buffer,
                 encode_plugin_response(outcome.get_response(0)))


if __name__ == "__main__":
    main()
