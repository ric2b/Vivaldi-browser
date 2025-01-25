// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_BORINGSSL_PARSED_CERTIFICATE_H_
#define CAST_COMMON_CERTIFICATE_BORINGSSL_PARSED_CERTIFICATE_H_

#include <openssl/x509.h>

#include <string>
#include <vector>

#include "cast/common/public/parsed_certificate.h"
#include "platform/base/error.h"
#include "platform/base/span.h"

namespace openscreen::cast {

class BoringSSLParsedCertificate final : public ParsedCertificate {
 public:
  BoringSSLParsedCertificate();

  // Takes ownership of `cert`.
  explicit BoringSSLParsedCertificate(bssl::UniquePtr<X509> cert);

  ~BoringSSLParsedCertificate() override;

  ErrorOr<std::vector<uint8_t>> SerializeToDER(
      int front_spacing) const override;

  ErrorOr<DateTime> GetNotBeforeTime() const override;
  ErrorOr<DateTime> GetNotAfterTime() const override;

  std::string GetCommonName() const override;

  std::string GetSpkiTlv() const override;

  ErrorOr<uint64_t> GetSerialNumber() const override;

  bool VerifySignedData(DigestAlgorithm algorithm,
                        const ByteView& data,
                        const ByteView& signature) const override;

  bool HasPolicyOid(const ByteView& oid) const override;

  void SetNotBeforeTimeForTesting(time_t not_before) override;
  void SetNotAfterTimeForTesting(time_t not_after) override;

  X509* get() { return cert_.get(); }

 private:
  bssl::UniquePtr<X509> cert_;
};

}  // namespace openscreen::cast

#endif  // CAST_COMMON_CERTIFICATE_BORINGSSL_PARSED_CERTIFICATE_H_
