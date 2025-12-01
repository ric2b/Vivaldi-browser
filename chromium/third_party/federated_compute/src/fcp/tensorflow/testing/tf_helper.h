/*
 * Copyright 2020 Google LLC
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

#ifndef FCP_TENSORFLOW_TESTING_TF_HELPER_H_
#define FCP_TENSORFLOW_TESTING_TF_HELPER_H_

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/cord.h"
#include "fcp/base/result.h"
#include "fcp/tensorflow/tf_session.h"
#include "tensorflow/cc/framework/scope.h"
#include "tensorflow/cc/ops/array_ops.h"
#include "tensorflow/cc/ops/const_op.h"
#include "tensorflow/cc/ops/io_ops.h"
#include "tensorflow/cc/ops/state_ops.h"
#include "tensorflow/core/protobuf/saver.pb.h"

namespace fcp {

/**
 * Get a serialized graph with the operations defined on the provided scope.
 */
absl::Cord CreateGraph(tensorflow::Scope* root);

}  // namespace fcp

#endif  // FCP_TENSORFLOW_TESTING_TF_HELPER_H_
