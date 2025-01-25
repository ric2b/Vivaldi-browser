#! /usr/bin/env bash
#                          __  __            _
#                       ___\ \/ /_ __   __ _| |_
#                      / _ \\  /| '_ \ / _` | __|
#                     |  __//  \| |_) | (_| | |_
#                      \___/_/\_\ .__/ \__,_|\__|
#                               |_| XML parser
#
# Copyright (c) 2021-2024 Sebastian Pipping <sebastian@pipping.org>
# Copyright (c) 2024      Dag-Erling Smørgrav <des@des.dev>
# Licensed under the MIT license:
#
# Permission is  hereby granted,  free of charge,  to any  person obtaining
# a  copy  of  this  software   and  associated  documentation  files  (the
# "Software"),  to  deal in  the  Software  without restriction,  including
# without  limitation the  rights  to use,  copy,  modify, merge,  publish,
# distribute, sublicense, and/or sell copies of the Software, and to permit
# persons  to whom  the Software  is  furnished to  do so,  subject to  the
# following conditions:
#
# The above copyright  notice and this permission notice  shall be included
# in all copies or substantial portions of the Software.
#
# THE  SOFTWARE  IS  PROVIDED  "AS  IS",  WITHOUT  WARRANTY  OF  ANY  KIND,
# EXPRESS  OR IMPLIED,  INCLUDING  BUT  NOT LIMITED  TO  THE WARRANTIES  OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
# NO EVENT SHALL THE AUTHORS OR  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR  OTHER LIABILITY, WHETHER  IN AN  ACTION OF CONTRACT,  TORT OR
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
# USE OR OTHER DEALINGS IN THE SOFTWARE.

PS4='# '
set -e -u -o pipefail -x

cppcheck --version

for xml_context_bytes in 0 1024; do
    for xml_ge in 0 1; do
        cppcheck_args=(
            --quiet
            --error-exitcode=1
            --force
            --check-level=exhaustive
            --suppress=objectIndex
            --suppress=unknownMacro
            -DXML_CONTEXT_BYTES=${xml_context_bytes}
            -DXML_GE=${xml_ge}
        )

        find_args=(
            -type f \(
                -name \*.cpp
                -o -name \*.c
            \)
            -not \(  # Exclude .c files that are merely included by other files
                -name xmltok_ns.c
                -o -name xmltok_impl.c
            \)
            -exec cppcheck "${cppcheck_args[@]}" {} +
        )

        time find . "${find_args[@]}"
    done
done
