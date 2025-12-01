/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "tensorflow/core/platform/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fcp/tensorflow/serve_slices_registry.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_testutil.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"
#include "tensorflow/core/public/session.h"

namespace fcp {
namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

using MockServeSlicesCallback = ::testing::MockFunction<std::string(
    RandomToken, std::vector<tensorflow::Tensor>, int32_t, absl::string_view,
    std::vector<std::string>, absl::string_view, absl::string_view,
    absl::string_view)>;

// Constants related to the GraphDef we test with
// See make_serve_slices_test_graph.py

char const* const kExampleGraphPath =
    "fcp/tensorflow/serve_slices_test.pbtxt";
char const* const kCallbackTokenPlaceholderName = "callback_token";
char const* const kServedAtTensorName = "served_at_id:0";

// Loads the example graph created by `make_serve_slices_test_graph.py`.
tensorflow::GraphDef LoadExampleGraph() {
  int fd = open(kExampleGraphPath, O_RDONLY);
  CHECK(fd != -1) << "Failed to open example graph at path "
                  << kExampleGraphPath;

  google::protobuf::io::FileInputStream fs(fd);
  fs.SetCloseOnDelete(true);

  tensorflow::GraphDef graph;
  bool parsed = google::protobuf::TextFormat::Parse(&fs, &graph);
  CHECK(parsed) << "Invalid text-format GraphDef";

  return graph;
}

// Loads the example graph created by `make_serve_slices_test_graph.py` into a
// `tensorflow::Session`.
std::unique_ptr<tensorflow::Session> PrepareExampleGraphSession() {
  tensorflow::GraphDef graph = LoadExampleGraph();

  std::unique_ptr<tensorflow::Session> session;
  {
    tensorflow::SessionOptions options;
    tensorflow::Session* raw_session = nullptr;
    absl::Status session_new_status =
        tensorflow::NewSession(options, &raw_session);
    TF_CHECK_OK(session_new_status);
    session = std::unique_ptr<tensorflow::Session>(raw_session);
  }

  absl::Status graph_build_status = session->Create(graph);
  TF_CHECK_OK(graph_build_status);
  return session;
}

class ServeSlicesOpTest : public ::testing::Test {
 protected:
  void SetUp() override { session_ = PrepareExampleGraphSession(); }

  // Runs a `ServeSlices` session and returns the result.
  //
  // Inputs:
  //   callback_token: A `tensorflow::Tensor` to use as the `callback_token`
  //   argument to `ServeSlices`. For successful calls, this must be a
  //   `RandomToken` corresponding to the `HostObjectRegistration returned by
  //    `register_serve_slices_callback`.
  //   served_at_id_out: An output parameter into which the `served_at_id`
  //     returned from `ServeSlices` is stored.
  //
  // Outputs:
  //   The status of the `session.Run` invocation.
  absl::Status RunSession(tensorflow::Tensor callback_token,
                          tensorflow::Tensor& served_at_id_out) {
    std::vector<tensorflow::Tensor> outputs;
    absl::Status run_status =
        session_->Run({{kCallbackTokenPlaceholderName, callback_token}},
                      {kServedAtTensorName}, {}, &outputs);

    if (run_status.ok()) {
      CHECK(outputs.size() == 1)
          << "Expected one output, found " << outputs.size();
      served_at_id_out = outputs[0];
    }

    return run_status;
  }

  // Runs a `ServeSlices` session and returns the result.
  //
  // This method is similar to `RunSession`, but it expects that the run is
  // successful and enforces that the inputs and outputs are correctly-typed.
  //
  // Inputs:
  //   callback_token: The `CallbackToken` of the callback to invoke from
  //     `ServeSlices`.
  //
  // Outputs:
  //   The `served_at_id` returned from `ServeSlices`.
  std::string RunSessionExpectingSuccess(RandomToken callback_token) {
    tensorflow::Tensor served_at_id_out;
    TF_CHECK_OK(RunSession(tensorflow::Tensor(callback_token.ToString()),
                           served_at_id_out));
    return served_at_id_out.scalar<tensorflow::tstring>()();
  }

 private:
  std::unique_ptr<tensorflow::Session> session_;
};

TEST_F(ServeSlicesOpTest, SessionRunCallsBackIntoCPP) {
  std::string mock_served_at_id = "mock_served_at_id";
  MockServeSlicesCallback mock_callback;
  HostObjectRegistration callback_registration =
      register_serve_slices_callback(mock_callback.AsStdFunction());
  RandomToken callback_token = callback_registration.token();
  EXPECT_CALL(mock_callback, Call(callback_token, _, _, _, _, _, _, _))
      .WillOnce(Return(mock_served_at_id));
  std::string served_at_id = RunSessionExpectingSuccess(callback_token);
  EXPECT_EQ(served_at_id, "mock_served_at_id");
}

TEST_F(ServeSlicesOpTest, SessionRunFailsOnMissingCallback) {
  std::optional<RandomToken> callback_token;
  {
    MockServeSlicesCallback mock_callback;
    HostObjectRegistration callback_registration =
        register_serve_slices_callback(mock_callback.AsStdFunction());
    callback_token = callback_registration.token();
    // The registration gets destructed here.
  }
  tensorflow::Tensor callback_token_tensor(callback_token->ToString());
  tensorflow::Tensor served_at_id_out;
  absl::Status status = RunSession(callback_token_tensor, served_at_id_out);
  EXPECT_THAT(status, tensorflow::testing::StatusIs(
                          absl::StatusCode::kInvalidArgument,
                          HasSubstr("No `ServeSlices` callback found")));
}

}  // namespace
}  // namespace fcp
