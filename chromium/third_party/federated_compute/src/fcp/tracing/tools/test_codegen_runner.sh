#!/bin/bash -eu
#
# Copyright 2021 Google LLC
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

set -o errexit
set -o nounset
if [ $# -ne 3 ]; then
  echo "Usage: $0 <input.fbs> <output-dir> <header-output-dir>"
  exit 1;
fi
FBS_FILE=$1
OUT_DIR=$2
HEADER_OUT_DIR=$3
# Running flatc to parse fbs and generate bfbs
external/flatbuffers/flatc -b --schema -o ${OUT_DIR} -I "." ${FBS_FILE} 1>&2
# Flatc should have produced the following files:
BFBS_FILE=$OUT_DIR/$(basename ${FBS_FILE%.fbs}).bfbs
GENERATED_FILE=$HEADER_OUT_DIR/$(basename ${FBS_FILE%.fbs})_generated.h
# Generate header file from bfbs (to stdout)
fcp/tracing/tools/tracing_traits_generator ${GENERATED_FILE} ${BFBS_FILE} ${FBS_FILE}
