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

#include "fcp/secagg/testing/test_matchers.h"

#include <string>

namespace fcp {
namespace secagg {
namespace testing {

SecAggVectorData ToSecAggVectorData(const SecAggVector& vector){
  return std::make_pair(vector.modulus(), vector.GetAsUint64Vector());
}

SecAggVectorDataMap ToDataMap(const SecAggVectorMap& secagg_vector_map) {
  SecAggVectorDataMap result;
  for (const auto& item : secagg_vector_map) {
    result.emplace(item.first, ToSecAggVectorData(item.second));
  }
  return result;
}

class SecAggVectorMapMatcherImpl
    : public ::testing::MatcherInterface<const SecAggVectorMap&> {
 public:
  explicit SecAggVectorMapMatcherImpl(SecAggVectorDataMap expected)
      : expected_(expected) {}
  void DescribeTo(::std::ostream* os) const override {
    for (const auto& item : expected_) {
      *os << "{name: \"" << item.first << "\", modulus: " << item.second.first
          << ", vector:";
      for (uint64_t val : item.second.second) {
        *os << " " << val;
      }
      *os << "} ";
    }
  }

  bool MatchAndExplain(
      const SecAggVectorMap& arg,
      ::testing::MatchResultListener* listener) const override {
    return ::testing::ExplainMatchResult(
        ::testing::UnorderedElementsAreArray(expected_), ToDataMap(arg),
        listener);
  }

 private:
  SecAggVectorDataMap expected_;
};

SecAggVectorMapMatcher::operator ::testing::Matcher<const SecAggVectorMap&>()
    const {
  return ::testing::MakeMatcher(new SecAggVectorMapMatcherImpl(expected_));
}

SecAggVectorMapMatcher MatchesSecAggVectorMap(const SecAggVectorMap& expected) {
  return SecAggVectorMapMatcher(ToDataMap(expected));
}

SecAggVectorMapMatcher MatchesSecAggVector(const std::string& name,
                                           const SecAggVector& vector) {
  SecAggVectorDataMap expected;
  expected.emplace(name, ToSecAggVectorData(vector));
  return SecAggVectorMapMatcher(expected);
}

}  // namespace testing
}  // namespace secagg
}  // namespace fcp
