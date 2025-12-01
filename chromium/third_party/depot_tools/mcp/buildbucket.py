# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tools for interacting with buildbucket"""
import json
import urllib.parse

from mcp.server import fastmcp
import telemetry

import common

BUILDBUCKET_SERVER = 'cr-buildbucket.appspot.com'

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
        request = {'id': build_id}
        response = await common.run_prpc_call(
            ctx, BUILDBUCKET_SERVER, 'buildbucket.v2.Builds.GetBuildStatus',
            request)
        return json.loads(response)['status']


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
      build_id: The BuildBucket build id. This is a unique identifier for the
        build.
      fields: A list of fields to return. Options are:
        status, input, output, id, builder, builder_info, tags, steps, infra

    Returns:
      The build in json format including the requested fields. See:
      https://chromium.googlesource.com/infra/luci/recipes-py/+/main/recipe_proto/go.chromium.org/luci/buildbucket/proto/build.proto
    """
    with tracer.start_as_current_span('chromium.mcp.get_build_from_id'):
        request = {'id': build_id, 'mask': {'fields': ','.join(fields)}}
        response = await common.run_prpc_call(ctx, BUILDBUCKET_SERVER,
                                              'buildbucket.v2.Builds.GetBuild',
                                              request)
        return response


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
    https://ci.chromium.org/ui/p/<builder_project>/builders/<builder_bucket>/<builder_name>/<build_number>/overview

    Args:
      build_number: The build number of the builder. This is a consecutive
        value that uniquely identifies the build per builder.
      builder_name: The name of the builder the build belongs to.
      builder_bucket: The bucket the builder belongs to. e.g. 'try' or 'ci'
      builder_project: The project the bucket belongs to. e.g. 'chromium'
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
        response = await common.run_prpc_call(ctx, BUILDBUCKET_SERVER,
                                              'buildbucket.v2.Builds.GetBuild',
                                              request)
        return response


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
        response = await common.run_prpc_call(ctx, BUILDBUCKET_SERVER,
                                              'buildbucket.v2.Builds.GetBuild',
                                              request)
        return response


async def get_recent_builds(
    ctx: fastmcp.Context,
    builder_name: str,
    builder_bucket: str,
    builder_project: str,
    num_builds: int,
) -> str:
    """Gets |num_builds| recent completed builds for a builder.

    This will consider any builds that have run to completion, regardless of
    status.

    The url of a builder can be deconstructed to get the relevant information,
    e.g.
    https://ci.chromium.org/ui/p/<project>/builders/<bucket>/<name>

    Args:
        builder_name: The name of the builder to get builds from. Any URL
            encoding will automatically be decoded.
        builder_bucket: The bucket the builder belongs to.
        builder_project: The project the builder belongs to.
        num_builds: How many builds to retrieve. Per the proto definition for
            the underlying request, values >1000 will be treated as 1000.

    Returns:
        The stdout of the prpc command which should be a JSON string for a
        buildbucket.v2.SearchBuildsResponse proto. See
        https://source.chromium.org/chromium/infra/infra_superproject/+/main:infra/go/src/go.chromium.org/luci/buildbucket/proto/builds_service.proto
        for more details.
    """
    with tracer.start_as_current_span('chromium.mcp.get_recent_builds'):
        return await _get_recent_builds(
            ctx,
            builder_name,
            builder_bucket,
            builder_project,
            num_builds,
            failed_builds_only=False,
        )


async def get_recent_failed_builds(
    ctx: fastmcp.Context,
    builder_name: str,
    builder_bucket: str,
    builder_project: str,
    num_builds: int,
) -> str:
    """Gets |num_builds| recent failed builds for a builder.

    This will only consider builds that have run to completion and exited with
    the FAILURE status, i.e. builds that show up as red in Milo.

    The url of a builder can be deconstructed to get the relevant information,
    e.g.
    https://ci.chromium.org/ui/p/<project>/builders/<bucket>/<name>

    Args:
        builder_name: The name of the builder to get builds from. Any URL
            encoding will automatically be decoded.
        builder_bucket: The bucket the builder belongs to.
        builder_project: The project the builder belongs to.
        num_builds: How many builds to retrieve. Per the proto definition for
            the underlying request, values >1000 will be treated as 1000.

    Returns:
        The stdout of the prpc command which should be a JSON string for a
        buildbucket.v2.SearchBuildsResponse proto. See
        https://source.chromium.org/chromium/infra/infra_superproject/+/main:infra/go/src/go.chromium.org/luci/buildbucket/proto/builds_service.proto
        for more details.
    """
    with tracer.start_as_current_span('chromium.mcp.get_recent_failed_builds'):
        return await _get_recent_builds(
            ctx,
            builder_name,
            builder_bucket,
            builder_project,
            num_builds,
            failed_builds_only=True,
        )


async def _get_recent_builds(
    ctx: fastmcp.Context,
    builder_name: str,
    builder_bucket: str,
    builder_project: str,
    num_builds: int,
    failed_builds_only: bool,
) -> str:
    """Helper function to get recent builds for a builder.

    See docstrings for get_recent_builds/get_recent_failed_builds for more
    information.

    Args:
        builder_name: Same as caller.
        builder_bucket: Same as caller.
        builder_project: Same as caller.
        num_builds: Same as caller.
        failed_builds_only: Whether to only search for failed builds instead of
            all completed builds.

    Returns:
        Same as caller.
    """
    if num_builds < 1:
        raise ValueError(f'Provided num_builds {num_builds} is not positive')
    request = {
        'predicate': {
            'builder': {
                'project': builder_project,
                'bucket': builder_bucket,
                'builder': urllib.parse.unquote(builder_name),
            },
            'status': 'FAILURE' if failed_builds_only else 'ENDED_MASK',
        },
        'page_size': f'{num_builds}'
    }
    response = await common.run_prpc_call(ctx, BUILDBUCKET_SERVER,
                                          'buildbucket.v2.Builds.SearchBuilds',
                                          request)
    return response
