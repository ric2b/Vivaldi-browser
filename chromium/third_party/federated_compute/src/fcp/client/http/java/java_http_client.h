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
#ifndef FCP_CLIENT_HTTP_JAVA_JAVA_HTTP_CLIENT_H_
#define FCP_CLIENT_HTTP_JAVA_JAVA_HTTP_CLIENT_H_

#include <jni.h>

#include <memory>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/java/jni.pb.h"

namespace fcp {
namespace client {
namespace http {
namespace java {

// An `HttpClient` implementation that performs HTTP requests by calling back
// into Java over JNI, and lets the Java code issue the actual requests.
class JavaHttpClient : public HttpClient {
 public:
  JavaHttpClient(JavaVM* jvm, jobject java_http_client);
  ~JavaHttpClient() override;

  ABSL_MUST_USE_RESULT
  std::unique_ptr<fcp::client::http::HttpRequestHandle> EnqueueRequest(
      std::unique_ptr<fcp::client::http::HttpRequest> request) override;

  absl::Status PerformRequests(
      std::vector<std::pair<fcp::client::http::HttpRequestHandle*,
                            fcp::client::http::HttpRequestCallback*>>
          requests) override;

 private:
  JavaVM* const jvm_;
  jobject jthis_;
  jmethodID enqueue_request_id_;
  jmethodID perform_requests_id_;
  jmethodID close_id_;
};

// An HttpResponse implementation that is based on data provided via the JNI
// callbacks.
class JavaHttpResponse : public fcp::client::http::HttpResponse {
 public:
  JavaHttpResponse() {}

  // Populates the response data based on the given JNI proto.
  void PopulateFromProto(const JniHttpResponse& response_proto) {
    code_ = response_proto.code();
    headers_.clear();
    for (const JniHttpHeader& header : response_proto.headers()) {
      headers_.push_back(Header(header.name(), header.value()));
    }
  }

  int code() const override { return code_; }
  const HeaderList& headers() const override { return headers_; }

 private:
  int code_ = -1;
  HeaderList headers_;
};

// An `HttpRequestHandle` implementation that performs HTTP requests by calling
// back into Java over JNI, and lets the Java code issue the actual requests.
class JavaHttpRequestHandle : public fcp::client::http::HttpRequestHandle {
 public:
  // Utility for extracting a JavaHttpRequestHandle pointer from a Java jlong.
  // When a JavaHttpRequestHandle object is constructed, it will 'attach' itself
  // to the corresponding Java object by writing its address to its
  // `nativeHandle` field. When Java then calls back into our C++ code, it
  // passes that address back to us, and this function can then be used to turn
  // the address into a proper pointer.
  static JavaHttpRequestHandle* FromJlong(jlong ptr);

  JavaHttpRequestHandle(
      JavaVM* jvm, jobject java_http_request_handle,
      std::unique_ptr<fcp::client::http::HttpRequest> request);

  ~JavaHttpRequestHandle() override ABSL_LOCKS_EXCLUDED(lock_);

  // --- HttpRequestHandle methods
  fcp::client::http::HttpRequestHandle::SentReceivedBytes
  TotalSentReceivedBytes() const override;
  void Cancel() override ABSL_LOCKS_EXCLUDED(lock_);

  // --- JNI handling methods
  jboolean ReadRequestBody(JNIEnv* env, jbyteArray buffer,
                           jlong requested_bytes, jintArray actual_bytes_read)
      ABSL_LOCKS_EXCLUDED(lock_);

  jboolean OnResponseStarted(JNIEnv* env, jbyteArray response_proto)
      ABSL_LOCKS_EXCLUDED(lock_);
  void OnResponseError(JNIEnv* env, jbyteArray status_proto)
      ABSL_LOCKS_EXCLUDED(lock_);

  jboolean OnResponseBody(JNIEnv* env, jbyteArray buffer, jint buffer_offset,
                          jint bytes_available) ABSL_LOCKS_EXCLUDED(lock_);
  void OnResponseBodyError(JNIEnv* env, jbyteArray status_proto)
      ABSL_LOCKS_EXCLUDED(lock_);
  void OnResponseCompleted() ABSL_LOCKS_EXCLUDED(lock_);

  // --- Internal methods.
  // Returns the (possibly empty/default-constructed) response object for this
  // handle. This object is populated with actual data after `OnResponseStarted`
  // is called.
  const JavaHttpResponse& response() const ABSL_LOCKS_EXCLUDED(lock_);

  // Returns the `HttpRequestCallback` associated with this handle, or a nullptr
  // if no callback is associated with it yet (i.e. no `PerformRequests` call
  // was made yet with this handle).
  fcp::client::http::HttpRequestCallback* callback() const
      ABSL_LOCKS_EXCLUDED(lock_);

  // Associates an `HttpRequestCallback` with this handle, or returns an
  // `INVALID_ARGUMENT` error if one was already associated with it (indicating
  // the handle is being used with more than one `PerformRequests` call, which
  // is not allowed).
  absl::Status SetCallback(fcp::client::http::HttpRequestCallback* callback)
      ABSL_LOCKS_EXCLUDED(lock_);

  // Returns the JNI reference for the Java handle object that corresponds to
  // this C++ handle object.
  jobject GetJobject() { return jthis_; }

 private:
  JavaVM* const jvm_;
  jobject jthis_;
  jmethodID get_total_sent_received_bytes_id_;
  jmethodID close_id_;
  jfieldID native_handle_id_;

  const std::unique_ptr<fcp::client::http::HttpRequest> request_;

  // A note on synchronization: most of JavaHttpRequestHandle's methods may be
  // called from any thread (incl. the JNI callback methods, which are
  // guaranteed to never be called concurrently from more than one thread, but
  // which for which subsequent calls may occur on different threads). This
  // object also has mutable state (e.g. `response_` which only gets populated
  // with data once the `OnResponseStarted` callback is invoked).
  //
  // We use this mutex to ensure that each JNI callback invocation observes the
  // effects of every prior callback invocation. This means we don't have to
  // rely on the Java side of the implementation for thread safety (even though
  // the Java side likely also implements its own synchronization as well).
  mutable absl::Mutex lock_;
  JavaHttpResponse response_ ABSL_GUARDED_BY(lock_);
  fcp::client::http::HttpRequestCallback* callback_ ABSL_GUARDED_BY(lock_) =
      nullptr;
  bool performed_ ABSL_GUARDED_BY(lock_) = false;
};

}  // namespace java
}  // namespace http
}  // namespace client
}  // namespace fcp
#endif  // FCP_CLIENT_HTTP_JAVA_JAVA_HTTP_CLIENT_H_
