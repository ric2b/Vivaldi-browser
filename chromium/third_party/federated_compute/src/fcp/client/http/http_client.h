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
#ifndef FCP_CLIENT_HTTP_HTTP_CLIENT_H_
#define FCP_CLIENT_HTTP_HTTP_CLIENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace fcp {
namespace client {
namespace http {

using Header = std::pair<std::string, std::string>;
// This is a vector of pairs and not a map since multiple request headers with
// the same name are allowed (see RFC2616 section 4.2).
using HeaderList = std::vector<Header>;

class HttpRequest;          // forward declaration
class HttpRequestCallback;  // forward declaration
class HttpRequestHandle;    // forward declaration
class HttpResponse;         // forward declaration

// An interface that allows the callers to make HTTP requests and receive their
// responses.
//
// Platforms will be required to pass an instance of this class to
// `RunFederatedComputation(...)`, and such instances must remain alive for at
// least the duration of those calls.
//
// Instances of this class must support being called from any thread. In such
// cases implementations should ideally handle multiple requests in parallel, at
// least up to an implementation-defined max number of parallel requests (i.e.
// ideally two `PerformRequests` calls on different threads can be handled in
// parallel, rather than having the 2nd call block until the 1st call is
// completed).
//
// Besides the requirements documented further below, the following high-level
// behavior is required of any `HttpClient` implementation:
// - Underlying protocols:
//   * Implementations must support at least HTTP/1.1 over TLS 1.2.
//   * Implementations are also allowed to serve requests using HTTP/2, QUIC or
//     other newer protocols.
//   * Implementations must support both IPv4 and IPv6 (but they are allowed to
//     fall back to IPv4).
// - Certificate validation:
//   * Implementations are responsible for TLS certificate validation and for
//     maintaining an up-to-date set of root certificates as well as an
//     up-to-date HTTP/TLS implementation.
//   * Implementations should not include user-added CAs, and should consider
//     restricting the set of CAs further to only those needed for connecting to
//     the expected endpoints.
// - Cookies:
//   * Implementations must not supply any cookies in requests (beyond those
//     that may be specified in the `HttpRequest::headers()` method).
//   * Implementations must not store any cookies returned by the server.
//   * Instead, they must return any server-specified "Set-Cookie" response
//     header via `HttpResponse::headers()`.
// - Redirects:
//   *  Implementations must follow HTTP redirects responses, up to an
//      implementation-defined maximum.
//   *  In such cases the response headers & body returned via the interfaces
//      below should be those of the final response.
//   *  See `HttpRequestCallback` docs below for more details.
// - Caching:
//   *  Implementations should not implement a cache as it is expected that
//      naive HTTP-level caching will not be effective (and since a cache may
//      ultimately be implemented over this interface, in the Federated Compute
//      library itself).
//   *  If implementations do implement one, however, they are expected to abide
//      by the standard HTTP caching rules (see the `HttpRequest::Method` docs
//      for more details).
// - Response body decompression & decoding:
//   *  If no "Accept-Encoding" request header is explicitly specified in
//      `HttpRequest::headers()`, then implementations must advertise an
//      "Accept-Encoding" request header themselves whose value includes at
//      least "gzip" (additional encodings are allowed to be specified in
//      addition to "gzip"), and must transparently decompress any compressed
//      server responses before returning the data via these interfaces.
//      Implementations are also allowed to advertise/support additional
//      encoding methods.
//   *  In such cases where no "Accept-Encoding" header is specified,
//      implementations must remove the "Content-Encoding" and
//      "Content-Length" headers from headers returned via
//      `HttpResponse::headers()` (since those wouldn't reflect the payload
//      delivered via this interface).
//   *  However, if an "Accept-Encoding" request header *is* explicitly
//      specified, then implementations must use that header verbatim and they
//      must not decompress the response (even if they natively support the
//      compression method), and they must leave the "Content-Encoding" and
//      "Content-Length" headers intact.
//   *  This ensures that the caller of this interface can take full control of
//      the decompression and/or choose to store decompressed payloads on disk
//      if it so chooses.
//   *  Implementations must transparently decode server responses served with
//      "Transfer-Encoding: chunked". In such cases they must remove the
//      "Transfer-Encoding" response header.
// - Request body compression & encoding:
//   *  If implementations receive a "Content-Encoding" request header, this
//      means that the request body stream they receive has already been
//      compressed. The implementation must leave the header and request body
//      intact in such cases (i.e. not re-compress it).
//   *  If implementations receive a "Content-Length" request header, they must
//      use it verbatim and they should then assume that the request body will
//      be of exactly that size.
//   *  If they do not receive such a header then they must use the
//      "Transfer-encoding: chunked" mechanism to transmit the request body
//      (i.e. they shouldn't specify a "Content-Length" header and they should
//      transmit the body in chunks), or use an equivalent method of streaming
//      the data (such as HTTP/2's data streaming).
class HttpClient {
 public:
  virtual ~HttpClient() = default;

  // Enqueues an HTTP request, without starting it yet. To start the request the
  // `HttpRequestHandle` must be passed to `PerformRequests`. Each
  // `HttpRequestHandle` must be passed to at most one `PerformRequests` call.
  //
  // The `HttpClient` implementation assumes ownership of the `HttpRequest`
  // object, and the implementation must delete the object when the
  // `HttpRequestHandle` is deleted.
  ABSL_MUST_USE_RESULT
  virtual std::unique_ptr<HttpRequestHandle> EnqueueRequest(
      std::unique_ptr<HttpRequest> request) = 0;

  // Performs the given requests. Results will be returned to each
  // corresponding `HttpRequestCallback` while this method is blocked. This
  // method must block until all requests have finished or have been cancelled,
  // and until all corresponding request callbacks have returned.
  //
  // By decoupling the enqueueing and starting of (groups of) requests,
  // implementations may be able to handle concurrent requests more optimally
  // (e.g. by issuing them over a shared HTTP connection). Having separate
  // per-request `HttpRequestHandle` objects also makes it easier to support
  // canceling specific requests, releasing resources for specific requests,
  // accessing stats for specific requests, etc.
  //
  // The `HttpRequestHandle` and `HttpRequestCallback` instances must outlive
  // the call to `PerformRequests`, but may be deleted any time after this call
  // has returned.
  //
  // Returns an `INVALID_ARGUMENT` error if a `HttpRequestHandle` was previously
  // already passed to another `PerformRequests` call, or if an
  // `HttpRequestHandle`'s `Cancel` method was already called before being
  // passed to this call.
  virtual absl::Status PerformRequests(
      std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>>
          requests) = 0;
};

// An HTTP request for a single resource. Implemented by the caller of
// `HttpClient`.
//
// Once instances are passed to `EnqueueRequest`, their lifetime is managed by
// the `HttpClient` implementation. Implementations must tie the `HttpRequest`
// instance lifetime to the lifetime of the `HttpRequestHandle` they return
// (i.e. they should delete the `HttpRequest` from the `HttpRequestHandle`
// destructor).
//
// Methods of this class may get called from any thread (and subsequent calls
// are not required to all happen on the same thread).
class HttpRequest {
 public:
  // Note: the request methods imply a set of standard request properties such
  // as cacheability, safety, and idempotency. See
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods.
  //
  // The caller of `HttpClient` may implement its own caching layer in the
  // future, so implementations are not expected to cache cacheable requests
  // (although they are technically allowed to).
  //
  // Implementations should not automatically retry requests, even if the
  // request method implies it is safe or idempotent. The caller of `HttpClient`
  // will own the responsibility for retrying requests.
  enum class Method { kHead, kGet, kPost, kPut, kPatch, kDelete };

  // Must not be called until any corresponding `HttpRequestHandle` has been
  // deleted.
  virtual ~HttpRequest() = default;

  // The URI to request. Will always have an "https://" scheme (but this may be
  // extended in the future).
  virtual absl::string_view uri() const = 0;

  // The HTTP method to use for this request.
  virtual Method method() const = 0;

  // Extra request headers to include with this request, in addition to any
  // headers specified by the `HttpClient` implementation.
  //
  // See the `HttpClient` comment for the expected behavior w.r.t. a few
  // specific headers.
  virtual const HeaderList& extra_headers() const = 0;

  // Returns true if the request has a request body (which can be read using
  // `ReadBody`). If the request body payload size is known ahead of time, then
  // the "Content-Length" header will be set in `extra_headers()`. If it isn't
  // known yet then the `HttpClient` implementation should use the
  // "Transfer-Encoding: chunked" encoding to transmit the request body to the
  // server in chunks (or use an equivalent method of streaming the data, e.g.
  // if the connection uses HTTP/2). See the `HttpClient` comment for more
  // details.
  virtual bool HasBody() const = 0;

  // HttpRequests that up to `requested` bytes of the request body be read into
  // `buffer`, and that the actual amount of bytes read is returned. The caller
  // retains ownership of the buffer.
  //
  // Callees must return at least 1 byte, but may otherwise return less than the
  // requested amount of data, if more data isn't available yet. Callees should
  // return `OUT_OF_RANGE` when the end of data has been reached, in which case
  // `buffer` should not be modified.
  //
  // Callees should return data ASAP, as delaying this for too long may cause
  // the network stream to fall idle/run out of data to transmit.
  //
  // May also return other errors, in which case the request will be ended and
  // `HttpRequestCallback::OnResponseError` will be called with the same error.
  virtual absl::StatusOr<int64_t> ReadBody(char* buffer, int64_t requested) = 0;

  // Returns a copy of this request, with any internal state reset to the same
  // state as if this request was just created, including resetting the read
  // cursor to the beginning of the request body.
  virtual std::unique_ptr<HttpRequest> Clone() const = 0;
};

// A handle to a pending `HttpRequest`, allowing a caller of `HttpClient` to
// access stats for the request or to cancel ongoing requests. Implemented by
// the `HttpClient` implementer.
//
// The lifetimes of instances of this class are owned by the caller of
// `HttpClient`.
//
// Methods of this class may get called from any thread (and subsequent calls
// are not required to all happen on the same thread).
class HttpRequestHandle {
 public:
  // When this is called, `HttpClient` implementations should delete all their
  // owned resources as well as the associated `HttpRequest`.
  virtual ~HttpRequestHandle() = default;

  // The total amount of data sent/received over the network for this request up
  // to this point. These numbers should reflect as close as possible the amount
  // of bytes sent "over the wire". This means, for example, that if the data is
  // compressed or if a `Transfer-Encoding` is used, the numbers should reflect
  // the compressed and/or encoded size of the data (if the implementation is
  // able to account for that). Implementations are allowed to account for the
  // overhead of TLS encoding in these numbers, but are not required to (since
  // many HTTP libraries also do not provide stats at that level of
  // granularity).
  //
  // If the request was served from a cache then this should reflect only the
  // actual bytes sent over the network (e.g. 0 if returned from disk directly,
  // or if a cache validation request was sent only those bytes used by the
  // validation request/response).
  //
  // If the request involved redirects, the numbers returned here should include
  // the bytes sent/received for those redirects, if the implementation supports
  // this. Otherwise they are allowed to reflect only the final
  // request/response's bytes sent/received.
  //
  // Implementations should strive to return as up-to-date numbers are possible
  // from these methods (e.g. ideally the 'sent' number should reflect the
  // amount of request body data that has been uploaded so far, even if the
  // upload hasn't completed fully yet; similarly the 'received' number should
  // reflect the amount of response body data received so far, even if the
  // response hasn't been fully received yet).
  //
  // The numbers returned here are not required to increase monotonically
  // between each call to the method. E.g. implementations are allowed to return
  // best-available estimates while the request is still in flight, and then
  // revise the numbers down to a more accurate number once the request has been
  // completed.
  struct SentReceivedBytes {
    int64_t sent_bytes;
    int64_t received_bytes;
  };
  virtual SentReceivedBytes TotalSentReceivedBytes() const = 0;

  // Used to indicate that the request should be cancelled and that
  // implementations may release resources associated with this request (e.g.
  // the socket used by the request).
  //
  // Callers are still only allowed to delete this instance once after any
  // corresponding `PerformRequests()` call has completed, and not before.
  //
  // If a `PerformRequests` call is ongoing for this handle, then the
  // corresponding `HttpRequestCallback` instance may still receive further
  // method invocations after this call returns (e.g. because an invocation may
  // already have been in flight).
  //
  // If a `PerformRequests` call is ongoing for this handle, and if the
  // `HttpRequestCallback::OnResponseStarted` method was not called yet, then
  // the `HttpRequestCallback::OnResponseError` method must be called with
  // status `CANCELLED`.
  //
  // Otherwise, if a `PerformRequests` call is ongoing for this handle, and if
  // the `HttpRequestCallback::OnResponseCompleted` method was not called yet,
  // then the `HttpRequestCallback::OnResponseBodyError` method must be called
  // with status `CANCELLED`.
  virtual void Cancel() = 0;
};

// The callback interface that `HttpClient` implementations must use to deliver
// the response to a `HttpRequest`. Implemented by the caller of `HttpClient`.
//
// The lifetimes of instances of this class are owned by the caller of
// `HttpClient`. Instances must remain alive for at least as long as their
// corresponding `PerformRequests` call.
//
// Methods of this class may get called from any thread (incl. concurrently),
// but callers of this class must always call the callback methods for a
// specific `HttpRequest` in the order specified in each method's documentation.
// Implementations of this class therefore likely should use internal
// synchronization.
//
// For example, a call to `OnResponseBody` for a given `HttpRequest` A will
// always be preceded by a completed call to `OnResponseStarted` for that same
// request A. However, callbacks for different `HttpRequest` objects may happen
// concurrently, so for example, `OnResponseStarted` may be called concurrently
// for two different requests A and B. This latter scenario means that if the
// same `HttpRequestCallback` object is used to handle callbacks for both
// requests, then the object has to handle concurrent calls correctly.
class HttpRequestCallback {
 public:
  virtual ~HttpRequestCallback() = default;

  // Called when the final HTTP response headers have been received (i.e. after
  // any redirects have been followed but before the response body may have been
  // received fully) for the given `HttpRequest`. The response data can be
  // accessed via the given `HttpResponse`, which will remain alive for the
  // lifetime of the corresponding `HttpRequestHandle`.
  //
  // Note that all the data in the `HttpResponse` object should reflect the
  // last/final response (i.e. it shouldn't reflect any already-followed
  // redirects).
  //
  // If the response has a body then after this method is called
  // `OnResponseBody` will be called one or more times to deliver the response
  // body (or `OnResponseBodyError` if an error occurs).
  //
  // Note that responses with an HTTP status code other than 200 ("OK") may
  // still have response bodies, and implementations must deliver these via the
  // `OnResponseBody` callback, just as they should for a successful response.
  //
  // If this method returns an error then the `HttpClient` implementation should
  // consider the `HttpRequest` canceled. No further methods must be called on
  // this `HttpRequestCallback` instance for the given `HttpRequest` after in
  // this case.
  virtual absl::Status OnResponseStarted(const HttpRequest& request,
                                         const HttpResponse& response) = 0;

  // Called when the request encountered an error or timed out, before receiving
  // the response headers completely. No further methods must be called on this
  // `HttpRequestCallback` instance for the given `HttpRequest` after this
  // method is called.
  //
  // If the implementation is able to discern that the error may have been
  // transient, they should return `UNAVAILABLE`.
  //
  // If more than the implementation's defined max number of redirects occurred
  // (without reaching the final response), then implementations should return
  // `OUT_OF_RANGE` here.
  //
  // If the implementation hit an implementation-specific timeout (even though
  // implementations are discouraged from imposing such timeouts), then this
  // should be `DEADLINE_EXCEEDED`.
  //
  // If the `HttpRequestHandle::Cancel` method was called before
  // `OnResponseStarted` was called for the given `HttpRequest`, then this
  // method will be called with a `CANCELLED` status.
  //
  // If the request's `HttpRequest::ReadBody` returned an unexpected error,
  // then method will be called with that error.
  virtual void OnResponseError(const HttpRequest& request,
                               const absl::Status& error) = 0;

  // Called (possibly multiple times per request) when a block of response data
  // is available in `data`. This method must only be called after
  // `OnResponseStarted` was called for the given `HttpRequest`.
  //
  // Callees must process the data ASAP, as delaying this for too long may
  // prevent additional data from arriving on the network stream.
  //
  // If this method returns an error then the `HttpClient` implementation should
  // consider the `HttpRequest` canceled. No further methods must be called on
  // this `HttpRequestCallback` instance for the given `HttpRequest` after in
  // this case.
  virtual absl::Status OnResponseBody(const HttpRequest& request,
                                      const HttpResponse& response,
                                      absl::string_view data) = 0;

  // Called when the request encountered an error or timed out while receiving
  // the response body (i.e. after `OnResponseStarted` was called). No further
  // methods must be called on this `HttpRequestCallback` instance for the given
  // `HttpRequest` after this method is called.
  //
  // If the implementation is able to discern that the error may have been
  // transient, they should return `UNAVAILABLE`.
  //
  // If the implementation hit an implementation-specific timeout (even though
  // implementations are discouraged from imposing such timeouts), then this
  // should be `DEADLINE_EXCEEDED`.
  //
  // If the `HttpRequestHandle::Cancel` method was called before
  // `OnResponseCompleted` was called for the given `HttpRequest`, then this
  // method will be called with a `CANCELLED` status.
  virtual void OnResponseBodyError(const HttpRequest& request,
                                   const HttpResponse& response,
                                   const absl::Status& error) = 0;

  // Called when the request has completed successfully (i.e. the response
  // headers were delivered, and if there was a response body then it was also
  // delivered successfully). Must not be called if one of the error callbacks
  // was already called for the given `HttpRequest`, and no further methods must
  // be called on this `HttpRequestCallback` instance for the given
  // `HttpRequest` after this method is called.
  virtual void OnResponseCompleted(const HttpRequest& request,
                                   const HttpResponse& response) = 0;
};

// A response to a given `HttpRequest`. Implemented by the `HttpClient`
// implementer.
//
// The lifetimes of instances of this class are managed by the `HttpClient`
// implementer. Instances of this class must remain alive for at least long as
// the corresponding `HttpRequestHandle` is alive.
//
// Note that all the data in this object should be for the last/final response.
// I.e. any responses corresponding to redirects should not be reflected here.
class HttpResponse {
 public:
  virtual ~HttpResponse() = default;

  // The response code returned by the server (e.g. 200).
  virtual int code() const = 0;

  // The response headers. Implementations are allowed to either coalesce
  // repeated headers using commas (as per RFC2616 section 4.2), or to return
  // them as separate entries.
  //
  // See `HttpClient` comment for the expected behavior w.r.t. a few specific
  // headers.
  virtual const HeaderList& headers() const = 0;
};

}  // namespace http
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_HTTP_HTTP_CLIENT_H_
