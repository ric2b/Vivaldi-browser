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

#include "fcp/client/http/curl/curl_http_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "curl/curl.h"
#include "curl/multi.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/http/curl/curl_api.h"
#include "fcp/client/http/curl/curl_http_request_handle.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/http_client_util.h"

namespace fcp::client::http::curl {
namespace {
// Cleans completed requests and calls the required callbacks.
void ReadCompleteMessages(CurlMultiHandle* multi_handle) {
  CURLMsg* msg;
  int messages_in_queue = 0;
  while ((msg = multi_handle->InfoRead(&messages_in_queue))) {
    if (msg->msg == CURLMSG_DONE) {
      FCP_LOG(INFO) << CurlEasyHandle::StrError(msg->data.result);
      void* user_data;
      curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &user_data);
      FCP_CHECK(user_data != nullptr);

      auto handle = static_cast<CurlHttpRequestHandle*>(user_data);
      handle->MarkAsCompleted();
      handle->RemoveFromMulti(multi_handle);
    }
  }
}

// Processes multiple requests while blocked.
absl::Status PerformMultiHandlesBlocked(CurlMultiHandle* multi_handle) {
  int num_running_handles = -1;
  while (num_running_handles) {
    CURLMcode code = multi_handle->Perform(&num_running_handles);
    if (code != CURLM_OK) {
      FCP_LOG(ERROR) << "MultiPerform failed with code: " << code;
      return absl::InternalError(
          absl::StrCat("MultiPerform failed with code: ", code));
    }

    ReadCompleteMessages(multi_handle);

    if (num_running_handles > 0) {
      code = multi_handle->Poll(/*extra_fds*/ nullptr,
                                /*extra_nfds*/ 0, /*timeout_ms*/ 1000,
                                /*numfds*/ nullptr);
    }
  }

  return absl::OkStatus();
}
}  // namespace

CurlHttpClient::CurlHttpClient(CurlApi* curl_api, std::string test_cert_path)
    : curl_api_(curl_api), test_cert_path_(std::move(test_cert_path)) {
  FCP_CHECK(curl_api_ != nullptr);
}

std::unique_ptr<HttpRequestHandle> CurlHttpClient::EnqueueRequest(
    std::unique_ptr<HttpRequest> request) {
  FCP_LOG(INFO) << "Creating a " << ConvertMethodToString(request->method())
                << " request to " << request->uri() << " with body "
                << request->HasBody() << " with headers "
                << request->extra_headers().size();
  for (const auto& [key, value] : request->extra_headers()) {
    FCP_LOG(INFO) << key << ": " << value;
  }

  return std::make_unique<CurlHttpRequestHandle>(
      std::move(request), curl_api_->CreateEasyHandle(), test_cert_path_);
}

absl::Status CurlHttpClient::PerformRequests(
    std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>> requests) {
  FCP_LOG(INFO) << "PerformRequests";
  std::unique_ptr<CurlMultiHandle> multi_handle =
      curl_api_->CreateMultiHandle();
  FCP_CHECK(multi_handle != nullptr);

  for (const auto& [request_handle, callback] : requests) {
    FCP_CHECK(request_handle != nullptr);
    FCP_CHECK(callback != nullptr);

    auto http_request_handle =
        static_cast<CurlHttpRequestHandle*>(request_handle);
    FCP_RETURN_IF_ERROR(
        http_request_handle->AddToMulti(multi_handle.get(), callback));
  }

  return PerformMultiHandlesBlocked(multi_handle.get());
}

}  // namespace fcp::client::http::curl
