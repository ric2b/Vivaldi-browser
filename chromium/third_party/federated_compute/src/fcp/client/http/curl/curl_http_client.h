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

#ifndef FCP_CLIENT_HTTP_CURL_CURL_HTTP_CLIENT_H_
#define FCP_CLIENT_HTTP_CURL_CURL_HTTP_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "fcp/client/http/curl/curl_api.h"
#include "fcp/client/http/http_client.h"

namespace fcp::client::http::curl {

// A curl-based implementation of the HttpClient interface that uses
// CurlHttpRequestHandle underneath. The implementation assumes that
// CurlHttpClient lives longer than CurlHttpRequestHandle; and
// CurlApi lives longer than CurlHttpClient
class CurlHttpClient : public HttpClient {
 public:
  explicit CurlHttpClient(CurlApi* curl_api, std::string test_cert_path = "");
  ~CurlHttpClient() override = default;
  CurlHttpClient(const CurlHttpClient&) = delete;
  CurlHttpClient& operator=(const CurlHttpClient&) = delete;

  // HttpClient overrides:
  std::unique_ptr<HttpRequestHandle> EnqueueRequest(
      std::unique_ptr<HttpRequest> request) override;

  // Performs the given requests while blocked. Results will be returned to each
  // corresponding `HttpRequestCallback`.
  absl::Status PerformRequests(
      std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>> requests)
      override;

 private:
  // Owned by the caller
  const CurlApi* const curl_api_;
  const std::string test_cert_path_;
};

}  // namespace fcp::client::http::curl

#endif  // FCP_CLIENT_HTTP_CURL_CURL_HTTP_CLIENT_H_
