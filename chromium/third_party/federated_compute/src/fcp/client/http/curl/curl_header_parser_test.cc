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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/client/http/http_client_util.h"

namespace fcp::client::http::curl {
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(CurlHeaderParserTest, Parse_HTTP_1_1_StatusCode) {
  CurlHeaderParser parser;
  EXPECT_THAT(parser.GetStatusCode(), -1);
  EXPECT_THAT(parser.GetHeaderList(), ElementsAre());

  auto line1 = "HTTP/1.1 200 OK\r\n";
  parser.ParseHeader(line1);

  EXPECT_THAT(parser.GetStatusCode(), 200);
  EXPECT_THAT(parser.GetHeaderList(), ElementsAre());

  auto line2 = "HTTP/1.1 500 Internal Error\r\n";
  parser.ParseHeader(line2);

  EXPECT_THAT(parser.GetStatusCode(), 500);
  EXPECT_THAT(parser.GetHeaderList(), ElementsAre());

  auto line3 = "HTTP/1.1 123 Some:Custom Message\r\n";
  parser.ParseHeader(line3);

  EXPECT_THAT(parser.GetStatusCode(), 123);
  EXPECT_THAT(parser.GetHeaderList(), ElementsAre());
}

TEST(CurlHeaderParserTest, Parse_HTTP_2_StatusCode) {
  CurlHeaderParser parser;
  EXPECT_THAT(parser.GetStatusCode(), -1);
  EXPECT_THAT(parser.GetHeaderList(), ElementsAre());

  auto line1 = "HTTP/2 200\r\n";
  parser.ParseHeader(line1);

  EXPECT_THAT(parser.GetStatusCode(), 200);
  EXPECT_THAT(parser.GetHeaderList(), ElementsAre());

  auto line2 = "HTTP/2 500\r\n";
  parser.ParseHeader(line2);

  EXPECT_THAT(parser.GetStatusCode(), 500);
  EXPECT_THAT(parser.GetHeaderList(), ElementsAre());
}

TEST(CurlHeaderParserTest, ParseHeaders) {
  CurlHeaderParser parser;
  EXPECT_THAT(parser.GetHeaderList(), ElementsAre());
  EXPECT_THAT(parser.IsLastHeader(), false);

  auto line = "HTTP/2 301\r\n";  // Redirect
  parser.ParseHeader(line);
  EXPECT_THAT(parser.GetHeaderList(), ElementsAre());
  EXPECT_THAT(parser.IsLastHeader(), false);

  line = "Content-Encoding: gzip\r\n";
  parser.ParseHeader(line);
  EXPECT_THAT(parser.GetHeaderList(),
              ElementsAre(Pair(kContentEncodingHdr, "gzip")));
  EXPECT_THAT(parser.IsLastHeader(), false);

  line = "\r\n";
  parser.ParseHeader(line);
  EXPECT_THAT(parser.GetHeaderList(),
              ElementsAre(Pair(kContentEncodingHdr, "gzip")));
  EXPECT_THAT(parser.IsLastHeader(), false);

  line = "HTTP/2 200\r\n";  // OK
  parser.ParseHeader(line);
  EXPECT_THAT(parser.GetHeaderList(), ElementsAre());
  EXPECT_THAT(parser.IsLastHeader(), false);

  line = "Content-Type: application/octet-stream\r\n";
  parser.ParseHeader(line);
  EXPECT_THAT(parser.GetHeaderList(),
              ElementsAre(Pair("Content-Type", "application/octet-stream")));
  EXPECT_THAT(parser.IsLastHeader(), false);

  line = "Content-Length: 150\r\n";
  parser.ParseHeader(line);
  EXPECT_THAT(parser.GetHeaderList(),
              ElementsAre(Pair("Content-Type", "application/octet-stream"),
                          Pair(kContentLengthHdr, "150")));
  EXPECT_THAT(parser.IsLastHeader(), false);

  line = "\r\n";
  parser.ParseHeader(line);
  EXPECT_THAT(parser.GetHeaderList(),
              ElementsAre(Pair("Content-Type", "application/octet-stream"),
                          Pair(kContentLengthHdr, "150")));
  EXPECT_THAT(parser.IsLastHeader(), true);
}
}  // namespace
}  // namespace fcp::client::http::curl
