/*
 * Copyright 2024 Google LLC
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

#ifndef FCP_TESTING_TRACING_H_
#define FCP_TESTING_TRACING_H_

#include "fcp/base/error.h"
#include "fcp/base/source_location.h"

namespace fcp {

// Utility function which creates and traces an instance of test error
Error TraceTestError(SourceLocation loc = SourceLocation::current());

}  // namespace fcp

#endif  // FCP_TESTING_TRACING_H_
