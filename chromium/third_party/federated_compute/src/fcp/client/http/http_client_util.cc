/*
 * Copyright 2021 Google LLC
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
#include "fcp/client/http/http_client_util.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "google/rpc/status.pb.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/http/http_client.h"

namespace fcp {
namespace client {
namespace http {

namespace {

absl::StatusCode ConvertHttpCodeToStatusCode(int code) {
  switch (code) {
    case kHttpBadRequest:
      return absl::StatusCode::kInvalidArgument;
    case kHttpForbidden:
      return absl::StatusCode::kPermissionDenied;
    case kHttpNotFound:
      return absl::StatusCode::kNotFound;
    case kHttpConflict:
      return absl::StatusCode::kAborted;
    case kHttpTooManyRequests:
      return absl::StatusCode::kResourceExhausted;
    case kHttpClientClosedRequest:
      return absl::StatusCode::kCancelled;
    case kHttpGatewayTimeout:
      return absl::StatusCode::kDeadlineExceeded;
    case kHttpNotImplemented:
      return absl::StatusCode::kUnimplemented;
    case kHttpServiceUnavailable:
      return absl::StatusCode::kUnavailable;
    case kHttpUnauthorized:
      return absl::StatusCode::kUnauthenticated;
    default: {
      // Importantly this range ensures that we treat not only "200 OK" as OK,
      // but also other codes such as "201 Created" etc.
      if (code >= 200 && code < 300) {
        return absl::StatusCode::kOk;
      }
      if (code >= 400 && code < 500) {
        return absl::StatusCode::kFailedPrecondition;
      }
      if (code >= 500 && code < 600) {
        return absl::StatusCode::kInternal;
      }
      return absl::StatusCode::kUnknown;
    }
  }
}

// Converts a `::google::rpc::Status` error code into an `absl::StatusCode`
// (there is a 1:1 mapping).
absl::StatusCode ConvertRpcCodeToStatusCode(int code) {
  switch (code) {
    case static_cast<int>(absl::StatusCode::kOk):
      return absl::StatusCode::kOk;
    case static_cast<int>(absl::StatusCode::kCancelled):
      return absl::StatusCode::kCancelled;
    case static_cast<int>(absl::StatusCode::kUnknown):
      return absl::StatusCode::kUnknown;
    case static_cast<int>(absl::StatusCode::kInvalidArgument):
      return absl::StatusCode::kInvalidArgument;
    case static_cast<int>(absl::StatusCode::kDeadlineExceeded):
      return absl::StatusCode::kDeadlineExceeded;
    case static_cast<int>(absl::StatusCode::kNotFound):
      return absl::StatusCode::kNotFound;
    case static_cast<int>(absl::StatusCode::kAlreadyExists):
      return absl::StatusCode::kAlreadyExists;
    case static_cast<int>(absl::StatusCode::kPermissionDenied):
      return absl::StatusCode::kPermissionDenied;
    case static_cast<int>(absl::StatusCode::kResourceExhausted):
      return absl::StatusCode::kResourceExhausted;
    case static_cast<int>(absl::StatusCode::kFailedPrecondition):
      return absl::StatusCode::kFailedPrecondition;
    case static_cast<int>(absl::StatusCode::kAborted):
      return absl::StatusCode::kAborted;
    case static_cast<int>(absl::StatusCode::kOutOfRange):
      return absl::StatusCode::kOutOfRange;
    case static_cast<int>(absl::StatusCode::kUnimplemented):
      return absl::StatusCode::kUnimplemented;
    case static_cast<int>(absl::StatusCode::kInternal):
      return absl::StatusCode::kInternal;
    case static_cast<int>(absl::StatusCode::kUnavailable):
      return absl::StatusCode::kUnavailable;
    case static_cast<int>(absl::StatusCode::kDataLoss):
      return absl::StatusCode::kDataLoss;
    case static_cast<int>(absl::StatusCode::kUnauthenticated):
      return absl::StatusCode::kUnauthenticated;
    default:
      // This should never be reached, since there should be a 1:1 mapping
      // between Absl and Google RPC status codes.
      return absl::StatusCode::kUnknown;
  }
}

absl::StatusOr<std::string> PercentEncode(
    absl::string_view input, std::function<bool(char c)> unencoded_chars) {
  std::string result;
  for (unsigned char c : input) {
    // We limit URIs only to ASCII characters.
    if (!absl::ascii_isascii(c)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Encountered unsupported char during URI encoding: ", c));
    }
    // The following characters are *not* percent-encoded.
    if (unencoded_chars(c)) {
      result.push_back(c);
      continue;
    }
    // Any other character is percent-encoded.
    result.append(absl::StrFormat("%%%X", c));
  }
  return result;
}

}  // namespace

absl::Status ConvertHttpCodeToStatus(int code) {
  absl::StatusCode status_code = ConvertHttpCodeToStatusCode(code);
  if (status_code == absl::StatusCode::kOk) {
    return absl::OkStatus();
  }
  std::string error_message =
      absl::StrCat("Request returned non-OK response (code: ", code, ")");
  return absl::Status(status_code, error_message);
}

absl::Status ConvertRpcStatusToAbslStatus(::google::rpc::Status rpc_status) {
  return absl::Status(ConvertRpcCodeToStatusCode(rpc_status.code()),
                      rpc_status.message());
}

::google::rpc::Status ConvertAbslStatusToRpcStatus(absl::Status status) {
  ::google::rpc::Status rpc_status;
  rpc_status.set_code(static_cast<int>(status.code()));
  rpc_status.set_message(std::string(status.message()));
  return rpc_status;
}

std::string ConvertMethodToString(HttpRequest::Method method) {
  switch (method) {
    case HttpRequest::Method::kGet:
      return "GET";
    case HttpRequest::Method::kHead:
      return "HEAD";
    case HttpRequest::Method::kDelete:
      return "DELETE";
    case HttpRequest::Method::kPatch:
      return "PATCH";
    case HttpRequest::Method::kPost:
      return "POST";
    case HttpRequest::Method::kPut:
      return "PUT";
  }
}

std::optional<std::string> FindHeader(const HeaderList& headers,
                                      absl::string_view needle) {
  // Normalize the needle (since header names are case insensitive, as per RFC
  // 2616 section 4.2).
  const std::string normalized_needle = absl::AsciiStrToLower(needle);
  const auto& header_entry = std::find_if(
      headers.begin(), headers.end(), [&normalized_needle](const Header& x) {
        // AsciiStrToLower safely handles non-ASCII data, and comparing
        // non-ASCII data w/ our needle is safe as well.
        return absl::AsciiStrToLower(std::get<0>(x)) == normalized_needle;
      });

  if (header_entry == headers.end()) {
    return std::nullopt;
  }
  return std::get<1>(*header_entry);
}

absl::StatusOr<std::string> JoinBaseUriWithSuffix(
    absl::string_view base_uri, absl::string_view uri_suffix) {
  if (!uri_suffix.empty() && uri_suffix[0] != '/') {
    return absl::InvalidArgumentError(
        "uri_suffix be empty or must have a leading '/'");
  }
  // Construct the full URI by joining the base URI we should use with the given
  // suffix, ensuring that there's always a single '/' in between the two parts.
  return absl::StrCat(absl::StripSuffix(base_uri, "/"), "/",
                      absl::StripPrefix(uri_suffix, "/"));
}

absl::StatusOr<std::string> EncodeUriSinglePathSegment(
    absl::string_view input) {
  return PercentEncode(input, [](char c) {
    return absl::ascii_isalnum(c) || c == '-' || c == '_' || c == '.' ||
           c == '~';
  });
}

absl::StatusOr<std::string> EncodeUriMultiplePathSegments(
    absl::string_view input) {
  return PercentEncode(input, [](char c) {
    return absl::ascii_isalnum(c) || c == '-' || c == '_' || c == '.' ||
           c == '~' || c == '/';
  });
}

absl::StatusOr<std::string> CreateByteStreamUploadUriSuffix(
    absl::string_view resource_name) {
  constexpr absl::string_view pattern = "/upload/v1/media/$0";
  FCP_ASSIGN_OR_RETURN(std::string encoded_resource_name,
                       EncodeUriMultiplePathSegments(resource_name));
  // Construct the URI suffix.
  return absl::Substitute(pattern, encoded_resource_name);
}

bool IsRetryableError(absl::StatusCode code) {
  // https://google.aip.dev/194 indicates that we should retry on UNAVAILABLE
  // errors.
  return code == absl::StatusCode::kUnavailable;
}

absl::Duration GetRetryDelay(absl::BitGen& bit_gen, absl::Duration retry_delay,
                             int32_t retry_attempt) {
  // `retry_delay` represents our minimum delay, and we increase the delay
  // exponentially with each retry attempt.

  // Ensure that the retry delay is at least 1 millisecond.
  if (retry_delay < absl::Milliseconds(1)) {
    retry_delay = absl::Milliseconds(1);
  }

  double backoff_random_multiplier = 0.4;
  double backoff_base = 1.3;
  const absl::Duration first_term = retry_delay * backoff_random_multiplier;
  // Generate a random number in [0.6, 1.0].
  double second_term_multiplier =
      absl::Uniform(bit_gen, 1.0 - backoff_random_multiplier, 1.0);
  const absl::Duration second_term = second_term_multiplier * retry_delay *
                                     std::pow(backoff_base, retry_attempt);
  return first_term + second_term;
}

}  // namespace http
}  // namespace client
}  // namespace fcp
