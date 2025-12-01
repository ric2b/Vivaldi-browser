/*
 * Copyright 2019 Google LLC
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

#ifndef FCP_SECAGG_TESTING_TEST_MATCHERS_H_
#define FCP_SECAGG_TESTING_TEST_MATCHERS_H_

#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "absl/container/node_hash_map.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {
namespace testing {

using SecAggVectorData = std::pair<int, std::vector<uint64_t> >;
using SecAggVectorDataMap = absl::node_hash_map<std::string, SecAggVectorData>;

class SecAggVectorMapMatcher {
 public:
  explicit SecAggVectorMapMatcher(SecAggVectorDataMap expected)
      : expected_(expected) {}
  // Intentionally allowed to be implicit.
  operator ::testing::Matcher<const SecAggVectorMap&>() const;  // NOLINT

 private:
  SecAggVectorDataMap expected_;
};

SecAggVectorMapMatcher MatchesSecAggVectorMap(const SecAggVectorMap& expected);
SecAggVectorMapMatcher MatchesSecAggVector(const std::string& name,
                                           const SecAggVector& vector);

}  // namespace testing
}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_TESTING_TEST_MATCHERS_H_
