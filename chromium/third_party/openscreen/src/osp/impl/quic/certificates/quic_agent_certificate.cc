// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/certificates/quic_agent_certificate.h"

#include <chrono>
#include <utility>

#include "openssl/bio.h"
#include "openssl/evp.h"
#include "openssl/nid.h"
#include "quiche/quic/core/quic_utils.h"
#include "util/base64.h"
#include "util/crypto/certificate_utils.h"
#include "util/crypto/pem_helpers.h"
#include "util/osp_logging.h"
#include "util/read_file.h"

namespace openscreen::osp {

namespace {

using FileUniquePtr = std::unique_ptr<FILE, decltype(&fclose)>;

constexpr char kCertificatesPath[] =
    "osp/impl/quic/certificates/agent_certificate.crt";
constexpr char kPrivateKeyPath[] = "osp/impl/quic/certificates/private_key.key";
constexpr int kOneYearInSeconds = 365 * 24 * 60 * 60;
constexpr auto kCertificateDuration = std::chrono::seconds(kOneYearInSeconds);

bssl::UniquePtr<EVP_PKEY> GeneratePrivateKey() {
  bssl::UniquePtr<EVP_PKEY_CTX> ctx(
      EVP_PKEY_CTX_new_id(EVP_PKEY_EC, /*e=*/nullptr));
  OSP_CHECK(EVP_PKEY_keygen_init(ctx.get()));
  OSP_CHECK(
      EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(), NID_X9_62_prime256v1));
  EVP_PKEY* key = nullptr;
  OSP_CHECK(EVP_PKEY_keygen(ctx.get(), &key));
  return bssl::UniquePtr<EVP_PKEY>(key);
}

// TODO(issuetracker.google.com/300236996): There are currently some spec issues
// about certificates that are still under discussion. Make all fields of the
// certificate comply with the requirements of the spec once all the issues are
// closed.
bssl::UniquePtr<X509> GenerateRootCert(const EVP_PKEY& root_key) {
  ErrorOr<bssl::UniquePtr<X509>> root_cert_or_error =
      CreateSelfSignedX509Certificate("Open Screen Certificate",
                                      kCertificateDuration, root_key,
                                      GetWallTimeSinceUnixEpoch(), true);
  OSP_CHECK(root_cert_or_error);
  return std::move(root_cert_or_error.value());
}

}  // namespace

QuicAgentCertificate::QuicAgentCertificate() {
  // Load the certificate and key files if they already exist, otherwise
  // generate new certificate and key files.
  bool result = LoadCredentials();
  if (!result) {
    result = GenerateCredentialsToFile() && LoadCredentials();
    OSP_CHECK(result);
  }
}

QuicAgentCertificate::~QuicAgentCertificate() = default;

bool QuicAgentCertificate::LoadAgentCertificate(std::string_view filename) {
  certificates_.clear();
  agent_fingerprint_.clear();

  // NOTE: There are currently some spec issues about certificates that are
  // still under discussion. Add validations to check if this is a valid OSP
  // agent certificate once all the issues are closed.
  certificates_ = ReadCertificatesFromPemFile(filename);
  if (!certificates_.empty()) {
    agent_fingerprint_ = base64::Encode(quic::RawSha256(certificates_[0]));
    return !agent_fingerprint_.empty();
  } else {
    return false;
  }
}

bool QuicAgentCertificate::LoadPrivateKey(std::string_view filename) {
  key_.reset();
  std::string file_data = ReadEntireFileToString(filename);
  if (file_data.empty()) {
    return false;
  }

  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(const_cast<char*>(file_data.data()),
                                           static_cast<int>(file_data.size())));
  if (!bio) {
    return false;
  }

  key_ = bssl::UniquePtr<EVP_PKEY>(
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
  return (key_ != nullptr);
}

bool QuicAgentCertificate::RotateAgentCertificate() {
  return GenerateCredentialsToFile() && LoadCredentials();
}

AgentFingerprint QuicAgentCertificate::GetAgentFingerprint() {
  return agent_fingerprint_;
}

std::unique_ptr<quic::ProofSource> QuicAgentCertificate::CreateProofSource() {
  if (certificates_.empty() || !key_ || agent_fingerprint_.empty()) {
    return nullptr;
  }

  auto chain = quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain>(
      new quic::ProofSource::Chain(certificates_));
  OSP_CHECK(chain) << "Failed to create the quic::ProofSource::Chain.";

  quic::CertificatePrivateKey key{std::move(key_)};
  return quic::ProofSourceX509::Create(std::move(chain), std::move(key));
}

void QuicAgentCertificate::ResetCredentials() {
  agent_fingerprint_.clear();
  certificates_.clear();
  key_.reset();
}

bool QuicAgentCertificate::GenerateCredentialsToFile() {
  bssl::UniquePtr<EVP_PKEY> root_key = GeneratePrivateKey();
  bssl::UniquePtr<X509> root_cert = GenerateRootCert(*root_key);

  FileUniquePtr f(fopen(kPrivateKeyPath, "w"), &fclose);
  if (PEM_write_PrivateKey(f.get(), root_key.get(), nullptr, nullptr, 0, 0,
                           nullptr) != 1) {
    OSP_LOG_ERROR << "Failed to write private key, check permissions?";
    return false;
  }
  OSP_LOG_INFO << "Generated new private key in file: " << kPrivateKeyPath;

  FileUniquePtr cert_file(fopen(kCertificatesPath, "w"), &fclose);
  if (PEM_write_X509(cert_file.get(), root_cert.get()) != 1) {
    OSP_LOG_ERROR << "Failed to write agent certificate, check permissions?";
    return false;
  }
  OSP_LOG_INFO << "Generated new agent certificate in file: "
               << kCertificatesPath;

  return true;
}

bool QuicAgentCertificate::LoadCredentials() {
  bool result = LoadAgentCertificate(kCertificatesPath) &&
                LoadPrivateKey(kPrivateKeyPath);
  if (!result) {
    ResetCredentials();
    return false;
  } else {
    return true;
  }
}

}  // namespace openscreen::osp
