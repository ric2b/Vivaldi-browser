// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_INVALIDATION_INVALIDATION_SERVICE_STOMP_WEBSOCKET_H_
#define SYNC_INVALIDATION_INVALIDATION_SERVICE_STOMP_WEBSOCKET_H_

#include <string>

#include "base/containers/queue.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"

namespace content {
class BrowserContext;
}

namespace vivaldi {

class InvalidationServiceStompWebsocket
    : public network::mojom::WebSocketHandshakeClient,
      network::mojom::WebSocketClient {
 public:
  class Client {
   public:
    virtual ~Client();
    virtual std::string GetLogin() = 0;
    virtual std::string GetVHost() = 0;
    virtual std::string GetChannel() = 0;
    virtual void OnConnected() = 0;
    virtual void OnClosed() = 0;
    virtual void OnInvalidation(base::Value::Dict) = 0;
  };

  InvalidationServiceStompWebsocket(
      network::mojom::NetworkContext* network_context,
      const GURL& url,
      Client* client);
  ~InvalidationServiceStompWebsocket() override;
  InvalidationServiceStompWebsocket(const InvalidationServiceStompWebsocket&) =
      delete;
  InvalidationServiceStompWebsocket& operator=(
      const InvalidationServiceStompWebsocket&) = delete;

  // Implementing WebSocketHandshakeClient
  void OnOpeningHandshakeStarted(
      network::mojom::WebSocketHandshakeRequestPtr request) override;
  void OnFailure(const std::string& message,
                 int net_error,
                 int response_code) override;
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::WebSocket> socket,
      mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
      network::mojom::WebSocketHandshakeResponsePtr response,
      mojo::ScopedDataPipeConsumerHandle readable,
      mojo::ScopedDataPipeProducerHandle writable) override;

  // Implementing WebSocketClient
  void OnDataFrame(bool finish,
                   network::mojom::WebSocketMessageType type,
                   uint64_t data_len) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override;
  void OnClosingHandshake() override;

 private:
  enum class StompState { kConnecting = 0, kSubscribing, kConnected };

  void ProcessIncoming();
  void Send(std::string message);
  void ProcessOutgoing();

  void OnMojoPipeDisconnect();
  void OnReadable(MojoResult result, const mojo::HandleSignalsState& state);
  void OnWritable(MojoResult result, const mojo::HandleSignalsState& state);

  void OnClose();

  bool HandleFrame();

  GURL url_;
  const raw_ptr<Client> client_;

  mojo::Receiver<network::mojom::WebSocketHandshakeClient> handshake_receiver_{
      this};
  mojo::Receiver<network::mojom::WebSocketClient> client_receiver_{this};

  mojo::Remote<network::mojom::WebSocket> websocket_;
  mojo::ScopedDataPipeConsumerHandle readable_;
  mojo::SimpleWatcher readable_watcher_;
  mojo::ScopedDataPipeProducerHandle writable_;
  mojo::SimpleWatcher writable_watcher_;

  base::queue<size_t> incoming_sizes_;
  std::string incoming_message_;
  bool incoming_ = false;

  bool is_writable_ready_ = true;
  base::queue<std::string> outgoing_messages_;
  size_t remaining_outgoing_size_ = 0;

  base::OneShotTimer heart_beats_in_timer_;
  base::RepeatingTimer heart_beats_out_timer_;

  StompState stomp_state_ = StompState::kConnecting;
};
}  // namespace vivaldi

#endif  // SYNC_INVALIDATION_INVALIDATION_SERVICE_STOMP_WEBSOCKET_H_
