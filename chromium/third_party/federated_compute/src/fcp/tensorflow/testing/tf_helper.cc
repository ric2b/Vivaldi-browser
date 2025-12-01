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

#include "fcp/tensorflow/testing/tf_helper.h"

#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "tensorflow/cc/framework/scope.h"

namespace fcp {

absl::Cord CreateGraph(tensorflow::Scope* root) {
  tensorflow::GraphDef def;
  absl::Status to_graph_status = root->ToGraphDef(&def);
  EXPECT_TRUE(to_graph_status.ok()) << to_graph_status;
  // TODO(team): Use SerializeAsCord when available.
  return absl::Cord(def.SerializeAsString());
}

}  // namespace fcp
