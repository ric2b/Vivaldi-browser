# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common code shared between different MCP server areas."""

import json
import subprocess

from mcp.server import fastmcp


async def run_prpc_call(ctx: fastmcp.Context, server: str, service: str,
                        message: dict) -> str:
    """Runs 'prpc call' with the given parameters.

    Args:
        server: The server the request is for, e.g. cr-buildbucket.appspot.com.
        service: The specific RPC service to call.
        message: The RPC message to send to the service.

    Returns:
        A string containing the JSON response of the call.
    """
    command = [
        'prpc',
        'call',
        server,
        service,
    ]
    result = subprocess.run(
        command,
        capture_output=True,
        input=json.dumps(message),
        check=True,
        text=True,
    )
    await ctx.info(result.stdout)
    await ctx.info(result.stderr)
    return result.stdout
