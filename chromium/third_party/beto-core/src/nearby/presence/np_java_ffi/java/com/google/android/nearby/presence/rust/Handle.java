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

/**
 * A handle to a natively-allocated object. This class should be subclassed in order to provide a
 * type for the native object. This class does not control the lifetime of the native object. See
 * {@link OwnedHandle} for a variant that allows Java to deallocate the native object.
 *
 * <p>This may be a handle to an object that has already been deallocated. In that case, any native
 * uses of this handle should throw {@link Handle.InvalidHandleException}.
 */
public abstract class Handle {

  /** Thrown when an invalid handle is used */
  public static final class InvalidHandleException extends RuntimeException {
    public InvalidHandleException() {
      super("The given handle is no longer valid");
    }
  }

  protected final long handleId;

  protected Handle(long handleId) {
    this.handleId = handleId;
  }

  public long getId() {
    return handleId;
  }
}
