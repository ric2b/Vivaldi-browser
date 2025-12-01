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

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.fcp.client.CallFromNativeWrapper;
import com.google.fcp.client.CallFromNativeWrapper.CallFromNativeRuntimeException;
import com.google.fcp.client.http.HttpRequestHandleImpl.HttpURLConnectionFactory;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.rpc.Code;
import com.google.rpc.Status;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.SocketTimeoutException;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.GZIPOutputStream;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

/**
 * Unit tests for {@link HttpClientForNativeImpl}.
 *
 * <p>This test doesn't actually call into any native code/JNI (instead the JNI callback methods are
 * faked out and replaced with Java-only code, which makes the code a lot easier to unit test). This
 * test also does <strong>not</strong> exercise all of the concurrency-related edge cases that could
 * arise (since these are difficult to conclusively test in general).
 */
@RunWith(JUnit4.class)
public final class HttpClientForNativeImplTest {

  private static final int DEFAULT_TEST_CHUNK_BUFFER_SIZE = 5;
  private static final double ESTIMATED_HTTP2_HEADER_COMPRESSION_RATIO = 0.5;
  // We use an executor with real background threads, just to exercise a bit more of the code and
  // possibly spot any concurrency issues. The use of background threads is conveniently hidden
  // behind the performRequests interface anyway.
  private static final ExecutorService TEST_EXECUTOR_SERVICE = Executors.newFixedThreadPool(2);
  // Do nothing in the UncaughtExceptionHandler, letting the exception bubble up instead.
  private static final CallFromNativeWrapper TEST_CALL_FROM_NATIVE_WRAPPER =
      new CallFromNativeWrapper((t, e) -> {});

  /**
   * A fake {@link HttpRequestHandleImpl} implementation which never actually calls into the native
   * layer over JNI, and instead uses a fake pure Java implementation that emulates how the native
   * layer would behave. This makes unit testing the Java layer possible.
   */
  static class TestHttpRequestHandleImpl extends HttpRequestHandleImpl {
    TestHttpRequestHandleImpl(
        JniHttpRequest request,
        HttpURLConnectionFactory urlConnectionFactory,
        boolean supportAcceptEncodingHeader,
        boolean disableTimeouts) {
      super(
          request,
          TEST_CALL_FROM_NATIVE_WRAPPER,
          TEST_EXECUTOR_SERVICE,
          urlConnectionFactory,
          /*connectTimeoutMs=*/ disableTimeouts ? -1 : 123,
          /*readTimeoutMs=*/ disableTimeouts ? -1 : 456,
          // Force the implementation to read 5 bytes at a time, to exercise the chunking logic.
          /*requestBodyChunkSizeBytes=*/ DEFAULT_TEST_CHUNK_BUFFER_SIZE,
          /*responseBodyChunkSizeBytes=*/ DEFAULT_TEST_CHUNK_BUFFER_SIZE,
          /*responseBodyGzipBufferSizeBytes=*/ DEFAULT_TEST_CHUNK_BUFFER_SIZE,
          /*callDisconnectWhenCancelled=*/ true,
          /*supportAcceptEncodingHeader=*/ supportAcceptEncodingHeader,
          /*estimatedHttp2HeaderCompressionRatio=*/ ESTIMATED_HTTP2_HEADER_COMPRESSION_RATIO);
    }

    // There should be no need for us to synchronize around these mutable fields, since the
    // implementation itself should already implement the necessary synchronization to ensure that
    // only one JNI callback method is called a time.
    ByteArrayInputStream fakeRequestBody = null;
    boolean readRequestBodyResult = true;
    boolean onResponseStartedResult = true;
    boolean onResponseBodyResult = true;

    JniHttpResponse responseProto = null;
    Status responseError = null;
    Status responseBodyError = null;
    ByteArrayOutputStream responseBody = new ByteArrayOutputStream();
    boolean completedSuccessfully = false;

    @Override
    protected boolean readRequestBody(byte[] buffer, long requestedBytes, int[] actualBytesRead) {
      if (!readRequestBodyResult) {
        return false;
      }
      int cursor;
      // Always return up to two bytes only. That way we ensure the implementation properly handles
      // the case when it gets less than data back than requested.
      for (cursor = 0; cursor < Long.min(2, requestedBytes); cursor++) {
        int newByte = fakeRequestBody.read();
        if (newByte == -1) {
          break;
        }
        buffer[cursor] = (byte) newByte;
      }
      actualBytesRead[0] = cursor == 0 ? -1 : cursor;
      return true;
    }

    @Override
    protected boolean onResponseStarted(byte[] responseProto) {
      if (!onResponseStartedResult) {
        return false;
      }
      try {
        this.responseProto =
            JniHttpResponse.parseFrom(responseProto, ExtensionRegistryLite.getEmptyRegistry());
      } catch (InvalidProtocolBufferException e) {
        throw new AssertionError("invalid responseProto", e);
      }
      return true;
    }

    @Override
    protected void onResponseError(byte[] statusProto) {
      try {
        responseError = Status.parseFrom(statusProto, ExtensionRegistryLite.getEmptyRegistry());
      } catch (InvalidProtocolBufferException e) {
        throw new AssertionError("invalid statusProto", e);
      }
    }

    @Override
    protected boolean onResponseBody(byte[] data, int dataOffset, int bytesAvailable) {
      if (!onResponseBodyResult) {
        return false;
      }
      responseBody.write(data, 0, bytesAvailable);
      return true;
    }

    @Override
    protected void onResponseBodyError(byte[] statusProto) {
      try {
        responseBodyError = Status.parseFrom(statusProto, ExtensionRegistryLite.getEmptyRegistry());
      } catch (InvalidProtocolBufferException e) {
        throw new AssertionError("invalid statusProto", e);
      }
    }

    @Override
    protected void onResponseCompleted() {
      completedSuccessfully = true;
    }

    /**
     * Checks that the request succeeded, based on which native callback methods were/were not
     * invoked.
     */
    void assertSuccessfulCompletion() {
      assertWithMessage("onResponseError was called").that(responseError).isNull();
      assertWithMessage("onResponseBodyError was called").that(responseBodyError).isNull();
      assertWithMessage("onResponseStarted was not called").that(responseProto).isNotNull();
      assertWithMessage("onResponseCompleted was not called").that(completedSuccessfully).isTrue();
    }
  }

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock HttpURLConnectionFactory urlConnectionFactory;

  HttpClientForNativeImpl httpClient;

  @Before
  public void setUp() throws Exception {
    httpClient =
        new HttpClientForNativeImpl(
            TEST_CALL_FROM_NATIVE_WRAPPER,
            (request) ->
                new TestHttpRequestHandleImpl(
                    request,
                    urlConnectionFactory,
                    /*supportAcceptEncodingHeader=*/ true,
                    /*disableTimeouts=*/ false));
  }

  @Test
  public void testSingleRequestWithoutRequestBodySucceeds() throws Exception {
    doTestSingleRequestWithoutRequestBodySucceeds(
        /*supportAcceptEncodingHeader=*/ true, /*expectTimeoutsToBeSet=*/ true);
  }

  @Test
  public void testSingleRequestWithoutRequestBodyAndDisableAcceptEncodingHeaderSupportSucceeds()
      throws Exception {
    httpClient =
        new HttpClientForNativeImpl(
            TEST_CALL_FROM_NATIVE_WRAPPER,
            (request) ->
                new TestHttpRequestHandleImpl(
                    request,
                    urlConnectionFactory,
                    /*supportAcceptEncodingHeader=*/ false,
                    /*disableTimeouts=*/ false));
    doTestSingleRequestWithoutRequestBodySucceeds(
        /*supportAcceptEncodingHeader=*/ false, /*expectTimeoutsToBeSet=*/ true);
  }

  @Test
  public void testSingleRequestWithoutRequestBodyAndDisableTimeoutsSucceeds() throws Exception {
    httpClient =
        new HttpClientForNativeImpl(
            TEST_CALL_FROM_NATIVE_WRAPPER,
            (request) ->
                new TestHttpRequestHandleImpl(
                    request,
                    urlConnectionFactory,
                    /*supportAcceptEncodingHeader=*/ false,
                    /*disableTimeouts=*/ true));
    doTestSingleRequestWithoutRequestBodySucceeds(
        /*supportAcceptEncodingHeader=*/ false, /*expectTimeoutsToBeSet=*/ false);
  }

  private void doTestSingleRequestWithoutRequestBodySucceeds(
      boolean supportAcceptEncodingHeader, boolean expectTimeoutsToBeSet) throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder()
                            .setName("Request-Header1")
                            .setValue("Foo")
                            .build())
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder()
                            .setName("Request-Header2")
                            .setValue("Bar")
                            .build())
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo.com")).thenReturn(mockConnection);

    int expectedResponseCode = 200;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);
    // Create a fake set of response headers. We use a LinkedHashMap rather than the less verbose
    // ImmutableMap.of utility to allow us to add an entry for the HTTP status line, which in
    // HttpURLConnection has a null key, which ImmutableMap disallows (to check that it gets
    // properly handled/filtered out before passing on to JNI). Because the map still has a defined
    // iteration order, we can still easily compare the whole response proto in one go (since we
    // know the order the header fields will be in).
    LinkedHashMap<String, List<String>> headerFields = new LinkedHashMap<>();
    headerFields.put("Response-Header1", ImmutableList.of("Bar", "Baz"));
    headerFields.put("Response-Header2", ImmutableList.of("Barbaz"));
    // And add a Content-Length and 'null' header (to check whether they are correctly redacted &
    // ignored.
    headerFields.put("Content-Length", ImmutableList.of("9999")); // Should be ignored.
    headerFields.put(null, ImmutableList.of("200 OK")); // Should be ignored.
    when(mockConnection.getHeaderFields()).thenReturn(headerFields);

    // Fake some response body data.
    String expectedResponseBody = "test_response_body";
    when(mockConnection.getInputStream())
        .thenReturn(new ByteArrayInputStream(expectedResponseBody.getBytes(UTF_8)));

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Verify the results..
    requestHandle.assertSuccessfulCompletion();
    assertThat(requestHandle.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(expectedResponseCode)
                // The Content-Length and 'null' headers should have been redacted.
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Baz").build())
                .addHeaders(
                    JniHttpHeader.newBuilder()
                        .setName("Response-Header2")
                        .setValue("Barbaz")
                        .build())
                .build());

    assertThat(requestHandle.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody);

    // Verify various important request properties.
    verify(mockConnection).setRequestMethod("GET");
    InOrder requestHeadersOrder = inOrder(mockConnection);
    requestHeadersOrder.verify(mockConnection).addRequestProperty("Request-Header1", "Foo");
    requestHeadersOrder.verify(mockConnection).addRequestProperty("Request-Header2", "Bar");
    verify(mockConnection, supportAcceptEncodingHeader ? times(1) : never())
        .setRequestProperty("Accept-Encoding", "gzip");
    verify(mockConnection, expectTimeoutsToBeSet ? times(1) : never()).setConnectTimeout(123);
    verify(mockConnection, expectTimeoutsToBeSet ? times(1) : never()).setReadTimeout(456);
    verify(mockConnection, never()).setDoOutput(anyBoolean());
    verify(mockConnection, never()).getOutputStream();
    verify(mockConnection).setDoInput(true);
    verify(mockConnection).setUseCaches(false);
    verify(mockConnection).setInstanceFollowRedirects(true);
  }

  @Test
  public void testSingleRequestWithRequestBodySucceeds() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_POST)
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder()
                            .setName("Request-Header1")
                            .setValue("Foo")
                            .build())
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder()
                            .setName("Request-Header1")
                            .setValue("Foobar")
                            .build())
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder()
                            .setName("Request-Header2")
                            .setValue("Bar")
                            .build())
                    .setHasBody(true)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo.com")).thenReturn(mockConnection);

    // Gather request body data sent to the HttpURLConnection in a stream we can inspect later on.
    ByteArrayOutputStream actualRequestBody = new ByteArrayOutputStream();
    when(mockConnection.getOutputStream()).thenReturn(actualRequestBody);

    // Fake some request body data.
    String expectedRequestBody = "test_request_body";
    requestHandle.fakeRequestBody = new ByteArrayInputStream(expectedRequestBody.getBytes(UTF_8));

    int expectedResponseCode = 200;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);
    LinkedHashMap<String, List<String>> headerFields = new LinkedHashMap<>();
    headerFields.put("Response-Header1", ImmutableList.of("Bar", "Baz"));
    headerFields.put("Response-Header2", ImmutableList.of("Barbaz"));
    headerFields.put(null, ImmutableList.of("HTTP/1.1 200 OK")); // Should be ignored.
    when(mockConnection.getHeaderFields()).thenReturn(headerFields);

    // Add the response message ("OK"), so that it gets included in the received bytes stats.
    when(mockConnection.getResponseMessage()).thenReturn("OK");

    // Fake some response body data.
    String expectedResponseBody = "test_response_body";
    when(mockConnection.getInputStream())
        .thenReturn(new ByteArrayInputStream(expectedResponseBody.getBytes(UTF_8)));

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Verify the results.
    requestHandle.assertSuccessfulCompletion();
    assertThat(requestHandle.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(expectedResponseCode)
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Baz").build())
                .addHeaders(
                    JniHttpHeader.newBuilder()
                        .setName("Response-Header2")
                        .setValue("Barbaz")
                        .build())
                .build());

    assertThat(actualRequestBody.toString(UTF_8.name())).isEqualTo(expectedRequestBody);
    assertThat(requestHandle.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody);

    // Verify the network stats are accurate (they should count the request headers, URL, request
    // method, request body, response headers and response body).
    assertThat(
            JniHttpSentReceivedBytes.parseFrom(
                requestHandle.getTotalSentReceivedBytes(),
                ExtensionRegistryLite.getEmptyRegistry()))
        .isEqualTo(
            JniHttpSentReceivedBytes.newBuilder()
                .setSentBytes(
                    ("POST https://foo.com HTTP/1.1\r\n"
                                + "Request-Header1: Foo\r\n"
                                + "Request-Header1: Foobar\r\n"
                                + "Request-Header2: Bar\r\n"
                                + "\r\n")
                            .length()
                        + expectedRequestBody.length())
                .setReceivedBytes(
                    ("HTTP/1.1 200 OK\r\n"
                                + "Response-Header1: Bar\r\n"
                                + "Response-Header1: Baz\r\n"
                                + "Response-Header2: Barbaz\r\n"
                                + "\r\n")
                            .length()
                        + requestHandle.responseBody.size())
                .build());

    // Verify various important request properties.
    verify(mockConnection).setRequestMethod("POST");
    InOrder requestHeadersOrder = inOrder(mockConnection);
    requestHeadersOrder.verify(mockConnection).addRequestProperty("Request-Header1", "Foo");
    requestHeadersOrder.verify(mockConnection).addRequestProperty("Request-Header1", "Foobar");
    requestHeadersOrder.verify(mockConnection).addRequestProperty("Request-Header2", "Bar");
    verify(mockConnection).setConnectTimeout(123);
    verify(mockConnection).setReadTimeout(456);
    verify(mockConnection).setDoOutput(true);
    // Since the request body content length wasn't known ahead of time, the
    // 'Transfer-Encoding: chunked' streaming mode should've been enabled.
    verify(mockConnection).setChunkedStreamingMode(5);
    verify(mockConnection, never()).setFixedLengthStreamingMode(anyInt());
    verify(mockConnection).setDoInput(true);
    verify(mockConnection).setUseCaches(false);
    verify(mockConnection).setInstanceFollowRedirects(true);
  }

  /**
   * Tests whether a single request with a <strong>known-ahead-of-time</strong> request body content
   * length is processed correctly.
   */
  @Test
  public void testSingleRequestWithKnownRequestContentLengthSucceeds() throws Exception {
    String expectedRequestBody = "another_test_request_body";
    String requestBodyLength = "25"; // the length of the above string.
    long requestBodyLengthLong = 25L;
    assertThat(expectedRequestBody).hasLength((int) requestBodyLengthLong);
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_PUT)
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder()
                            .setName("Request-Header1")
                            .setValue("Foo")
                            .build())
                    .addExtraHeaders(
                        // Add a Content-Length request header, which should result in 'fixed
                        // length'
                        // request body streaming mode.
                        JniHttpHeader.newBuilder()
                            // We purposely use a mixed-case header name to ensure header matching
                            // is
                            // case insensitive.
                            .setName("Content-length")
                            .setValue(requestBodyLength))
                    .setHasBody(true)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo.com")).thenReturn(mockConnection);

    // Gather request body data sent to the HttpURLConnection in a stream we can inspect later on.
    ByteArrayOutputStream actualRequestBody = new ByteArrayOutputStream();
    when(mockConnection.getOutputStream()).thenReturn(actualRequestBody);

    // Fake some request body data.
    requestHandle.fakeRequestBody = new ByteArrayInputStream(expectedRequestBody.getBytes(UTF_8));

    int expectedResponseCode = 201;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);

    // Fake some response body data.
    String expectedResponseBody = "another_test_response_body";
    when(mockConnection.getInputStream())
        .thenReturn(new ByteArrayInputStream(expectedResponseBody.getBytes(UTF_8)));

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Verify the results..
    requestHandle.assertSuccessfulCompletion();
    assertThat(requestHandle.responseProto)
        .isEqualTo(JniHttpResponse.newBuilder().setCode(expectedResponseCode).build());

    assertThat(actualRequestBody.toString(UTF_8.name())).isEqualTo(expectedRequestBody);
    assertThat(requestHandle.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody);

    verify(mockConnection).setRequestMethod("PUT");
    verify(mockConnection).setDoOutput(true);
    InOrder requestHeadersOrder = inOrder(mockConnection);
    requestHeadersOrder.verify(mockConnection).addRequestProperty("Request-Header1", "Foo");
    requestHeadersOrder
        .verify(mockConnection)
        .addRequestProperty("Content-length", requestBodyLength);
    // Since the request body content length *was* known ahead of time, the fixed length streaming
    // mode should have been enabled.
    verify(mockConnection).setFixedLengthStreamingMode(requestBodyLengthLong);
    verify(mockConnection, never()).setChunkedStreamingMode(anyInt());
  }

  /**
   * Tests whether a single request with a request body that is smaller than our read buffer size is
   * processed correctly.
   */
  @Test
  public void testSingleRequestWithKnownRequestContentLengthThatFitsInSingleBufferSucceeds()
      throws Exception {
    String expectedRequestBody = "1234";
    String requestBodyLength =
        "4"; // the length of the above string, which is smaller than the buffer size.
    long requestBodyLengthLong = 4L;
    assertThat(expectedRequestBody).hasLength((int) requestBodyLengthLong);
    assertThat(requestBodyLengthLong).isLessThan(DEFAULT_TEST_CHUNK_BUFFER_SIZE);
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_PUT)
                    .addExtraHeaders(
                        // Add a Content-Length request header, which should result in 'fixed
                        // length'
                        // request body streaming mode.
                        JniHttpHeader.newBuilder()
                            // We purposely use a mixed-case header name to ensure header matching
                            // is
                            // case insensitive.
                            .setName("content-Length")
                            .setValue(requestBodyLength))
                    .setHasBody(true)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo.com")).thenReturn(mockConnection);

    // Gather request body data sent to the HttpURLConnection in a stream we can inspect later on.
    ByteArrayOutputStream actualRequestBody = new ByteArrayOutputStream();
    when(mockConnection.getOutputStream()).thenReturn(actualRequestBody);

    // Fake some request body data.
    requestHandle.fakeRequestBody = new ByteArrayInputStream(expectedRequestBody.getBytes(UTF_8));

    int expectedResponseCode = 503;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);

    // Fake some response body data (via the error stream this time).
    String expectedResponseBody = "abc";
    when(mockConnection.getErrorStream())
        .thenReturn(new ByteArrayInputStream(expectedResponseBody.getBytes(UTF_8)));

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Verify the results..
    requestHandle.assertSuccessfulCompletion();
    assertThat(requestHandle.responseProto)
        .isEqualTo(JniHttpResponse.newBuilder().setCode(expectedResponseCode).build());

    assertThat(actualRequestBody.toString(UTF_8.name())).isEqualTo(expectedRequestBody);
    assertThat(requestHandle.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody);

    verify(mockConnection).setRequestMethod("PUT");
    verify(mockConnection).setDoOutput(true);
    verify(mockConnection).addRequestProperty("content-Length", requestBodyLength);
    // Since the request body content length *was* known ahead of time, the fixed length streaming
    // mode should have been enabled.
    verify(mockConnection).setFixedLengthStreamingMode(requestBodyLengthLong);
    verify(mockConnection, never()).setChunkedStreamingMode(anyInt());
  }

  /** Tests whether issuing multiple concurrent requests is handled correctly. */
  @Test
  public void testMultipleRequestsWithRequestBodiesSucceeds() throws Exception {
    TestHttpRequestHandleImpl requestHandle1 =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_POST)
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder()
                            .setName("Request-Header1")
                            .setValue("Foo")
                            .build())
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder()
                            .setName("Request-Header2")
                            .setValue("Bar")
                            .build())
                    .setHasBody(true)
                    .build()
                    .toByteArray());

    TestHttpRequestHandleImpl requestHandle2 =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo2.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_PATCH)
                    .setHasBody(true)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection1 = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo.com")).thenReturn(mockConnection1);
    HttpURLConnection mockConnection2 = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo2.com")).thenReturn(mockConnection2);

    // Gather request body data sent to the HttpURLConnection in a stream we can inspect later on.
    ByteArrayOutputStream actualRequestBody1 = new ByteArrayOutputStream();
    when(mockConnection1.getOutputStream()).thenReturn(actualRequestBody1);
    ByteArrayOutputStream actualRequestBody2 = new ByteArrayOutputStream();
    when(mockConnection2.getOutputStream()).thenReturn(actualRequestBody2);

    // Fake some request body data.
    String expectedRequestBody1 = "test_request_body1";
    requestHandle1.fakeRequestBody = new ByteArrayInputStream(expectedRequestBody1.getBytes(UTF_8));
    String expectedRequestBody2 = "another_request_body2";
    requestHandle2.fakeRequestBody = new ByteArrayInputStream(expectedRequestBody2.getBytes(UTF_8));

    int expectedResponseCode1 = 200;
    int expectedResponseCode2 = 300;
    when(mockConnection1.getResponseCode()).thenReturn(expectedResponseCode1);
    when(mockConnection2.getResponseCode()).thenReturn(expectedResponseCode2);
    when(mockConnection1.getHeaderFields()).thenReturn(ImmutableMap.of());
    when(mockConnection2.getHeaderFields())
        .thenReturn(
            ImmutableMap.of(
                "Response-Header1",
                ImmutableList.of("Bar"),
                "Response-Header2",
                ImmutableList.of("Barbaz")));

    // Fake some response body data.
    String expectedResponseBody1 = "test_response_body";
    when(mockConnection1.getInputStream())
        .thenReturn(new ByteArrayInputStream(expectedResponseBody1.getBytes(UTF_8)));

    String expectedResponseBody2 = "test_response_body";
    when(mockConnection2.getInputStream())
        .thenReturn(new ByteArrayInputStream(expectedResponseBody2.getBytes(UTF_8)));

    // Run both requests (we provide them in opposite order to how they were created, just to try
    // to exercise more edge conditions).
    byte[] result = httpClient.performRequests(new Object[] {requestHandle2, requestHandle1});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle1.close();
    requestHandle2.close();

    // Verify the results..
    requestHandle1.assertSuccessfulCompletion();
    assertThat(requestHandle1.responseProto)
        .isEqualTo(JniHttpResponse.newBuilder().setCode(expectedResponseCode1).build());

    requestHandle2.assertSuccessfulCompletion();
    assertThat(requestHandle2.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(expectedResponseCode2)
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .addHeaders(
                    JniHttpHeader.newBuilder()
                        .setName("Response-Header2")
                        .setValue("Barbaz")
                        .build())
                .build());

    assertThat(actualRequestBody1.toString(UTF_8.name())).isEqualTo(expectedRequestBody1);
    assertThat(requestHandle1.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody1);
    assertThat(actualRequestBody2.toString(UTF_8.name())).isEqualTo(expectedRequestBody2);
    assertThat(requestHandle2.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody2);

    // Verify various important request properties.
    verify(mockConnection1).setRequestMethod("POST");
    verify(mockConnection2).setRequestMethod("PATCH");
    InOrder requestHeadersOrder = inOrder(mockConnection1);
    requestHeadersOrder.verify(mockConnection1).addRequestProperty("Request-Header1", "Foo");
    requestHeadersOrder.verify(mockConnection1).addRequestProperty("Request-Header2", "Bar");
    verify(mockConnection2, never()).addRequestProperty(any(), any());
  }

  @Test
  public void testGzipResponseBodyDecompressionSucceeds() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder()
                            .setName("Request-Header1")
                            .setValue("Foo")
                            .build())
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo.com")).thenReturn(mockConnection);

    int expectedResponseCode = 200;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);
    when(mockConnection.getResponseMessage()).thenReturn("OK");

    // Fake some response body data.
    String expectedResponseBody = "test_response_body";
    ByteArrayOutputStream compressedResponseBody = new ByteArrayOutputStream();
    GZIPOutputStream compressedResponseBodyGzipStream =
        new GZIPOutputStream(compressedResponseBody);
    compressedResponseBodyGzipStream.write(expectedResponseBody.getBytes(UTF_8));
    compressedResponseBodyGzipStream.finish();
    when(mockConnection.getInputStream())
        .thenReturn(new ByteArrayInputStream(compressedResponseBody.toByteArray()));
    // And add Content-Encoding and Transfer-Encoding headers (to check whether they are correctly
    // redacted).
    when(mockConnection.getHeaderFields())
        .thenReturn(
            ImmutableMap.of(
                "Response-Header1",
                ImmutableList.of("Bar"),
                "Content-Encoding",
                ImmutableList.of("gzip"),
                "Transfer-Encoding",
                ImmutableList.of("chunked")));

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Verify the results..
    requestHandle.assertSuccessfulCompletion();
    assertThat(requestHandle.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(expectedResponseCode)
                // The Content-Encoding and Transfer-Encoding headers should have been redacted.
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .build());

    assertThat(requestHandle.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody);

    // Verify the network stats are accurate (they should count the request headers, URL, request
    // method, request body, response headers and *compressed* response body, since decompression
    // was performed by us and hence we were able to observe and count the compressed bytes).
    assertThat(
            JniHttpSentReceivedBytes.parseFrom(
                requestHandle.getTotalSentReceivedBytes(),
                ExtensionRegistryLite.getEmptyRegistry()))
        .isEqualTo(
            JniHttpSentReceivedBytes.newBuilder()
                .setSentBytes(
                    ("GET https://foo.com HTTP/1.1\r\n" + "Request-Header1: Foo\r\n" + "\r\n")
                        .length())
                .setReceivedBytes(
                    ("HTTP/1.1 200 OK\r\n"
                                + "Response-Header1: Bar\r\n"
                                + "Content-Encoding: gzip\r\n"
                                + "Transfer-Encoding: chunked\r\n"
                                + "\r\n")
                            .length()
                        + compressedResponseBody.size())
                .build());

    // Verify various important request properties.
    verify(mockConnection).setRequestMethod("GET");
    verify(mockConnection).addRequestProperty("Request-Header1", "Foo");
    verify(mockConnection).setRequestProperty("Accept-Encoding", "gzip");
  }

  @Test
  public void testGzipResponseBodyWithAcceptEncodingRequestHeaderShouldNotAutoDecompress()
      throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder()
                            .setName("Request-Header1")
                            .setValue("Foo")
                            .build())
                    .addExtraHeaders(
                        // We purposely use mixed-case, to ensure case-insensitive matching is used.
                        JniHttpHeader.newBuilder()
                            .setName("Accept-encoding")
                            .setValue("gzip,foobar")
                            .build())
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo.com")).thenReturn(mockConnection);

    int expectedResponseCode = 200;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);

    // Fake some response body data.
    String expectedResponseBody = "i_should_not_be_decompressed";
    when(mockConnection.getInputStream())
        .thenReturn(new ByteArrayInputStream(expectedResponseBody.getBytes(UTF_8)));
    // And add Content-Encoding and Content-Length headers (to check whether the first header is
    // correctly left *un*redacted, and the second is still redacted).
    when(mockConnection.getHeaderFields())
        .thenReturn(
            ImmutableMap.of(
                "Response-Header1",
                ImmutableList.of("Bar"),
                "Content-Encoding",
                ImmutableList.of("gzip"),
                "Content-Length",
                ImmutableList.of("9999")));

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Verify the results..
    requestHandle.assertSuccessfulCompletion();
    assertThat(requestHandle.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(expectedResponseCode)
                // The Content-Length header should have been redacted.
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Content-Encoding").setValue("gzip").build())
                .build());

    // The response body should have been returned without trying to decompress it.
    assertThat(requestHandle.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody);

    // Verify various important request properties.
    verify(mockConnection).setRequestMethod("GET");
    verify(mockConnection).addRequestProperty("Request-Header1", "Foo");
    // The Accept-Encoding header provided by the native layer should have been used, verbatim.
    verify(mockConnection).addRequestProperty("Accept-encoding", "gzip,foobar");
    verify(mockConnection, never()).setRequestProperty(eq("Accept-Encoding"), any());
  }

  @Test
  public void testChunkedTransferEncodingResponseHeaderShouldBeRemoved() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo.com")).thenReturn(mockConnection);

    int expectedResponseCode = 200;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);

    // Fake some response body data.
    String expectedResponseBody = "another_test_response_body";
    when(mockConnection.getInputStream())
        .thenReturn(new ByteArrayInputStream(expectedResponseBody.getBytes(UTF_8)));

    // And make the response headers include a "Transfer-Encoding: chunked" header, simulating the
    // case when HttpClientForNativeImpl is used with the JDK, which will un-chunk response data but
    // which will not remove the Transfer-Encoding header afterwards (contrary to Android's
    // HttpURLConnection implementation which *does* remove the header in this case).
    when(mockConnection.getHeaderFields())
        .thenReturn(
            ImmutableMap.of(
                "Response-Header1",
                ImmutableList.of("Bar"),
                "Transfer-Encoding",
                ImmutableList.of("chunked")));
    // Make the response body length *not* be known ahead of time (in accordance with the "chunked"
    // transfer encoding having been used.
    when(mockConnection.getContentLength()).thenReturn(-1);

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Verify the results.
    requestHandle.assertSuccessfulCompletion();
    assertThat(requestHandle.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(expectedResponseCode)
                // The Transfer-Encoding header should have been redacted.
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .build());
    assertThat(requestHandle.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody);
  }

  @Test
  public void testContentLengthResponseHeaderShouldDetermineReceivedBytesEstimate()
      throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo.com")).thenReturn(mockConnection);

    int expectedResponseCode = 200;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);
    when(mockConnection.getResponseMessage()).thenReturn("OK");

    // Fake some response body data.
    String expectedResponseBody = "another_test_response_body";
    when(mockConnection.getInputStream())
        .thenReturn(new ByteArrayInputStream(expectedResponseBody.getBytes(UTF_8)));

    // And make the response headers include a "Content-Length" header. The header should be ignored
    // for the most part, *but* it should be used to produce the final estimated 'received bytes'
    // statistic, if the request completes successfully.
    int expectedContentLength = 5;
    when(mockConnection.getHeaderFields())
        .thenReturn(
            ImmutableMap.of(
                "Response-Header1",
                ImmutableList.of("Bar"),
                // Simulate a Content-Length header that has value that is smaller than the length
                // of the response body we actually observe (e.g. a Cronet-based implementation has
                // decompressed the content for us, but still told us the original length).
                "Content-Length",
                ImmutableList.of(Integer.toString(expectedContentLength))));

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Verify the results.
    requestHandle.assertSuccessfulCompletion();
    assertThat(requestHandle.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(expectedResponseCode)
                // The Content-Length header should have been redacted.
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .build());
    assertThat(requestHandle.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody);

    // Verify the network stats are accurate (they should count the request headers, URL, request
    // method, request body, response headers and the *content length* rather than the observed
    // response body).
    assertThat(
            JniHttpSentReceivedBytes.parseFrom(
                requestHandle.getTotalSentReceivedBytes(),
                ExtensionRegistryLite.getEmptyRegistry()))
        .isEqualTo(
            JniHttpSentReceivedBytes.newBuilder()
                .setSentBytes("GET https://foo.com HTTP/1.1\r\n\r\n".length())
                .setReceivedBytes(
                    ("HTTP/1.1 200 OK\r\n"
                                + "Response-Header1: Bar\r\n"
                                + "Content-Length: 5\r\n"
                                + "\r\n")
                            .length()
                        + expectedContentLength)
                .build());
  }

  @Test
  public void testHttp2RequestsShouldUseEstimatedHeaderCompressionRatio() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection("https://foo.com")).thenReturn(mockConnection);

    int expectedResponseCode = 200;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);
    // Return an empty response message, which is the heuristic that indicates HTTP/2 was likely
    // used to service the request.
    when(mockConnection.getResponseMessage()).thenReturn("");

    // Fake some response body data.
    String expectedResponseBody = "another_test_response_body";
    when(mockConnection.getInputStream())
        .thenReturn(new ByteArrayInputStream(expectedResponseBody.getBytes(UTF_8)));

    // And make the response headers include a "Content-Length" header. The header should be ignored
    // for the most part, *but* it should be used to produce the final estimated 'received bytes'
    // statistic, if the request completes successfully.
    when(mockConnection.getHeaderFields())
        .thenReturn(ImmutableMap.of("Response-Header1", ImmutableList.of("Bar")));

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Verify the results.
    requestHandle.assertSuccessfulCompletion();
    assertThat(requestHandle.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(expectedResponseCode)
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .build());
    assertThat(requestHandle.responseBody.toString(UTF_8.name())).isEqualTo(expectedResponseBody);

    // Verify the network stats are accurate (they should count the request headers, URL, request
    // method, request body, response headers and the response body). Since HTTP/2 was used
    // (according to the heuristic), the request/response headers should have a compression factor
    // applied to them.
    assertThat(
            JniHttpSentReceivedBytes.parseFrom(
                requestHandle.getTotalSentReceivedBytes(),
                ExtensionRegistryLite.getEmptyRegistry()))
        .isEqualTo(
            JniHttpSentReceivedBytes.newBuilder()
                // Even though HTTP/2 was used, our sent/received bytes estimates hardcode an
                // assumption that HTTP/1.1-style status lines and CRLF-terminated headers were sent
                // received (and then simply applies a compression factor over the length of those
                // strings).
                .setSentBytes(
                    (long)
                        ("GET https://foo.com HTTP/1.1\r\n\r\n".length()
                            * ESTIMATED_HTTP2_HEADER_COMPRESSION_RATIO))
                .setReceivedBytes(
                    (long)
                            (("HTTP/1.1 200 \r\n" + "Response-Header1: Bar\r\n" + "\r\n").length()
                                * ESTIMATED_HTTP2_HEADER_COMPRESSION_RATIO)
                        + expectedResponseBody.length())
                .build());
  }

  @Test
  public void testPerformOnClosedRequestShouldThrow() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .build()
                    .toByteArray());

    // Close the request before we issue it.
    requestHandle.close();

    // Since performRequests wasn't called yet, no callbacks should've been invoked as a result of
    // the call to close().
    assertThat(requestHandle.responseError).isNull();
    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();

    // Try to perform the request, it should fail.
    CallFromNativeRuntimeException thrown =
        assertThrows(
            CallFromNativeRuntimeException.class,
            () -> httpClient.performRequests(new Object[] {requestHandle}));
    assertThat(thrown).hasCauseThat().isInstanceOf(IllegalStateException.class);
  }

  @Test
  public void testRequestWithAcceptEncodingHeaderIfNotSupportedShouldResultInError()
      throws Exception {
    // Disable support for the Accept-Encoding header.
    httpClient =
        new HttpClientForNativeImpl(
            TEST_CALL_FROM_NATIVE_WRAPPER,
            (request) ->
                new TestHttpRequestHandleImpl(
                    request,
                    urlConnectionFactory,
                    /*supportAcceptEncodingHeader=*/ false,
                    /*disableTimeouts=*/ false));

    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder().setName("Content-Length").setValue("1"))
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder().setName("Accept-Encoding").setValue("gzip"))
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    assertThat(requestHandle.responseError).isNotNull();
    assertThat(requestHandle.responseError.getCode()).isEqualTo(Code.INVALID_ARGUMENT_VALUE);
    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  @Test
  public void testNoBodyButHasRequestContentLengthShouldResultInError() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .addExtraHeaders(
                        JniHttpHeader.newBuilder().setName("Content-Length").setValue("1").build())
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    assertThat(requestHandle.responseError).isNotNull();
    assertThat(requestHandle.responseError.getCode()).isEqualTo(Code.INVALID_ARGUMENT_VALUE);
    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  /**
   * If something about the network OutputStream throws an exception during request body upload,
   * then we should return an error to native.
   */
  @Test
  public void testSendRequestBodyExceptionShouldResultInResponseError() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_POST)
                    .setHasBody(true)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    when(mockConnection.getOutputStream()).thenThrow(new IOException("my error"));

    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    assertThat(requestHandle.responseError).isNotNull();
    assertThat(requestHandle.responseError.getCode()).isEqualTo(Code.UNAVAILABLE_VALUE);
    assertThat(requestHandle.responseError.getMessage()).contains("IOException");
    assertThat(requestHandle.responseError.getMessage()).contains("my error");
    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  /**
   * If the request got cancelled during request body upload, then we should return a CANCELLED
   * error to native.
   */
  @Test
  public void testCancellationDuringSendRequestBodyShouldResultInResponseError() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_POST)
                    .setHasBody(true)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    when(mockConnection.getOutputStream())
        .thenAnswer(
            invocation -> {
              // Trigger the request cancellationF
              requestHandle.close();
              return new ByteArrayOutputStream();
            });

    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    assertThat(requestHandle.responseError).isNotNull();
    assertThat(requestHandle.responseError.getCode()).isEqualTo(Code.CANCELLED_VALUE);
    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  /**
   * If something fails when reading request body data from JNI, then we should *not* call any more
   * JNI callbacks again, since the native layer will already have handled the error.
   */
  @Test
  public void testReadRequestBodyFromNativeFailureShouldNotCallJNICallback() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_POST)
                    .setHasBody(true)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    when(mockConnection.getOutputStream()).thenReturn(new ByteArrayOutputStream());
    // Make the fake readRequestBody JNI method return an error.
    requestHandle.readRequestBodyResult = false;

    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    // No callbacks should have been invoked.
    assertThat(requestHandle.responseError).isNull();
    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  /** If establishing the connections fails, then we should return an error to native. */
  @Test
  public void testConnectExceptionShouldResultInResponseError() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    doThrow(new IOException("my error")).when(mockConnection).connect();

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseError).isNotNull();
    assertThat(requestHandle.responseError.getCode()).isEqualTo(Code.UNAVAILABLE_VALUE);
    assertThat(requestHandle.responseError.getMessage()).contains("IOException");
    assertThat(requestHandle.responseError.getMessage()).contains("my error");
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();

    // Verify that the request headers are counted in the network stats, since we can't really know
    // whether the connect() method failed before any network connection was established, or whether
    // it failed after we did already send our request onto the wire (each HttpURLConnection
    // implementation can have slightly different behavior in this regard).
    assertThat(
            JniHttpSentReceivedBytes.parseFrom(
                requestHandle.getTotalSentReceivedBytes(),
                ExtensionRegistryLite.getEmptyRegistry()))
        .isEqualTo(
            JniHttpSentReceivedBytes.newBuilder()
                .setSentBytes("GET https://foo.com HTTP/1.1\r\n\r\n".length())
                .setReceivedBytes(0)
                .build());
  }

  /**
   * If something about the network InputStream throws an exception during response headers
   * receiving, then we should return an error to native.
   */
  @Test
  public void testReceiveResponseHeadersExceptionShouldResultInResponseError() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    when(mockConnection.getResponseCode()).thenThrow(new IOException("my error"));

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseError).isNotNull();
    assertThat(requestHandle.responseError.getCode()).isEqualTo(Code.UNAVAILABLE_VALUE);
    assertThat(requestHandle.responseError.getMessage()).contains("IOException");
    assertThat(requestHandle.responseError.getMessage()).contains("my error");
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  /**
   * If something about the network InputStream throws a {@link java.net.SocketTimeoutException}
   * during response headers receiving, then we should return a specific DEADLINE_EXCEEDED error to
   * native.
   */
  @Test
  public void testReceiveResponseHeadersTimeoutExceptionShouldResultInResponseError()
      throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    when(mockConnection.getResponseCode()).thenThrow(new SocketTimeoutException("my error"));

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseError.getCode()).isEqualTo(Code.DEADLINE_EXCEEDED_VALUE);
    assertThat(requestHandle.responseError.getMessage()).contains("SocketTimeoutException");
    assertThat(requestHandle.responseError.getMessage()).contains("my error");
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  /**
   * If the request gets cancelled during response headers receiving, then we should return a
   * CANCELLED error to native.
   */
  @Test
  public void testCancellationDuringReceiveResponseHeadersShouldResultInResponseError()
      throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    when(mockConnection.getResponseCode())
        .thenAnswer(
            invocation -> {
              // Trigger a cancellation of the request.
              requestHandle.close();
              return 200;
            });

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseError.getCode()).isEqualTo(Code.CANCELLED_VALUE);
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  /**
   * If something fails when writing response header data to JNI, then we should *not* call any more
   * JNI callbacks again, since the native layer will already have handled the error.
   */
  @Test
  public void testWriteResponseHeadersToNativeFailureShouldNotCallJNICallback() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    int expectedResponseCode = 300;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);
    // Make the onResponseStarted JNI method fail when it receives the data.
    requestHandle.onResponseStartedResult = false;

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // No callbacks should have been invoked.
    assertThat(requestHandle.responseError).isNull();
    assertThat(requestHandle.responseProto).isNull();
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  /**
   * If something about the network InputStream throws an exception during response body download,
   * then we should return an error to native.
   */
  @Test
  public void testReceiveResponseBodyExceptionShouldResultInResponseBodyError() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    int expectedResponseCode = 300;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);
    when(mockConnection.getHeaderFields())
        .thenReturn(ImmutableMap.of("Response-Header1", ImmutableList.of("Bar")));

    // Make the response body input stream throw an exception.
    when(mockConnection.getInputStream()).thenThrow(new IOException("my error"));
    when(mockConnection.getContentLength()).thenReturn(-1);

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Despite having hit an IOException during the response body download, we should still first
    // have passed the response headers to native.
    assertThat(requestHandle.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(300)
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .build());
    assertThat(requestHandle.responseError).isNull();
    assertThat(requestHandle.responseBodyError).isNotNull();
    assertThat(requestHandle.responseBodyError.getCode()).isEqualTo(Code.UNAVAILABLE_VALUE);
    assertThat(requestHandle.responseBodyError.getMessage()).contains("IOException");
    assertThat(requestHandle.responseBodyError.getMessage()).contains("my error");
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  /**
   * If something fails when writing response body data to JNI, then we should *not* call any more
   * JNI callbacks again, since the native layer will already have handled the error.
   */
  @Test
  public void testWriteResponseBodyToNativeFailureShouldNotCallJNICallback() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    int expectedResponseCode = 300;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);
    when(mockConnection.getHeaderFields())
        .thenReturn(ImmutableMap.of("Response-Header1", ImmutableList.of("Bar")));

    // Make the response body contain some data.
    when(mockConnection.getInputStream())
        .thenReturn(new ByteArrayInputStream("test_response".getBytes(UTF_8)));
    when(mockConnection.getContentLength()).thenReturn(-1);
    // But make the onResponseBody JNI method fail when it receives the data.
    requestHandle.onResponseBodyResult = false;

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Despite having hit an IOException during the response body download, we should still first
    // have passed the response headers to native.
    assertThat(requestHandle.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(300)
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .build());
    // No callbacks should have been invoked after we received the response headers.
    assertThat(requestHandle.responseError).isNull();
    assertThat(requestHandle.responseBodyError).isNull();
    assertThat(requestHandle.completedSuccessfully).isFalse();
  }

  @Test
  public void testCancellationDuringReceiveResponseBodyShouldResultInError() throws Exception {
    TestHttpRequestHandleImpl requestHandle =
        (TestHttpRequestHandleImpl)
            httpClient.enqueueRequest(
                JniHttpRequest.newBuilder()
                    .setUri("https://foo.com")
                    .setMethod(JniHttpMethod.HTTP_METHOD_GET)
                    .setHasBody(false)
                    .build()
                    .toByteArray());

    HttpURLConnection mockConnection = mock(HttpURLConnection.class);
    when(urlConnectionFactory.createUrlConnection(any())).thenReturn(mockConnection);

    int expectedResponseCode = 300;
    when(mockConnection.getResponseCode()).thenReturn(expectedResponseCode);
    when(mockConnection.getResponseMessage()).thenReturn("Multiple Choices");
    when(mockConnection.getHeaderFields())
        .thenReturn(
            ImmutableMap.of(
                "Response-Header1", ImmutableList.of("Bar"),
                // The Content-Length header should be ignored, and should *not* be used to estimate
                // the 'received bytes', since the request will not complete successfully.
                "Content-Length", ImmutableList.of("9999")));

    // Make the response body contain some data. But when the data gets read, the request gets
    // cancelled.
    String fakeResponseBody = "test_response";
    when(mockConnection.getInputStream())
        .thenAnswer(
            invocation -> {
              requestHandle.close();
              return new ByteArrayInputStream(fakeResponseBody.getBytes(UTF_8));
            });
    when(mockConnection.getContentLength()).thenReturn(-1);

    // Run the request.
    byte[] result = httpClient.performRequests(new Object[] {requestHandle});
    assertThat(Status.parseFrom(result, ExtensionRegistryLite.getEmptyRegistry()).getCode())
        .isEqualTo(Code.OK_VALUE);

    requestHandle.close();

    // Despite having hit a cancellation during the response body download, we should still first
    // have passed the response headers to native.
    assertThat(requestHandle.responseProto)
        .isEqualTo(
            JniHttpResponse.newBuilder()
                .setCode(300)
                .addHeaders(
                    JniHttpHeader.newBuilder().setName("Response-Header1").setValue("Bar").build())
                .build());
    assertThat(requestHandle.responseError).isNull();
    // The response body should not have been read to completion, since the request got cancelled
    // in the middle of the read.
    assertThat(requestHandle.responseBody.toString(UTF_8.name())).isNotEqualTo(fakeResponseBody);
    assertThat(requestHandle.responseBodyError).isNotNull();
    assertThat(requestHandle.responseBodyError.getCode()).isEqualTo(Code.CANCELLED_VALUE);
    assertThat(requestHandle.completedSuccessfully).isFalse();

    // Verify the network stats are accurate (they should count the request headers, URL, request
    // method, request body, response headers and the *content length* rather than the observed
    // response body).
    assertThat(
            JniHttpSentReceivedBytes.parseFrom(
                requestHandle.getTotalSentReceivedBytes(),
                ExtensionRegistryLite.getEmptyRegistry()))
        .isEqualTo(
            JniHttpSentReceivedBytes.newBuilder()
                .setSentBytes("GET https://foo.com HTTP/1.1\r\n\r\n".length())
                // The Content-Length response header value should not be taken into account in the
                // estimated 'received bytes' stat, since the request did not succeed. Instead,
                // by the time the HttpRequestHandleImpl#close() method is called we will be in the
                // process of having read a single buffer's worth of response body data, and hence
                // that's the amount of response body data that should be accounted for. This
                // ensures that we try as best as possible to only count bytes we actually received
                // up until the point of cancellation.
                .setReceivedBytes(
                    ("HTTP/1.1 300 Multiple Choices\r\n"
                                + "Response-Header1: Bar\r\n"
                                + "Content-Length: 9999\r\n"
                                + "\r\n")
                            .length()
                        + DEFAULT_TEST_CHUNK_BUFFER_SIZE)
                .build());
  }
}
