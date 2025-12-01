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

#ifndef FCP_CLIENT_HTTP_CURL_CURL_HTTP_RESPONSE_H_
#define FCP_CLIENT_HTTP_CURL_CURL_HTTP_RESPONSE_H_

#include "absl/base/attributes.h"
#include "fcp/client/http/http_client.h"

namespace fcp::client::http::curl {
// A simple http response. This class is thread-safe.
class CurlHttpResponse : public HttpResponse {
 public:
  CurlHttpResponse(int status_code, HeaderList header_list);
  ~CurlHttpResponse() override = default;

  // HttpResponse:
  ABSL_MUST_USE_RESULT int code() const override;
  ABSL_MUST_USE_RESULT const HeaderList& headers() const override;

 private:
  const int status_code_;
  const HeaderList header_list_;
};
}  // namespace fcp::client::http::curl

#endif  // FCP_CLIENT_HTTP_CURL_CURL_HTTP_RESPONSE_H_
