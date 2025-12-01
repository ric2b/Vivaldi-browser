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

#ifndef FCP_TESTING_TESTING_H_
#define FCP_TESTING_TESTING_H_

#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

#include "google/protobuf/util/message_differencer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/platform.h"

#include "fcp/testing/parse_text_proto.h"

// This file defines platform dependent utilities for testing,
// based on the public version of googletest.

namespace fcp {

// Convenience macros for `EXPECT_THAT(s, IsOk())`, where `s` is either
// a `Status` or a `StatusOr<T>`.
// Old versions of the protobuf library define EXPECT_OK as well, so we only
// conditionally define our version.
#if !defined(EXPECT_OK)
#define EXPECT_OK(result) EXPECT_THAT(result, fcp::IsOk())
#endif
#define ASSERT_OK(result) ASSERT_THAT(result, fcp::IsOk())

/** Returns the current test's name. */
std::string TestName();

/**
 * Gets path to a test data file based on a path relative to project root.
 */
std::string GetTestDataPath(absl::string_view relative_path);

/**
 * Creates a temporary file name with given suffix unique for the running test.
 */
std::string TemporaryTestFile(absl::string_view suffix);

/**
 * Verifies a provided content against an expected stored in a baseline file.
 * Returns an empty string if both are identical, otherwise a diagnostic
 * message for error reports.
 *
 * A return status of not ok indicates an operational error which made the
 * comparison impossible.
 *
 * The baseline file name must be provided relative to the project root.
 */
StatusOr<std::string> VerifyAgainstBaseline(absl::string_view baseline_file,
                                            absl::string_view content);

/**
 * Polymorphic matchers for Status or StatusOr on status code.
 */
template <typename T>
bool IsCode(StatusOr<T> const& x, StatusCode code) {
  return x.status().code() == code;
}
inline bool IsCode(Status const& x, StatusCode code) {
  return x.code() == code;
}

template <typename T>
class StatusMatcherImpl : public ::testing::MatcherInterface<T> {
 public:
  explicit StatusMatcherImpl(StatusCode code) : code_(code) {}
  void DescribeTo(::std::ostream* os) const override {
    *os << "is " << absl::StatusCodeToString(code_);
  }
  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "is not " << absl::StatusCodeToString(code_);
  }
  bool MatchAndExplain(
      T x, ::testing::MatchResultListener* listener) const override {
    return IsCode(x, code_);
  }

 private:
  StatusCode code_;
};

class StatusMatcher {
 public:
  explicit StatusMatcher(StatusCode code) : code_(code) {}

  template <typename T>
  operator testing::Matcher<T>() const {  // NOLINT
    return ::testing::MakeMatcher(new StatusMatcherImpl<T>(code_));
  }

 private:
  StatusCode code_;
};

StatusMatcher IsCode(StatusCode code);
StatusMatcher IsOk();

template <typename T>
class ProtoMatcherImpl : public ::testing::MatcherInterface<T> {
 public:
  explicit ProtoMatcherImpl(const google::protobuf::Message& arg)
      : arg_(CloneMessage(arg)) {}

  explicit ProtoMatcherImpl(const std::string& arg) : arg_(ParseMessage(arg)) {}

  void DescribeTo(::std::ostream* os) const override {
    *os << "is " << arg_->DebugString();
  }
  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "is not " << arg_->DebugString();
  }
  bool MatchAndExplain(
      T x, ::testing::MatchResultListener* listener) const override {
    if (x.GetDescriptor()->full_name() != arg_->GetDescriptor()->full_name()) {
      *listener << "Argument proto is of type "
                << arg_->GetDescriptor()->full_name()
                << " but expected proto of type "
                << x.GetDescriptor()->full_name();
      return false;
    }

    google::protobuf::util::MessageDifferencer differencer;
    std::string reported_differences;
    differencer.ReportDifferencesToString(&reported_differences);
    if (!differencer.Compare(*arg_, x)) {
      *listener << reported_differences;
      return false;
    }
    return true;
  }

 private:
  static std::unique_ptr<google::protobuf::Message> CloneMessage(
      const google::protobuf::Message& message) {
    std::unique_ptr<google::protobuf::Message> copy_of_message =
        absl::WrapUnique(message.New());
    copy_of_message->CopyFrom(message);
    return copy_of_message;
  }

  static std::unique_ptr<google::protobuf::Message> ParseMessage(
      const std::string& proto_text) {
    using V = std::remove_cv_t<std::remove_reference_t<T>>;
    std::unique_ptr<V> message = std::make_unique<V>();
    *message = PARSE_TEXT_PROTO(proto_text);
    return message;
  }

  std::unique_ptr<google::protobuf::Message> arg_;
};

template <typename T>
class ProtoMatcher {
 public:
  explicit ProtoMatcher(const T& arg) : arg_(arg) {}

  template <typename U>
  operator testing::Matcher<U>() const {  // NOLINT
    using V = std::remove_cv_t<std::remove_reference_t<U>>;
    static_assert(std::is_base_of<google::protobuf::Message, V>::value &&
                  !std::is_same<google::protobuf::Message, V>::value);
    return ::testing::MakeMatcher(new ProtoMatcherImpl<U>(arg_));
  }

 private:
  T arg_;
};

// Proto matcher that takes another proto message reference as an argument.
template <class T,
          typename std::enable_if<std::is_base_of<google::protobuf::Message, T>::value &&
                                      !std::is_same<google::protobuf::Message, T>::value,
                                  int>::type = 0>
inline ProtoMatcher<T> EqualsProto(const T& arg) {
  return ProtoMatcher<T>(arg);
}

// Proto matcher that takes a text proto as an argument.
inline ProtoMatcher<std::string> EqualsProto(const std::string& arg) {
  return ProtoMatcher<std::string>(arg);
}

}  // namespace fcp

#endif  // FCP_TESTING_TESTING_H_
