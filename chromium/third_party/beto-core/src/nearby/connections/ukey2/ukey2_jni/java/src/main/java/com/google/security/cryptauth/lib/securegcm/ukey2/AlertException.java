// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.security.cryptauth.lib.securegcm.ukey2;

/**
 * An alert exception that contains the alert message to send to the connection peer in case
 * anything went wrong.
 */
public class AlertException extends Exception {

  private final byte[] alertMessageToSend;

  public AlertException(String message, byte[] alertMessageToSend) {
    super(message);
    this.alertMessageToSend = alertMessageToSend;
  }

  /**
   * @return a message suitable for sending to other member of handshake.
   */
  public byte[] getAlertMessageToSend() {
    return alertMessageToSend;
  }
}
