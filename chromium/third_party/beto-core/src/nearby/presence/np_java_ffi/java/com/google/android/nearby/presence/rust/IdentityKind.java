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

package com.google.android.nearby.presence.rust;

import static java.lang.annotation.RetentionPolicy.SOURCE;

import androidx.annotation.IntDef;
import java.lang.annotation.Retention;

/**
 * The kind of identity that was discovered for a decrypted advertisement. Values greater than
 * {@code 0} are legible. Values less than {@code 0} are not.
 */
@IntDef({
  IdentityKind.NO_MATCHING_CREDENTIALS,
  IdentityKind.PLAINTEXT,
  IdentityKind.DECRYPTED,
})
@Retention(SOURCE)
public @interface IdentityKind {
  /** An encrypted identity that we do not have a credential for. */
  public static final int NO_MATCHING_CREDENTIALS = -1;

  /** A plaintext identity. */
  public static final int PLAINTEXT = 1;

  /** An encrypted identity that we have a credential for. */
  public static final int DECRYPTED = 2;
}
