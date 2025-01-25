// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_INVALIDATION_INVALIDATION_SERVICE_STOMP_CLIENT_H_
#define SYNC_INVALIDATION_INVALIDATION_SERVICE_STOMP_CLIENT_H_

#include <string>

#include "base/containers/queue.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace content {
class BrowserContext;
}

namespace vivaldi {
namespace stomp {
class StompFrameBuilder;
}

class InvalidationServiceStompClient: public network::mojom::SocketObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate();
    virtual std::string GetLogin() const = 0;
    virtual std::string GetVHost() const = 0;
    virtual std::string GetChannel() const = 0;
    virtual void OnConnected() = 0;
    virtual void OnClosed() = 0;
    virtual void OnInvalidation(std::string) = 0;
  };

  InvalidationServiceStompClient(
      network::mojom::NetworkContext* network_context,
      const GURL& url,
      Delegate* delegate);
  ~InvalidationServiceStompClient() override;
  InvalidationServiceStompClient(const InvalidationServiceStompClient&) =
      delete;
  InvalidationServiceStompClient& operator=(
      const InvalidationServiceStompClient&) = delete;

  std::string session_id() { return session_id_; }

  void OnConnectionEstablished(
      int result,
      const std::optional<net::IPEndPoint>& local_addr,
      const std::optional<net::IPEndPoint>& peer_addr,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream);

  // Implementing network::mojom::SocketObserver
  void OnReadError(int32_t net_error) override;
  void OnWriteError(int32_t net_error) override;

 private:
  enum class StompState { kConnecting, kSubscribing, kConnected };

  void Send(std::string message);
  void SendRaw(std::string message);
  void ProcessOutgoing();
  void ProcessIncoming();

  void OnReadable(MojoResult result, const mojo::HandleSignalsState& state);
  void OnWritable(MojoResult result, const mojo::HandleSignalsState& state);

  void OnClose();

  bool HandleFrame(std::unique_ptr<stomp::StompFrameBuilder> frame);

  const raw_ptr<Delegate> delegate_;

  mojo::Remote<network::mojom::ProxyResolvingSocketFactory> socket_factory_;
  mojo::Remote<network::mojom::ProxyResolvingSocket> socket_;
  mojo::Receiver<network::mojom::SocketObserver> socket_observer_receiver_{this};

  mojo::ScopedDataPipeConsumerHandle readable_;
  mojo::SimpleWatcher readable_watcher_;
  mojo::ScopedDataPipeProducerHandle writable_;
  mojo::SimpleWatcher writable_watcher_;

  std::unique_ptr<stomp::StompFrameBuilder> incoming_frames_;

  StompState stomp_state_ = StompState::kConnecting;

  bool is_writable_ready_ = true;
  base::queue<std::string> outgoing_messages_;
  size_t remaining_outgoing_size_ = 0;

  std::string session_id_;
  base::OneShotTimer heart_beats_in_timer_;
  base::RepeatingTimer heart_beats_out_timer_;
};
}  // namespace vivaldi

#endif  // SYNC_INVALIDATION_INVALIDATION_SERVICE_STOMP_WEBSOCKET_H_
