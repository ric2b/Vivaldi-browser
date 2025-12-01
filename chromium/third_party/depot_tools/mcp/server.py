#!/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The MCP server that provides tools"""
from collections.abc import Sequence
import pathlib
import os
import sys

sys.path.insert(
    0,
    os.path.abspath(
        pathlib.Path(__file__).resolve().parent.parent.joinpath(
            pathlib.Path('infra_lib'))))
import telemetry

import buildbucket

from absl import app

from mcp.server import fastmcp

mcp = fastmcp.FastMCP('chrome-infra-mcp')


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError('Too many command-line arguments.')

    # Only initialize telemetry if the user is opted in. The MCP does not
    # currently have the ability to show the banner so we need to rely on other
    # tools to get consent
    if telemetry.opted_in():
        telemetry.initialize('chromium.mcp')

    mcp.add_tool(buildbucket.get_build)
    mcp.add_tool(buildbucket.get_build_from_build_number)
    mcp.add_tool(buildbucket.get_build_from_id)
    mcp.add_tool(buildbucket.get_build_status)
    mcp.run()


if __name__ == '__main__':
    app.run(main)
