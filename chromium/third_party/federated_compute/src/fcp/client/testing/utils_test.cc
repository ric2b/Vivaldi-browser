// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fcp/client/testing/utils.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/testing/testing.h"  // IWYU pragma: keep

namespace fcp::client::testing {
namespace {

using google::internal::federated::plan::Dataset;

TEST(TestExampleIteratorTest, NextCallsForEmptyDataset) {
  std::vector<std::string> data;

  TestExampleIterator iterator(data);

  absl::StatusOr<std::string> element;

  element = iterator.Next();
  EXPECT_FALSE(element.ok());

  element = iterator.Next();
  EXPECT_FALSE(element.ok());

  element = iterator.Next();
  EXPECT_FALSE(element.ok());
}

TEST(TestExampleIteratorTest, NextCallsForSingleElementDataset) {
  std::vector<std::string> data;
  data.push_back("abc");

  TestExampleIterator iterator(data);

  absl::StatusOr<std::string> element;

  element = iterator.Next();
  ASSERT_TRUE(element.ok());
  EXPECT_EQ(element.value(), "abc");

  element = iterator.Next();
  EXPECT_FALSE(element.ok());

  element = iterator.Next();
  EXPECT_FALSE(element.ok());
}

// Tests that the Foo::Bar() method does Abc.
TEST(TestExampleIteratorTest, NextCallsForThreeElementDataset) {
  std::vector<std::string> data;
  data.push_back("a");
  data.push_back("b");
  data.push_back("c");

  TestExampleIterator iterator(data);

  absl::StatusOr<std::string> element;

  element = iterator.Next();
  ASSERT_TRUE(element.ok());
  EXPECT_EQ(element.value(), "a");

  element = iterator.Next();
  ASSERT_TRUE(element.ok());
  EXPECT_EQ(element.value(), "b");

  element = iterator.Next();
  ASSERT_TRUE(element.ok());
  EXPECT_EQ(element.value(), "c");

  element = iterator.Next();
  EXPECT_FALSE(element.ok());

  element = iterator.Next();
  EXPECT_FALSE(element.ok());
}

TEST(TestTaskEnvironmentTest, DefaultExamples) {
  Dataset data;
  auto* client_data = data.add_client_data();
  client_data->add_example("a");
  client_data->add_example("b");

  TestTaskEnvironment env(*data.client_data().data(), "base_dir");
  ExampleSelector selector;
  selector.set_collection_uri("app://my_collection");
  auto iterator = env.CreateExampleIterator(selector);
  ASSERT_OK(iterator);

  auto element = (*iterator)->Next();
  ASSERT_OK(element);
  EXPECT_EQ(*element, "a");

  element = (*iterator)->Next();
  ASSERT_OK(element);
  EXPECT_EQ(*element, "b");

  element = (*iterator)->Next();
  EXPECT_FALSE(element.ok());
}

TEST(TestTaskEnvironmentTest, SelectedExamples) {
  Dataset data;
  auto* client_data = data.add_client_data();
  ExampleSelector selector_1;
  selector_1.set_collection_uri("app://collection_1");
  auto* selected_example_1 = client_data->add_selected_example();
  selected_example_1->add_example("a");
  selected_example_1->add_example("b");
  *selected_example_1->mutable_selector() = selector_1;

  ExampleSelector selector_2;
  selector_2.set_collection_uri("app://collection_2");
  auto* selected_example_2 = client_data->add_selected_example();
  selected_example_2->add_example("c");
  *selected_example_2->mutable_selector() = selector_2;

  TestTaskEnvironment env(*data.client_data().data(), "base_dir");
  auto iterator = env.CreateExampleIterator(selector_1);
  ASSERT_OK(iterator);

  auto element = (*iterator)->Next();
  ASSERT_OK(element);
  EXPECT_EQ(*element, "a");

  element = (*iterator)->Next();
  ASSERT_OK(element);
  EXPECT_EQ(*element, "b");

  element = (*iterator)->Next();
  EXPECT_FALSE(element.ok());

  iterator = env.CreateExampleIterator(selector_2);
  element = (*iterator)->Next();
  ASSERT_OK(element);
  EXPECT_EQ(*element, "c");

  element = (*iterator)->Next();
  EXPECT_FALSE(element.ok());
}

}  // namespace
}  // namespace fcp::client::testing
