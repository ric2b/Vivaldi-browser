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

#include "fcp/client/http/curl/curl_http_request_handle.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "curl/curl.h"
#include "curl/multi.h"
#include "curl/system.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/http/curl/curl_api.h"
#include "fcp/client/http/curl/curl_header_parser.h"
#include "fcp/client/http/curl/curl_http_response.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/http_client_util.h"

namespace fcp::client::http::curl {
namespace {
// A type check for the macro.
inline CURLcode AsCode(CURLcode code) { return code; }
/**
 * Macro which allows to check for a status code and return from the
 * current method if not OK. Example:
 *
 *     Status DoSomething() {
 *       CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(...));
 *     }
 */
#define CURL_RETURN_IF_ERROR(expr)                        \
  do {                                                    \
    CURLcode __code = AsCode(expr);                       \
    if (__code != CURLE_OK) {                             \
      FCP_LOG(ERROR) << "Easy handle failed with "        \
                     << CurlEasyHandle::StrError(__code); \
      return (__code);                                    \
    }                                                     \
  } while (false)

// Add a new element to a C-style list.
curl_slist* AddToCurlHeaderList(curl_slist* header_list, const std::string& key,
                                const std::string& value) {
  // A null pointer is returned if anything went wrong, otherwise a new
  // list pointer is returned.
  curl_slist* tmp =
      curl_slist_append(header_list, absl::StrCat(key, ": ", value).c_str());
  FCP_CHECK(tmp != nullptr);
  return tmp;
}

}  // namespace

size_t CurlHttpRequestHandle::HeaderCallback(char* buffer, size_t size,
                                             size_t n_items, void* user_data) {
  auto self = static_cast<CurlHttpRequestHandle*>(user_data);
  std::string str_header(static_cast<char*>(buffer), size * n_items);

  self->header_parser_.ParseHeader(str_header);
  if (!self->header_parser_.IsLastHeader()) {
    return size * n_items;
  }

  self->response_ =
      std::make_unique<CurlHttpResponse>(self->header_parser_.GetStatusCode(),
                                         self->header_parser_.GetHeaderList());

  FCP_CHECK(self->callback_ != nullptr);
  absl::Status status =
      self->callback_->OnResponseStarted(*self->request_, *self->response_);

  if (!status.ok()) {
    FCP_LOG(ERROR) << "Called OnResponseStarted. Received status: " << status;
    self->callback_->OnResponseError(*self->request_, status);
  }

  return size * n_items;
}

size_t CurlHttpRequestHandle::DownloadCallback(void* body, size_t size,
                                               size_t nmemb, void* user_data) {
  auto self = static_cast<CurlHttpRequestHandle*>(user_data);
  absl::string_view str_body(static_cast<char*>(body), size * nmemb);

  absl::Status status = self->callback_->OnResponseBody(
      *self->request_, *self->response_, str_body);

  if (!status.ok()) {
    FCP_LOG(ERROR) << "Called OnResponseBody. Received status: " << status;
    self->callback_->OnResponseBodyError(*self->request_, *self->response_,
                                         status);
  }

  return size * nmemb;
}

size_t CurlHttpRequestHandle::UploadCallback(char* buffer, size_t size,
                                             size_t num, void* user_data) {
  auto self = static_cast<CurlHttpRequestHandle*>(user_data);
  size_t buffer_size = size * num;

  absl::StatusOr<int64_t> read_size =
      self->request_->ReadBody(buffer, buffer_size);
  if (read_size.ok()) {
    return read_size.value();
  } else if (read_size.status().code() == absl::StatusCode::kOutOfRange) {
    return 0;
  }
  return CURL_READFUNC_ABORT;
}

size_t CurlHttpRequestHandle::ProgressCallback(void* user_data,
                                               curl_off_t dltotal,
                                               curl_off_t dlnow,
                                               curl_off_t ultotal,
                                               curl_off_t ulnow) {
  auto self = static_cast<CurlHttpRequestHandle*>(user_data);
  absl::MutexLock lock(&self->mutex_);
  // Abort is any number except zero.
  return (self->is_cancelled_) ? 1 : 0;
}

CurlHttpRequestHandle::CurlHttpRequestHandle(
    std::unique_ptr<HttpRequest> request,
    std::unique_ptr<CurlEasyHandle> easy_handle,
    const std::string& test_cert_path)
    : request_(std::move(request)),
      response_(nullptr),
      easy_handle_(std::move(easy_handle)),
      callback_(nullptr),
      is_being_performed_(false),
      is_completed_(false),
      is_cancelled_(false),
      header_list_(nullptr) {
  FCP_CHECK(request_ != nullptr);
  FCP_CHECK(easy_handle_ != nullptr);

  CURLcode code = InitializeConnection(test_cert_path);
  if (code != CURLE_OK) {
    FCP_LOG(ERROR) << "easy_handle initialization failed with code "
                   << CurlEasyHandle::StrError(code);
    FCP_LOG(ERROR) << error_buffer_;
    callback_->OnResponseError(*request_, absl::InternalError(error_buffer_));
    return;
  }
}

CurlHttpRequestHandle::~CurlHttpRequestHandle() {
  curl_slist_free_all(header_list_);
}

void CurlHttpRequestHandle::Cancel() {
  absl::MutexLock lock(&mutex_);

  if (callback_ == nullptr || is_cancelled_ || is_completed_) {
    return;
  }
  if (response_ != nullptr) {
    callback_->OnResponseBodyError(*request_, *response_,
                                   absl::CancelledError());
  } else {
    callback_->OnResponseError(*request_, absl::CancelledError());
  }
  is_cancelled_ = true;
}

void CurlHttpRequestHandle::MarkAsCompleted() {
  absl::MutexLock lock(&mutex_);

  FCP_CHECK(callback_ != nullptr);
  if (!is_cancelled_ && !is_completed_) {
    if (response_ != nullptr) {
      callback_->OnResponseCompleted(*request_, *response_);
    } else {
      callback_->OnResponseError(*request_,
                                 absl::InternalError("response_ is nullptr"));
    }
  }
  is_completed_ = true;
}

absl::Status CurlHttpRequestHandle::AddToMulti(CurlMultiHandle* multi_handle,
                                               HttpRequestCallback* callback) {
  absl::MutexLock lock(&mutex_);

  FCP_CHECK(callback != nullptr);
  FCP_CHECK(multi_handle != nullptr);

  if (is_cancelled_) {
    callback->OnResponseError(*request_, absl::CancelledError());
    return absl::CancelledError();
  } else if (is_being_performed_ || is_completed_) {
    return absl::ResourceExhaustedError(
        "The handle was previously passed to another PerformRequests call.");
  }

  is_being_performed_ = true;
  callback_ = callback;

  CURLMcode code = multi_handle->AddEasyHandle(easy_handle_.get());
  if (code != CURLM_OK) {
    FCP_LOG(ERROR) << "AddEasyHandle failed with code " << code;
    FCP_LOG(ERROR) << error_buffer_;
    callback_->OnResponseError(*request_, absl::InternalError(error_buffer_));
    return absl::InternalError(error_buffer_);
  }

  return absl::OkStatus();
}

void CurlHttpRequestHandle::RemoveFromMulti(CurlMultiHandle* multi_handle) {
  absl::MutexLock lock(&mutex_);

  FCP_CHECK(multi_handle != nullptr);
  CURLMcode code = multi_handle->RemoveEasyHandle(easy_handle_.get());
  if (code != CURLM_OK) {
    FCP_LOG(ERROR) << "RemoveEasyHandle failed with code "
                   << CurlMultiHandle::StrError(code);
    FCP_LOG(ERROR) << error_buffer_;
  }
}

HttpRequestHandle::SentReceivedBytes
CurlHttpRequestHandle::TotalSentReceivedBytes() const {
  absl::MutexLock lock(&mutex_);
  curl_off_t total_sent_bytes = 0;
  CURLcode code =
      easy_handle_->GetInfo(CURLINFO_SIZE_UPLOAD_T, &total_sent_bytes);
  if (code != CURLE_OK) {
    FCP_LOG(ERROR) << "TotalSentBytes failed with code " << code;
    FCP_LOG(ERROR) << error_buffer_;
  }
  curl_off_t total_received_bytes = 0;
  code = easy_handle_->GetInfo(CURLINFO_SIZE_DOWNLOAD_T, &total_received_bytes);
  if (code != CURLE_OK) {
    FCP_LOG(ERROR) << "TotalReceivedBytes failed with code " << code;
    FCP_LOG(ERROR) << error_buffer_;
  }
  return {.sent_bytes = total_sent_bytes,
          .received_bytes = total_received_bytes};
}

CURLcode CurlHttpRequestHandle::InitializeConnection(
    const std::string& test_cert_path) {
  error_buffer_[0] = 0;
  // Needed to read an error message.
  CURL_RETURN_IF_ERROR(
      easy_handle_->SetOpt(CURLOPT_ERRORBUFFER, error_buffer_));

  // Skip all signal handling because is not thread-safe.
  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_NOSIGNAL, 1L));

  CURL_RETURN_IF_ERROR(
      easy_handle_->SetOpt(CURLOPT_URL, std::string(request_->uri())));

  // Forces curl to follow redirects.
  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_FOLLOWLOCATION, 1L));

  // Suppresses headers added by a proxy.
  CURL_RETURN_IF_ERROR(
      easy_handle_->SetOpt(CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L));

  // Force curl to verify the ssl connection.
  CURL_RETURN_IF_ERROR(
      test_cert_path.empty()
          ? easy_handle_->SetOpt(CURLOPT_CAINFO, nullptr)
          : easy_handle_->SetOpt(CURLOPT_CAINFO, test_cert_path));

  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_SSL_VERIFYPEER, 2L));

  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_SSL_VERIFYHOST, 1L));

  // Force curl to never timeout.
  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_TIMEOUT_MS,
                                            std::numeric_limits<int>::max()));

  // Called when a response header received.
  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(
      CURLOPT_HEADERFUNCTION, &CurlHttpRequestHandle::HeaderCallback));

  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_HEADERDATA, this));

  // Called when a response body received.
  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(
      CURLOPT_WRITEFUNCTION, &CurlHttpRequestHandle::DownloadCallback));

  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_WRITEDATA, this));

  // Called to send a request body
  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(
      CURLOPT_READFUNCTION, &CurlHttpRequestHandle::UploadCallback));

  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_READDATA, this));

  // Called periodically. We use it to check whether the request is cancelled.
  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(
      CURLOPT_XFERINFOFUNCTION, &CurlHttpRequestHandle::ProgressCallback));
  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_XFERINFODATA, this));
  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_NOPROGRESS, 0L));

  // Private storage. Used by a multi-handle.
  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_PRIVATE, this));

  switch (request_->method()) {
    case HttpRequest::Method::kGet:
      CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_HTTPGET, 1L));
      break;
    case HttpRequest::Method::kHead:
      CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_NOBODY, 1L));
      break;
    case HttpRequest::Method::kPost:
      CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_POST, 1L));
      // Forces curl to use the callback.
      CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_POSTFIELDS, nullptr));
      break;
    case HttpRequest::Method::kPut:
      CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_UPLOAD, 1L));
      break;
    case HttpRequest::Method::kPatch:
    case HttpRequest::Method::kDelete:
      FCP_LOG(ERROR) << "Unsupported request type";
      return CURLE_UNSUPPORTED_PROTOCOL;
  }

  return InitializeHeaders(request_->extra_headers(), request_->method());
}

CURLcode CurlHttpRequestHandle::InitializeHeaders(
    const HeaderList& extra_headers, HttpRequest::Method method) {
  // If no "Accept-Encoding" request header is explicitly specified
  // advertise an "Accept-Encoding: gzip" else leave decoded.
  std::optional<std::string> accept_encoding =
      FindHeader(request_->extra_headers(), kAcceptEncodingHdr);
  if (!accept_encoding.has_value()) {
    // Libcurl is responsible for the encoding.
    CURL_RETURN_IF_ERROR(
        easy_handle_->SetOpt(CURLOPT_ACCEPT_ENCODING, kGzipEncodingHdrValue));
    header_parser_.UseCurlEncoding();
  } else {
    // The caller is responsible for the encoding.
    CURL_RETURN_IF_ERROR(
        easy_handle_->SetOpt(CURLOPT_ACCEPT_ENCODING, nullptr));
  }

  for (auto& [key, value] : extra_headers) {
    if (absl::EqualsIgnoreCase(key, kAcceptEncodingHdr)) {
      continue;
    } else if (absl::EqualsIgnoreCase(key, kContentLengthHdr)) {
      if (method == HttpRequest::Method::kPost) {
        // For post less than 2GB
        CURL_RETURN_IF_ERROR(
            easy_handle_->SetOpt(CURLOPT_POSTFIELDSIZE, std::stol(value)));

        // Removes the header to prevent libcurl from setting it
        // to 'Expect: 100-continue' by default, which causes an additional
        // and unnecessary network round trip.
        header_list_ = AddToCurlHeaderList(header_list_, kExpectHdr, "");
      } else if (method == HttpRequest::Method::kPut) {
        CURL_RETURN_IF_ERROR(
            easy_handle_->SetOpt(CURLOPT_INFILESIZE, std::stol(value)));
        header_list_ = AddToCurlHeaderList(header_list_, kExpectHdr, "");
      }
    } else {
      // A user-defined "Expect" header is not supported.
      FCP_CHECK(!absl::EqualsIgnoreCase(key, kExpectHdr));
      header_list_ = AddToCurlHeaderList(header_list_, key, value);
    }
  }

  CURL_RETURN_IF_ERROR(easy_handle_->SetOpt(CURLOPT_HTTPHEADER, header_list_));
  return CURLE_OK;
}
}  // namespace fcp::client::http::curl
