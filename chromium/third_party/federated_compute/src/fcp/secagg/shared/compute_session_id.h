/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_SECAGG_SHARED_COMPUTE_SESSION_ID_H_
#define FCP_SECAGG_SHARED_COMPUTE_SESSION_ID_H_

#include <string>

#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace secagg {

inline constexpr int kSha256Length = 32;

// A SessionId is the id of a given SecAgg session. Every session's SessionId
// should be unique.
typedef struct SessionId {
  std::string data;
} SessionId;

// Computes the session ID for a specific protocol session given the first
// message (ShareKeysRequest) sent by the server.
//
// The session id is computed as a SHA-256 hash of the concatenation of all the
// PairOfPublicKeys inside the request message (in the same order in which they
// appear in the message). More specifically, for each PairOfPublicKeys inside
// the message, the following are concatenated to the input of the hash
// function:
//
// - The length of the prng ECDH public key
// - The prng ECDH public key
// - The length of the encryption ECDH public key
// - The encryption ECDH public key
//
// Lengths are prepended to the keys so that the encoding is not ambiguous and
// there are no unexpected collisions.
//
// The output of this method is 32 bytes long.
SessionId ComputeSessionId(const ShareKeysRequest& request);

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_COMPUTE_SESSION_ID_H_
