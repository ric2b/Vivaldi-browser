/*
 * Copyright 2020 Google LLC
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

#include "fcp/secagg/server/ssl_bit_gen.h"

#include <type_traits>

// Thread-safety guarantees only apply in BoringSSL.
#include "fcp/base/monitoring.h"
#include "openssl/is_boringssl.h"
#include "openssl/rand.h"

namespace fcp {
namespace secagg {

SslBitGen::result_type SslBitGen::operator()() {
  static_assert(std::is_same<uint8_t, unsigned char>::value,
                "uint8_t being other than unsigned char isn't supported by "
                "BoringSSL");
  SslBitGen::result_type random_integer;
  int success = RAND_bytes(reinterpret_cast<unsigned char*>(&random_integer),
                           sizeof(random_integer));
  // RAND_bytes always returns 1 in BoringSSL
  FCP_CHECK(success == 1);

  return random_integer;
}

}  // namespace secagg
}  // namespace fcp
