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

import static com.google.common.base.Strings.nullToEmpty;

import com.google.common.base.Ascii;
import com.google.common.io.CountingInputStream;
import com.google.errorprone.annotations.concurrent.GuardedBy;
import com.google.fcp.client.CallFromNativeWrapper;
import com.google.fcp.client.http.HttpClientForNative.HttpRequestHandle;
import com.google.fcp.client.http.HttpClientForNativeImpl.UncheckedHttpClientForNativeException;
import com.google.rpc.Code;
import com.google.rpc.Status;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.CookieHandler;
import java.net.HttpURLConnection;
import java.net.ProtocolException;
import java.net.SocketTimeoutException;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.zip.GZIPInputStream;
import javax.annotation.Nullable;

/**
 * An implementation of {@link HttpRequestHandle} that uses {@link HttpURLConnection} (the
 * implementation of which is provided via the {@link HttpURLConnectionFactory} indirection) for
 * issuing network requests.
 *
 * <p>Note: this class is non-final to allow the native callback methods defined in {@link
 * HttpRequestHandle} to be overridden in unit tests. This is class is otherwise not meant to be
 * extended and should hence be considered effectively 'final'.
 */
public class HttpRequestHandleImpl extends HttpRequestHandle {

  /**
   * A factory for creating an {@link HttpURLConnection} for a given URI.
   *
   * <p>To use the system's default {@link HttpURLConnection} implementation one can simply use
   * {@code new URL(uri).openConnection()} as the factory implementation.
   */
  public interface HttpURLConnectionFactory {
    HttpURLConnection createUrlConnection(String uri) throws IOException;
  }

  // The Content-Length header name.
  private static final String CONTENT_LENGTH_HEADER = "Content-Length";
  // The Accept-Encoding header name.
  private static final String ACCEPT_ENCODING_HEADER = "Accept-Encoding";
  // The Content-Encoding response header name.
  private static final String CONTENT_ENCODING_HEADER = "Content-Encoding";
  // The Accept-Encoding and Content-Encoding value to indicate gzip-based compression.
  private static final String GZIP_ENCODING = "gzip";
  // The Transfer-Encoding header name.
  private static final String TRANSFER_ENCODING_HEADER = "Transfer-Encoding";
  // The Transfer-Encoding value indicating "chunked" encoding.
  private static final String CHUNKED_TRANSFER_ENCODING = "chunked";

  /** Used to indicate that the request was invalid in some way. */
  private static final class InvalidHttpRequestException extends Exception {
    private InvalidHttpRequestException(String message) {
      super(message);
    }

    private InvalidHttpRequestException(String message, Throwable cause) {
      super(message, cause);
    }
  }

  /**
   * Used to indicate that the request was cancelled or encountered an unrecoverable error in the
   * middle of an operation. The request should be aborted without invoking any further callbacks to
   * the native layer.
   */
  private static final class AbortRequestException extends Exception {}

  private enum State {
    /**
     * The state when this object is created, but before it has been passed to {@link
     * HttpClientForNative#performRequests}.
     */
    NOT_STARTED,
    /**
     * The state before any response headers have been received. Errors should go to the {@link
     * #onResponseError} callback.
     */
    BEFORE_RESPONSE_HEADERS,
    /**
     * The state after any response headers have been received. Errors should go to the {@link
     * #onResponseBodyError} callback.
     */
    AFTER_RESPONSE_HEADERS,
    /**
     * The state after the request was finished (either successfully, with an error, or via
     * cancellation), and no more callbacks should be invoked.
     */
    CLOSED
  }

  private final JniHttpRequest request;
  private final CallFromNativeWrapper callFromNativeWrapper;
  private final ExecutorService executorService;
  private final HttpURLConnectionFactory urlConnectionFactory;
  private final int connectTimeoutMs;
  private final int readTimeoutMs;
  private final int requestBodyChunkSizeBytes;
  private final int responseBodyChunkSizeBytes;
  private final int responseBodyGzipBufferSizeBytes;
  private final boolean callDisconnectWhenCancelled;
  private final boolean supportAcceptEncodingHeader;
  private final double estimatedHttp2HeaderCompressionRatio;

  // Until we have an actual connection, this is a no-op.
  @GuardedBy("this")
  private Runnable disconnectRunnable = () -> {};

  @GuardedBy("this")
  private State state = State.NOT_STARTED;

  @GuardedBy("this")
  @Nullable
  private Future<?> ongoingWork;

  // These are "volatile" and not synchronized so that they can be read easily from any thread even
  // if the lock is currently held. They're only incremented from a single thread, so their being
  // volatile is sufficient to safely increment/update them.
  private volatile long sentHeaderBytes = 0;
  private volatile long sentBodyBytes = 0;
  private volatile long receivedHeaderBytes = 0;
  private volatile long receivedBodyBytes = 0;
  private volatile boolean requestUsedHttp2Heuristic = false;

  /**
   * Creates a new handle representing a single request. See {@link HttpClientForNativeImpl} for a
   * description of the parameters.
   *
   * @param request the {@link JniHttpRequest} the handle is being created for.
   * @param callFromNativeWrapper the wrapper to use for all calls that arrive over JNI, to ensure
   *     uncaught exceptions are handled correctly.
   * @param executorService the {@link ExecutorService} to use for background work.
   * @param urlConnectionFactory the factory to use to instance new {@link HttpURLConnection}s.
   * @param connectTimeoutMs the value to use with {@link HttpURLConnection#setConnectTimeout(int)}.
   *     If this is -1 then {@code setConnectTimeout} will not be called at all.
   * @param readTimeoutMs the value to use with {@link HttpURLConnection#setReadTimeout(int)}. If
   *     this is -1 then {@code setReadTimeout} will not be called at all.
   *     <p>If {@code getInputStream().read(...)} or other methods like {@code getResponseCode()}
   *     take longer than this amount of time, they will throw a {@link
   *     java.net.SocketTimeoutException} and request will fail. Setting it to -1 will result in an
   *     infinite timeout being used.
   *     <p>Note that this only affects the reads of the response body, and does not affect the
   *     writes of the request body.
   * @param requestBodyChunkSizeBytes the value to use with {@link
   *     HttpURLConnection#setChunkedStreamingMode(int)}, when chunked transfer encoding is used to
   *     upload request bodies. This also determines the amount of request body data we'll read from
   *     the native layer before pushing it onto the network's {@link java.io.OutputStream}.
   * @param responseBodyChunkSizeBytes determines the amount of response body data we'll try to read
   *     from the network's {@link java.io.InputStream} (or from the {@link
   *     java.util.zip.GZIPInputStream} wrapping the network's {@code InputStream}) before pushing
   *     it to the native layer.
   * @param responseBodyGzipBufferSizeBytes determines the amount of response body data the {@link
   *     java.util.zip.GZIPInputStream} wrapping the network's {@link java.io.InputStream} will try
   *     to read before starting another round of decompression (in case we receive a compressed
   *     response body that we need to decompress on the fly).
   * @param callDisconnectWhenCancelled whether to call {@link HttpURLConnection#disconnect()} (from
   *     a different thread than the request is being run on) when a request gets cancelled. See
   *     note in {@link HttpRequestHandleImpl#close()}.
   * @param supportAcceptEncodingHeader whether to set the "Accept-Encoding" request header by
   *     default. Some {@link HttpURLConnection} implementations don't allow setting it, and this
   *     flag allows turning that behavior off. When this setting is false, the assumption is that
   *     the implementation at the very least sets "Accept-Encoding: gzip" (as required by the C++
   *     `HttpClient` contract).
   * @param estimatedHttp2HeaderCompressionRatio the compression ratio to account for in the
   *     calculation of sent/received bytes estimates for the header data, in case HTTP/2 is used
   *     for the request. HTTP/2 supports HPACK, and hence counting the header data in uncompressed
   *     form likely results in over-estimates. This only affects requests that are determined to
   *     have used HTTP/2, which is based on the somewhat fragile heuristic of whether {@link
   *     HttpURLConnection#getResponseMessage()} is empty (since HTTP/2 does not support status line
   *     'reason phrases').
   */
  public HttpRequestHandleImpl(
      JniHttpRequest request,
      CallFromNativeWrapper callFromNativeWrapper,
      ExecutorService executorService,
      HttpURLConnectionFactory urlConnectionFactory,
      int connectTimeoutMs,
      int readTimeoutMs,
      int requestBodyChunkSizeBytes,
      int responseBodyChunkSizeBytes,
      int responseBodyGzipBufferSizeBytes,
      boolean callDisconnectWhenCancelled,
      boolean supportAcceptEncodingHeader,
      double estimatedHttp2HeaderCompressionRatio) {
    this.request = request;
    this.callFromNativeWrapper = callFromNativeWrapper;
    this.executorService = executorService;
    this.urlConnectionFactory = urlConnectionFactory;
    this.connectTimeoutMs = connectTimeoutMs;
    this.readTimeoutMs = readTimeoutMs;
    this.requestBodyChunkSizeBytes = requestBodyChunkSizeBytes;
    this.responseBodyChunkSizeBytes = responseBodyChunkSizeBytes;
    this.responseBodyGzipBufferSizeBytes = responseBodyGzipBufferSizeBytes;
    this.callDisconnectWhenCancelled = callDisconnectWhenCancelled;
    this.supportAcceptEncodingHeader = supportAcceptEncodingHeader;
    this.estimatedHttp2HeaderCompressionRatio = estimatedHttp2HeaderCompressionRatio;
  }

  @Override
  public final void close() {
    // This method is called when the request should be cancelled and/or is otherwise not
    // needed anymore. It may be called from any thread.
    callFromNativeWrapper.wrapVoidCall(
        () -> {
          synchronized (this) {
            // If the request was already closed, then this means that the request was either
            // already interrupted before, or that the request completed successfully. In both
            // cases there's nothing left to do for us.
            if (state == State.CLOSED) {
              return;
            }
            // Otherwise, this indicates that the request is being *cancelled* while it was still
            // running.

            // We mark the connection closed, to prevent any further callbacks to the native layer
            // from being issued. We do this *before* invoking the callback, just in case our
            // invoking the callback causes this close() method to be invoked again by the native
            // layer (we wouldn't want to enter an infinite loop)
            State oldState = state;
            state = State.CLOSED;
            // We signal the closure/cancellation to the native layer right away, using the
            // appropriate callback for the state we were in.
            doError(Code.CANCELLED, "request cancelled via close()", oldState);
            // We unblock the blocked thread on which HttpClientForNativeImpl#performRequests was
            // called (although that thread may be blocked on other, still-pending requests).
            if (ongoingWork != null) {
              ongoingWork.cancel(/* mayInterruptIfRunning=*/ true);
            }

            // Note that HttpURLConnection isn't documented to be thread safe, and hence it isn't
            // 100% clear that calling its #disconnect() method from a different thread (as we are
            // about to do here) will correctly either. However, it seems to be the only way to
            // interrupt an ongoing request when it is blocked writing to or reading from the
            // network socket.
            //
            // At least on Android the OkHttp-based implementation does seem to be thread safe (it
            // uses OkHttp's HttpEngine.cancel() method, which is thread safe). The JDK
            // implementation seems to not be thread safe (but behaves well enough?). The
            // callDisconnectWhenCancelled parameter can be used to control this behavior.
            if (callDisconnectWhenCancelled) {
              disconnectRunnable.run();
            }

            // Handling cancellations/closures this way ensures that the native code is unblocked
            // even before the network requests have been fully aborted. Any still-pending HTTP
            // connections will be cleaned up in their corresponding background threads.
          }
        });
  }

  @Override
  public byte[] getTotalSentReceivedBytes() {
    double headerCompressionRatio =
        requestUsedHttp2Heuristic ? estimatedHttp2HeaderCompressionRatio : 1.0;
    // Note that this estimate of sent/received bytes is not necessarily monotonically increasing:
    // - We'll initially estimate the amount of received response body bytes based on the bytes we
    //   observe in the response InputStream (which may count the uncompressed response bytes). This
    //   will account, as best as possible, for how much has data been received so far (incl. in
    //   case the request gets cancelled mid-flight), although it may be an over-estimate due to not
    //   accounting for response body compression (depending on the HttpURLConnection
    //   implementation, e.g. in case of Cronet's).
    // - Once the request has completed successfully, we'll estimate the received response body
    //   bytes based on the Content-Length response header, if there was one. This gives us a chance
    //   to revise our estimate down to a more accurate value, if the HttpURLConnection
    //   implementation exposes the original Content-Length header to us (e.g. in the case of
    //   Cronet).
    // - Once we know from the response headers that the request used HTTP/2, we'll apply the header
    //   compression ratio. But before we know that, we don't apply it.
    //
    // Note that the estimates we provide here also won't take into account various other sources of
    // network usage: the bytes transmitted to establish TLS channels, request/responses for
    // followed HTTP redirects, HTTP/1.1-to-HTTP/2 upgrades etc.
    return JniHttpSentReceivedBytes.newBuilder()
        .setSentBytes((long) (sentHeaderBytes * headerCompressionRatio) + sentBodyBytes)
        .setReceivedBytes((long) (receivedHeaderBytes * headerCompressionRatio) + receivedBodyBytes)
        .build()
        .toByteArray();
  }

  final synchronized void performRequest() {
    if (state != State.NOT_STARTED) {
      throw new IllegalStateException("must not call perform() more than once");
    }
    state = State.BEFORE_RESPONSE_HEADERS;
    ongoingWork = executorService.submit(this::runRequestToCompletion);
  }

  final void waitForRequestCompletion() {
    // Get a copy of the Future, if it is set. Then call .get() without holding the lock.
    Future<?> localOngoingWork;
    synchronized (this) {
      if (ongoingWork == null) {
        throw new IllegalStateException("must not call waitForCompletion() before perform()");
      }
      localOngoingWork = ongoingWork;
    }
    try {
      localOngoingWork.get();
    } catch (ExecutionException e) {
      // This shouldn't happen, since the run(...) method shouldn't throw any exceptions. If one
      // does get thrown, it is a RuntimeException or Error, in which case we'll just let it bubble
      // up to the uncaught exception handler.
      throw new UncheckedHttpClientForNativeException("unexpected exception", e);
    } catch (InterruptedException e) {
      // This shouldn't happen, since no one should be interrupting the calling thread.
      throw new UncheckedHttpClientForNativeException("unexpected interruption", e);
    } catch (CancellationException e) {
      // Do nothing. This will happen when a request gets cancelled in the middle of execution, but
      // in those cases there's nothing left for us to do, and we should just gracefully return.
      // This will allow #performRequests(...) to be unblocked, while the background thread may
      // still be cleaning up some resources.
    }
  }

  /** Convenience method for checking for the closed state, in a synchronized fashion. */
  private synchronized boolean isClosed() {
    return state == State.CLOSED;
  }

  /**
   * Convenience method for checking for the closed state in a synchronized fashion, throwing an
   * {@link AbortRequestException} if the request is closed.
   */
  private synchronized void checkClosed() throws AbortRequestException {
    if (state == State.CLOSED) {
      throw new AbortRequestException();
    }
  }

  /**
   * Calls either the {@link #onResponseError} or {@link #onResponseBodyError} callback, including
   * the originating Java exception description in the status message. Which callback is used
   * depends on the current {@link #state}.
   */
  private synchronized void doError(String message, Exception e) {
    // We mark the state as CLOSED, since no more callbacks should be invoked after signaling an
    // error. We do this before issuing the callback to the native layer, to ensure that if that
    // call results in another call to the Java layer, we don't emit any callbacks anymore.
    State oldState = state;
    state = State.CLOSED;
    Code code = Code.UNAVAILABLE;
    if (e instanceof SocketTimeoutException) {
      code = Code.DEADLINE_EXCEEDED;
    } else if (e instanceof InvalidHttpRequestException) {
      code = Code.INVALID_ARGUMENT;
    }
    doError(code, String.format("%s (%s)", message, e), oldState);
  }

  @GuardedBy("this")
  private void doError(Code code, String message, State state) {
    byte[] error =
        Status.newBuilder().setCode(code.getNumber()).setMessage(message).build().toByteArray();
    switch (state) {
      case BEFORE_RESPONSE_HEADERS:
        onResponseError(error);
        break;
      case AFTER_RESPONSE_HEADERS:
        onResponseBodyError(error);
        break;
      case NOT_STARTED:
      case CLOSED:
        // If the request had already been closed, or if it hadn't been passed to {@link
        // HttpClientForNative#performRequests} yet, then we shouldn't issue any (further)
        // callbacks.
        break;
    }
  }

  /** Calls the {@link #readRequestBody} callback, but only if the request isn't closed yet. */
  private synchronized void doReadRequestBody(
      byte[] buffer, long requestedBytes, int[] actualBytesRead) throws AbortRequestException {
    // If the request has already been closed, then we shouldn't issue any further callbacks.
    checkClosed();
    checkCallToNativeResult(readRequestBody(buffer, requestedBytes, actualBytesRead));
  }

  /** Calls the {@link #onResponseStarted} callback, but only if the request isn't closed yet. */
  private synchronized void doOnResponseStarted(byte[] responseProto) throws AbortRequestException {
    // Ensure that we call the onResponseStarted callback *and* update the object state as a
    // single atomic transaction, so that any errors/cancellations occurring before or after
    // this block result in the correct error callback being called.

    // If the request has already been closed, then we shouldn't issue any further callbacks.
    checkClosed();
    // After this point, any errors we signal to the native layer should go through
    // 'onResponseBodyError', so we update the object state. We do this before invoking the
    // callback, to ensure that if our call into the native layer causes a call back into Java
    // that then triggers an error callback, we invoke the right one.
    state = State.AFTER_RESPONSE_HEADERS;
    checkCallToNativeResult(onResponseStarted(responseProto));
  }

  /** Calls the {@link #onResponseBody} callback, but only if the request isn't closed yet. */
  private synchronized void doOnResponseBody(byte[] buffer, int bytesAvailable)
      throws AbortRequestException {
    // If the request has already been closed, then we shouldn't issue any further callbacks.
    checkClosed();
    checkCallToNativeResult(onResponseBody(buffer, /* dataOffset= */ 0, bytesAvailable));
  }

  /** Calls the {@link #onResponseCompleted} callback, but only if the request isn't closed yet. */
  private synchronized void doOnResponseCompleted(long originalContentLengthHeader) {
    // If the request has already been closed, then we shouldn't issue any further callbacks.
    if (state == State.CLOSED) {
      return;
    }
    // If we did receive a Content-Length header, then once we've fully completed the request, we
    // can use it to estimate the total received bytes (and it will be the most accurate estimate
    // available to us).
    //
    // E.g. the Cronet HttpURLConnection implementation will return the original Content-Length
    // header, even though it decompresses any response body Content-Encoding for us and doesn't let
    // use see the original compressed bytes.
    //
    // If there was no Content-Length header at all, then we must go by our own calculation of the
    // number of received bytes (i.e. based on the bytes we observed in the response InputStream).
    if (originalContentLengthHeader > -1) {
      receivedBodyBytes = originalContentLengthHeader;
    }
    // If the request hadn't already been closed, it should be considered closed now (since we're
    // about to call the final callback).
    state = State.CLOSED;
    onResponseCompleted();
  }

  /**
   * Transitions to the CLOSED {@link #state} and throws an AbortRequestException, if the given
   * result from a call to the native layer is false.
   */
  @GuardedBy("this")
  private void checkCallToNativeResult(boolean result) throws AbortRequestException {
    if (!result) {
      // If any call to the native layer fails, then we shouldn't invoke any more callbacks.
      state = State.CLOSED;
      throw new AbortRequestException();
    }
  }

  private void runRequestToCompletion() {
    // If we're already closed by the time the background thread started executing this method,
    // there's nothing left to do for us.
    if (isClosed()) {
      return;
    }

    // Create the HttpURLConnection instance (this usually doesn't do any real work yet, even
    // though it is declared to throw IOException).
    HttpURLConnection connection;
    try {
      connection = urlConnectionFactory.createUrlConnection(request.getUri());
    } catch (IOException e) {
      doError("failure during connection creation", e);
      return;
    }

    // Register a runnable that will allow us to cancel an ongoing request from a different
    // thread.
    synchronized (this) {
      disconnectRunnable = connection::disconnect;
    }

    // From this point on we should call connection.disconnect() at the end of this method
    // invocation, *except* when the request reaches a successful end (see comment below).
    boolean doDisconnect = true;
    try {
      // Set and validate connection parameters (timeouts, HTTP method, request body, etc.).
      String acceptEncodingHeader = findRequestHeader(ACCEPT_ENCODING_HEADER);
      long requestContentLength;
      try {
        requestContentLength = parseContentLengthHeader(findRequestHeader(CONTENT_LENGTH_HEADER));
        configureConnection(connection, requestContentLength, acceptEncodingHeader);
      } catch (InvalidHttpRequestException e) {
        doError("invalid request", e);
        return;
      }

      // If there is a request body then start sending it. This is usually when the actual network
      // connection is first established (subject to the #getRequestConnectTimeoutMs).
      if (request.getHasBody()) {
        try {
          sendRequestBody(connection, requestContentLength);
        } catch (IOException e) {
          doError("failure during request body send", e);
          return;
        }
      }

      // Check one more time, before waiting on the response headers, if the request has already
      // been cancelled (to avoid starting any blocking network IO we can't easily interrupt).
      checkClosed();

      // If there was no request body, then this will establish the connection (subject to the
      // #getRequestConnectTimeoutMs). If there was a request body, this will be a noop.
      try {
        connection.connect();
      } catch (IOException e) {
        doError("failure during connect", e);
        return;
      }

      // Wait for the request headers to be received (subject to #getRequestReadTimeOutMs).
      ResponseHeadersWithMetadata response;
      try {
        response = receiveResponseHeaders(connection, acceptEncodingHeader);
      } catch (IOException e) {
        doError("failure during response header receive", e);
        return;
      }
      doOnResponseStarted(response.responseProto.toByteArray());

      try {
        receiveResponseBody(connection, response.shouldDecodeGzip);
      } catch (IOException e) {
        doError("failure during response body receive", e);
        return;
      }
      // Note that we purposely don't call connection.disconnect() once we reach this point, since
      // we will have gracefully finished the request (e.g. by having readall of its response data),
      // and this means that the underlying socket/connection may be reused for other connections to
      // the same endpoint. Calling connection.disconnect() would prevent such connection reuse,
      // which can be detrimental to the overall throughput. The underlying HttpURLConnection
      // implementation will eventually reap the socket if doesn't end up being reused within a set
      // amount of time.
      doDisconnect = false;
      doOnResponseCompleted(response.originalContentLengthHeader);
    } catch (AbortRequestException e) {
      // Nothing left for us to do.
    } finally {
      if (doDisconnect) {
        connection.disconnect();
      }
      // At this point we will either have reached the end of the request successfully (in which
      // case doOnResponseCompleted will have updated the object state to CLOSED), or we will have
      // hit a AbortRequestException (in which case the state will already have been set to CLOSED),
      // or we will have signaled an error (which will have set the state to CLOSED as well).
      // Hence we don't have to modify the object state here anymore.
    }
  }

  /** Returns the HTTP request method we should use, as a string. */
  private String getRequestMethod() {
    switch (request.getMethod()) {
      case HTTP_METHOD_HEAD:
        return "HEAD";
      case HTTP_METHOD_GET:
        return "GET";
      case HTTP_METHOD_POST:
        return "POST";
      case HTTP_METHOD_PUT:
        return "PUT";
      case HTTP_METHOD_PATCH:
        return "PATCH";
      case HTTP_METHOD_DELETE:
        return "DELETE";
      default:
        // This shouldn't happen, as it would indicate a bug in either this code or the native C++
        // code calling us.
        throw new UncheckedHttpClientForNativeException(
            String.format("unexpected method: %s", request.getMethod().getNumber()));
    }
  }

  /**
   * Finds the given header (case-insensitively) and returns its value, if there is one. Otherwise
   * returns null.
   */
  @Nullable
  private String findRequestHeader(String name) {
    for (JniHttpHeader header : request.getExtraHeadersList()) {
      if (Ascii.equalsIgnoreCase(name, header.getName())) {
        return header.getValue();
      }
    }
    return null;
  }

  /**
   * Tries to parse a "Content-Length" header value and returns it as a long, if it isn't null.
   * Otherwise returns -1.
   */
  private long parseContentLengthHeader(String contentLengthHeader)
      throws InvalidHttpRequestException {
    if (contentLengthHeader == null) {
      return -1;
    }
    try {
      return Long.parseLong(contentLengthHeader);
    } catch (NumberFormatException e) {
      throw new InvalidHttpRequestException(
          String.format("invalid Content-Length request header value: %s", contentLengthHeader), e);
    }
  }

  /**
   * Configures the {@link HttpURLConnection} object before it is used to establish the actual
   * network connection.
   */
  @SuppressWarnings("NonAtomicVolatileUpdate")
  private void configureConnection(
      HttpURLConnection connection,
      long requestContentLength,
      @Nullable String acceptEncodingHeader)
      throws InvalidHttpRequestException {
    String requestMethod = getRequestMethod();
    try {
      connection.setRequestMethod(requestMethod);
    } catch (ProtocolException e) {
      // This should never happen, as we take care to only call this method with appropriate
      // parameters.
      throw new UncheckedHttpClientForNativeException("unexpected ProtocolException", e);
    }
    for (JniHttpHeader header : request.getExtraHeadersList()) {
      // Note that we use addRequestProperty rather than setRequestProperty, to ensure that
      // request headers that occur multiple times are properly specified (rather than just the
      // last value being specified).
      connection.addRequestProperty(header.getName(), header.getValue());
    }
    // The C++ `HttpClient` contract requires us to set the Accept-Encoding header, if there isn't
    // one provided by the native layer. Note that on Android the HttpURLConnection implementation
    // does this by default, but the JDK's implementation does not. Note that by setting this header
    // we must also handle the response InputStream data correctly (by inflating it, if the
    // Content-Encoding indicates the data is compressed).
    // Some HttpURLConnection implementations (such as Cronet's) don't allow setting this header,
    // and print out a warning if you do. The supportAcceptEncodingHeader allows turning this
    // behavior off (thereby avoiding the warning being logged).
    if (supportAcceptEncodingHeader && acceptEncodingHeader == null) {
      connection.setRequestProperty(ACCEPT_ENCODING_HEADER, GZIP_ENCODING);
    } else if (!supportAcceptEncodingHeader && acceptEncodingHeader != null) {
      throw new InvalidHttpRequestException("cannot support Accept-Encoding header");
    }

    if (connectTimeoutMs >= 0) {
      connection.setConnectTimeout(connectTimeoutMs);
    }
    if (readTimeoutMs >= 0) {
      connection.setReadTimeout(readTimeoutMs);
    }

    connection.setDoInput(true);
    if (request.getHasBody()) {
      connection.setDoOutput(true);
      if (requestContentLength >= 0) {
        // If the Content-Length header is set then we don't have to use Transfer-Encoding, since
        // we know the size of the request body ahead of time.
        connection.setFixedLengthStreamingMode(requestContentLength);
      } else {
        // If we don't know the size of the request body ahead of time, we should turn on
        // "Transfer-Encoding: chunked" using the following method.
        connection.setChunkedStreamingMode(requestBodyChunkSizeBytes);
      }
    } else if (requestContentLength > 0) {
      // If getHasBody() is false but a non-zero Content-Length header is set, then something went
      // wrong in the native layer.
      throw new InvalidHttpRequestException("Content-Length > 0 but no request body available");
    }

    // As per the interface contract in C++'s http_client.h, we should not use any caches.
    connection.setUseCaches(false);
    // As per the interface contract in C++'s http_client.h, we should follow redirects.
    connection.setInstanceFollowRedirects(true);

    // Ensure that no system-wide CookieHandler was installed, since we must not store any cookies.
    if (CookieHandler.getDefault() != null) {
      throw new IllegalStateException("must not set a CookieHandler");
    }

    // Count the request headers as part of the sent bytes. We do this before we actually open the
    // connection, so that if the connection fails to be established we still account for the
    // possibly already-transmitted data.
    //
    // Note that if the implementation uses HTTP2 with HPACK header compression this could lead to
    // an overestimation of the total bytes sent. The estimatedHttp2HeaderCompressionRatio parameter
    // can be used to account for this heuristically.
    //
    // If HTTP/2 is used, then some of our estimates will also be overestimates since we assume that
    // headers are terminated by \r\n lines etc., while HTTP/2 generally represents headers more
    // compactly. To avoid complicating things too much, we don't account for that.
    //
    // Aside from not accounting for HTTP/2 and header compression, some request headers may also be
    // set by the HttpUrlConnection implementation which we cannot observe here, and hence we won't
    // be counting those either. Hence, this number could be both an over or under-estimate, and
    // should really be considered a best-effort estimate.
    //
    // Note that while it might seem we could use getRequestProperties() to get the actual request
    // headers (incl. implementation-specified ones), this isn't actually the case for most
    // HttpURLConnection implementations (and some implementations don't return anything from
    // getRequestProperties(), even if we've already called addRequestProperty()).
    // First, account for the HTTP request status line.
    sentHeaderBytes +=
        requestMethod.length()
            + " ".length()
            + request.getUri().length()
            + " HTTP/1.1\r\n".length();
    // Then account for each header we know is will be included.
    for (JniHttpHeader header : request.getExtraHeadersList()) {
      // Each entry should count the lengths of the header name + header value (rather than only
      // counting the header name length once), since duplicated headers are likely to be sent in
      // separate header lines (rather than being coalesced into a single header line by the
      // HttpURLConnection implementation).
      sentHeaderBytes +=
          header.getName().length() + ": ".length() + header.getValue().length() + "\r\n".length();
    }
    // Account for the \r\n characters at the end of the request headers.
    sentHeaderBytes += "\r\n".length();
  }

  /**
   * Sends the request body (received from the native layer via the JNI callbacks) to the server
   * after establishing a connection (blocking until all request body data has been written to the
   * network, or an error occurs).
   *
   * @param connection the HttpURLConnection to send the request body for.
   * @param requestContentLength the length of the request body if it is known ahead of time, or -1
   *     if the request body's length is not known ahead of time.
   */
  @SuppressWarnings("NonAtomicVolatileUpdate")
  private void sendRequestBody(HttpURLConnection connection, long requestContentLength)
      throws IOException, AbortRequestException {
    // Check one more time, before issuing the request, if it's already been cancelled (to avoid
    // starting any blocking network IO we can't easily interrupt).
    checkClosed();

    // Note that we don't wrap the OutputStream in a BufferedOutputStream, since we already write
    // data to the unbuffered OutputStream in fairly large chunks at a time, so adding another
    // buffering layer in between isn't helpful.
    //
    // The call to getOutputStream or OutputStream.write() is what will establish the actual
    // network connection.
    try (OutputStream outputStream = connection.getOutputStream()) {
      // Allocate a buffer for reading the request body data into via JNI.
      byte[] buffer = new byte[calculateRequestBodyBufferSize(requestContentLength)];
      // Allocate an array for the native layer to write the number of actually read bytes into.
      // Because arrays are mutable, this effectively serves as an 'output parameter', allowing the
      // native code to return this bit of information in addition to its primary success/failure
      // return value.
      int[] actualBytesRead = new int[1];
      while (true) {
        // Read data from native. This may be very fast, but may also block on disk IO and/or
        // on-the-fly payload compression.
        doReadRequestBody(buffer, buffer.length, actualBytesRead);
        // The native layer signals the end of the request body data by returning -1 as the
        // "actually read bytes" value (this corresponds to C++'s `HttpRequest::ReadBody` returning
        // `OUT_OF_RANGE`).
        if (actualBytesRead[0] == -1) {
          // End of data reached (successfully).
          break;
        }
        // Otherwise, the native layer is required to have read at least 1 byte into our buffer at
        // this point (and hence actualBytesRead[0] will be >= 1).

        // Account for the data we're about to send in our 'sent bytes' stats. We do this before we
        // write it to the output stream (so that this over rather than under-estimates the number,
        // in case we get interrupted mid-write).
        sentBodyBytes += actualBytesRead[0];

        // Write the data from the native layer to the network socket.
        outputStream.write(buffer, 0, actualBytesRead[0]);

        // Before trying to read another chunk of data, make sure that the request hasn't been
        // aborted yet.
        checkClosed();
      }
      // Flush the stream before we close it, for good measure.
      outputStream.flush();
    }
    // We're done uploading.
  }

  private int calculateRequestBodyBufferSize(long requestContentLength) {
    // If the request body size is known ahead of time, and is smaller than the chunk size we
    // otherwise would use, then we allocate a buffer of just the exact size we need. If the
    // request body size is unknown or too large, then we use a set chunk buffer size to read one
    // chunk at a time.
    if (requestContentLength > 0 && requestContentLength < requestBodyChunkSizeBytes) {
      // This cast from long to int is safe, because we know requestContentLength is smaller than
      // the int bufferSize at this point.
      return (int) requestContentLength;
    }
    return requestBodyChunkSizeBytes;
  }

  private static final class ResponseHeadersWithMetadata {
    private final JniHttpResponse responseProto;
    private final boolean shouldDecodeGzip;
    private final long originalContentLengthHeader;

    ResponseHeadersWithMetadata(
        JniHttpResponse responseProto, boolean shouldDecodeGzip, long originalContentLengthHeader) {
      this.responseProto = responseProto;
      this.shouldDecodeGzip = shouldDecodeGzip;
      this.originalContentLengthHeader = originalContentLengthHeader;
    }
  }

  /**
   * Receives the response headers from the server (blocking until that data is available, or an
   * error occurs), and passes it to the native layer via the JNI callbacks.
   */
  @SuppressWarnings("NonAtomicVolatileUpdate")
  private ResponseHeadersWithMetadata receiveResponseHeaders(
      HttpURLConnection connection, String originalAcceptEncodingHeader) throws IOException {
    // This call will block until the response headers are received (or throw if an error occurred
    // before headers were received, or if no response header data is received before
    // #getRequestReadTimeOutMs).
    int responseCode = connection.getResponseCode();

    // If the original headers we received from the native layer did not include an Accept-Encoding
    // header, then *if we specified an "Accept-Encoding" header ourselves and subsequently received
    // an encoded response body* we should a) remove the Content-Encoding header (since they refer
    // to the encoded data, not the decoded data we will return to the native layer), and b) decode
    // the response body data before returning it to the native layer. Note that if we did receive
    // an "Accept-Encoding" header (even if it specified "gzip"), we must not auto-decode the
    // response body and we should also leave the headers alone.
    boolean shouldDecodeGzip = false;
    if (supportAcceptEncodingHeader && originalAcceptEncodingHeader == null) {
      // We need to strip the headers, if the body is encoded. Determine if it is encoded first.
      for (Map.Entry<String, List<String>> header : connection.getHeaderFields().entrySet()) {
        List<String> headerValues = header.getValue();
        if (Ascii.equalsIgnoreCase(CONTENT_ENCODING_HEADER, nullToEmpty(header.getKey()))
            && !headerValues.isEmpty()
            && Ascii.equalsIgnoreCase(GZIP_ENCODING, nullToEmpty(headerValues.get(0)))) {
          shouldDecodeGzip = true;
          break;
        }
      }
    }

    JniHttpResponse.Builder response = JniHttpResponse.newBuilder();
    response.setCode(responseCode);

    // Account for the response status line in the 'received bytes' stats.
    String responseMessage = connection.getResponseMessage();
    // Note that responseMessage could be null or empty if an HTTP/2 implementation is used (since
    // HTTP/2 doesn't have 'reason phrases' in the status line anymore, only codes).
    responseMessage = nullToEmpty(responseMessage);
    receivedHeaderBytes += "HTTP/1.1 XXX ".length() + responseMessage.length() + "\r\n".length();
    // Add two bytes to account for the \r\n at the end of the response headers.
    receivedHeaderBytes += "\r\n".length();

    // If the response message was empty, then we assume that the request used HTTP/2. This is a
    // flawed heuristic, but the best we have available.
    requestUsedHttp2Heuristic = responseMessage.isEmpty();

    // Now let's process the response headers.
    long receivedContentLength = -1;
    for (Map.Entry<String, List<String>> header : connection.getHeaderFields().entrySet()) {
      // First, let's account for the received headers in our 'received bytes' stats. See note about
      // counting bytes for request headers above, which applies similarly to response
      // headers.
      //
      // Note that for some HttpURLConnection implementations the HTTP response status line may be
      // included in the getHeadersField() result under the null header key, while others don't
      // include it at all. We just skip counting the status line from getHeaderFields() sinec we
      // already accounted for it above.
      if (header.getKey() == null) {
        continue;
      }
      // Count the bytes for all the headers (including accounting for the colon, space, and
      // newlines that would've been sent over the wire).
      for (String headerValue : header.getValue()) {
        receivedHeaderBytes +=
            header.getKey() == null ? 0 : (header.getKey().length() + ": ".length());
        receivedHeaderBytes += headerValue == null ? 0 : headerValue.length();
        // Account for the \r\n chars at the end of the header.
        receivedHeaderBytes += "\r\n".length();
      }

      // Now let's skip headers we shouldn't return to the C++ layer.
      //
      // The HttpURLConnection implementation generally unchunks response bodies that used
      // "Transfer-Encoding: chunked". However, while Android's implementation also then removes the
      // "Transfer-Encoding" header, the JDK implementation does not. Since the HttpClient contract
      // requires us to remove that header, we explicitly filter it out here.
      //
      // Finally, if the response will automatically be gzip-decoded by us, then we must redact any
      // Content-Encoding header too.
      if ((Ascii.equalsIgnoreCase(TRANSFER_ENCODING_HEADER, header.getKey())
              && header.getValue().size() == 1
              && Ascii.equalsIgnoreCase(
                  CHUNKED_TRANSFER_ENCODING, nullToEmpty(header.getValue().get(0))))
          || (shouldDecodeGzip
              && Ascii.equalsIgnoreCase(CONTENT_ENCODING_HEADER, header.getKey()))) {
        continue;
      }
      // Also, the "Content-Length" value returned by HttpURLConnection may or may not correspond to
      // the response body data we will see via  + " - " + receivedBodyBytesgetInputStream() (e.g.
      // it may reflect the length of
      // the previously compressed data, even if the data is already decompressed for us when we
      // read it from the InputStream). Hence, we ignore it as well. We do so even though the C++
      // `HttpClient` asks us to leave it unredacted, because its value cannot be interpreted
      // consistently. However, if the "Content-Length" header *is* available, then we do use it to
      // estimate the network bytes we've received (but only once the request has completed
      // successfully).
      if (Ascii.equalsIgnoreCase(CONTENT_LENGTH_HEADER, header.getKey())) {
        if (header.getValue().size() == 1) {
          try {
            receivedContentLength = Long.parseLong(header.getValue().get(0));
          } catch (NumberFormatException e) {
            // ignore
          }
        }
        continue;
      }

      // Pass the remaining headers to the C++ layer.
      for (String headerValue : header.getValue()) {
        response.addHeaders(
            JniHttpHeader.newBuilder().setName(header.getKey()).setValue(headerValue));
      }
    }

    // If we receive a positive cache hit (i.e. HTTP_NOT_MODIFIED), then the response will not have
    // a body even though the "Content-Encoding" header may still be set. In such cases we shouldn't
    // try pass the InputStream to a GZIPInputStream (in the receiveResponseBody function below),
    // since GZIPInputStream would crash on the 0-byte stream. Note that while we disable any
    // HttpURLConnection-level cache explicitly in this file, it's still possible that the native
    // layer itself implements a cache, which could result in us receiving HTTP_NOT_MODIFIED
    // responses after all, and we should handle those correctly.
    shouldDecodeGzip =
        shouldDecodeGzip && connection.getResponseCode() != HttpURLConnection.HTTP_NOT_MODIFIED;
    return new ResponseHeadersWithMetadata(
        response.build(), shouldDecodeGzip, receivedContentLength);
  }

  /**
   * Receives the response body from the server and passes it to the native layer via the JNI
   * callbacks (blocking until all response body data has been received, or an error occurs).
   */
  @SuppressWarnings("NonAtomicVolatileUpdate")
  private void receiveResponseBody(HttpURLConnection connection, boolean shouldDecodeGzip)
      throws IOException, AbortRequestException {
    // Check one more time, before blocking on the InputStream, if it request has already been
    // cancelled (to avoid starting any blocking network IO we can't easily interrupt).
    checkClosed();

    try (CountingInputStream networkStream = getResponseBodyStream(connection);
        InputStream inputStream = getDecodedResponseBodyStream(networkStream, shouldDecodeGzip)) {
      long networkReceivedBytes = 0;
      // Allocate a buffer for reading the response body data into memory and passing it to JNI.
      int bufferSize = responseBodyChunkSizeBytes;
      byte[] buffer = new byte[bufferSize];
      // This outer loop runs until we reach the end of the response body stream (or hit an
      // error).
      int actualBytesRead = -1;
      do {
        int cursor = 0;
        // Read data from the network stream (or from the decompressing input stream wrapping the
        // network stream), filling up the buffer that we will pass to the native layer. It's likely
        // that each read returns less data than we request. Hence, this inner loop runs until our
        // buffer is full, the end of the data is reached, or we hit an error.
        while (cursor < buffer.length) {
          actualBytesRead = inputStream.read(buffer, cursor, buffer.length - cursor);

          // Update the number of received bytes (at the network level, as best as we can measure).
          // We must do this before we break out of the loop.
          //
          // Note that for some implementations like Cronet's, this would count uncompressed bytes
          // even if the original response was compressed using a Content-Encoding. Hence, this
          // would be an over-estimate of actual network data usage. We will, however, try to
          // provide a more accurate value once the request is completed successfully, if a
          // Content-Length response header was available. See doOnResponseCompleted.
          long newNetworkReceivedBytes = networkStream.getCount();
          receivedBodyBytes += (newNetworkReceivedBytes - networkReceivedBytes);
          networkReceivedBytes = newNetworkReceivedBytes;

          if (actualBytesRead == -1) {
            // End of data reached (successfully). Break out of inner loop.
            break;
          }
          // Some data was read.
          cursor += actualBytesRead;
        }
        // If our buffer is still empty, then we must've hit the end of the data right away. No need
        // to call back into the native layer anymore.
        if (cursor == 0) {
          break;
        }
        // If our buffer now has some data in it, we must pass it to the native layer via the JNI
        // callback. This may be very fast, but may also block on disk IO and/or on-the-fly
        // payload decompression.
        doOnResponseBody(buffer, cursor);

        // Before trying to read another chunk of data, make sure that the request hasn't been
        // aborted yet.
        checkClosed();
      } while (actualBytesRead != -1);
    }
    // We're done downloading. The InputStream will be closed, letting the network layer reclaim
    // the socket and possibly return it to a connection pool for later reuse (as long as we don't
    // call #disconnect() on it, which would prevent the socket from being reused).
  }

  /** Returns the {@link java.io.InputStream} that will return the response body data. */
  private static CountingInputStream getResponseBodyStream(HttpURLConnection connection)
      throws IOException {
    // If the response was an error, then we need to call getErrorStream() to get the response
    // body. Otherwise we need to use getInputStream().
    //
    // Note that we don't wrap the InputStream in a BufferedInputStream, since we already read data
    // from the unbuffered InputStream in large chunks at a time, so adding another buffering layer
    // in between isn't helpful.
    InputStream errorStream = connection.getErrorStream();
    if (errorStream == null) {
      return new CountingInputStream(connection.getInputStream());
    }
    return new CountingInputStream(errorStream);
  }

  /**
   * Returns an {@link java.io.InputStream} that, if we should automatically decode/decompress the
   * response body, will do so.
   *
   * <p>Note that if we should not automatically decode the response body, then this will simply
   * return {@code inputStream}.
   */
  private InputStream getDecodedResponseBodyStream(
      InputStream inputStream, boolean shouldDecodeGzip) throws IOException {
    if (shouldDecodeGzip) {
      // Note that GZIPInputStream's default internal buffer size is quite small (512 bytes). We
      // therefore specify a buffer size explicitly, to ensure that we read in large enough chunks
      // from the network stream (which in turn can improve overall throughput).
      return new GZIPInputStream(inputStream, responseBodyGzipBufferSizeBytes);
    }
    return inputStream;
  }
}
