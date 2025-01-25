// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_AGENT_CERTIFICATE_H_
#define OSP_PUBLIC_AGENT_CERTIFICATE_H_

#include <string>

namespace openscreen::osp {

typedef std::string AgentFingerprint;

// This class centrally deal with agent certificate.
class AgentCertificate {
 public:
  AgentCertificate();
  AgentCertificate(const AgentCertificate&) = delete;
  AgentCertificate& operator=(const AgentCertificate&) = delete;
  AgentCertificate(AgentCertificate&&) noexcept = delete;
  AgentCertificate& operator=(AgentCertificate&&) noexcept = delete;
  virtual ~AgentCertificate();

  // Load an agent certificate from the file identified by `filename`.
  // Returns true when loads the agent certificate successfully, false
  // otherwise.
  virtual bool LoadAgentCertificate(std::string_view filename) = 0;

  // Load a private key from the file identified by `filename`.
  // Returns true when loads the private key successfully, false otherwise.
  virtual bool LoadPrivateKey(std::string_view filename) = 0;

  // Generate a new agent certificate to overwrite old one and use the new agent
  // certificate thereafter.
  // Returns true when generates new agent certificate successfully, false
  // otherwise.
  virtual bool RotateAgentCertificate() = 0;

  // Returns the fingerprint of the currently active agent certificate. The
  // fingerprint is sent to the client as a DNS TXT record. Client uses the
  // fingerprint to verify agent certificate.
  virtual AgentFingerprint GetAgentFingerprint() = 0;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_AGENT_CERTIFICATE_H_
