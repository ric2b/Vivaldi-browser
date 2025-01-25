/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.security.cryptauth.lib.securegcm.ukey2;

import javax.annotation.Nonnull;

public class D2DHandshakeContext {
  static {
    System.loadLibrary("ukey2_jni");
  }

  public enum Role {
    INITIATOR,
    RESPONDER,
  }

  public enum NextProtocol {
    AES_256_GCM_SIV,
    AES_256_CBC_HMAC_SHA256,
  }

  private final long contextPtr;

  private static native boolean is_handshake_complete(long contextPtr) throws BadHandleException;

  private static native long create_context(boolean isClient, int[] supported_next_protocols);

  private static native byte[] get_next_handshake_message(long contextPtr)
      throws BadHandleException;

  private static native void parse_handshake_message(long contextPtr, byte[] message)
      throws AlertException, BadHandleException, HandshakeException;

  private static native byte[] get_verification_string(long contextPtr, int length)
      throws BadHandleException, HandshakeException;

  private static native long to_connection_context(long contextPtr) throws HandshakeException;

  public D2DHandshakeContext(@Nonnull Role role) throws HandshakeException {
    this(role, new NextProtocol[] {NextProtocol.AES_256_CBC_HMAC_SHA256});
  }

  public D2DHandshakeContext(@Nonnull Role role, @Nonnull NextProtocol[] nextProtocols)
      throws HandshakeException {
    if (nextProtocols.length < 1) {
      throw new HandshakeException("Need more than one supported next protocol!");
    }
    int[] nextProtocolCodes = new int[nextProtocols.length];
    for (int i = 0; i < nextProtocols.length; i++) {
      nextProtocolCodes[i] = nextProtocols[i].ordinal();
    }

    this.contextPtr = create_context(role == Role.INITIATOR, nextProtocolCodes);
  }

  /**
   * Convenience constructor that creates a UKEY2 D2DHandshakeContext for the initiator role.
   *
   * @return a D2DHandshakeContext for the role of initiator in the handshake.
   */
  public static D2DHandshakeContext forInitiator() throws HandshakeException {
    return new D2DHandshakeContext(Role.INITIATOR);
  }

  /**
   * Convenience constructor that creates a UKEY2 D2DHandshakeContext for the initiator role.
   *
   * @param nextProtocols Specification for the supported next protocols for this initiator.
   * @return a D2DHandshakeContext for the role of initiator in the handshake.
   */
  public static D2DHandshakeContext forInitiator(NextProtocol[] nextProtocols)
      throws HandshakeException {
    return new D2DHandshakeContext(Role.INITIATOR, nextProtocols);
  }

  /**
   * Convenience constructor that creates a UKEY2 D2DHandshakeContext for the initiator role.
   *
   * @return a D2DHandshakeContext for the role of responder/server in the handshake.
   */
  public static D2DHandshakeContext forResponder() throws HandshakeException {
    return new D2DHandshakeContext(Role.RESPONDER);
  }

  /**
   * Convenience constructor that creates a UKEY2 D2DHandshakeContext for the initiator role.
   *
   * @param nextProtocols Specification for the supported next protocols for this responder.
   * @return a D2DHandshakeContext for the role of responder/server in the handshake.
   */
  public static D2DHandshakeContext forResponder(NextProtocol[] nextProtocols)
      throws HandshakeException {
    return new D2DHandshakeContext(Role.RESPONDER, nextProtocols);
  }

  /**
   * Function that checks if the handshake is completed.
   *
   * @return true/false depending on if the handshake is complete.
   */
  public boolean isHandshakeComplete() throws BadHandleException {
    return is_handshake_complete(contextPtr);
  }

  /**
   * Gets the next handshake message in the exchange.
   *
   * @return handshake message encoded in a SecureMessage.
   */
  @Nonnull
  public byte[] getNextHandshakeMessage() throws BadHandleException {
    return get_next_handshake_message(contextPtr);
  }

  /**
   * Parses the handshake message.
   *
   * @param message - handshake message from the other side.
   * @throws AlertException - Thrown if the message is unable to be parsed.
   * @throws BadHandleException - Thrown if the handle is no longer valid, for example after calling
   *     {@link D2DHandshakeContext#toConnectionContext()}
   * @throws HandshakeException - Thrown if the handshake is not complete when this function is
   *     called.
   */
  @Nonnull
  public void parseHandshakeMessage(@Nonnull byte[] message)
      throws AlertException, BadHandleException, HandshakeException {
    parse_handshake_message(contextPtr, message);
  }

  /**
   * Returns an authentication string suitable for authenticating the handshake out-of-band. Note
   * that the authentication string can be short (e.g., a 6 digit visual confirmation code). Note:
   * this should only be called when {#isHandshakeComplete} returns true. This code is analogous to
   * the authentication string described in the spec.
   *
   * @param length - The length of the returned verification string.
   * @return - The returned verification string as a byte array.
   * @throws BadHandleException - Thrown if the handle is no longer valid, for example after calling
   *     {@link D2DHandshakeContext#toConnectionContext()}
   * @throws HandshakeException - Thrown if the handshake is not complete when this function is
   *     called.
   */
  @Nonnull
  public byte[] getVerificationString(int length) throws BadHandleException, HandshakeException {
    return get_verification_string(contextPtr, length);
  }

  /**
   * Function to create a secure communication channel from the handshake after confirming the auth
   * string generated by the handshake out-of-band (i.e. via a user-facing UI).
   *
   * @return a new {@link D2DConnectionContextV1} with the next protocol specified when creating the
   *     D2DHandshakeContext.
   * @throws HandshakeException if the handsshake is not complete when this function is called.
   */
  public D2DConnectionContextV1 toConnectionContext() throws HandshakeException {
    return new D2DConnectionContextV1(to_connection_context(contextPtr));
  }
}
