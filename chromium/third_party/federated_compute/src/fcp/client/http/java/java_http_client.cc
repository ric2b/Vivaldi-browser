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
#include "fcp/client/http/java/java_http_client.h"

#include <jni.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/rpc/status.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/http_client_util.h"
#include "fcp/client/http/java/jni.pb.h"
#include "fcp/jni/jni_util.h"

namespace fcp {
namespace client {
namespace http {
namespace java {

using fcp::client::http::HttpRequestCallback;
using fcp::client::http::HttpRequestHandle;
using fcp::jni::JavaFieldSig;
using fcp::jni::JavaMethodSig;
using fcp::jni::LocalRefDeleter;
using fcp::jni::ParseProtoFromJByteArray;
using fcp::jni::ScopedJniEnv;
using fcp::jni::SerializeProtoToJByteArray;

namespace {

// The Java method signatures for the Java class corresponding to the C++
// `JavaHttpClient` class.
struct JavaHttpClientClassDesc {
  static constexpr JavaMethodSig kEnqueueRequest = {
      "enqueueRequest",
      "([B)Lcom/google/fcp/client/http/HttpClientForNative$HttpRequestHandle;"};
  static constexpr JavaMethodSig kPerformRequests = {"performRequests",
                                                     "([Ljava/lang/Object;)[B"};
  static constexpr JavaMethodSig kClose = {"close", "()V"};
};

// The Java method and field signatures for the Java class corresponding to the
// C++ `JavaHttpRequestHandle` class.
struct JavaHttpRequestHandleClassDesc {
  static constexpr JavaMethodSig kGetTotalSentReceivedBytes = {
      "getTotalSentReceivedBytes", "()[B"};
  static constexpr JavaMethodSig kClose = {"close", "()V"};
  static constexpr JavaFieldSig kNativeHandle = {"nativeHandle", "J"};
};

JniHttpMethod ConvertHttpClientMethodToProtoMethod(
    fcp::client::http::HttpRequest::Method method) {
  switch (method) {
    case fcp::client::http::HttpRequest::Method::kHead:
      return JniHttpMethod::HTTP_METHOD_HEAD;
    case fcp::client::http::HttpRequest::Method::kGet:
      return JniHttpMethod::HTTP_METHOD_GET;
    case fcp::client::http::HttpRequest::Method::kPost:
      return JniHttpMethod::HTTP_METHOD_POST;
    case fcp::client::http::HttpRequest::Method::kPut:
      return JniHttpMethod::HTTP_METHOD_PUT;
    case fcp::client::http::HttpRequest::Method::kPatch:
      return JniHttpMethod::HTTP_METHOD_PATCH;
    case fcp::client::http::HttpRequest::Method::kDelete:
      return JniHttpMethod::HTTP_METHOD_DELETE;
    default:
      return JniHttpMethod::HTTP_METHOD_UNKNOWN;
  }
}

// Calls JNIEnv::GetMethodID and ensures that its return value is valid.
jmethodID GetMethodIdOrAbort(JNIEnv& env, jclass clazz, JavaMethodSig method) {
  jmethodID id = env.GetMethodID(clazz, method.name, method.signature);
  FCP_CHECK(id != nullptr);
  return id;
}
}  // namespace

JavaHttpClient::JavaHttpClient(JavaVM* jvm, jobject java_http_client)
    : jvm_(jvm) {
  ScopedJniEnv scoped_env(jvm_);
  JNIEnv* env = scoped_env.env();
  jthis_ = env->NewGlobalRef(java_http_client);
  FCP_CHECK(jthis_ != nullptr);
  // We get the class from the jobject here instead of looking it up by name in
  // the classloader because we may be using this class from a non java thread
  // that has been attached to the jvm, and thus has a classloader with only
  // "system" classes.
  jclass java_http_client_class = env->GetObjectClass(java_http_client);
  FCP_CHECK(java_http_client_class != nullptr);
  LocalRefDeleter java_http_client_class_deleter(env, java_http_client_class);

  // Look up the method IDs for the Java methods we'll call later on.
  enqueue_request_id_ = GetMethodIdOrAbort(
      *env, java_http_client_class, JavaHttpClientClassDesc::kEnqueueRequest);
  perform_requests_id_ = GetMethodIdOrAbort(
      *env, java_http_client_class, JavaHttpClientClassDesc::kPerformRequests);
  close_id_ = GetMethodIdOrAbort(*env, java_http_client_class,
                                 JavaHttpClientClassDesc::kClose);
}

JavaHttpClient::~JavaHttpClient() {
  ScopedJniEnv scoped_env(jvm_);
  JNIEnv* env = scoped_env.env();

  // We call the Java close() method when the destructor is invoked. This gives
  // the Java code a chance to clean things up on its side, if it needs to.
  env->CallVoidMethod(jthis_, close_id_);
  FCP_CHECK(!env->ExceptionCheck());

  // Delete the global reference to the Java object.
  env->DeleteGlobalRef(jthis_);
}

std::unique_ptr<HttpRequestHandle> JavaHttpClient::EnqueueRequest(
    std::unique_ptr<fcp::client::http::HttpRequest> request) {
  // Convert the `HttpRequest`'s info into a proto we can serialize and pass
  // over the JNI boundary.
  JniHttpRequest request_proto;
  request_proto.set_uri(std::string(request->uri()));
  request_proto.set_method(
      ConvertHttpClientMethodToProtoMethod(request->method()));
  for (auto request_header : request->extra_headers()) {
    JniHttpHeader* header = request_proto.add_extra_headers();
    header->set_name(request_header.first);
    header->set_value(request_header.second);
  }
  request_proto.set_has_body(request->HasBody());

  // Call into Java to create the Java request handle object that will cooperate
  // with our C++ `JavaHttpRequestHandle` object.
  ScopedJniEnv scoped_env(jvm_);
  JNIEnv* env = scoped_env.env();
  jbyteArray serialized_request_proto =
      SerializeProtoToJByteArray(env, request_proto);
  LocalRefDeleter serialized_request_proto_deleter(env,
                                                   serialized_request_proto);
  jobject java_http_request_handle = env->CallObjectMethod(
      jthis_, enqueue_request_id_, serialized_request_proto);
  FCP_CHECK(java_http_request_handle != nullptr);
  FCP_CHECK(!env->ExceptionCheck());

  // Create the C++ `JavaHttpRequestHandle` object (which will 'attach' itself
  // to the Java object by writing its address to the `nativeHandle` Java
  // field).
  auto result = std::make_unique<JavaHttpRequestHandle>(
      jvm_, java_http_request_handle, std::move(request));
  return result;
}

absl::Status JavaHttpClient::PerformRequests(
    std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>>
        generic_requests) {
  // We're about to kick off a group of requests. Each request has a matching
  // callback, as well as a corresponding Java object. To prepare for the
  // `performRequests` call into Java:
  // 1. Create an Object[] array, consisting of each request's corresponding
  //    Java object.
  // 2. For each request, register the callback that should be used for it.
  ScopedJniEnv scoped_env(jvm_);
  JNIEnv* env = scoped_env.env();

  // The object array we'll create will just be of type Object[]. The Java
  // code/JNI runtime will be in charge of downcasting the individual elements
  // back to its concrete Java HttpRequestHandle implementation class. This
  // avoids us having to try and look up the Java class (which can be difficult,
  // since we may be running on a thread with a ClassLoader containing only Java
  // system classes).
  jclass object_class = env->FindClass("java/lang/Object");
  FCP_CHECK(object_class != nullptr);
  FCP_CHECK(!env->ExceptionCheck());
  LocalRefDeleter object_class_deleter(env, object_class);

  // Create the Object[] array.
  jobjectArray request_handle_array = env->NewObjectArray(
      static_cast<jsize>(generic_requests.size()), object_class, nullptr);
  FCP_CHECK(request_handle_array != nullptr);
  FCP_CHECK(!env->ExceptionCheck());
  LocalRefDeleter request_handle_array_deleter(env, request_handle_array);

  // Populate the Object[] array with the Java objects corresponding to each
  // request, and register each callback with the `JavaHttpRequestHandle`.
  int i = 0;
  for (const auto& [generic_handle, callback] : generic_requests) {
    auto request_handle = static_cast<JavaHttpRequestHandle*>(generic_handle);
    env->SetObjectArrayElement(request_handle_array, i++,
                               request_handle->GetJobject());
    FCP_CHECK(!env->ExceptionCheck());
    FCP_RETURN_IF_ERROR(request_handle->SetCallback(callback));
  }

  // Call the Java `performRequests` method over JNI, passing it the Object[]
  // array.
  jbyteArray perform_requests_result =
      static_cast<jbyteArray>(env->CallObjectMethod(
          jthis_, perform_requests_id_, request_handle_array));
  FCP_CHECK(!env->ExceptionCheck());
  FCP_CHECK(perform_requests_result != nullptr);
  LocalRefDeleter perform_requests_result_deleter(env, perform_requests_result);

  // Convert the return value from Java to an absl::Status.
  return ConvertRpcStatusToAbslStatus(
      ParseProtoFromJByteArray<google::rpc::Status>(env,
                                                    perform_requests_result));
}

JavaHttpRequestHandle* JavaHttpRequestHandle::FromJlong(jlong ptr) {
  // If the Java code erroneously calls a JNI callback with a handle that has
  // already been destroyed, then `ptr` will be 0. We want to catch such bugs
  // early.
  FCP_CHECK(ptr != 0)
      << "cannot call JNI callback before enqueueRequest has been called";
  return reinterpret_cast<JavaHttpRequestHandle*>(ptr);
}

JavaHttpRequestHandle::JavaHttpRequestHandle(
    JavaVM* jvm, jobject java_http_request_handle,
    std::unique_ptr<fcp::client::http::HttpRequest> request)
    : jvm_(jvm), request_(std::move(request)) {
  ScopedJniEnv scoped_env(jvm_);
  JNIEnv* env = scoped_env.env();
  jthis_ = env->NewGlobalRef(java_http_request_handle);
  FCP_CHECK(jthis_ != nullptr);

  // We get the class from the jobject here instead of looking up by name in
  // the classloader because we may be using this class from a non java thread
  // that has been attached to the jvm, and thus has a classloader with only
  // "system" classes.
  jclass java_http_request_handle_class =
      env->GetObjectClass(java_http_request_handle);
  LocalRefDeleter java_http_request_handle_class_deleter(
      env, java_http_request_handle_class);

  get_total_sent_received_bytes_id_ = GetMethodIdOrAbort(
      *env, java_http_request_handle_class,
      JavaHttpRequestHandleClassDesc::kGetTotalSentReceivedBytes);

  close_id_ = GetMethodIdOrAbort(*env, java_http_request_handle_class,
                                 JavaHttpRequestHandleClassDesc::kClose);

  native_handle_id_ =
      env->GetFieldID(java_http_request_handle_class,
                      JavaHttpRequestHandleClassDesc::kNativeHandle.name,
                      JavaHttpRequestHandleClassDesc::kNativeHandle.signature);

  // Register this object's address inside the `nativeHandle` field, so we can
  // look this object up during later calls back into native.
  env->SetLongField(jthis_, native_handle_id_, reinterpret_cast<jlong>(this));
  FCP_CHECK(!env->ExceptionCheck());
}

JavaHttpRequestHandle::~JavaHttpRequestHandle() {
  ScopedJniEnv scoped_env(jvm_);
  JNIEnv* env = scoped_env.env();

  absl::MutexLock locked(&lock_);
  // We call the Java close() method when the destructor is invoked, to let the
  // Java code know the request's resources (if any) can be released. The
  // close() method may not have been invoked yet if the JavaHttpRequestHandle
  // never ended up being passed to `performRequests`.
  env->CallVoidMethod(jthis_, close_id_);
  FCP_CHECK(!env->ExceptionCheck());

  // Unset the native handle (this is an additional safety check, so that if the
  // Java object erroneously calls back into the native layer again, we will be
  // able to detect it, rather than us accidentally accessing a destructed
  // object).
  env->SetLongField(jthis_, native_handle_id_, 0);

  // Delete the reference to the Java object.
  env->DeleteGlobalRef(jthis_);
}

void JavaHttpRequestHandle::Cancel() {
  {
    absl::MutexLock locked(&lock_);
    // We mark the request 'performed'. This way if the handle is subsequently
    // still erroneously passed to `PerformRequests`, we can detect the error.
    performed_ = true;
  }
  // Note that we release the lock before calling into Java, to ensure that if
  // the Java call itself calls back into the native layer (e.g. by calling on
  // of the request handle callbacks), we don't accidentally try to acquire
  // the same mutex twice.

  ScopedJniEnv scoped_env(jvm_);
  JNIEnv* env = scoped_env.env();

  // We call the Java close() method to indicate that the request should be
  // cancelled. If `PerformRequests` wasn't called yet, then this will be a
  // no-op.
  env->CallVoidMethod(jthis_, close_id_);
  FCP_CHECK(!env->ExceptionCheck());
}

HttpRequestHandle::SentReceivedBytes
JavaHttpRequestHandle::TotalSentReceivedBytes() const {
  ScopedJniEnv scoped_env(jvm_);
  JNIEnv* env = scoped_env.env();

  jbyteArray sent_received_bytes_result = static_cast<jbyteArray>(
      env->CallObjectMethod(jthis_, get_total_sent_received_bytes_id_));
  FCP_CHECK(!env->ExceptionCheck());
  FCP_CHECK(sent_received_bytes_result != nullptr);
  LocalRefDeleter sent_received_bytes_result_deleter(
      env, sent_received_bytes_result);

  // Convert the return value from a Java byte[] to the expected proto.
  auto sent_received_bytes = ParseProtoFromJByteArray<JniHttpSentReceivedBytes>(
      env, sent_received_bytes_result);
  return {.sent_bytes = sent_received_bytes.sent_bytes(),
          .received_bytes = sent_received_bytes.received_bytes()};
}

fcp::client::http::HttpRequestCallback* JavaHttpRequestHandle::callback()
    const {
  // This method acquires and immediately releases the lock to ensure that all
  // JNI callback invocations observe the effects of any prior JNI callback
  // invocation, prior to invoking another `HttpRequestCallback` method.
  //
  // We don't hold the lock while invoking the actual `HttpRequestCallback`
  // method though, to ensure that if the `HttpRequestCallback` invocation
  // ultimately causes another JNI callback to be invoked, we don't attempt to
  // acquire the lock twice on the same thread.
  absl::MutexLock _(&lock_);
  return callback_;
}

const JavaHttpResponse& JavaHttpRequestHandle::response() const {
  // We synchronize for the same purpose as in callback().
  absl::MutexLock _(&lock_);
  return response_;
}

absl::Status JavaHttpRequestHandle::SetCallback(
    fcp::client::http::HttpRequestCallback* callback) {
  absl::MutexLock locked(&lock_);
  // If the request was already 'performed' then we should detect that error.
  if (performed_) {
    return absl::InvalidArgumentError(
        "can't perform a request twice, or perform an already-cancelled "
        "request");
  }
  performed_ = true;
  callback_ = callback;
  return absl::OkStatus();
}

jboolean JavaHttpRequestHandle::ReadRequestBody(JNIEnv* env, jbyteArray buffer,
                                                jlong requested_bytes,
                                                jintArray actual_bytes_read) {
  // Get a pointer to the output buffer's raw data. Note that this may make a
  // copy of the Java data, but depending on JVM implementation it may also
  // return a direct pointer to it, avoiding the copy (on Android, ART will
  // generally avoid copying if the array is large enough).
  jbyte* raw_buffer = env->GetByteArrayElements(buffer, nullptr);
  FCP_CHECK(raw_buffer != nullptr);
  FCP_CHECK(!env->ExceptionCheck());
  // Ask the `HttpRequest` to write the request body data into the buffer.
  absl::StatusOr<int64_t> read_body_result =
      request_->ReadBody(reinterpret_cast<char*>(raw_buffer), requested_bytes);
  // Release the raw buffer pointer (we must always do this, even if we hit an
  // error and didn't write anything to the buffer).
  //
  // This ensures that the data in raw_buffer is now visible via the Java buffer
  // (as noted above, this may result in copying the data into the Java heap,
  // but if a direct pointer was returned earlier on then this will be a no-op).
  env->ReleaseByteArrayElements(buffer, raw_buffer, 0);
  FCP_CHECK(!env->ExceptionCheck());

  // Out of range is expected, and marks the end of the body. Any other error is
  // unrecoverable, and should result in the OnResponseError being called.
  if (!read_body_result.ok() &&
      read_body_result.status().code() != absl::StatusCode::kOutOfRange) {
    // If we receive an error during the reading of the request body, we
    // immediately forward that error to the HttpRequestCallback (i.e. the Java
    // layer will not need to call this callback method anymore). This ensures
    // that we can forward the original error back to the callback (without
    // having to convert it to a Java representation and back to a Status
    // again).
    callback()->OnResponseError(
        *request_,
        absl::Status(read_body_result.status().code(),
                     absl::StrCat("failed to read request body",
                                  read_body_result.status().message())));
    return JNI_FALSE;
  }

  // Otherwise, if everything went successfully, then we still need to write the
  // actual amount of data we read (or -1 if we hit the end of the data) to the
  // `actual_bytes_read` output array (the output array provides a convenient
  // way to return something in addition to the return value, while still using
  // only primitive Java types to keep the JNI boilerplate to a minimum).
  //
  // Note: we know that casting from int64_t to jint (aka a 32 bit int) should
  // be safe, since `requested_bytes` is a jint, and the actual bytes read can
  // never be larger than that number.
  jint actual_bytes_read_result[] = {
      static_cast<jint>(read_body_result.value_or(-1))};
  env->SetIntArrayRegion(actual_bytes_read, 0, 1, actual_bytes_read_result);
  return JNI_TRUE;
}

jboolean JavaHttpRequestHandle::OnResponseStarted(JNIEnv* env,
                                                  jbyteArray response_proto) {
  // Populate the response_ field based on the serialized response proto. This
  // will allow us to access it in subsequent callbacks as well.
  {
    absl::MutexLock _(&lock_);
    response_.PopulateFromProto(
        ParseProtoFromJByteArray<JniHttpResponse>(env, response_proto));
  }
  return callback()->OnResponseStarted(*request_, response()).ok() ? JNI_TRUE
                                                                   : JNI_FALSE;
}

void JavaHttpRequestHandle::OnResponseError(JNIEnv* env,
                                            jbyteArray status_proto) {
  absl::Status status = ConvertRpcStatusToAbslStatus(
      ParseProtoFromJByteArray<google::rpc::Status>(env, status_proto));
  callback()->OnResponseError(*request_, status);
}

jboolean JavaHttpRequestHandle::OnResponseBody(JNIEnv* env, jbyteArray buffer,
                                               jint buffer_offset,
                                               jint bytes_available) {
  // Get a pointer to the input buffer's raw data. Note that this may make a
  // copy of the Java data, but depending on JVM implementation it may also
  // return a direct pointer to it, avoiding the copy (on Android, ART will
  // generally avoid copying if the array is large enough).
  jbyte* raw_buffer = env->GetByteArrayElements(buffer, nullptr);
  FCP_CHECK(raw_buffer != nullptr);
  FCP_CHECK(!env->ExceptionCheck());
  absl::string_view buffer_view(
      reinterpret_cast<char*>(raw_buffer) + buffer_offset, bytes_available);
  // Pass the response body data to the HttpRequestCallback.
  auto result = callback()->OnResponseBody(*request_, response(), buffer_view);

  // JNI_ABORT ensures that we don't copy the bytes in the raw buffer back to
  // the main buffer (since we know they weren't modified).
  env->ReleaseByteArrayElements(buffer, raw_buffer, JNI_ABORT);

  return result.ok() ? JNI_TRUE : JNI_FALSE;
}

void JavaHttpRequestHandle::OnResponseBodyError(JNIEnv* env,
                                                jbyteArray status_proto) {
  absl::Status status = ConvertRpcStatusToAbslStatus(
      ParseProtoFromJByteArray<google::rpc::Status>(env, status_proto));
  callback()->OnResponseBodyError(*request_, response(), status);
}

void JavaHttpRequestHandle::OnResponseCompleted() {
  callback()->OnResponseCompleted(*request_, response());
}

// JNI functions. These are called from Java. We just forward them to the
// appropriate JavaHttpRequestHandle instance's member function.
#define JFUN(METHOD_NAME) \
  Java_com_google_fcp_client_http_HttpClientForNative_##METHOD_NAME  // NOLINT

extern "C" JNIEXPORT jboolean JNICALL JFUN(readRequestBody)(
    JNIEnv* env, jclass, jlong request_handle_ptr, jbyteArray buffer,
    jlong requested_bytes, jintArray actual_bytes_read) {
  return JavaHttpRequestHandle::FromJlong(request_handle_ptr)
      ->ReadRequestBody(env, buffer, requested_bytes, actual_bytes_read);
}

extern "C" JNIEXPORT jboolean JNICALL JFUN(onResponseStarted)(
    JNIEnv* env, jclass, jlong request_handle_ptr, jbyteArray response_proto) {
  return JavaHttpRequestHandle::FromJlong(request_handle_ptr)
      ->OnResponseStarted(env, response_proto);
}

extern "C" JNIEXPORT void JNICALL JFUN(onResponseError)(
    JNIEnv* env, jclass, jlong request_handle_ptr, jbyteArray status_proto) {
  return JavaHttpRequestHandle::FromJlong(request_handle_ptr)
      ->OnResponseError(env, status_proto);
}

extern "C" JNIEXPORT jboolean JNICALL JFUN(onResponseBody)(
    JNIEnv* env, jclass, jlong request_handle_ptr, jbyteArray buffer,
    jint buffer_offset, jint bytes_available) {
  return JavaHttpRequestHandle::FromJlong(request_handle_ptr)
      ->OnResponseBody(env, buffer, buffer_offset, bytes_available);
}

extern "C" JNIEXPORT void JNICALL JFUN(onResponseBodyError)(
    JNIEnv* env, jclass, jlong request_handle_ptr, jbyteArray status_proto) {
  JavaHttpRequestHandle::FromJlong(request_handle_ptr)
      ->OnResponseBodyError(env, status_proto);
}

extern "C" JNIEXPORT void JNICALL
JFUN(onResponseCompleted)(JNIEnv* env, jclass, jlong request_handle_ptr) {
  JavaHttpRequestHandle::FromJlong(request_handle_ptr)->OnResponseCompleted();
}

}  // namespace java
}  // namespace http
}  // namespace client
}  // namespace fcp
