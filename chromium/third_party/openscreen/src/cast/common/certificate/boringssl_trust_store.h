// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_BORINGSSL_TRUST_STORE_H_
#define CAST_COMMON_CERTIFICATE_BORINGSSL_TRUST_STORE_H_

#include <openssl/x509.h>

#include <cstdint>
#include <string>
#include <vector>

#include "cast/common/public/trust_store.h"
#include "platform/base/span.h"

namespace openscreen::cast {

class BoringSSLTrustStore final : public TrustStore {
 public:
  BoringSSLTrustStore();
  explicit BoringSSLTrustStore(ByteView trust_anchor_der);
  explicit BoringSSLTrustStore(std::vector<bssl::UniquePtr<X509>> certs);
  ~BoringSSLTrustStore() override;

  ErrorOr<CertificatePathResult> FindCertificatePath(
      const std::vector<std::string>& der_certs,
      const DateTime& time) override;

 private:
  std::vector<bssl::UniquePtr<X509>> certs_;
};

}  // namespace openscreen::cast

#endif  // CAST_COMMON_CERTIFICATE_BORINGSSL_TRUST_STORE_H_
