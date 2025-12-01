# Copyright 2018 Google LLC
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

"""Provides the golden_file_test() rule."""

GOLDEN_FILE_CHECKER_TEMPLATE = """
#!/bin/bash
set -o nounset
set -o errexit

readonly EXPECTED={EXPECTED}
readonly ACTUAL={ACTUAL}

diff -u --label EXPECTED "$EXPECTED" --label ACTUAL "$ACTUAL"
readonly r=$?

if (( $r != 0 )); then
  echo "***" >&2
  echo "*** FAIL: Contents of $ACTUAL do not match $EXPECTED" >&2
  echo "***" >&2
fi

exit $r
"""

def _golden_file_test_impl(ctx):
    replacements = {
        "{EXPECTED}": repr(ctx.file.expected.short_path),
        "{ACTUAL}": repr(ctx.file.actual.short_path),
    }

    contents = GOLDEN_FILE_CHECKER_TEMPLATE
    for k, v in replacements.items():
        contents = contents.replace(k, v)

    ctx.actions.write(
        ctx.outputs.sh,
        contents,
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [ctx.file.expected, ctx.file.actual])
    return [DefaultInfo(executable = ctx.outputs.sh, runfiles = runfiles)]

golden_file_test = rule(
    implementation = _golden_file_test_impl,
    outputs = {"sh": "%{name}.sh"},
    test = True,
    attrs = {
        "actual": attr.label(allow_single_file = True),
        "expected": attr.label(allow_single_file = True),
    },
)
"""Checks that two files are equal; fails with a text diff otherwise."""
