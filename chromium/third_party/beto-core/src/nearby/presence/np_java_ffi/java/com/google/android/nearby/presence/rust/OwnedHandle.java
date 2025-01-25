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

import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;

/**
 * A handle to natively-allocated object with lifetime control. This is a {@code Handle} that also
 * supports deallocating the native object.
 *
 * <p>Users should call {@link OwnedHandle#close()} when finished with this handle to free the
 * native resources. This can be automatically done when using try-with-resources. If neither are
 * use the handle will still be freed sometime after this object has been garbage collected by a
 * subsequent call to {@link OwnedHandle#close()} on another {@code OwnedHandle} instance.
 */
public abstract class OwnedHandle extends Handle implements AutoCloseable {

  /**
   * A destructor to be called when this {@link OwnedHandle} is no longer used.
   *
   * <p>This MUST not hold a reference to the {@link OwnedHandle} instance. Do not implement this on
   * your subclass; however, it may be implemented by a method reference to a static method.
   *
   * <p>This destructor will be run unless {@link OwnedHandle#leak()} is called. It will be run on
   * the thread that calls {@link OwnedHandle#close()} if the handle is closed. If neither leak nor
   * close are called and this handle is garbage collected, then it will be run on an unspecified
   * thread. See {@link CooperativeCleaner}.
   */
  public interface Destructor {
    void deallocate(long handleId);
  }

  /** Thrown when a new handle cannot be allocated due to lack of space */
  public static final class NoSpaceLeftException extends RuntimeException {
    public NoSpaceLeftException() {
      super("No space remaining in the associated HandleMap");
    }
  }

  private final CleanupAction cleanupAction;
  private final CooperativeCleaner.Registration cleanerRegistration;

  /**
   * Create a new instance and register it with the given cleaner.
   *
   * @param handleId The handle's id
   * @param cleaner The cleaner thread to register with for GC cleanup
   * @param destructor The destructor to run when this handle is closed
   */
  protected OwnedHandle(long handleId, CooperativeCleaner cleaner, Destructor destructor) {
    super(handleId);
    this.cleanupAction = new CleanupAction(handleId, destructor);

    cleanerRegistration = cleaner.register(this, this.cleanupAction);
  }

  /** Leak this handle. The associated native object will not be deallocated. */
  protected final void leak() {
    cleanupAction.leak();
    close();
  }

  /** Implement AutoCloseable for try-with-resources support */
  @Override
  public final void close() {
    cleanerRegistration.close();
  }

  /**
   * A {@link Runnable} to be given to the associated {@link Cleaner} to clean up a handle. This
   * MUST not hold a reference to the {@link OwnedHandle} that it is associated with.
   */
  private static final class CleanupAction implements Runnable {
    private final long handleId;

    @GuardedBy("this")
    @Nullable
    private Destructor destructor;

    public CleanupAction(long handleId, Destructor destructor) {
      this.handleId = handleId;
      this.destructor = destructor;
    }

    /** Skip performing cleanup and leak the object instead */
    private void leak() {
      synchronized (this) {
        this.destructor = null;
      }
    }

    /**
     * Deallocate the handle using the given {@link Destructor}.
     *
     * <p>The Destructor will only be called once, and future calls to this method will return
     * {@code false}.
     *
     * @return {@code true} if the destructor was called.
     */
    private boolean deallocate() {
      // Take the destructor if present
      Destructor destructor = null;
      synchronized (this) {
        if (this.destructor != null) {
          destructor = this.destructor;
          this.destructor = null;
        }
      }

      // Run destructor if it was present
      if (destructor != null) {
        destructor.deallocate(handleId);
        return true;
      }
      return false;
    }

    @Override
    public void run() {
      boolean unused = deallocate();
    }
  }
}
