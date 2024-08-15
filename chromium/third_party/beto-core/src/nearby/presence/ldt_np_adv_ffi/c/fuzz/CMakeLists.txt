# Copyright 2022 Google LLC
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

fuzztest_setup_fuzzing_flags()

add_executable(
        ldt_fuzzer
        ldt_fuzzer.cc
)

target_link_libraries(
        ldt_fuzzer
        PRIVATE
        optimized "${NEARBY_ROOT}/target/release/${CMAKE_STATIC_LIBRARY_PREFIX}ldt_np_adv_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}"
        debug "${NEARBY_ROOT}/target/debug/${CMAKE_STATIC_LIBRARY_PREFIX}ldt_np_adv_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}"
        ssl)

link_fuzztest(ldt_fuzzer)
gtest_discover_tests(ldt_fuzzer)