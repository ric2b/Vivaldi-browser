// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_REQUEST_HANDLER_H_
#define VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_REQUEST_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromium/net/http/http_request_headers.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

class Profile;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}

namespace vivaldi {

class VivaldiAccountManagerRequestHandler {
 public:
  using RequestDoneCallback =
      base::RepeatingCallback<void(std::unique_ptr<network::SimpleURLLoader>,
                                   std::unique_ptr<std::string>)>;
  VivaldiAccountManagerRequestHandler(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& request_url,
      const std::string& body,
      const net::HttpRequestHeaders& headers,
      RequestDoneCallback callback);
  ~VivaldiAccountManagerRequestHandler();
  VivaldiAccountManagerRequestHandler(
      const VivaldiAccountManagerRequestHandler&) = delete;
  VivaldiAccountManagerRequestHandler& operator=(
      const VivaldiAccountManagerRequestHandler&) = delete;

  void Retry();

  base::Time request_start_time() const { return request_start_time_; }
  bool done() const { return done_; }
  base::Time GetNextRequestTime() const;

 private:
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  void HandleRequest();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  GURL request_url_;
  net::HttpRequestHeaders headers_;
  std::string body_;
  RequestDoneCallback callback_;
  base::Time request_start_time_;
  bool done_ = false;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  net::BackoffEntry request_backoff_;
  base::OneShotTimer request_backoff_timer_;
};

}  // namespace vivaldi

#endif  // VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_REQUEST_HANDLER_H_
