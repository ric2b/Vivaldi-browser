/*
 * Copyright 2024 Google LLC
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
import androidx.annotation.VisibleForTesting;
import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.util.HashSet;
import org.checkerframework.checker.initialization.qual.Initialized;
import org.checkerframework.checker.initialization.qual.UnknownInitialization;

/**
 * A similar class to {@link java.lang.ref.Cleaner} that doesn't require a dedicated thread.
 *
 * <p>To avoid using a thread for the reference queue, clients are expected to call {@link
 * Registration#close()} on the object returned from {@link register(Object, Runnable)}. This will
 * perform two actions:
 *
 * <ol>
 *   <li>Run the cleanup actions for any registered objects that have been finaized without {@code
 *       close()} being called.
 *   <li>Run the cleanup action for the object being closed.
 * </ol>
 *
 * <p>The application should keep a reference to {@code this} instance to avoid the cleanup actions
 * themselves being garbage collected!
 */
public class CooperativeCleaner {

  /**
   * An object registration with this CooperativeCleaner. Clients should call the {@link
   * Registration#close()} method when the registered object is ready to be cleaned up.
   *
   * @see CooperativeCleaner documentation for more details.
   */
  public class Registration implements AutoCloseable {
    private final CleanableReference ref;

    private Registration(CleanableReference ref) {
      this.ref = ref;
    }

    @Override
    public void close() {
      // Cleanup any queued objects first (Cooperation!)
      processQueuedObjects();

      // Run cleanup for the referenced object.
      ref.performCleanup();
    }
  }

  /**
   * Objects that are not closed will be posted to this reference queue by the garbage collector.
   */
  @GuardedBy("queue")
  private final ReferenceQueue<Object> queue = new ReferenceQueue<>();

  /**
   * Holds strong references to {@link CleanableReference} instances so that the reference objects
   * are not garbage collected.
   */
  @GuardedBy("refs")
  private final HashSet<CleanableReference> refs = new HashSet<>();

  /**
   * Register an object for cleanup. The object will be cleaned up when the returned {@code
   * Registration} is closed or sometime after the object has been finalized. The returned value
   * should always be closed if possible. If it is closed the {@code cleanupAction} will be run on
   * the thread that called {@code close()}; otherwise, it will be run during {@code close()} for a
   * different object. In all cases {@code cleanupAction} will be run exactly once.
   *
   * <p>The given {@code cleanupAction} <b>SHOULD NOT</b> hold a reference to {@code object}.
   * Holding such a reference would mean that {@code object} is always reachable and it will never
   * be garbaged collected.
   */
  public Registration register(@UnknownInitialization Object object, Runnable cleanupAction) {
    // The cast here is safe because it is impossible to get object back out of this reference.
    // PhantomReference will always return null for {@link PhantomReference#get()}. We don't care
    // about the initialization state of the object; rather, we only care about the identity of the
    // object. Likewise, an actual reference to object is never stored by this class; that would
    // stop the object from ever being garbage collected.
    @SuppressWarnings("nullness:cast.unsafe")
    CleanableReference ref = new CleanableReference((@Initialized Object) object, cleanupAction);

    // Keep a copy of the reference in {@code refs} to avoid being garbage collected.
    synchronized (refs) {
      refs.add(ref);
    }

    return new Registration(ref);
  }

  /** Gets the number of objects that are currently registered with this cleaner. */
  @VisibleForTesting
  public int getRegisteredObjectCount() {
    synchronized (refs) {
      return refs.size();
    }
  }

  /** Gets the next {@link CleanableReference} from the queue if there is one. */
  @Nullable
  private CleanableReference pollQueue() {
    synchronized (queue) {
      return (CleanableReference) queue.poll();
    }
  }

  /** Process cleanup actions for all objects in the reference queue. */
  @VisibleForTesting
  public void processQueuedObjects() {
    CleanableReference ref;
    while ((ref = pollQueue()) != null) {
      ref.performCleanup();
    }
  }

  /**
   * Stores the {@code cleanupAction} and tracks the lifecycle of {@code object}. {@link
   * #performCleanup()} can be called early to perform the cleanup action.
   */
  private class CleanableReference extends PhantomReference<Object> {
    @Nullable private Runnable cleanupAction;

    private CleanableReference(Object object, Runnable cleanupAction) {
      // This reference will be enqueued onto {@code queue} if object becomes phantom-reachable and
      // {@link #clear()} hasn't been called.
      super(object, queue);

      this.cleanupAction = cleanupAction;
    }

    private void performCleanup() {
      // Cleanup this object
      @Nullable Runnable action;
      synchronized (refs) {
        // Do not post this to the reference queue if this was called before the object was
        // posted.
        this.clear();

        // This object is no longer needed, so we can remove our reference to it so that it may be
        // garbaged collected.
        refs.remove(this);

        // Make sure that cleanupAction is only run once by taking the value before running it
        action = this.cleanupAction;
        this.cleanupAction = null;
      }

      if (action != null) {
        // Run cleanupAction outside of the lock so that multiple cleanups may occur on separate
        // threads.
        action.run();
      }
    }
  }
}
