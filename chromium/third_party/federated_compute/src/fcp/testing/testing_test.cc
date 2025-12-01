/*
 * Copyright 2017 Google LLC
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

#include "fcp/testing/testing.h"

#include "gtest/gtest.h"
#include "absl/flags/flag.h"
#include "fcp/base/monitoring.h"
#include "fcp/testing/test_messages.pb.h"

ABSL_FLAG(std::string, baseline_path, "", "Path to baseline");

namespace fcp {

namespace {

using ::testing::Not;

TEST(TestingTest, TestName) { ASSERT_EQ(TestName(), "TestName"); }

TEST(TestingTest, TestDataPath) {
  auto path = GetTestDataPath(absl::GetFlag(FLAGS_baseline_path));
  ASSERT_TRUE(FileExists(path));
}

TEST(TestingTest, TemporaryTestFile) {
  auto path = TemporaryTestFile(".dat");
  ASSERT_EQ(WriteStringToFile(path, "test").code(), OK);
  ASSERT_EQ(ReadFileToString(path).value(), "test");
}

TEST(TestingTest, VerifyAgainstBaseline) {
  auto status_or_diff = VerifyAgainstBaseline(
      absl::GetFlag(FLAGS_baseline_path), "Dies ist ein Test.");
  ASSERT_TRUE(status_or_diff.ok()) << status_or_diff.status();
  if (!status_or_diff.value().empty()) {
    FAIL() << status_or_diff.value();
  }
}

TEST(TestingTest, VerifyAgainstBaselineFailure) {
  auto status_or_diff = VerifyAgainstBaseline(
      absl::GetFlag(FLAGS_baseline_path), "Dies ist kein Test.");
  ASSERT_TRUE(status_or_diff.ok()) << status_or_diff.status();
  // The actual output of the diff is much dependent on which mode we run
  // in and on which platform. Hence only test whether *some* thing is reported.
  ASSERT_FALSE(status_or_diff.value().empty());
}

TEST(TestingTest, EqualsProtoMessage) {
  testing::Foo foo1;
  foo1.set_foo("foo");
  testing::Foo foo2;
  foo2.set_foo("foo");
  ASSERT_THAT(foo1, EqualsProto(foo2));
}

TEST(TestingTest, NotEqualsProtoMessage) {
  testing::Foo foo1;
  foo1.set_foo("foo-1");
  testing::Foo foo2;
  foo2.set_foo("foo-2");
  ASSERT_THAT(foo1, Not(EqualsProto(foo2)));
}

TEST(TestingTest, NotEqualsProtoMessageType) {
  testing::Foo foo;
  foo.set_foo("foo");
  testing::Bar bar;
  bar.set_bar(1);
  ASSERT_THAT(foo, Not(EqualsProto(bar)));
}

TEST(TestingTest, EqualsProtoMessageText) {
  testing::Bar bar;
  bar.set_bar(1);
  ASSERT_THAT(bar, EqualsProto("bar: 1"));
}

TEST(TestingTest, NotEqualsProtoMessageText) {
  testing::Bar bar;
  bar.set_bar(1);
  ASSERT_THAT(bar, Not(EqualsProto("bar: 2")));
}

}  // namespace

}  // namespace fcp
