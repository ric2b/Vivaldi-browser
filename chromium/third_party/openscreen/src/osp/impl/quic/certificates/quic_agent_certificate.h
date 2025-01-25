// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_CERTIFICATES_QUIC_AGENT_CERTIFICATE_H_
#define OSP_IMPL_QUIC_CERTIFICATES_QUIC_AGENT_CERTIFICATE_H_

#include <memory>
#include <string>
#include <vector>

#include "openssl/base.h"
#include "osp/public/agent_certificate.h"
#include "quiche/quic/core/crypto/proof_source_x509.h"

namespace openscreen::osp {

class QuicAgentCertificate final : public AgentCertificate {
 public:
  QuicAgentCertificate();
  ~QuicAgentCertificate() override;
  QuicAgentCertificate(const QuicAgentCertificate&) = delete;
  QuicAgentCertificate& operator=(const QuicAgentCertificate&) = delete;
  QuicAgentCertificate(QuicAgentCertificate&&) noexcept = delete;
  QuicAgentCertificate& operator=(QuicAgentCertificate&&) noexcept = delete;

  // AgentCertificate overrides.
  bool LoadAgentCertificate(std::string_view filename) override;
  bool LoadPrivateKey(std::string_view filename) override;
  bool RotateAgentCertificate() override;
  AgentFingerprint GetAgentFingerprint() override;

  // Create a ProofSource using currently active agent certificate and private
  // key.
  std::unique_ptr<quic::ProofSource> CreateProofSource();

  void ResetCredentials();

 private:
  // Generate private key and agent certificate to the specified file for
  // use.
  // Returns true if generates credentials succeed, false otherwise.
  bool GenerateCredentialsToFile();

  // Load private key and agent certificate from the specified files for
  // use.
  // Returns true if loads credentials succeed, false otherwise.
  bool LoadCredentials();

  AgentFingerprint agent_fingerprint_;
  std::vector<std::string> certificates_;
  bssl::UniquePtr<EVP_PKEY> key_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_CERTIFICATES_QUIC_AGENT_CERTIFICATE_H_
