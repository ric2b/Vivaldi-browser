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
package com.google.fcp.client;

import java.lang.Thread.UncaughtExceptionHandler;
import java.util.concurrent.Callable;

/**
 * Utility class to wrap java method calls that originate from the native layer over JNI, which
 * ensures that any uncaught {@link Throwable} is passed to the given {@link
 * UncaughtExceptionHandler} (which is expected to generate a crash report and terminate the
 * process), as opposed to being passed back to the native layer.
 */
public class CallFromNativeWrapper {

  private final UncaughtExceptionHandler uncaughtExceptionHandler;

  /** A {@link Callable} that does not throw checked exceptions. */
  public interface NativeToJavaCallable<T> extends Callable<T> {
    @Override
    T call();
  }

  public CallFromNativeWrapper(UncaughtExceptionHandler uncaughtExceptionHandler) {
    this.uncaughtExceptionHandler = uncaughtExceptionHandler;
  }

  /**
   * Wraps a java method call from native code on an arbitrary thread (i.e. one created by
   * TensorFlow). If a {@link Throwable} is thrown the exception will be passed to the {@code
   * uncaughtExceptionHandler}.
   */
  public <T> T wrapCall(NativeToJavaCallable<T> callable) {
    try {
      return callable.call();
    } catch (Throwable t) {
      uncaughtExceptionHandler.uncaughtException(Thread.currentThread(), t);
      // The uncaught exception handler generally will have killed us by here.
      //
      // On Android, the system should see our thread crash and kill the process before reaching
      // here. Just in case we make it this far, we wrap the exception in a runtime exception and
      // let it pass to the native layer (which will generally then abort the process upon detecting
      // the exception).
      throw new CallFromNativeRuntimeException(t);
    }
  }

  public void wrapVoidCall(Runnable runnable) {
    wrapCall(
        () -> {
          runnable.run();
          return null;
        });
  }

  /**
   * A {@link RuntimeException} signifying there was an unchecked exception when calling from native
   * to java.
   */
  public static class CallFromNativeRuntimeException extends RuntimeException {
    CallFromNativeRuntimeException(Throwable t) {
      super(t);
    }
  }
}
