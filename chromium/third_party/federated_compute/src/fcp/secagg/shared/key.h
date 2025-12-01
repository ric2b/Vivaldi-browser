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

#ifndef FCP_SECAGG_SHARED_KEY_H_
#define FCP_SECAGG_SHARED_KEY_H_

#include <cstdint>
#include <string>

namespace fcp {
namespace secagg {
// An immutable type that encapsulates a key to be used with OpenSSL. Stores the
// key as std::string, but for better interaction with the OpenSSL API, the Key
// API treats the key as either a string or a const uint8_t*.
//
// Note that this doesn't replace any OpenSSL structure, it simply allows for
// storage of keys at rest without needing to store associated OpenSSL data.
class Key {
 public:
  Key() : data_("") {}

  Key(const uint8_t* data, int size)
      : data_(reinterpret_cast<const char*>(data), size) {}

  inline const uint8_t* data() const {
    return reinterpret_cast<const uint8_t*>(data_.c_str());
  }

  inline int size() const { return data_.size(); }

  inline std::string AsString() const { return data_; }

  friend inline bool operator==(const Key& lhs, const Key& rhs) {
    return lhs.data_ == rhs.data_;
  }

 private:
  std::string data_;  // The binary key data.
};
}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_KEY_H_
