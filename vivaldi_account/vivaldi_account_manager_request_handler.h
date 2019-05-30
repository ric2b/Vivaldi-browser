// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_REQUEST_HANDLER_H_
#define VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_REQUEST_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromium/net/http/http_request_headers.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

class Profile;

namespace network {
class SimpleURLLoader;
}

namespace vivaldi {

class VivaldiAccountManagerRequestHandler {
 public:
  using RequestDoneCallback =
      base::RepeatingCallback<void(std::unique_ptr<network::SimpleURLLoader>,
                                   std::unique_ptr<std::string>)>;
  VivaldiAccountManagerRequestHandler(Profile* profile,
                                      const GURL& request_url,
                                      const std::string& body,
                                      const net::HttpRequestHeaders& headers,
                                      RequestDoneCallback callback);
  ~VivaldiAccountManagerRequestHandler();

  void Retry();

  base::Time request_start_time() { return request_start_time_; }
  bool done() { return done_; }
  base::Time GetNextRequestTime();

 private:
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  void HandleRequest();

  Profile* profile_;
  GURL request_url_;
  net::HttpRequestHeaders headers_;
  std::string body_;
  RequestDoneCallback callback_;
  base::Time request_start_time_;
  bool done_ = false;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  net::BackoffEntry request_backoff_;
  base::OneShotTimer request_backoff_timer_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAccountManagerRequestHandler);
};

}  // namespace vivaldi

#endif  // VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_REQUEST_HANDLER_H_
