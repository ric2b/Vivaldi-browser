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

# This script will build libyuv for the default ABI targets supported by
# android. You must pass the path to the android NDK as a parameter to this
# script.
#
# Android NDK: https://developer.android.com/ndk/downloads

set -e

if [ $# -ne 1 ]; then
  echo "Usage: ${0} <path_to_android_ndk>"
  exit 1
fi

#git clone --single-branch https://chromium.googlesource.com/libyuv/libyuv

cd libyuv
: # When changing the commit below to a newer version of libyuv, it is best to make sure it is being used by chromium,
: # because the test suite of chromium provides additional test coverage of libyuv.
: # It can be looked up at https://source.chromium.org/chromium/chromium/src/+/main:DEPS?q=libyuv.
git checkout 464c51a0

mkdir build.android
cd build.android

ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"
for abi in ${ABI_LIST}; do
  mkdir "${abi}"
  cd "${abi}"
  cmake ../.. \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_TOOLCHAIN_FILE=${1}/build/cmake/android.toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DANDROID_ABI=${abi}
  make yuv
  cd ..
done

cd ../..
