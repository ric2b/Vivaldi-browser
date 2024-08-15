# Copyright 2023 Google LLC
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

add_executable(
        np_ffi_tests
        byte_buffer_tests.cc
        credential_book_tests.cc
        credential_slab_tests.cc
        deserialize_result_tests.cc
        np_cpp_test.h
        np_cpp_test.cc
	v0_encrypted_deserialization_tests.cc
	v0_encrypted_serialization_tests.cc
	v0_unencrypted_deserialization_tests.cc
	v0_unencrypted_serialization_tests.cc
	v1_encrypted_deserialization_tests.cc
	v1_unencrypted_deserialization_tests.cc
)

target_link_libraries(
        np_ffi_tests
        nearby_protocol
        shared_test_util
        GTest::gtest_main
)

if (UNIX)
    target_link_libraries(
            np_ffi_tests
            dl -fsanitize=address
    )
elseif (MSVC)
    target_link_libraries(
            np_ffi_tests
            bcrypt ntdll userenv ws2_32
    )
endif ()

gtest_discover_tests(np_ffi_tests)
