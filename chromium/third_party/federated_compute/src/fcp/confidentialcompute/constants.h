/*
 * Copyright 2025 Google LLC
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

#ifndef FCP_CONFIDENTIALCOMPUTE_CONSTANTS_H_
#define FCP_CONFIDENTIALCOMPUTE_CONSTANTS_H_

namespace fcp {
namespace confidential_compute {

inline constexpr char kComposedTeeUri[] = "composed_tee";
inline constexpr char kComposedTeeLeafUri[] = "composed_tee/leaf";
inline constexpr char kComposedTeeRootUri[] = "composed_tee/root";

inline constexpr char kDirectDataUploadTensorName[] = "output_tensor_name";

inline constexpr char kFileInfoKeySeparator[] = "/";

inline constexpr char kEventTimeColumnName[] =
    "confidential_compute_event_time";

inline constexpr char kPrivacyIdColumnName[] =
    "confidential_compute_privacy_id";

}  // namespace confidential_compute
}  // namespace fcp

#endif  // FCP_CONFIDENTIALCOMPUTE_CONSTANTS_H_
