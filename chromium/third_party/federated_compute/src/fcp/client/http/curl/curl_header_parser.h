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
#ifndef FCP_CLIENT_HTTP_CURL_CURL_HEADER_PARSER_H_
#define FCP_CLIENT_HTTP_CURL_CURL_HEADER_PARSER_H_

#include <string>

#include "absl/base/attributes.h"
#include "fcp/client/http/http_client.h"

namespace fcp::client::http::curl {

// A custom parser that is needed to call the first callback after all the
// headers received
class CurlHeaderParser {
 public:
  explicit CurlHeaderParser();
  ~CurlHeaderParser() = default;
  CurlHeaderParser(const CurlHeaderParser&) = delete;
  CurlHeaderParser& operator=(const CurlHeaderParser&) = delete;

  // Parses the next header string
  void ParseHeader(const std::string& header_string);

  // Removes the "Content-Encoding", "Content-Length", and "Content-Length"
  // headers from the response when the curl encoding in use because they
  // reflect in-flight encoded values
  void UseCurlEncoding();
  // Indicates that the parser reached the last header
  ABSL_MUST_USE_RESULT bool IsLastHeader() const;
  ABSL_MUST_USE_RESULT int GetStatusCode() const;
  ABSL_MUST_USE_RESULT HeaderList GetHeaderList() const;

 private:
  // Extracts status codes from HTTP/1.1 and HTTP/2 responses
  bool ParseAsStatus(const std::string& header_string);
  // Parses a header into a key-value pair
  bool ParseAsHeader(const std::string& header_string);
  // Decides whether it is the last header
  bool ParseAsLastLine(const std::string& header_string);

  int status_code_;
  HeaderList header_list_;
  bool is_last_header_line_;
  bool use_curl_encoding_;
};

}  // namespace fcp::client::http::curl

#endif  // FCP_CLIENT_HTTP_CURL_CURL_HEADER_PARSER_H_
