/*
 * Copyright 2022 Google LLC
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
#ifndef FCP_CLIENT_SECAGG_RUNNER_H_
#define FCP_CLIENT_SECAGG_RUNNER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "fcp/client/federated_protocol.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/secagg_event_publisher.h"
#include "fcp/secagg/client/secagg_client.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace client {

// Base SecAggSendToServer class which provides message size and network
// bandwidth usage metrics. When the child class inherit from this class, it's
// up to the child class to record metrics correctly.
class SecAggSendToServerBase : public secagg::SendToServerInterface {
 public:
  size_t last_sent_message_size() const { return last_sent_message_size_; }

 protected:
  size_t last_sent_message_size_ = 0;
};

// A delegate class which handles server to client communication protocol
// specific details (HTTP vs gRPC etc).
class SecAggProtocolDelegate {
 public:
  virtual ~SecAggProtocolDelegate() = default;
  // Retrieve the modulus for a given SecAgg vector.
  virtual absl::StatusOr<uint64_t> GetModulus(const std::string& key) = 0;
  // Receive Server message.
  virtual absl::StatusOr<secagg::ServerToClientWrapperMessage>
  ReceiveServerMessage() = 0;
  // Called when the SecAgg protocol is interrupted.
  virtual void Abort() = 0;
  virtual size_t last_received_message_size() = 0;
};

// A helper class which runs the secure aggregation protocol.
class SecAggRunner {
 public:
  virtual ~SecAggRunner() = default;
  virtual absl::Status Run(ComputationResults results) = 0;
};

// Implementation of SecAggRunner.
class SecAggRunnerImpl : public SecAggRunner {
 public:
  SecAggRunnerImpl(std::unique_ptr<SecAggSendToServerBase> send_to_server_impl,
                   std::unique_ptr<SecAggProtocolDelegate> protocol_delegate,
                   SecAggEventPublisher* secagg_event_publisher,
                   LogManager* log_manager,
                   InterruptibleRunner* interruptible_runner,
                   int64_t expected_number_of_clients,
                   int64_t minimum_surviving_clients_for_reconstruction);
  // Run the secure aggregation protocol.
  // SecAggProtocolDelegate and SecAggSendToServerBase will only be invoked from
  // a single thread.
  absl::Status Run(ComputationResults results) override;

 private:
  void AbortInternal();

  std::unique_ptr<SecAggSendToServerBase> send_to_server_impl_;
  std::unique_ptr<SecAggProtocolDelegate> protocol_delegate_;
  std::unique_ptr<secagg::SecAggClient> secagg_client_;
  SecAggEventPublisher& secagg_event_publisher_;
  LogManager& log_manager_;
  InterruptibleRunner& interruptible_runner_;
  const int64_t expected_number_of_clients_;
  const int64_t minimum_surviving_clients_for_reconstruction_;
};

// A factory interface for SecAggRunner.
class SecAggRunnerFactory {
 public:
  virtual ~SecAggRunnerFactory() = default;
  virtual std::unique_ptr<SecAggRunner> CreateSecAggRunner(
      std::unique_ptr<SecAggSendToServerBase> send_to_server_impl,
      std::unique_ptr<SecAggProtocolDelegate> protocol_delegate,
      SecAggEventPublisher* secagg_event_publisher, LogManager* log_manager,
      InterruptibleRunner* interruptible_runner,
      int64_t expected_number_of_clients,
      int64_t minimum_surviving_clients_for_reconstruction) = 0;
};

class SecAggRunnerFactoryImpl : public SecAggRunnerFactory {
  std::unique_ptr<SecAggRunner> CreateSecAggRunner(
      std::unique_ptr<SecAggSendToServerBase> send_to_server_impl,
      std::unique_ptr<SecAggProtocolDelegate> protocol_delegate,
      SecAggEventPublisher* secagg_event_publisher, LogManager* log_manager,
      InterruptibleRunner* interruptible_runner,
      int64_t expected_number_of_clients,
      int64_t minimum_surviving_clients_for_reconstruction) override;
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_SECAGG_RUNNER_H_
