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

import com.google.fcp.client.CallFromNativeWrapper;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.rpc.Code;
import com.google.rpc.Status;
import java.net.HttpURLConnection;
import java.util.ArrayList;

/**
 * An implementation of {@link HttpClientForNativeImpl} that uses {@link HttpURLConnection} for
 * issuing network requests.
 */
public final class HttpClientForNativeImpl extends HttpClientForNative {

  /** Used to bubble up unexpected errors and exceptions. */
  static final class UncheckedHttpClientForNativeException extends RuntimeException {
    UncheckedHttpClientForNativeException(String message) {
      super(message);
    }

    UncheckedHttpClientForNativeException(String message, Throwable cause) {
      super(message, cause);
    }
  }

  /** A factory for creating an {@link HttpRequestHandleImpl} for a given {@link JniHttpRequest}. */
  public interface HttpRequestHandleImplFactory {
    /**
     * Creates a new request handle, which must be an instance of {@link HttpRequestHandleImpl} or
     * one of its subclasses. This indirection is used to provide a different subclass in unit
     * tests.
     */
    HttpRequestHandleImpl create(JniHttpRequest request);
  }

  private final CallFromNativeWrapper callFromNativeWrapper;
  private final HttpRequestHandleImplFactory requestHandleFactory;

  /**
   * Creates a new instance, configured with the provided parameters.
   *
   * @param callFromNativeWrapper the wrapper to use for all calls that arrive over JNI, to ensure
   *     uncaught exceptions are handled correctly.
   * @param requestHandleFactory the factory to use to create new {@link HttpRequestHandleImpl} for
   *     a given {@link JniHttpRequest}.
   */
  public HttpClientForNativeImpl(
      CallFromNativeWrapper callFromNativeWrapper,
      HttpRequestHandleImplFactory requestHandleFactory) {
    this.callFromNativeWrapper = callFromNativeWrapper;
    this.requestHandleFactory = requestHandleFactory;
  }

  @Override
  public HttpRequestHandleImpl enqueueRequest(byte[] requestProto) {
    return callFromNativeWrapper.wrapCall(
        () -> {
          // Parse the request given to us over JNI.
          JniHttpRequest request;
          try {
            request =
                JniHttpRequest.parseFrom(requestProto, ExtensionRegistryLite.getEmptyRegistry());
          } catch (InvalidProtocolBufferException e) {
            // If parsing failed then the native code did something horribly wrong, just let the
            // exception bubble up to the unchecked exception handler.
            throw new UncheckedHttpClientForNativeException("invalid JniHttpRequest", e);
          }
          return requestHandleFactory.create(request);
        });
  }

  @Override
  public byte[] performRequests(Object[] requestsParam) {
    return callFromNativeWrapper.wrapCall(
        () -> {
          ArrayList<HttpRequestHandleImpl> handles = new ArrayList<>(requestsParam.length);
          for (Object requestHandle : requestsParam) {
            // Note: if this cast fails, then it means that the native layer has somehow passed us a
            // different object than we returned from enqueueRequest, which would indicate a bug. In
            // those cases we just let the exception bubble up to create a crash report.
            HttpRequestHandleImpl handle = (HttpRequestHandleImpl) requestHandle;
            handles.add(handle);
            // Handle each request on the ExecutorService (i.e. on background threads).
            handle.performRequest();
          }
          // Wait for each request to finish.
          for (HttpRequestHandleImpl handle : handles) {
            handle.waitForRequestCompletion();
          }

          return Status.newBuilder().setCode(Code.OK_VALUE).build().toByteArray();
        });
  }

  @Override
  public void close() {
    // Nothing to do here.
  }
}
