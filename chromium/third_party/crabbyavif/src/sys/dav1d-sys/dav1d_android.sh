#!/bin/bash

# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script will build dav1d for the default ABI targets supported by android.
# This script only works on linux. You must pass the path to the android NDK as
# a parameter to this script.
#
# Android NDK: https://developer.android.com/ndk/downloads
#
# The git tag below is known to work, and will occasionally be updated. Feel
# free to use a more recent commit.

set -e

if [ $# -ne 1 ]; then
  echo "Usage: ${0} <path_to_android_ndk>"
  exit 1
fi

if [ ! -d dav1d ]; then
  git clone -b 1.2.1 --depth 1 https://code.videolan.org/videolan/dav1d.git
fi
cd dav1d
mkdir build.android
cd build.android

# This only works on linux and mac.
if [ "$(uname)" == "Darwin" ]; then
  HOST_TAG="darwin"
else
  HOST_TAG="linux"
fi
android_bin="${1}/toolchains/llvm/prebuilt/${HOST_TAG}-x86_64/bin"

ARCH_LIST=("arm" "aarch64" "x86" "x86_64")
for i in "${!ARCH_LIST[@]}"; do
  arch="${ARCH_LIST[i]}"
  mkdir "${arch}"
  cd "${arch}"
  PATH=$PATH:${android_bin} meson setup --default-library=static --buildtype release \
    --cross-file="../../package/crossfiles/${arch}-android.meson" \
    -Denable_tools=false -Denable_tests=false ../..
  PATH=$PATH:${android_bin} ninja -j50
  cd ..
done

cd ../..
