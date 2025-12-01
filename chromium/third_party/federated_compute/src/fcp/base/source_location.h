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
#ifndef FCP_BASE_SOURCE_LOCATION_H_
#define FCP_BASE_SOURCE_LOCATION_H_

namespace fcp {

#if (__clang_major__ >= 9) || (__GNUC__ >= 7)
// Using non-standard builtin extensions of gcc and clang to capture call-site
// location
#define FCP_HAS_SOURCE_LOCATION
#define FCP_BUILTIN_LINE() __builtin_LINE()
#define FCP_BUILTIN_FILE() __builtin_FILE()
#else
// If compiler feature unavailable replace with stub values
#define FCP_BUILTIN_LINE() (0)
#define FCP_BUILTIN_FILE() ("<unknown_source>")

#endif

class SourceLocationImpl {
 public:
  static constexpr SourceLocationImpl current(
      // Builtins _must_ be referenced from default arguments, so they get
      // evaluated at the callsite.
      int line = FCP_BUILTIN_LINE(),
      const char* file_name = FCP_BUILTIN_FILE()) {
    return SourceLocationImpl(line, file_name);
  }
  constexpr int line() const { return line_; }
  constexpr const char* file_name() const { return file_name_; }

 private:
  constexpr SourceLocationImpl(int line, const char* file_name)
      : line_(line), file_name_(file_name) {}
  int line_;
  const char* file_name_;
};

using SourceLocation = SourceLocationImpl;

}  // namespace fcp

#endif  // FCP_BASE_SOURCE_LOCATION_H_
