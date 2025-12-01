/*
 * Copyright 2018 Google LLC
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

#include "fcp/secagg/shared/input_vector_specification.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Eq;

TEST(InputVectorSpecificationTest, GettersReturnAppropriateValues) {
  std::string name = "test";
  int length = 2;
  uint64_t modulus = 256;

  InputVectorSpecification vec_spec(name, length, modulus);
  EXPECT_THAT(vec_spec.modulus(), Eq(modulus));
  EXPECT_THAT(vec_spec.length(), Eq(length));
  EXPECT_THAT(vec_spec.name(), Eq(name));
}

TEST(InputVectorSpecificationTest, ConstructorDiesOnSmallModulus) {
  EXPECT_DEATH(InputVectorSpecification vec_spec("test", 5, 1),
               "The specified modulus is not valid");
}

TEST(InputVectorSpecificationTest, ConstructorDiesOnModulusGreatorThanMax) {
  EXPECT_DEATH(InputVectorSpecification vec_spec("test", 5,
                                                 SecAggVector::kMaxModulus + 1),
               "The specified modulus is not valid");
}

TEST(InputVectorSpecificationTest, ConstructorDiesOnNegativeLength) {
  EXPECT_DEATH(InputVectorSpecification vec_spec("test", -1, 256),
               "Length must be >= 0");
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
