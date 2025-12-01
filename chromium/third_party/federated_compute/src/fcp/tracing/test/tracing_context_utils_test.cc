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

#include "fcp/tracing/tracing_context_utils.h"

#include <iostream>
#include <optional>
#include <ostream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/tracing/test/test_api_message.pb.h"

namespace fcp {
namespace {

using fcp::tracing::test::ApiMessageWithContext;
using fcp::tracing::test::ApiMessageWithContextBytes;
using fcp::tracing::test::ApiMessageWithContextInt;
using fcp::tracing::test::ApiMessageWithoutContext;
using fcp::tracing::test::TestTracingContext;

TEST(Tracing, SetAndRetrieveContextOnMessage) {
  TestTracingContext original_context;
  original_context.set_first(222);
  original_context.set_second(333);

  ApiMessageWithContext message;
  fcp::tracing_internal::SetTracingContextOnMessage(original_context, message);

  TestTracingContext context =
      fcp::tracing_internal::GetContextFromMessage<TestTracingContext>(message);

  EXPECT_EQ(context.first(), 222);
  EXPECT_EQ(context.second(), 333);
}

TEST(Tracing, SetAndRetrieveContextBytesOnMessage) {
  TestTracingContext original_context;
  original_context.set_first(222);
  original_context.set_second(333);

  ApiMessageWithContextBytes message;
  fcp::tracing_internal::SetTracingContextOnMessage(original_context, message);

  TestTracingContext context =
      fcp::tracing_internal::GetContextFromMessage<TestTracingContext>(message);

  EXPECT_EQ(context.first(), 222);
  EXPECT_EQ(context.second(), 333);
}

TEST(Tracing, MessageWithoutContext) {
  TestTracingContext original_context;
  original_context.set_first(222);
  ApiMessageWithoutContext message;
  // Setting the context on a message without it will be a no-op.
  fcp::tracing_internal::SetTracingContextOnMessage(original_context, message);

  TestTracingContext context =
      fcp::tracing_internal::GetContextFromMessage<TestTracingContext>(message);

  EXPECT_EQ(context.first(), 0);
  EXPECT_EQ(context.second(), 0);
}

TEST(Tracing, SetTracingContextOnMessageWithIntContextCheckFailure) {
  TestTracingContext original_context;
  ApiMessageWithContextInt message;
  // Setting the context on a message with the wrong context type will be a
  // no-op.
  EXPECT_DEATH(fcp::tracing_internal::SetTracingContextOnMessage(
                   original_context, message),
               fcp::tracing_internal::kContextWrongTypeMessage);
}

TEST(Tracing, GetTracingContextFromMessageWithIntContextCheckFailure) {
  ApiMessageWithContextInt message;
  EXPECT_DEATH(
      TestTracingContext context =
          fcp::tracing_internal::GetContextFromMessage<TestTracingContext>(
              message),
      fcp::tracing_internal::kContextWrongTypeMessage);
}

}  // namespace
}  // namespace fcp
