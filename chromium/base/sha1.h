// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SHA1_H_
#define BASE_SHA1_H_

#include <string>

#include "base/base_export.h"

#include "base/basictypes.h"

namespace base {

// Implementation of SHA-1. Only handles data in byte-sized blocks,
// which simplifies the code a fair bit.

// Identifier names follow notation in FIPS PUB 180-3, where you'll
// also find a description of the algorithm:
// http://csrc.nist.gov/publications/fips/fips180-3/fips180-3_final.pdf

// Usage example:
//
// SecureHashAlgorithm sha;
// while(there is data to hash)
//   sha.Update(moredata, size of data);
// sha.Final();
// memcpy(somewhere, sha.Digest(), 20);
//
// to reuse the instance of sha, call sha.Init();

// TODO(jhawkins): Replace this implementation with a per-platform
// implementation using each platform's crypto library.  See
// http://crbug.com/47218

class SecureHashAlgorithm {
 public:
  BASE_EXPORT SecureHashAlgorithm();

  static const int kDigestSizeBytes;

  BASE_EXPORT void Init();
  BASE_EXPORT void Update(const void* data, size_t nbytes);
  BASE_EXPORT void Final();

  // 20 bytes of message digest.
  const unsigned char* Digest() const {
    return reinterpret_cast<const unsigned char*>(H);
  }

 private:
  void Pad();
  void Process();

  uint32 A, B, C, D, E;

  uint32 H[5];

  union {
    uint32 W[80];
    uint8 M[64];
  };

  uint32 cursor;
  uint64 l;
};

// These functions perform SHA-1 operations.

static const size_t kSHA1Length = 20;  // Length in bytes of a SHA-1 hash.

// Computes the SHA-1 hash of the input string |str| and returns the full
// hash.
BASE_EXPORT std::string SHA1HashString(const std::string& str);

// Computes the SHA-1 hash of the |len| bytes in |data| and puts the hash
// in |hash|. |hash| must be kSHA1Length bytes long.
BASE_EXPORT void SHA1HashBytes(const unsigned char* data, size_t len,
                               unsigned char* hash);

}  // namespace base

#endif  // BASE_SHA1_H_
