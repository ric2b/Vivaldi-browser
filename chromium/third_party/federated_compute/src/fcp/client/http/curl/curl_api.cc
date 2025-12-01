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
#include "fcp/client/http/curl/curl_api.h"

#include <memory>
#include <string>

#include "absl/synchronization/mutex.h"
#include "curl/curl.h"
#include "curl/easy.h"
#include "curl/multi.h"
#include "curl/system.h"
#include "fcp/base/monitoring.h"

namespace fcp::client::http::curl {
// CurlEasyHandle

CurlEasyHandle::CurlEasyHandle() : easy_handle_(curl_easy_init()) {
  FCP_CHECK(easy_handle_ != nullptr);
}

CurlEasyHandle::~CurlEasyHandle() { curl_easy_cleanup(easy_handle_); }

CURLcode CurlEasyHandle::GetInfo(CURLINFO info, curl_off_t* value) const {
  curl_off_t data = 0;
  CURLcode code = curl_easy_getinfo(easy_handle_, info, &data);
  if (code == CURLE_OK) *value = static_cast<curl_off_t>(data);
  return code;
}

std::string CurlEasyHandle::StrError(CURLcode code) {
  return curl_easy_strerror(code);
}

CURL* CurlEasyHandle::GetEasyHandle() const { return easy_handle_; }

// CurlMultiHandle

CurlMultiHandle::CurlMultiHandle() : multi_handle_(curl_multi_init()) {
  FCP_CHECK(multi_handle_ != nullptr);
}

CurlMultiHandle::~CurlMultiHandle() { curl_multi_cleanup(multi_handle_); }

CURLMsg* CurlMultiHandle::InfoRead(int* msgs_in_queue) {
  return curl_multi_info_read(multi_handle_, msgs_in_queue);
}

CURLMcode CurlMultiHandle::AddEasyHandle(CurlEasyHandle* easy_handle) {
  return curl_multi_add_handle(multi_handle_, easy_handle->GetEasyHandle());
}

CURLMcode CurlMultiHandle::RemoveEasyHandle(CurlEasyHandle* easy_handle) {
  return curl_multi_remove_handle(multi_handle_, easy_handle->GetEasyHandle());
}

CURLMcode CurlMultiHandle::Perform(int* num_running_handles) {
  return curl_multi_perform(multi_handle_, num_running_handles);
}

CURLMcode CurlMultiHandle::Poll(curl_waitfd extra_fds[],
                                unsigned int extra_nfds, int timeout_ms,
                                int* numfds) {
  return curl_multi_poll(multi_handle_, extra_fds, extra_nfds, timeout_ms,
                         numfds);
}

std::string CurlMultiHandle::StrError(CURLMcode code) {
  return curl_multi_strerror(code);
}

// CurlApi

CurlApi::CurlApi() { curl_global_init(CURL_GLOBAL_ALL); }

CurlApi::~CurlApi() { curl_global_cleanup(); }

std::unique_ptr<CurlEasyHandle> CurlApi::CreateEasyHandle() const {
  absl::MutexLock lock(&mutex_);
  // make_unique cannot access the private constructor, so we use
  // an old-fashioned new.
  return std::unique_ptr<CurlEasyHandle>(new CurlEasyHandle());
}

std::unique_ptr<CurlMultiHandle> CurlApi::CreateMultiHandle() const {
  absl::MutexLock lock(&mutex_);
  // make_unique cannot access the private constructor, so we use
  // an old-fashioned new.
  return std::unique_ptr<CurlMultiHandle>(new CurlMultiHandle());
}

}  // namespace fcp::client::http::curl
