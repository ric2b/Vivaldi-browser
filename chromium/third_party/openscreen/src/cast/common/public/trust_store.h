// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_PUBLIC_TRUST_STORE_H_
#define CAST_COMMON_PUBLIC_TRUST_STORE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cast/common/public/certificate_types.h"
#include "cast/common/public/parsed_certificate.h"
#include "platform/base/error.h"
#include "platform/base/span.h"

namespace openscreen::cast {

// This class represents a set of certificates that form a root trust set.  The
// only operation on this set is to check whether a given set of certificates
// can be used to form a valid certificate chain to one of the root
// certificates.
class TrustStore {
 public:
  using CertificatePathResult = std::vector<std::unique_ptr<ParsedCertificate>>;

  static std::unique_ptr<TrustStore> CreateInstanceFromPemFile(
      std::string_view file_path);

  static std::unique_ptr<TrustStore> CreateInstanceForTest(
      ByteView trust_anchor_der);

  TrustStore() = default;
  virtual ~TrustStore() = default;

  // Checks whether a subset of the certificates in `der_certs` can form a valid
  // certificate chain to one of the root certificates in this trust store,
  // where the time at which all certificates need to be valid is `time`.
  // Returns an error if no path is found, otherwise returns the certificate
  // chain that is found.
  //
  // While more error codes could be used by a specific implementation, the
  // likely error codes are:
  // - kErrCertsMissing: `der_certs` is empty.
  // - kErrCertsParse: there was an error parsing a certificate from
  //   `der_certs`.
  // - kErrCertsDateInvalid: a certificate was not valid for the current time.
  // - kErrCertsRestrictions: a certificate restriction, such as key usage, was
  //   invalid.
  // - kErrCertsVerifyUntrustedCert: no path to a certificate in the trust store
  //   was found.
  // - kErrCertsVerifyGeneric: a generic error code for covering other
  //   miscellaneous conditions
  virtual ErrorOr<CertificatePathResult> FindCertificatePath(
      const std::vector<std::string>& der_certs,
      const DateTime& time) = 0;
};

class CastTrustStore {
 public:
  // Creates a TrustStore instance that is the root of trust for Cast device
  // certificates.
  static std::unique_ptr<TrustStore> Create();
};

class CastCRLTrustStore {
 public:
  // Creates a TrustStore instance that is the root of trust for signed CRL
  // data.
  static std::unique_ptr<TrustStore> Create();
};

}  // namespace openscreen::cast

#endif  // CAST_COMMON_PUBLIC_TRUST_STORE_H_
