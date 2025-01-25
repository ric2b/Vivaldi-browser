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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.Verifier;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

@RunWith(JUnit4.class)
public class OwnedHandleTests {

  @Rule public MockitoRule mockito = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

  @Rule
  public Verifier checkCleanerAfterTest =
      new Verifier() {
        @Override
        protected void verify() {
          assertThat(cleaner.getRegisteredObjectCount()).isEqualTo(0);
        }
      };

  CooperativeCleaner cleaner = new CooperativeCleaner();
  @Mock OwnedHandle.Destructor destructor;

  class MyHandle extends OwnedHandle {
    public MyHandle(long handleId) {
      super(handleId, cleaner, destructor);
    }

    public MyHandle(CooperativeCleaner cleaner, long handleId) {
      super(handleId, cleaner, destructor);
    }
  }

  @Test
  public void constructor_registersWithCleaner() {
    CooperativeCleaner spyCleaner = spy(cleaner);
    try (MyHandle handle = new MyHandle(spyCleaner, 123)) {
      verify(spyCleaner).register(same(handle), any());
    }
  }

  @Test
  public void close_callsDestructor() {
    try (MyHandle handle = new MyHandle(123)) {}
    verify(destructor).deallocate(eq(123L));
  }

  @Test
  public void leaked_willEndUpInCleanerQueue() throws Exception {
    {
      // leaked
      waitForGc(new MyHandle(0xbad));
    }

    cleaner.processQueuedObjects();

    verify(destructor).deallocate(eq(0xbadL));
  }

  @Test
  public void close_callsDestructorOfLeakedObject() throws Exception {
    {
      // leaked
      waitForGc(new MyHandle(0xbad));
    }

    MyHandle handle = new MyHandle(123);
    handle.close();

    verify(destructor).deallocate(eq(0xbadL));
    verify(destructor).deallocate(eq(123L));
  }

  private void waitForGc(Object object) throws InterruptedException {
    ReferenceQueue<Object> queue = new ReferenceQueue<>();
    // Need to keep this around to be notified when object is GC'd
    PhantomReference<Object> ref = new PhantomReference<>(object, queue);

    object = null;
    System.gc();

    assertThat(queue.remove()).isSameInstanceAs(ref);
  }
}
