// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fcp/tensorflow/serve_slices_registry.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/base/random_token.h"
#include "fcp/tensorflow/host_object.h"
#include "tensorflow/core/framework/tensor.h"

namespace fcp {
namespace {

using ::testing::_;
using ::testing::Return;

using MockServeSlicesCallback = ::testing::MockFunction<std::string(
    RandomToken, std::vector<tensorflow::Tensor>, int32_t, absl::string_view,
    std::vector<std::string>, absl::string_view, absl::string_view,
    absl::string_view)>;

TEST(ServeSlicesRegistryTest, CanRegisterGetAndUnregisterCallback) {
  MockServeSlicesCallback mock_callback;
  std::optional<RandomToken> id = std::nullopt;
  {
    HostObjectRegistration registration =
        register_serve_slices_callback(mock_callback.AsStdFunction());
    id = registration.token();
    std::optional<std::shared_ptr<ServeSlicesCallback>> returned_callback =
        get_serve_slices_callback(*id);
    ASSERT_TRUE(returned_callback.has_value());

    std::string mock_served_at_id = "served_at_id";
    EXPECT_CALL(mock_callback, Call(*id, _, _, _, _, _, _, _))
        .WillOnce(Return(mock_served_at_id));
    EXPECT_EQ(mock_served_at_id,
              (**returned_callback)(*id, {}, 0, "", {}, "", "", ""));
  }
  // Check that it is gone after `registration` has been destroyed.
  EXPECT_EQ(std::nullopt, get_serve_slices_callback(*id));
}

TEST(ServeSlicesRegistryTest, CanRegisterMultipleDifferentCallbacks) {
  constexpr int8_t num_callbacks = 5;
  MockServeSlicesCallback mock_callbacks[num_callbacks];
  std::vector<HostObjectRegistration> callback_tokens;
  // Register all callbacks.
  for (int8_t i = 0; i < num_callbacks; i++) {
    callback_tokens.push_back(
        register_serve_slices_callback(mock_callbacks[i].AsStdFunction()));
  }
  // Get and invoke all callbacks.
  for (int8_t i = 0; i < num_callbacks; i++) {
    RandomToken id = callback_tokens[i].token();
    std::optional<std::shared_ptr<ServeSlicesCallback>> returned_callback =
        get_serve_slices_callback(id);
    ASSERT_TRUE(returned_callback.has_value());

    std::string mock_served_at_id = absl::StrCat("served_at_id_", i);
    EXPECT_CALL(mock_callbacks[i], Call(id, _, _, _, _, _, _, _))
        .WillOnce(Return(mock_served_at_id));
    EXPECT_EQ(mock_served_at_id,
              (**returned_callback)(id, {}, 0, "", {}, "", "", ""));
  }
}

}  // namespace
}  // namespace fcp
