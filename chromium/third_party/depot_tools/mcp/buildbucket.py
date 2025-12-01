# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tools for interacting with buildbucket"""
import json
import subprocess

from mcp.server import fastmcp
import telemetry

tracer = telemetry.get_tracer(__name__)


async def get_build_status(
    ctx: fastmcp.Context,
    build_id: str,
) -> str:
    """Gets the build status from the provided build_id

    Args:
      build_id: The buildbucket id of the build. This is not the build number.
    Return:
      The status of the build as a string
    """
    with tracer.start_as_current_span('chromium.mcp.get_build_status'):
        await ctx.info(f'Received request {build_id}')
        command = [
            'prpc',
            'call',
            'cr-buildbucket.appspot.com',
            'buildbucket.v2.Builds.GetBuildStatus',
        ]
        try:
            result = subprocess.run(
                command,
                capture_output=True,
                input=json.dumps({'id': build_id}),
                check=False,
                text=True,
            )
            await ctx.info(result.stdout)
            await ctx.info(result.stderr)
            return json.loads(result.stdout)['status']
        except Exception as e:
            await ctx.info('Exception calling prpc')
            return f'Exception calling prpc return {e}'


async def get_build_from_id(
    ctx: fastmcp.Context,
    build_id: str,
    fields: list[str],
) -> str:
    """Gets a buildbucket build from its ID

    The url of a build can be deconstructed and used to get more details about
    the build. e.g.
    https://ci.chromium.org/b/<build_id>

    Args:
      build_id: The request body for the RPC. All fields should be represented
        by strings. Integer fields will be parsed later.
        https://chromium.googlesource.com/infra/luci/luci-go/+/main/buildbucket/proto/builds_service.proto
        for more details.

        The request's mask can be set to get more information. By default only
        high level statuses will be returned. Some useful fields to include in
        this mask are:
        status, input, output, id, builder, builder_info, tags, steps, infra
        Multiple fields in the mask can be included as a comma separated string
        e.g.
        The build_number is mutually exclusive with the build_id. To get the
        build from a build_id, only the build_id is needed. e.g.
        {
          "id": "<build id>",
          "mask": {
            "fields": "steps,tags"
          }
        }
      fields: A list of fields to return. Options are:
        status, input, output, id, builder, builder_info, tags, steps, infra

    Returns:
      The build in json format including the requested fields. See:
      https://chromium.googlesource.com/infra/luci/recipes-py/+/main/recipe_proto/go.chromium.org/luci/buildbucket/proto/build.proto
    """
    with tracer.start_as_current_span('chromium.mcp.get_build_from_id'):
        request = {'id': build_id, 'mask': {'fields': ','.join(fields)}}
        command = [
            'prpc',
            'call',
            'cr-buildbucket.appspot.com',
            'buildbucket.v2.Builds.GetBuild',
        ]
        try:
            result = subprocess.run(
                command,
                capture_output=True,
                input=json.dumps(request),
                check=False,
                text=True,
            )
            await ctx.info(result.stdout)
            await ctx.info(result.stderr)
        except Exception as e:
            await ctx.info('Exception calling prpc')
            return f'Exception calling prpc return {e}'
        return result.stdout


async def get_build_from_build_number(
    ctx: fastmcp.Context,
    build_number: int,
    builder_name: int,
    builder_bucket: int,
    builder_project: int,
    fields: list[str],
) -> str:
    """Gets a buildbucket build from its build number and builder

    The url of a build can be deconstructed and used to get more details about
    the build. e.g.
    https://ci.chromium.org/b/<build_id>

    Args:
      build_id: The request body for the RPC. All fields should be represented
        by strings. Integer fields will be parsed later.
        https://chromium.googlesource.com/infra/luci/luci-go/+/main/buildbucket/proto/builds_service.proto
        for more details.

        The request's mask can be set to get more information. By default only
        high level statuses will be returned. Some useful fields to include in
        this mask are:
        status, input, output, id, builder, builder_info, tags, steps, infra
        Multiple fields in the mask can be included as a comma separated string
        The build_number is mutually exclusive with the build_id. To get the
        build from a build_id only the build_id is needed. e.g.
        {
          "id": "<build_id>",
          "mask": {
            "fields": "steps,tags"
          }
        }
      fields: A list of fields to return. Options are:
        status, input, output, id, builder, builder_info, tags, steps, infra

    Returns:
      The build in json format including the requested fields. See
      https://chromium.googlesource.com/infra/luci/recipes-py/+/main/recipe_proto/go.chromium.org/luci/buildbucket/proto/build.proto
    """
    with tracer.start_as_current_span(
            'chromium.mcp.get_build_from_build_number'):
        request = {
            'buildNumber': build_number,
            'builder': {
                'builder': builder_name,
                'bucket': builder_bucket,
                'project': builder_project
            },
            'mask': {
                'fields': ','.join(fields)
            }
        }
        command = [
            'prpc',
            'call',
            'cr-buildbucket.appspot.com',
            'buildbucket.v2.Builds.GetBuild',
        ]
        try:
            result = subprocess.run(
                command,
                capture_output=True,
                input=json.dumps(request),
                check=False,
                text=True,
            )
            await ctx.info(result.stdout)
            await ctx.info(result.stderr)
        except Exception as e:
            await ctx.info('Exception calling prpc')
            return f'Exception calling prpc return {e}'
        return result.stdout


async def get_build(
    ctx: fastmcp.Context,
    request: dict,
) -> str:
    """Calls the buildbucket.v2.Builds.GetBuild RPC to fetch a build

    The url of a build can be deconstructed and used to get more details about
    the
    build. e.g.
    https://ci.chromium.org/ui/p/chromium/builders/<bucket>/<builder_name>/<build_number>/infra
    Builds can also be in the form:
    https://ci.chromium.org/b/<build id>

    Args:
      request: The request body for the RPC. All fields should be represented
        by strings. Integer fields will be parsed later.
        https://chromium.googlesource.com/infra/luci/luci-go/+/main/buildbucket/proto/builds_service.proto
        for more details.

        The request's mask can be set to get more information. By default only
        high level statuses will be returned. Some useful fields to include in
        this mask are:
        status, input, output, id, builder, builder_info, tags, steps, infra
        Multiple fields in the mask can be included as a comma separated string
        e.g.
        {
          "build_number": "<build number>",
          "builder": {
            "bucket": "<bucket>",
            "builder": "<builder name>",
            "project": "chromium"
          },
          "mask": {
            "fields": "steps,tags"
          }
        }
        The build_number is mutually exclusive with the build_id. To get the
        build from a build_id, only the build_id is needed. e.g.
        {
          "id": "<build id>",
          "mask": {
            "fields": "steps,tags"
          }
        }

    Returns:
      The stdout of the prpc command which should be a JSON string for a
      buildbucket.v2.Build proto. See
      https://chromium.googlesource.com/infra/luci/recipes-py/+/main/recipe_proto/go.chromium.org/luci/buildbucket/proto/build.proto
      for more details.
    """
    with tracer.start_as_current_span('chromium.mcp.get_build'):
        await ctx.info(f'Received request {request}')
        command = [
            'prpc',
            'call',
            'cr-buildbucket.appspot.com',
            'buildbucket.v2.Builds.GetBuild',
        ]
        try:
            result = subprocess.run(
                command,
                capture_output=True,
                input=json.dumps(request),
                check=False,
                text=True,
            )
            await ctx.info(result.stdout)
            await ctx.info(result.stderr)
        except Exception as e:
            await ctx.info('Exception calling prpc')
            return f'Exception calling prpc return {e}'
        return result.stdout
