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

#include "fcp/base/source_location.h"

#include "gtest/gtest.h"

namespace fcp {

SourceLocation Foo() { return SourceLocation::current(); }

SourceLocation Bar() { return SourceLocation::current(); }

TEST(SourceLocation, Test) {
  EXPECT_EQ(Foo().line(), Foo().line());
  EXPECT_EQ(Bar().line(), Bar().line());
  EXPECT_EQ(Bar().file_name(), Bar().file_name());
#ifdef FCP_HAS_SOURCE_LOCATION
  EXPECT_NE(Foo().line(), Bar().line());
#endif
}

}  // namespace fcp
