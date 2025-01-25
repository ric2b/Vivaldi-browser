// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_
#define OSP_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "osp/public/presentation/presentation_common.h"
#include "osp/public/presentation/presentation_connection.h"
#include "osp/public/protocol_connection.h"
#include "osp/public/service_listener.h"
#include "platform/api/time.h"
#include "platform/base/error.h"

namespace openscreen::osp {

class UrlAvailabilityRequester;

class RequestDelegate {
 public:
  RequestDelegate();
  RequestDelegate(const RequestDelegate&) = delete;
  RequestDelegate& operator=(const RequestDelegate&) = delete;
  RequestDelegate(RequestDelegate&&) noexcept = delete;
  RequestDelegate& operator=(RequestDelegate&&) noexcept = delete;
  virtual ~RequestDelegate();

  virtual void OnConnection(std::unique_ptr<Connection> connection) = 0;
  virtual void OnError(const Error& error) = 0;
};

class ReceiverObserver {
 public:
  ReceiverObserver();
  ReceiverObserver(const ReceiverObserver&) = delete;
  ReceiverObserver& operator=(const ReceiverObserver&) = delete;
  ReceiverObserver(ReceiverObserver&&) noexcept = delete;
  ReceiverObserver& operator=(ReceiverObserver&&) noexcept = delete;
  virtual ~ReceiverObserver();

  // Called when there is an unrecoverable error in requesting availability.
  // This means the availability is unknown and there is no further response to
  // wait for.
  virtual void OnRequestFailed(const std::string& presentation_url,
                               const std::string& instance_name) = 0;

  // Called when receivers compatible with `presentation_url` are known to be
  // available.
  virtual void OnReceiverAvailable(const std::string& presentation_url,
                                   const std::string& instance_name) = 0;
  // Only called for `instance_name` values previously advertised as available.
  virtual void OnReceiverUnavailable(const std::string& presentation_url,
                                     const std::string& instance_name) = 0;
};

class Controller final : public ServiceListener::Observer,
                         public Connection::Controller {
 public:
  class ReceiverWatch {
   public:
    ReceiverWatch();
    ReceiverWatch(Controller* controller,
                  const std::vector<std::string>& urls,
                  ReceiverObserver* observer);
    ReceiverWatch(const ReceiverWatch&) = delete;
    ReceiverWatch& operator=(const ReceiverWatch&) = delete;
    ReceiverWatch(ReceiverWatch&&) noexcept;
    ReceiverWatch& operator=(ReceiverWatch&&) noexcept;
    ~ReceiverWatch();

    explicit operator bool() const { return observer_; }
    // Stop this ReceiverWatch by calling `StopWatching()` and reset its
    // members.
    void Reset();

   private:
    void StopWatching();

    std::vector<std::string> urls_;
    ReceiverObserver* observer_ = nullptr;
    Controller* controller_ = nullptr;
  };

  class ConnectRequest {
   public:
    ConnectRequest();
    ConnectRequest(Controller* controller,
                   const std::string& instance_name,
                   bool is_reconnect,
                   uint64_t request_id);
    ConnectRequest(const ConnectRequest&) = delete;
    ConnectRequest& operator=(const ConnectRequest&) = delete;
    ConnectRequest(ConnectRequest&&) noexcept;
    ConnectRequest& operator=(ConnectRequest&&) noexcept;
    ~ConnectRequest();

    explicit operator bool() const { return request_id_ > 0; }
    // Cancel this ConnectRequest by calling `CancelRequest()` and reset its
    // members.
    void Reset();

   private:
    void CancelRequest();

    std::string instance_name_;
    bool is_reconnect_ = false;
    uint64_t request_id_ = 0;
    Controller* controller_ = nullptr;
  };

  explicit Controller(ClockNowFunctionPtr now_function);
  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;
  Controller(Controller&&) noexcept = delete;
  Controller& operator=(Controller&&) noexcept = delete;
  ~Controller() override;

  // Connection::Controller overrides.
  Error CloseConnection(Connection* connection,
                        Connection::CloseReason reason) override;
  Error OnPresentationTerminated(const std::string& presentation_id,
                                 TerminationSource source,
                                 TerminationReason reason) override;
  void OnConnectionDestroyed(Connection* connection) override;

  // Requests receivers compatible with all urls in `urls` and registers
  // `observer` for availability changes.  The screens will be a subset of the
  // screen list maintained by the ServiceListener.  Returns an RAII object that
  // tracks the registration.
  ReceiverWatch RegisterReceiverWatch(const std::vector<std::string>& urls,
                                      ReceiverObserver* observer);

  // Requests that a new presentation be created on `instance_name` using
  // `presentation_url`, with the result passed to `delegate`.
  // `conn_delegate` is passed to the resulting connection.  The returned
  // ConnectRequest object may be destroyed before any `delegate` methods are
  // called to cancel the request.
  ConnectRequest StartPresentation(const std::string& url,
                                   const std::string& instance_name,
                                   RequestDelegate* delegate,
                                   Connection::Delegate* conn_delegate);

  // Requests reconnection to the presentation with the given id and URL running
  // on `instance_name`, with the result passed to `delegate`.  `conn_delegate`
  // is passed to the resulting connection.  The returned ConnectRequest object
  // may be destroyed before any `delegate` methods are called to cancel the
  // request.
  ConnectRequest ReconnectPresentation(const std::vector<std::string>& urls,
                                       const std::string& presentation_id,
                                       const std::string& instance_name,
                                       RequestDelegate* delegate,
                                       Connection::Delegate* conn_delegate);

  // Requests reconnection with a previously-connected connection.  This both
  // avoids having to respecify the parameters and connection delegate but also
  // simplifies the implementation of the Presentation API requirement to return
  // the same connection object where possible.
  ConnectRequest ReconnectConnection(std::unique_ptr<Connection> connection,
                                     RequestDelegate* delegate);

  // Returns an empty string if no such presentation ID is found.
  std::string GetServiceIdForPresentationId(
      const std::string& presentation_id) const;

  ProtocolConnection* GetConnectionRequestGroupStream(
      const std::string& instance_name);

 private:
  class TerminationListener;
  class MessageGroupStreams;

  struct ControlledPresentation {
    std::string instance_name;
    std::string url;
    std::vector<Connection*> connections;
  };

  // ServiceListener::Observer overrides.
  void OnStarted() override;
  void OnStopped() override;
  void OnSuspended() override;
  void OnSearching() override;
  void OnReceiverAdded(const ServiceInfo& info) override;
  void OnReceiverChanged(const ServiceInfo& info) override;
  void OnReceiverRemoved(const ServiceInfo& info) override;
  void OnAllReceiversRemoved() override;
  void OnError(const Error& error) override;
  void OnMetrics(ServiceListener::Metrics) override;

  static std::string MakePresentationId(const std::string& url,
                                        const std::string& instance_name);

  void AddConnection(Connection* connection);
  void OpenConnection(uint64_t connection_id,
                      uint64_t instance_id,
                      const std::string& instance_name,
                      RequestDelegate* request_delegate,
                      std::unique_ptr<Connection>&& connection,
                      std::unique_ptr<ProtocolConnection>&& stream);

  void TerminatePresentationById(const std::string& presentation_id);

  // Cancels compatible receiver monitoring for the given `urls`, `observer`
  // pair.
  void CancelReceiverWatch(const std::vector<std::string>& urls,
                           ReceiverObserver* observer);

  // Cancels a presentation connect request for the given `request_id` if one is
  // pending.
  void CancelConnectRequest(const std::string& instance_name,
                            bool is_reconnect,
                            uint64_t request_id);

  std::unique_ptr<ConnectionManager> connection_manager_;
  std::unique_ptr<UrlAvailabilityRequester> availability_requester_;

  std::map<std::string, ControlledPresentation> presentations_by_id_;
  // TODO(crbug.com/347268871): Replace instance_name as an agent identifier
  std::map<std::string, std::unique_ptr<MessageGroupStreams>>
      group_streams_by_instance_name_;
  std::map<std::string, std::unique_ptr<TerminationListener>>
      termination_listener_by_id_;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_
