# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tools for interacting with ResultDB"""

import json
import posixpath
import requests

import bs4
from mcp.server import fastmcp
import telemetry

import common

tracer = telemetry.get_tracer(__name__)

RESULTDB_SERVER = 'results.api.luci.app'


async def get_non_exonerated_unexpected_results_from_build(
    ctx: fastmcp.Context,
    build_id: str,
):
    """Gets test results from the specified build.

    The returned results will only be for unexpected results which were not
    exonerated, i.e. unexpected test failures.

    Args:
        build_id: The Buildbucket ID of the build to retrieve results for. This
            should only be the numerical ID, i.e. not prefixed with 'build-'.

    Returns:
        The stdout of the prpc command which should be a JSON string for a
        luci.resultdb.v1.ResultDB.QueryTestResultsResponse proto. See
        https://source.chromium.org/chromium/infra/infra_superproject/+/main:infra/go/src/go.chromium.org/luci/resultdb/proto/v1/resultdb.proto
        for more details.
    """
    with tracer.start_as_current_span(
            'chromium.mcp.get_non_exonerated_unexpected_results_from_build'):
        if not build_id.isnumeric():
            raise ValueError(
                f'Provided build_id {build_id} contains non-numeric characters')
        request = {
            'invocations': [
                f'invocations/build-{build_id}',
            ],
            'predicate': {
                'expectancy': 'VARIANTS_WITH_UNEXPECTED_RESULTS',
                'exclude_exonerated': True,
            },
            'read_mask': ('name,resultId,variant,status,statusV2,duration,'
                          'failureReason,summary_html'),
        }
        response = await common.run_prpc_call(
            ctx, RESULTDB_SERVER, 'luci.resultdb.v1.ResultDB.QueryTestResults',
            request)
        return response


async def expand_summary_html(
    ctx: fastmcp.Context,
    result_name: str,
    summary_html: str,
) -> str:
    """Expands the given summary HTML with referenced artifact content.

    The summaryHtml field included in ResultDB test results often references
    artifacts with `text-artifact` tags which contain the bulk of the useful
    information. These references are automatically expanded in Milo, but need
    to be manually expanded if accessing ResultDB directly.

    Args:
        result_name: The name of the result whose summary is being expanded.
            This corresponds to the `name` field of a ResultDB result. Any URL
            encoding must be left as-is.
        summary_html: The summary HTML to expand. Corresponds to the
            `summaryHtml` field of a ResultDB result.

    Returns:
        A copy of |summary_html| with any `text-artifact` tags replaced with
        the contents of the artifacts they reference.
    """
    with tracer.start_as_current_span('chromium.mcp.expand_summary_html'):
        soup = bs4.BeautifulSoup(summary_html, 'html.parser')
        for tag in soup.find_all('text-artifact'):
            artifact_id = tag.get('artifact-id')
            if not artifact_id:
                continue
            artifact_content = await get_test_level_text_artifact(
                ctx, result_name, artifact_id)
            tag.replace_with(artifact_content)
        return str(soup)


async def get_test_level_text_artifact(
    ctx: fastmcp.Context,
    result_name: str,
    artifact_id: str,
) -> str:
    """Retrieves the content for the specified test result level text artifact.

    Since the expected content type is text, this cannot be used for retrieving
    binary artifacts.

    Test result level artifacts are associated with a single test (i.e. one
    test case), which is different from an invocation level artifact (i.e.
    for an entire Swarming task).

    Args:
        result_name: The name of the result whose artifact is being retrieved.
            This corresponds to the `name` field of a ResultDB result. Any URL
            encoding must be left as-is.
        artifact_id: The ID of the artifact being retrieved. When combined with
            |result_name|, this uniquely identifies an artifact within ResultDB.

    Returns:
        A string containing the contents of the specified artifact.
    """
    with tracer.start_as_current_span(
            'chromium.mcp.get_test_level_text_artifact'):
        artifact_name = posixpath.join(result_name, 'artifacts', artifact_id)
        request = {
            'name': artifact_name,
        }
        prpc_response = await common.run_prpc_call(
            ctx, RESULTDB_SERVER, 'luci.resultdb.v1.ResultDB.GetArtifact',
            request)
        response = json.loads(prpc_response)

        content_type = response['contentType']
        if 'text/plain' not in content_type:
            raise ValueError(
                f'Expected text artifact, got content type {content_type}')

        r = requests.get(response['fetchUrl'])
        r.raise_for_status()
        return r.text
