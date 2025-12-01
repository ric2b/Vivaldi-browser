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

#ifndef FCP_BASE_ERROR_H_
#define FCP_BASE_ERROR_H_

namespace fcp {

// An Error indicates that something went wrong. Errors carry data, but are
// opaque to most code; error data is intended to support diagnostics, but not
// program behavior.
//
// Code should be written such that values are turned into opaque Errors only
// after they are judged to be problematic (something went wrong). If one finds
// the need to inspect the "reason" for an Error, some earlier (non-error) value
// should be used instead.
//
// Errors are typically returned inside of a Result.
class Error {
  // TODO(team): Make it possible to get a stack trace from a tracing span
  // and save this in the error.
 public:
  class ConstructorAccess {
    template <class FlatBufferTable, class... Arg>
    friend Error TraceError(Arg&&... args);

   private:
    constexpr ConstructorAccess() = default;
    constexpr ConstructorAccess(const ConstructorAccess&) = default;
  };

  // Error is copyable but not trivially constructible.
  // Use global TraceError() function to construct an Error.
  explicit constexpr Error(ConstructorAccess) {}
  Error(const Error&) = default;
};

}  // namespace fcp

#endif  // FCP_BASE_ERROR_H_
