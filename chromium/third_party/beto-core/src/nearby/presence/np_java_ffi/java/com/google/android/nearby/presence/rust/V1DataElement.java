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

import java.util.Arrays;

/** Base class for V1 data element types. */
public abstract class V1DataElement {

  /**
   * A visitor interface that can be used while iterating data elements in an advertisement to avoid
   * checking {@code instanceof} on every one.
   */
  public interface Visitor {
    void visitGeneric(Generic generic);
  }

  // All subclasses should be in this file
  private V1DataElement() {}

  /** Visit this advertisement with the given visitor. */
  public abstract void visit(Visitor v);

  /**
   * A generic data element. This is a data element which has a type that is not known by this
   * library (e.g. a vendor-specific data element).
   */
  public static final class Generic extends V1DataElement {
    private final long type;
    private final byte[] data;

    public Generic(long type, byte[] data) {
      this.type = type;
      this.data = Arrays.copyOf(data, data.length);
    }

    public long getType() {
      return type;
    }

    public byte[] getData() {
      return Arrays.copyOf(data, data.length);
    }

    @Override
    public void visit(Visitor v) {
      v.visitGeneric(this);
    }
  }
}
