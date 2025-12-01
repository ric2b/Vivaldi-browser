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
#include "fcp/client/http/curl/curl_header_parser.h"

#include <string>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/http_client_util.h"

namespace fcp::client::http::curl {

CurlHeaderParser::CurlHeaderParser()
    : status_code_(-1),
      is_last_header_line_(false),
      use_curl_encoding_(false) {}

void CurlHeaderParser::ParseHeader(const std::string& header_string) {
  if (ParseAsStatus(header_string)) {
    return;
  }
  if (ParseAsHeader(header_string)) {
    return;
  }
  if (ParseAsLastLine(header_string)) {
    return;
  }
}

bool CurlHeaderParser::ParseAsStatus(const std::string& header_string) {
  if (!absl::StartsWith(header_string, "HTTP/")) {
    return false;
  }

  std::pair<std::string, std::string> split =
      absl::StrSplit(header_string, ' ');
  int status_code;
  if (!absl::SimpleAtoi(split.second.substr(0, 3), &status_code)) {
    return false;
  }

  status_code_ = status_code;
  // It is required that we store only the final header list. So we keep the
  // last set of headers
  header_list_.clear();
  return true;
}

bool CurlHeaderParser::ParseAsHeader(const std::string& header_string) {
  if (!absl::StrContains(header_string, ':')) {
    return false;
  }

  std::pair<std::string, std::string> split =
      absl::StrSplit(header_string, ':');
  std::string key = split.first;
  std::string value = std::string(absl::StripAsciiWhitespace(split.second));

  // Removes the "Content-Encoding", "Content-Length", and "Content-Length"
  // headers from the response when the curl encoding in use because they
  // reflect in-flight encoded values
  if (!use_curl_encoding_ ||
      (!absl::EqualsIgnoreCase(key, kContentEncodingHdr) &&
       !absl::EqualsIgnoreCase(key, kContentLengthHdr) &&
       !absl::EqualsIgnoreCase(key, kTransferEncodingHdr))) {
    header_list_.push_back({key, value});
  }
  return true;
}

bool CurlHeaderParser::ParseAsLastLine(const std::string& header_string) {
  // In general, it is impossible to tell when curl will reach the last
  // header because there could another one in some special cases. In
  // particular, it happens when curl hits a redirect status code (301).
  // In this case, we need to proceed.
  if (std::string(header_string) == "\r\n" &&
      status_code_ != HttpResponseCode::kHttpMovedPermanently) {
    is_last_header_line_ = true;
    return true;
  }

  return false;
}

void CurlHeaderParser::UseCurlEncoding() { use_curl_encoding_ = true; }

bool CurlHeaderParser::IsLastHeader() const { return is_last_header_line_; }

int CurlHeaderParser::GetStatusCode() const { return status_code_; }

HeaderList CurlHeaderParser::GetHeaderList() const { return header_list_; }

}  // namespace fcp::client::http::curl
