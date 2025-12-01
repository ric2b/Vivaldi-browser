/*
 * Copyright 2018 Google LLC
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

#ifndef FCP_SECAGG_CLIENT_OTHER_CLIENT_STATE_H_
#define FCP_SECAGG_CLIENT_OTHER_CLIENT_STATE_H_

namespace fcp {
namespace secagg {

// Used by descendants of {@link SecAggClientState} to track the state of other
// clients, from the perspective of this client.

enum class OtherClientState {
  kAlive,
  kDeadAtRound1,
  kDeadAtRound2,
  kDeadAtRound3
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_OTHER_CLIENT_STATE_H_
