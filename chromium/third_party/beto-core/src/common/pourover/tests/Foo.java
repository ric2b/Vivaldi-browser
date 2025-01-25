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

package com.example;

/** Test class for integration tests. See {@code common/foo_class.rs} for how to compile and use. */
public final class Foo {

  public static long sfoo = 321;

  public int foo;

  public static int smfoo() {
    return 3;
  }

  public Foo(int f) {
    this.foo = f;
  }

  public int getFoo() {
    return foo;
  }

  public boolean mfoo() {
    return true;
  }

  public native int nativeReturnsInt(int n);

  public native String nativeReturnsObject(int n);
}
