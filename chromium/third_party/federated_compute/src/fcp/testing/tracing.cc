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

#include "fcp/testing/tracing.h"

#include "fcp/base/error.h"
#include "fcp/base/source_location.h"
#include "fcp/testing/tracing_schema.h"  // IWYU pragma: keep
#include "fcp/tracing/tracing_span.h"

namespace fcp {

Error TraceTestError(SourceLocation loc) {
  return TraceError<TestError>(loc.file_name(), loc.line());
}

}  // namespace fcp
