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
#include "fcp/client/http/in_memory_request_response.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/memory/memory.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/compression.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/cache/resource_cache.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/http_client_util.h"
#include "fcp/client/http/http_resource_metadata.pb.h"
#include "fcp/client/interruptible_runner.h"

namespace fcp {
namespace client {
namespace http {
namespace {

// Returns the resource from the cache, or NOT_FOUND if it was not in the cache.
// If the resource was compressed, it will be decompressed.
absl::StatusOr<absl::Cord> TryGetResourceFromCache(
    absl::string_view client_cache_id,
    const std::optional<absl::Duration>& max_age,
    cache::ResourceCache& resource_cache) {
  FCP_ASSIGN_OR_RETURN(
      cache::ResourceCache::ResourceAndMetadata cached_resource_and_metadata,
      resource_cache.Get(client_cache_id, max_age));
  HttpResourceMetadata metadata;
  if (!cached_resource_and_metadata.metadata.UnpackTo(&metadata)) {
    return absl::InternalError("Failed to unpack metadata!");
  }
  absl::Cord cached_resource = cached_resource_and_metadata.resource;
  if (metadata.compression_format() ==
      ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP) {
    FCP_ASSIGN_OR_RETURN(cached_resource,
                         UncompressWithGzip(std::string(cached_resource)));
  }
  return cached_resource;
}

absl::Status TryPutResourceInCache(absl::string_view client_cache_id,
                                   const absl::Cord& response_body,
                                   bool response_encoded_with_gzip,
                                   absl::Duration max_age,
                                   cache::ResourceCache& resource_cache) {
  // We fetched a resource that has a client_cache_id and was not
  // loaded from the cache, put it in the cache.
  HttpResourceMetadata metadata;
  if (response_encoded_with_gzip) {
    metadata.set_compression_format(
        ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  } else {
    metadata.set_compression_format(
        ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_UNSPECIFIED);
  }
  google::protobuf::Any metadata_wrapper;
  metadata_wrapper.PackFrom(metadata);
  return resource_cache.Put(client_cache_id, response_body, metadata_wrapper,
                            max_age);
}

absl::StatusOr<std::vector<absl::StatusOr<InMemoryHttpResponse>>>
PerformMultipleRequestsInMemory(
    HttpClient& http_client, InterruptibleRunner& interruptible_runner,
    std::vector<std::unique_ptr<http::HttpRequest>> requests,
    int64_t* bytes_received_acc, int64_t* bytes_sent_acc) {
  // A vector that will own the request handles and callbacks (and will
  // determine their lifetimes).
  std::vector<std::pair<std::unique_ptr<HttpRequestHandle>,
                        std::unique_ptr<InMemoryHttpRequestCallback>>>
      handles_and_callbacks;
  handles_and_callbacks.reserve(requests.size());

  // An accompanying vector that contains just the raw pointers, for passing to
  // `HttpClient::PerformRequests`.
  std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>>
      handles_and_callbacks_ptrs;
  handles_and_callbacks_ptrs.reserve(requests.size());

  // Enqueue each request, and create a simple callback for each request which
  // will simply buffer the response body in-memory and allow us to consume that
  // buffer once all requests have finished.
  for (std::unique_ptr<HttpRequest>& request : requests) {
    std::unique_ptr<HttpRequestHandle> handle =
        http_client.EnqueueRequest(std::move(request));
    auto callback = std::make_unique<InMemoryHttpRequestCallback>();
    handles_and_callbacks_ptrs.push_back({handle.get(), callback.get()});
    handles_and_callbacks.push_back({std::move(handle), std::move(callback)});
  }

  // Issue the requests in one call (allowing the HttpClient to issue them
  // concurrently), in an interruptible fashion.
  absl::Status result = interruptible_runner.Run(
      [&http_client, &handles_and_callbacks_ptrs]() {
        return http_client.PerformRequests(handles_and_callbacks_ptrs);
      },
      [&handles_and_callbacks_ptrs] {
        // If we get aborted then call HttpRequestHandle::Cancel on all handles.
        // This should result in the PerformRequests call returning early and
        // InterruptibleRunner::Run returning CANCELLED.
        for (auto [handle, callback] : handles_and_callbacks_ptrs) {
          handle->Cancel();
        }
      });
  // Update the network stats *before* we return (just in case a failed
  // `PerformRequests` call caused some network traffic to have been sent
  // anyway).
  for (auto& [handle, callback] : handles_and_callbacks) {
    HttpRequestHandle::SentReceivedBytes sent_received_bytes =
        handle->TotalSentReceivedBytes();
    if (bytes_received_acc != nullptr) {
      *bytes_received_acc += sent_received_bytes.received_bytes;
    }
    if (bytes_sent_acc != nullptr) {
      *bytes_sent_acc += sent_received_bytes.sent_bytes;
    }
  }

  FCP_RETURN_IF_ERROR(result);

  // Gather and return the results.
  std::vector<absl::StatusOr<InMemoryHttpResponse>> results;
  results.reserve(handles_and_callbacks.size());
  for (auto& [handle, callback] : handles_and_callbacks) {
    results.push_back(callback->Response());
  }
  return results;
}

}  // namespace

using CompressionFormat =
    ::fcp::client::http::UriOrInlineData::InlineData::CompressionFormat;

static constexpr char kOctetStream[] = "application/octet-stream";
constexpr absl::string_view kClientDecodedGzipSuffix = "+gzip";

absl::StatusOr<std::unique_ptr<HttpRequest>> InMemoryHttpRequest::Create(
    absl::string_view uri, Method method, HeaderList extra_headers,
    std::string body, bool use_compression) {
  // Allow http://localhost:xxxx as an exception to the https-only policy,
  // so that we can use a local http test server.
  if (!absl::StartsWithIgnoreCase(uri, kHttpsScheme) &&
      !absl::StartsWithIgnoreCase(uri, kLocalhostUri)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Non-HTTPS URIs are not supported: ", uri));
  }
  if (use_compression) {
    FCP_ASSIGN_OR_RETURN(body, CompressWithGzip(body));
    extra_headers.push_back({kContentEncodingHdr, kGzipEncodingHdrValue});
  }
  std::optional<std::string> content_length_hdr =
      FindHeader(extra_headers, kContentLengthHdr);
  if (content_length_hdr.has_value()) {
    return absl::InvalidArgumentError(
        "Content-Length header should not be provided!");
  }

  if (!body.empty()) {
    switch (method) {
      case HttpRequest::Method::kPost:
      case HttpRequest::Method::kPatch:
      case HttpRequest::Method::kPut:
      case HttpRequest::Method::kDelete:
        break;
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "Request method does not allow request body: ", method));
    }
    // Add a Content-Length header, but only if there's a request body.
    extra_headers.push_back({kContentLengthHdr, std::to_string(body.size())});
  }

  return absl::WrapUnique(new InMemoryHttpRequest(
      uri, method, std::move(extra_headers), std::move(body)));
}

absl::StatusOr<int64_t> InMemoryHttpRequest::ReadBody(char* buffer,
                                                      int64_t requested) {
  // This method is called from the HttpClient's thread (we don't really care
  // which one). Hence, we use a mutex to ensure that subsequent calls to this
  // method see the modifications to cursor_.
  absl::WriterMutexLock _(&mutex_);

  // Check whether there's any bytes left to read, and indicate the end has been
  // reached if not.
  int64_t bytes_left = body_.size() - cursor_;
  if (bytes_left == 0) {
    return absl::OutOfRangeError("End of stream reached");
  }
  FCP_CHECK(buffer != nullptr);
  FCP_CHECK(requested > 0);
  // Calculate how much data we can return, based on the size of `buffer`.
  int64_t actual_read = bytes_left <= requested ? bytes_left : requested;
  std::memcpy(buffer, body_.data() + cursor_, actual_read);
  cursor_ += actual_read;
  return actual_read;
}

std::unique_ptr<HttpRequest> InMemoryHttpRequest::Clone() const {
  return absl::WrapUnique(
      new InMemoryHttpRequest(uri_, method_, headers_, body_));
}

absl::Status InMemoryHttpRequestCallback::OnResponseStarted(
    const HttpRequest& request, const HttpResponse& response) {
  absl::WriterMutexLock _(&mutex_);
  response_code_ = response.code();

  std::optional<std::string> content_encoding_header =
      FindHeader(response.headers(), kContentEncodingHdr);
  if (content_encoding_header.has_value()) {
    // We don't expect the response body to be "Content-Encoding" encoded,
    // because the `HttpClient` is supposed to transparently handle the decoding
    // for us (unless we specified a "Accept-Encoding" header in the request,
    // which would indicate that we wanted to handle the response decoding).
    if (!FindHeader(request.extra_headers(), kAcceptEncodingHdr).has_value()) {
      // Note: technically, we should only receive Content-Encoding values that
      // match the Accept-Encoding values provided in the request headers. The
      // check above isn't quite that strict, but that's probably fine (since
      // such issues should be rare, and can be handled farther up the stack).
      status_ = absl::InvalidArgumentError(
          absl::StrCat("Unexpected header: ", kContentEncodingHdr));
      return status_;
    }
    content_encoding_ = *content_encoding_header;
  }

  content_type_ = FindHeader(response.headers(), kContentTypeHdr).value_or("");

  // Similarly, we should under no circumstances receive a non-identity
  // Transfer-Encoding header, since the `HttpClient` is unconditionally
  // required to undo any such encoding for us.
  std::optional<std::string> transfer_encoding_header =
      FindHeader(response.headers(), kTransferEncodingHdr);
  if (transfer_encoding_header.has_value() &&
      absl::AsciiStrToLower(*transfer_encoding_header) !=
          kIdentityEncodingHdrValue) {
    status_ = absl::InvalidArgumentError(
        absl::StrCat("Unexpected header: ", kTransferEncodingHdr));
    return status_;
  }

  // If no Content-Length header is provided, this means that the server either
  // didn't provide one and is streaming the response, or that the HttpClient
  // implementation transparently decompressed the data for us and stripped the
  // Content-Length header (as per the HttpClient contract).
  std::optional<std::string> content_length_hdr =
      FindHeader(response.headers(), kContentLengthHdr);
  if (!content_length_hdr.has_value()) {
    return absl::OkStatus();
  }

  // A Content-Length header available. Let's parse it so that we know how much
  // data to expect.
  int64_t content_length;
  // Note that SimpleAtoi safely handles non-ASCII data.
  if (!absl::SimpleAtoi(*content_length_hdr, &content_length)) {
    status_ = absl::InvalidArgumentError(
        "Could not parse Content-Length response header");
    return status_;
  }
  if (content_length < 0) {
    status_ = absl::OutOfRangeError(absl::StrCat(
        "Invalid Content-Length response header: ", content_length));
    return status_;
  }
  expected_content_length_ = content_length;

  return absl::OkStatus();
}

void InMemoryHttpRequestCallback::OnResponseError(const HttpRequest& request,
                                                  const absl::Status& error) {
  absl::WriterMutexLock _(&mutex_);
  status_ = absl::Status(
      error.code(), absl::StrCat("Error receiving response headers (error: ",
                                 error.message(), ")"));
}

absl::Status InMemoryHttpRequestCallback::OnResponseBody(
    const HttpRequest& request, const HttpResponse& response,
    absl::string_view data) {
  // This runs on a thread chosen by the HttpClient implementation (i.e. it
  // could be our original thread, or a different one). Ensure that if
  // subsequent callbacks occur on different threads each thread sees the
  // previous threads' updates to response_buffer_.
  absl::WriterMutexLock _(&mutex_);

  // Ensure we're not receiving more data than expected.
  if (expected_content_length_.has_value() &&
      response_buffer_.size() + data.size() > *expected_content_length_) {
    status_ = absl::OutOfRangeError(absl::StrCat(
        "Too much response body data received (rcvd: ", response_buffer_.size(),
        ", new: ", data.size(), ", max: ", *expected_content_length_, ")"));
    return status_;
  }

  // Copy the data into the target buffer. Note that this means we'll always
  // store the response body as a number of memory fragments (rather than a
  // contiguous buffer). However, because HttpClient implementations are
  // encouraged to return response data in fairly large chunks, we don't expect
  // this too cause much overhead.
  response_buffer_.Append(data);

  return absl::OkStatus();
}

void InMemoryHttpRequestCallback::OnResponseBodyError(
    const HttpRequest& request, const HttpResponse& response,
    const absl::Status& error) {
  absl::WriterMutexLock _(&mutex_);
  status_ = absl::Status(
      error.code(),
      absl::StrCat("Error receiving response body (response code: ",
                   response.code(), ", error: ", error.message(), ")"));
}

void InMemoryHttpRequestCallback::OnResponseCompleted(
    const HttpRequest& request, const HttpResponse& response) {
  // Once the body has been received correctly, turn the response code into a
  // canonical code.
  absl::WriterMutexLock _(&mutex_);
  // Note: the case when too *much* response data is unexpectedly received is
  // handled in OnResponseBody (while this handles the case of too little data).
  if (expected_content_length_.has_value() &&
      response_buffer_.size() != *expected_content_length_) {
    status_ = absl::InvalidArgumentError(
        absl::StrCat("Too little response body data received (rcvd: ",
                     response_buffer_.size(),
                     ", expected: ", *expected_content_length_, ")"));
    return;
  }

  status_ = ConvertHttpCodeToStatus(*response_code_);
}

absl::StatusOr<InMemoryHttpResponse> InMemoryHttpRequestCallback::Response()
    const {
  absl::ReaderMutexLock _(&mutex_);
  FCP_RETURN_IF_ERROR(status_);
  // If status_ is OK, then response_code_ and response_headers_ are guaranteed
  // to have values.

  return InMemoryHttpResponse{*response_code_, content_encoding_, content_type_,
                              response_buffer_};
}

absl::StatusOr<InMemoryHttpResponse> PerformRequestInMemory(
    HttpClient& http_client, InterruptibleRunner& interruptible_runner,
    std::unique_ptr<http::HttpRequest> request, int64_t* bytes_received_acc,
    int64_t* bytes_sent_acc, Clock* clock, absl::BitGen* bit_gen,
    int32_t retry_max_attempts, int32_t retry_delay_ms) {
  // Note: we must explicitly instantiate a vector here as opposed to passing an
  // initializer list to PerformRequestsInMemory, because initializer lists do
  // not support move-only values.
  std::vector<std::unique_ptr<http::HttpRequest>> requests;
  requests.push_back(std::move(request));
  FCP_ASSIGN_OR_RETURN(
      auto result, PerformMultipleRequestsInMemoryWithRetry(
                       http_client, interruptible_runner, std::move(requests),
                       bytes_received_acc, bytes_sent_acc, clock, bit_gen,
                       retry_max_attempts, retry_delay_ms));
  return std::move(result[0]);
}

absl::StatusOr<std::vector<absl::StatusOr<InMemoryHttpResponse>>>
PerformMultipleRequestsInMemoryWithRetry(
    HttpClient& http_client, InterruptibleRunner& interruptible_runner,
    std::vector<std::unique_ptr<http::HttpRequest>> requests,
    int64_t* bytes_received_acc, int64_t* bytes_sent_acc, Clock* clock,
    absl::BitGen* bit_gen, int32_t retry_max_attempts, int32_t retry_delay_ms) {
  if (retry_max_attempts == 0) {
    return PerformMultipleRequestsInMemory(http_client, interruptible_runner,
                                           std::move(requests),
                                           bytes_received_acc, bytes_sent_acc);
  }
  // clone requests so we can use them again.
  std::vector<std::unique_ptr<http::HttpRequest>> requests_copy;
  requests_copy.reserve(requests.size());
  // It would be more efficient to copy the requests on failure, e.g. by having
  // the http_client return the original request if a request could be retried,
  // but given the small body sizes and the small number of requests, this was
  // not worth the effort.
  for (auto& request : requests) {
    requests_copy.push_back(request->Clone());
  }

  std::vector<absl::StatusOr<InMemoryHttpResponse>> responses;

  FCP_ASSIGN_OR_RETURN(responses, PerformMultipleRequestsInMemory(
                                      http_client, interruptible_runner,
                                      std::move(requests_copy),
                                      bytes_received_acc, bytes_sent_acc));
  // Get indexes of responses that have a retry-able error
  std::vector<int> retry_indexes;
  for (int i = 0; i < responses.size(); i++) {
    if (!responses[i].ok() && IsRetryableError(responses[i].status().code())) {
      retry_indexes.push_back(i);
    }
  }
  int32_t retry_attempt = 1;
  while (!retry_indexes.empty() && retry_attempt <= retry_max_attempts) {
    // Create new vector of requests that had retry-able responses.
    std::vector<std::unique_ptr<http::HttpRequest>> retry_requests;
    retry_requests.reserve(retry_indexes.size());
    for (int index : retry_indexes) {
      retry_requests.push_back(requests[index]->Clone());
    }

    // Wait for retry window, using interruptible runner to correctly propagate
    // cancellation.
    FCP_RETURN_IF_ERROR(interruptible_runner.Run(
        [clock, bit_gen, retry_delay_ms, retry_attempt]() {
          clock->Sleep(GetRetryDelay(
              *bit_gen, absl::Milliseconds(retry_delay_ms), retry_attempt));
          return absl::OkStatus();
        },
        // Nothing to abort if we get interrupted here.
        []() {}));

    // then call PerformMultipleRequestsInMemory again with new vector of
    // requests
    std::vector<absl::StatusOr<InMemoryHttpResponse>> retry_responses;
    FCP_ASSIGN_OR_RETURN(
        retry_responses,
        PerformMultipleRequestsInMemory(http_client, interruptible_runner,
                                        std::move(retry_requests),
                                        bytes_received_acc, bytes_sent_acc));

    // replace responses from original requests with responses from retry.
    for (int i = 0; i < retry_responses.size(); i++) {
      responses[retry_indexes[i]] = retry_responses[i];
    }
    // Update retry_indexes with new indexes of retry-able responses.
    retry_indexes.clear();
    for (int i = 0; i < responses.size(); i++) {
      if (!responses[i].ok() &&
          IsRetryableError(responses[i].status().code())) {
        retry_indexes.push_back(i);
      }
    }
    retry_attempt++;
  }
  return responses;
}

absl::StatusOr<std::vector<absl::StatusOr<InMemoryHttpResponse>>>
FetchResourcesInMemory(HttpClient& http_client,
                       InterruptibleRunner& interruptible_runner,
                       const std::vector<UriOrInlineData>& resources,
                       int64_t* bytes_received_acc, int64_t* bytes_sent_acc,
                       cache::ResourceCache* resource_cache, Clock* clock,
                       absl::BitGen* bit_gen, int32_t retry_max_attempts,
                       int32_t retry_delay_ms) {
  // Each resource may have the data already available (by having been included
  // in a prior response inline), or may need to be fetched.

  // We'll create an 'accessor' for each resource, providing access to that
  // resource's data by the end of this function (either the fetched data, or
  // the inline data). Additionally, this struct will contain the
  // client_cache_id and max_age for the resource if the resource should be put
  // in the cache. If the resource should not be put in the cache,
  // client_cache_id will be an empty string.
  struct AccessorAndCacheMetadata {
    std::function<absl::StatusOr<InMemoryHttpResponse>()> accessor;
    std::string client_cache_id;
    absl::Duration max_age;
  };
  std::vector<AccessorAndCacheMetadata> response_accessors;

  // We'll compile HttpRequest instances for those resources that do need to be
  // fetched, then we'll fire them off all at once, and then we'll gather their
  // responses once all requests have finished.
  std::vector<std::unique_ptr<http::HttpRequest>> http_requests;
  std::vector<absl::StatusOr<InMemoryHttpResponse>> http_responses;
  bool caching_enabled = resource_cache != nullptr;

  for (const UriOrInlineData& resource : resources) {
    if (!resource.uri().uri.empty()) {
      // If the resource has a cache_id, try getting it out of the cache. If any
      // condition happens outside the happy path, fetch the resource normally.
      if (caching_enabled && !resource.uri().client_cache_id.empty()) {
        absl::StatusOr<absl::Cord> cached_resource =
            TryGetResourceFromCache(resource.uri().client_cache_id,
                                    resource.uri().max_age, *resource_cache);
        if (cached_resource.ok()) {
          // Resource was successfully fetched from the cache, so we do not set
          // the client_cache_id or the max_age.
          response_accessors.push_back({.accessor =
                                            [cached_resource]() {
                                              return InMemoryHttpResponse{
                                                  kHttpOk, "", kOctetStream,
                                                  *cached_resource};
                                            },
                                        .client_cache_id = "",
                                        .max_age = absl::ZeroDuration()});
          continue;
        }
      }
      // If the resource URI is set, then create a request to fetch the data for
      // it, and point the accessor at the slot in http_responses where that
      // request's response will eventually live.
      FCP_ASSIGN_OR_RETURN(std::unique_ptr<http::HttpRequest> request,
                           InMemoryHttpRequest::Create(
                               resource.uri().uri, HttpRequest::Method::kGet,
                               {}, "", /*use_compression=*/
                               false));
      http_requests.push_back(std::move(request));
      int64_t response_index = http_requests.end() - http_requests.begin() - 1;
      auto response_accessing_fn = [&http_responses, response_index]() {
        return std::move(http_responses.at(response_index));
      };
      if (caching_enabled) {
        // We didn't load the resource from the cache, so set the
        // client_cache_id and max_age in the response_accessor.
        response_accessors.push_back(
            {.accessor = response_accessing_fn,
             .client_cache_id = resource.uri().client_cache_id,
             .max_age = resource.uri().max_age});
      } else {
        response_accessors.push_back({.accessor = response_accessing_fn});
      }
    } else {
      // The data is available inline. Make the accessor just return a "fake"
      // successful HTTP response (that way the caller can have unified error
      // handling logic and doesn't have to know whether a resource was truly
      // fetched via HTTP or not). Because the inline_data field is an
      // absl::Cord, making a copy of it should be very cheap.
      response_accessors.push_back({.accessor = [resource]() {
        std::string content_type(kOctetStream);
        switch (resource.inline_data().compression_format) {
          case UriOrInlineData::InlineData::CompressionFormat::kUncompressed:
            break;
          case UriOrInlineData::InlineData::CompressionFormat::kGzip:
            absl::StrAppend(&content_type, kClientDecodedGzipSuffix);
            break;
        }
        return InMemoryHttpResponse{kHttpOk, "", content_type,
                                    resource.inline_data().data};
      }});
    }
  }

  // Perform the requests.
  auto resource_fetch_result = PerformMultipleRequestsInMemoryWithRetry(
      http_client, interruptible_runner, std::move(http_requests),
      bytes_received_acc, bytes_sent_acc, clock, bit_gen, retry_max_attempts,
      retry_delay_ms);
  // Check whether issuing the requests failed as a whole (generally indicating
  // a programming error).
  FCP_RETURN_IF_ERROR(resource_fetch_result);
  http_responses = std::move(*resource_fetch_result);

  // Compile the result vector by getting each resource's response using the
  // corresponding accessor.
  // Note that the order of results returned corresponds to the order of
  // resources in the vector we originally received.
  std::vector<absl::StatusOr<InMemoryHttpResponse>> result;
  result.reserve(response_accessors.size());
  for (const auto& response_accessor : response_accessors) {
    absl::StatusOr<InMemoryHttpResponse> response =
        response_accessor.accessor();
    if (response.ok()) {
      bool encoded_with_gzip = absl::EndsWithIgnoreCase(
          response->content_type, kClientDecodedGzipSuffix);
      if (!response_accessor.client_cache_id.empty()) {
        TryPutResourceInCache(response_accessor.client_cache_id, response->body,
                              encoded_with_gzip, response_accessor.max_age,
                              *resource_cache)
            .IgnoreError();
      }
      if (encoded_with_gzip) {
        std::string response_body_temp(response->body);
        // We're going to overwrite the response body with the decoded
        // contents shortly, no need to keep an extra copy of it in memory.
        response->body.Clear();
        absl::StatusOr<absl::Cord> decoded_response_body =
            UncompressWithGzip(response_body_temp);
        if (!decoded_response_body.ok()) {
          response = decoded_response_body.status();
        } else {
          response->body = *std::move(decoded_response_body);
        }
      }
    }
    result.push_back(response);
  }
  return result;
}

}  // namespace http
}  // namespace client
}  // namespace fcp
