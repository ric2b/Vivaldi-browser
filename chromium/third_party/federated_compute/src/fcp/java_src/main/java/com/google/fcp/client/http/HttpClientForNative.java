// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package com.google.fcp.client.http;

import com.google.errorprone.annotations.concurrent.GuardedBy;
import java.io.Closeable;

/**
 * A base class for building a Java/JNI-based implementation of the C++ {@code HttpClient}
 * interface.
 *
 * <p>This class is defined in conjunction with the {@code java_http_client.cc/h} C++ code that
 * invokes it via JNI.
 *
 * <p>A note on thread safety:
 *
 * <ol>
 *   <li>Incoming calls from the native layer can generally come from any thread, and hence
 *       implementations of these classes must be thread safe.
 *   <li>Outgoing calls to the native layer (e.g. {@link #readRequestBody}, {@link
 *       #onResponseStarted}, etc.) may also be made from any thread, but for a single {@link
 *       HttpRequestHandle} there must never be any concurrent outgoing calls from more than one
 *       thread (hence they are {@code @GuardedBy("this")}).
 *   <li>Outgoing calls to the native layer must only be made once an {@link #performRequests} has
 *       been called on a given {@link HttpRequestHandle}, and not before.
 * </ol>
 */
public abstract class HttpClientForNative implements Closeable {
  /**
   * A base class for building a Java/JNI-based implementation of the C++ {@code HttpRequestHandle}
   * and related interfaces.
   *
   * <p>This class is defined in conjunction with the {@code java_http_client.cc/h} C++ code that
   * invokes it via JNI.
   */
  public abstract static class HttpRequestHandle implements Closeable {
    /**
     * Called by the native layer to get the request's latest total sent/received bytes stats. May
     * be called multiple times, and from any thread.
     *
     * <p>See C++'s {@code HttpRequestHandle::TotalSentReceivedBytes}.
     *
     * @return a serialized {@link JniHttpSentReceivedBytes} proto.
     */
    public abstract byte[] getTotalSentReceivedBytes();

    /**
     * Called by the native layer when the request isn't needed anymore. May be called multiple
     * times, and from any thread.
     */
    @Override
    public abstract void close();

    /**
     * Reads up to {@code requestedBytes} of request body data into {@code buffer}, via the native
     * layer. If the end of the data is reached, then -1 will be placed in the mutable
     * single-element {@code actualBytesRead} array (this corresponds to C++'s {@code
     * HttpRequest::ReadBody} returning {@code OUT_OF_RANGE}). Otherwise, at least 1 byte of data
     * will have been read, and the actual amount of bytes that were read will be placed in the
     * {@code actualBytesRead} array.
     *
     * <p>If the return value is false, then {@link HttpClientForNative} implementation must not
     * call {@link #onResponseError} anymore, as the native layer will already have called the
     * corresponding C++ callback.
     *
     * <p>See C++'s {@code HttpRequest::ReadBody}.
     *
     * <p>Must only be called <strong>after</strong> {@link #performRequests} is called on this
     * handle. Only one of the callback methods on this handle may be called at any given time (but
     * they may be called from any thread).
     *
     * @return true if the read succeeded (incl. if the end of data was reached), false if the read
     *     failed (in which case the request should be aborted without calling any more callback
     *     methods).
     */
    // Note: can be overridden in unit tests, to intercept/mock out calls to the native layer.
    @GuardedBy("this")
    protected boolean readRequestBody(byte[] buffer, long requestedBytes, int[] actualBytesRead) {
      return HttpClientForNative.readRequestBody(
          nativeHandle, buffer, requestedBytes, actualBytesRead);
    }

    /**
     * Signals to the native layer that the response headers (provided as a serialized {@link
     * JniHttpResponse}) have been received.
     *
     * <p>See C++'s {@code HttpRequestCallback::OnResponseStarted}.
     *
     * <p>Must only be called <strong>after</strong> {@link #performRequests} is called on this
     * handle. Only one of the callback methods on this handle may be called at any given time (but
     * they may be called from any thread).
     *
     * @return true if the response headers were successfully processed, false if not (in which case
     *     the request should be aborted without calling any more callback methods).
     */
    // Note: can be overridden in unit tests, to intercept/mock out calls to the native layer.
    @GuardedBy("this")
    protected boolean onResponseStarted(byte[] responseProto) {
      return HttpClientForNative.onResponseStarted(nativeHandle, responseProto);
    }

    /**
     * Signals to the native layer that an error (provided as a serialized {@link
     * com.google.rpc.Status} proto) occurred before the response headers were received.
     *
     * <p>See C++'s {@code HttpRequestCallback::OnResponseError}.
     *
     * <p>Must only be called <strong>after</strong> {@link #performRequests} is called on this
     * handle. Only one of the callback methods on this handle may be called at any given time (but
     * they may be called from any thread).
     */
    // Note: can be overridden in unit tests, to intercept/mock out calls to the native layer.
    @GuardedBy("this")
    protected void onResponseError(byte[] statusProto) {
      HttpClientForNative.onResponseError(nativeHandle, statusProto);
    }

    /**
     * Provides {@code bytesAvailable} bytes of the response body to the native layer, via {@code
     * data}.
     *
     * <p>See C++'s {@code HttpRequestCallback::OnResponseBody}.
     *
     * <p>Must only be called <strong>after</strong> {@link #performRequests} is called on this
     * handle. Only one of the callback methods on this handle may be called at any given time (but
     * they may be called from any thread).
     *
     * @return true if the data was successfully processed, or false if not (in which case the
     *     request should be aborted without calling any more callback methods).
     */
    // Note: can be overridden in unit tests, to intercept/mock out calls to the native layer.
    @GuardedBy("this")
    protected boolean onResponseBody(byte[] data, int dataOffset, int bytesAvailable) {
      return HttpClientForNative.onResponseBody(nativeHandle, data, dataOffset, bytesAvailable);
    }

    /**
     * Signals to the native layer that an error (provided as a serialized {@link
     * com.google.rpc.Status} proto) occurred while reading the response body.
     *
     * <p>See C++'s {@code HttpRequestCallback::OnResponseBodyError}.
     *
     * <p>Must only be called <strong>after</strong> {@link #performRequests} is called on this
     * handle. Only one of the callback methods on this handle may be called at any given time (but
     * they may be called from any thread).
     */
    // Note: can be overridden in unit tests, to intercept/mock out calls to the native layer.
    @GuardedBy("this")
    protected void onResponseBodyError(byte[] statusProto) {
      HttpClientForNative.onResponseBodyError(nativeHandle, statusProto);
    }

    /**
     * Signals to the native layer that the request completed successfully.
     *
     * <p>See C++'s {@code HttpRequestCallback::OnResponseBodyCompleted}.
     *
     * <p>Must only be called <strong>after</strong> {@link #performRequests} is called on this
     * handle. Only one of the callback methods on this handle may be called at any given time (but
     * they may be called from any thread).
     */
    // Note: can be overridden in unit tests, to intercept/mock out calls to the native layer.
    @GuardedBy("this")
    protected void onResponseCompleted() {
      HttpClientForNative.onResponseCompleted(nativeHandle);
    }

    /**
     * A field that native code uses to associate a native pointer with this object. This field must
     * never be modified by Java code.
     */
    // Note: this field is volatile to ensure that if it is read from a different thread than the
    // one that wrote to it earlier, the second thread will see the updated value.
    private volatile long nativeHandle = 0;
  }

  /**
   * Creates an {@link HttpRequestHandle} for use with {@link #performRequests}.
   *
   * <p>May be called from any thread.
   *
   * @param requestProto a serialized {@link JniHttpRequest} proto.
   */
  public abstract HttpRequestHandle enqueueRequest(byte[] requestProto);

  /**
   * Performs the requests corresponding to the given objects, which must be {@link
   * HttpRequestHandle} instances previously returned by {@link #enqueueRequest}.
   *
   * <p>May be called from any thread.
   *
   * @return a serialized {@link com.google.rpc.Status} proto indicating success or failure.
   */
  // NOTE: The parameter type is an 'Object[]' array, because this makes it easier for the native
  // code calling this over JNI to construct the array (it can simply look up the 'Object') class.
  // The Java implementation is expected to downcast the objects in the array to its RequestHandle
  // implementation class.
  public abstract byte[] performRequests(Object[] requests);

  /**
   * Called by native when the client is no longer used and all resources can be released. May be
   * called multiple times, and from any thread.
   */
  @Override
  public abstract void close();

  // The actual native callback methods, which the HttpRequestHandle class provides wrappers for.
  // See that class's docs for more info.
  private static native boolean readRequestBody(
      long nativeRequestHandle, byte[] buffer, long requestedBytes, int[] actualBytesRead);

  private static native boolean onResponseStarted(long nativeRequestHandle, byte[] responseProto);

  private static native void onResponseError(long nativeRequestHandle, byte[] statusProto);

  private static native boolean onResponseBody(
      long nativeRequestHandle, byte[] data, int dataOffset, int bytesAvailable);

  private static native void onResponseBodyError(long nativeRequestHandle, byte[] statusProto);

  private static native void onResponseCompleted(long nativeRequestHandle);
}
