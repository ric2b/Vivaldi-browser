# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tools for interacting with git cl"""
import subprocess

from mcp.server import fastmcp
import telemetry

tracer = telemetry.get_tracer(__name__)


async def try_builder_results(
    ctx: fastmcp.Context,
    checkout: str,
    change_list_issue: int | None = None,
):
    """Gets the try builder results for the provided change list issue

    The url of a gerrit change can be parsed from a gerrit cl e.g.
    https://chromium-review.googlesource.com/c/chromium/src/+/<change_list_issue>

    Args:
      checkout: Location of the current checkout.
      change_list_issue: The change list (CL) issue id. If none is provided,
        the current branch and its associated CL is used.

    Returns:
      A json list of builds that either ran or are still running on the provided
      CL or current branch.
    """
    with tracer.start_as_current_span('chromium.mcp.try_builder_results'):
        command = [
            "git",
            "cl",
            "try-results",
            "--json=-",
        ]
        if change_list_issue:
            command.extend(['-i', str(change_list_issue)])
        result = subprocess.run(
            command,
            capture_output=True,
            check=True,
            text=True,
            cwd=checkout,
        )
        await ctx.info(f'stdout {result.stdout}')
        await ctx.info(f'stderr {result.stderr}')
        return result.stdout


async def get_current_changes(
    ctx: fastmcp.Context,
    checkout: str,
) -> str:
    """Shows differences between local tree and last upload.

    Args:
      checkout: Location of the current checkout.

    Returns:
      A diff of the current checkout and the last upload.
    """
    with tracer.start_as_current_span('chromium.mcp.get_current_changes'):
        command = [
            "git",
            "cl",
            "diff",
        ]
        result = subprocess.run(
            command,
            capture_output=True,
            check=True,
            text=True,
            cwd=checkout,
        )
        await ctx.info(f'stdout {result.stdout}')
        await ctx.info(f'stderr {result.stderr}')
        return result.stdout


async def format_checkout(
    ctx: fastmcp.Context,
    checkout: str,
) -> None:
    """Format the current checkout.

    This step should be called before attempting to upload any
    code.

    Args:
      checkout: Location of the current checkout.

    Returns:
      None
    """
    with tracer.start_as_current_span('chromium.mcp.format'):
        command = [
            "git",
            "cl",
            "format",
        ]
        result = subprocess.run(
            command,
            capture_output=True,
            check=True,
            text=True,
            cwd=checkout,
        )
        await ctx.info(f'stdout {result.stdout}')
        await ctx.info(f'stderr {result.stderr}')
        return result.stdout


async def upload_change_list(
    ctx: fastmcp.Context,
    checkout: str,
) -> None:
    """Uploads the current committed changes to codereview

    Args:
      checkout: Location of the current checkout.

    Returns:
      None
    """
    command = [
        "git",
        "cl",
        "upload",
        "-f",
    ]
    result = subprocess.run(
        command,
        capture_output=True,
        check=True,
        text=True,
        cwd=checkout,
    )
    await ctx.info(f'stdout {result.stdout}')
    await ctx.info(f'stderr {result.stderr}')
    return result.stdout
