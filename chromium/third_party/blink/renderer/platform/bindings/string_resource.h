// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_STRING_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_STRING_RESOURCE_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "v8/include/v8.h"

namespace blink {

// StringResource is a helper class for V8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class StringResourceBase {
  USING_FAST_MALLOC(StringResourceBase);

 public:
  explicit StringResourceBase(String string)
      : plain_string_(std::move(string)) {
    DCHECK(!plain_string_.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        plain_string_.CharactersSizeInBytes());
  }

  explicit StringResourceBase(AtomicString string)
      : atomic_string_(std::move(string)) {
    DCHECK(!atomic_string_.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        atomic_string_.CharactersSizeInBytes());
  }

  explicit StringResourceBase(ParkableString string)
      : parkable_string_(string) {
    // TODO(lizeb): This is only true without compression.
    DCHECK(!parkable_string_.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        parkable_string_.CharactersSizeInBytes());
  }

  StringResourceBase(const StringResourceBase&) = delete;
  StringResourceBase& operator=(const StringResourceBase&) = delete;

  virtual ~StringResourceBase() {
    int64_t reduced_external_memory = plain_string_.CharactersSizeInBytes();
    if (plain_string_.Impl() != atomic_string_.Impl() &&
        !atomic_string_.IsNull())
      reduced_external_memory += atomic_string_.CharactersSizeInBytes();
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -reduced_external_memory);
  }

  String GetWTFString() {
    if (!parkable_string_.IsNull()) {
      DCHECK(plain_string_.IsNull());
      DCHECK(atomic_string_.IsNull());
      return parkable_string_.ToString();
    }
    return String(GetStringImpl());
  }

  AtomicString GetAtomicString() {
    if (!parkable_string_.IsNull()) {
      DCHECK(plain_string_.IsNull());
      DCHECK(atomic_string_.IsNull());
      return AtomicString(parkable_string_.ToString());
    }
    if (atomic_string_.IsNull()) {
      atomic_string_ = AtomicString(plain_string_);
      DCHECK(!atomic_string_.IsNull());
      if (plain_string_.Impl() != atomic_string_.Impl()) {
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
            atomic_string_.CharactersSizeInBytes());
      }
    }
    return atomic_string_;
  }

 protected:
  StringImpl* GetStringImpl() const {
    if (!plain_string_.IsNull())
      return plain_string_.Impl();
    DCHECK(!atomic_string_.IsNull());
    return atomic_string_.Impl();
  }

  const ParkableString& GetParkableString() const { return parkable_string_; }

  // Helper functions for derived constructors.
  template <typename Str>
  static inline Str Assert8Bit(Str&& str) {
    DCHECK(str.Is8Bit());
    return str;
  }

  template <typename Str>
  static inline Str Assert16Bit(Str&& str) {
    DCHECK(!str.Is8Bit());
    return str;
  }

 private:
  // If this StringResourceBase was initialized from a String then plain_string_
  // will be non-null. If the string becomes atomic later, the atomic version
  // of the string will be held in atomic_string_. When that happens, it is
  // necessary to keep the original string alive because v8 may keep derived
  // pointers into that string.
  // If this StringResourceBase was initialized from an AtomicString then
  // plain_string_ will be null and atomic_string_ will be non-null.
  String plain_string_;
  AtomicString atomic_string_;

  // If this string is parkable, its value is held here, and the other
  // members above are null.
  ParkableString parkable_string_;
};

// Even though StringResource{8,16}Base are effectively empty in release mode,
// they are needed as they serve as a common ancestor to Parkable and regular
// strings.
//
// See the comment in |ToBlinkString()|'s implementation for the rationale.
class StringResource16Base : public StringResourceBase,
                             public v8::String::ExternalStringResource {
 public:
  explicit StringResource16Base(String string)
      : StringResourceBase(Assert16Bit(std::move(string))) {}

  explicit StringResource16Base(AtomicString string)
      : StringResourceBase(Assert16Bit(std::move(string))) {}

  explicit StringResource16Base(ParkableString parkable_string)
      : StringResourceBase(Assert16Bit(std::move(parkable_string))) {}

  StringResource16Base(const StringResource16Base&) = delete;
  StringResource16Base& operator=(const StringResource16Base&) = delete;
};

class StringResource16 final : public StringResource16Base {
 public:
  explicit StringResource16(String string)
      : StringResource16Base(std::move(string)) {}

  explicit StringResource16(AtomicString string)
      : StringResource16Base(std::move(string)) {}

  StringResource16(const StringResource16&) = delete;
  StringResource16& operator=(const StringResource16&) = delete;

  size_t length() const override { return GetStringImpl()->length(); }
  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(GetStringImpl()->Characters16());
  }
};

class ParkableStringResource16 final : public StringResource16Base {
 public:
  explicit ParkableStringResource16(ParkableString string)
      : StringResource16Base(std::move(string)) {}

  ParkableStringResource16(const ParkableStringResource16&) = delete;
  ParkableStringResource16& operator=(const ParkableStringResource16&) = delete;

  bool IsCacheable() const override {
    return !GetParkableString().may_be_parked();
  }

  void Lock() const override { GetParkableString().Lock(); }

  void Unlock() const override { GetParkableString().Unlock(); }

  size_t length() const override { return GetParkableString().length(); }

  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(
        GetParkableString().Characters16());
  }
};

class StringResource8Base : public StringResourceBase,
                            public v8::String::ExternalOneByteStringResource {
 public:
  explicit StringResource8Base(String string)
      : StringResourceBase(Assert8Bit(std::move(string))) {}

  explicit StringResource8Base(AtomicString string)
      : StringResourceBase(Assert8Bit(std::move(string))) {}

  explicit StringResource8Base(ParkableString parkable_string)
      : StringResourceBase(Assert8Bit(std::move(parkable_string))) {}

  StringResource8Base(const StringResource8Base&) = delete;
  StringResource8Base& operator=(const StringResource8Base&) = delete;
};

class StringResource8 final : public StringResource8Base {
 public:
  explicit StringResource8(String string)
      : StringResource8Base(std::move(string)) {}

  explicit StringResource8(AtomicString string)
      : StringResource8Base(std::move(string)) {}

  StringResource8(const StringResource8&) = delete;
  StringResource8& operator=(const StringResource8&) = delete;

  size_t length() const override { return GetStringImpl()->length(); }
  const char* data() const override {
    return reinterpret_cast<const char*>(GetStringImpl()->Characters8());
  }
};

class ParkableStringResource8 final : public StringResource8Base {
 public:
  explicit ParkableStringResource8(ParkableString string)
      : StringResource8Base(std::move(string)) {}

  ParkableStringResource8(const ParkableStringResource8&) = delete;
  ParkableStringResource8& operator=(const ParkableStringResource8&) = delete;

  bool IsCacheable() const override {
    return !GetParkableString().may_be_parked();
  }

  void Lock() const override { GetParkableString().Lock(); }

  void Unlock() const override { GetParkableString().Unlock(); }

  size_t length() const override { return GetParkableString().length(); }

  const char* data() const override {
    return reinterpret_cast<const char*>(GetParkableString().Characters8());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_STRING_RESOURCE_H_
