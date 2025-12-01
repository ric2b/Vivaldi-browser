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

#include "fcp/client/http/curl/curl_http_response.h"

#include <utility>

#include "fcp/client/http/http_client.h"

namespace fcp::client::http::curl {
CurlHttpResponse::CurlHttpResponse(int status_code, HeaderList header_list)
    : status_code_(status_code), header_list_(std::move(header_list)) {}

int CurlHttpResponse::code() const { return status_code_; }

const HeaderList& CurlHttpResponse::headers() const { return header_list_; }

}  // namespace fcp::client::http::curl
