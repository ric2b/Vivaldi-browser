/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/base/bounds.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace fcp {

using ::testing::Eq;

//
// Now follows a bunch of support for the TryCastInteger test. That
// implementation is very easy to get wrong, due to the sneaky conversion rules
// imposed on us by C++ (nice example: (int32_t)-1 > (uint32_t)1).
//
// In fact, it's fairly easy for the same class of issues to manifest in the
// test itself (or, even just mistaking the type of an integer literal).
//
// So, we approach this test like so:
//
//   - Prevent unintended conversions in the test itself, and be explicit about
//     integer types. Recall that this tends to compile:
//       uint8_t u = -1; // Oh dear.
//
//     But meanwhile, the rules for integer template arguments (maybe constant
//     expressions generally?) are much stricter.
//
//     We introduce a macro to raise an integer literal into a template
//     argument. Subsequent attempts to cast it are checked at compile time with
//     the stricter rules (Clang, by default, complains under
//     -Wc++11-narrowing).
//
//       LITERAL(-1)::As<uint8_t> // Error!
//       LITERAL(-1)::As<int8_t>  // Good.
//
//   - Be thorough: just test all the interesting combinations!
//
//     We start by defining a set (AllIntTypes) of the common integer types to
//     test with: the signed and unsigned variants with sizes of 1, 2, 4, and 8
//     bytes.
//
//     Then, we choose some interesting LITERALs to test: for example, -1, and
//     the min / max values for each type in AllIntTypes. For each LITERAL, we
//     write down the _expected_ set of types that can fully represent it. As a
//     shorthand, we work in terms of the _minimum_ sizes that work (a larger
//     type of the same signed-ness always works). Some examples:
//
//         // Any signed type (i.e. >= 8 bits) can represent -1
//         VerifyIntCasts<LITERAL(-1), Signed<k8>>();
//         // The max value of int64_t also fits in uint64_t
//         VerifyIntCasts<MAX_LITERAL(int64_t), SignedOrUnsigned<k64>>();
//         // Nothing else can hold a uint64_t's max value
//         VerifyIntCasts<MAX_LITERAL(uint64_t), Unsigned<k64>>();
//
//    (a LITERAL is safely cast to each type in its expected set at compile
//    time)
//
//    For each literal, we test runtime casts like follows:
//
//        for (Type From : Expected) {
//          From e = As<From>(); // Checked at compile time
//          for (Type To : AllIntTypes) {
//            bool succeeded = TryCastInteger<To, From>(...);
//            if (To in Expected) {
//              EXPECT_TRUE(succeeded);
//              ...
//            } else {
//              EXPECT_FALSE(succeeded);
//            }
//          }
//        }
//

enum IntSize { kNone, k8 = 1, k16 = 2, k32 = 4, k64 = 8 };

/**
 * The set of the integer types we care about, with a filter applied.
 * Apply() instantiates a given template, for each integer type passing the
 * filter. We use the filter to model the set of types that can represent a
 * literal (each instantiation tries to compile ::As).
 */
template <typename FilterType>
class IntTypeSet {
 public:
  template <typename F>
  static void Apply(F f) {
    ApplySingle<uint8_t>(f);
    ApplySingle<uint16_t>(f);
    ApplySingle<uint32_t>(f);
    ApplySingle<uint64_t>(f);

    ApplySingle<int8_t>(f);
    ApplySingle<int16_t>(f);
    ApplySingle<int32_t>(f);
    ApplySingle<int64_t>(f);
  }

  template <typename T>
  static constexpr bool Matches() {
    return FilterType::template Matches<T>();
  }

 private:
  template <bool B>
  using BoolTag = std::integral_constant<bool, B>;

  template <typename T, typename F>
  static void ApplySingle(F f) {
    ApplySingleImpl<T>(f, BoolTag<Matches<T>()>{});
  }

  template <typename T, typename F>
  static void ApplySingleImpl(F f, BoolTag<true>) {
    f.template Apply<T>();
  }

  template <typename T, typename F>
  static void ApplySingleImpl(F f, BoolTag<false>) {}
};

struct NoFilter {
  template <typename T>
  static constexpr bool Matches() {
    return true;
  }
};

/**
 * The filter type we use per literal. It's sufficient to give a minimum size,
 * separately per signed / unsigned.
 */
template <IntSize MinSignedSize, IntSize MinUnsignedSize>
struct IntSizeFilter {
  template <typename T>
  static constexpr bool Matches() {
    return SizeRequiredForType<T>() != IntSize::kNone &&
           sizeof(T) >= SizeRequiredForType<T>();
  }

  template <typename T>
  static constexpr IntSize SizeRequiredForType() {
    return std::is_signed<T>() ? MinSignedSize : MinUnsignedSize;
  }
};

using AllIntTypes = IntTypeSet<NoFilter>;

template <IntSize MinSignedSize>
using Signed = IntTypeSet<IntSizeFilter<MinSignedSize, kNone>>;
template <IntSize MinUnsignedSize>
using Unsigned = IntTypeSet<IntSizeFilter<kNone, MinUnsignedSize>>;
template <IntSize MinSignedSize, IntSize MinUnsignedSize = MinSignedSize>
using SignedOrUnsigned =
    IntTypeSet<IntSizeFilter<MinSignedSize, MinUnsignedSize>>;

template <typename T, T Value_>
struct Literal {
  template <typename R>
  using As = Literal<R, Value_>;

  static constexpr T Value() { return Value_; }
};

/**
 * This is the per-literal test as described at the top of the file -
 * but uglier.
 */
template <typename LiteralType, typename SetType>
struct VerifyCastFromEachInSetToAll {
  // Outer loop body: called for each type in the literal's 'expected' set.
  template <typename FromType>
  void Apply() {
    AllIntTypes::Apply(ForAll<FromType>{});
  }

  template <typename FromType>
  struct ForAll {
    static constexpr FromType From() {
      return LiteralType::template As<FromType>::Value();
    }

    // Inner loop body: called for all integer types.
    template <typename ToType>
    void Apply() {
      ToType actual;
      bool succeeded = TryCastInteger(From(), &actual);
      if (SetType::template Matches<ToType>()) {
        EXPECT_TRUE(succeeded);
        if (succeeded) {
          EXPECT_THAT(actual, Eq(static_cast<ToType>(From())));
          EXPECT_THAT(static_cast<FromType>(actual), Eq(From()));
        }
      } else {
        EXPECT_FALSE(succeeded);
      }
    }
  };
};

template <typename LiteralType, typename SetType>
void VerifyIntCasts() {
  SetType::Apply(VerifyCastFromEachInSetToAll<LiteralType, SetType>{});
}

#define LITERAL(i) Literal<decltype(i), i>
#define MAX_LITERAL(t) Literal<t, std::numeric_limits<t>::max()>
#define MIN_LITERAL(t) Literal<t, std::numeric_limits<t>::min()>

TEST(BoundsTest, TryCastInteger) {
  VerifyIntCasts<LITERAL(-1), Signed<k8>>();
  VerifyIntCasts<LITERAL(0), SignedOrUnsigned<k8>>();

  VerifyIntCasts<MAX_LITERAL(int8_t), SignedOrUnsigned<k8>>();
  VerifyIntCasts<MAX_LITERAL(int16_t), SignedOrUnsigned<k16>>();
  VerifyIntCasts<MAX_LITERAL(int32_t), SignedOrUnsigned<k32>>();
  VerifyIntCasts<MAX_LITERAL(int64_t), SignedOrUnsigned<k64>>();

  VerifyIntCasts<MAX_LITERAL(uint8_t), SignedOrUnsigned<k16, k8>>();
  VerifyIntCasts<MAX_LITERAL(uint16_t), SignedOrUnsigned<k32, k16>>();
  VerifyIntCasts<MAX_LITERAL(uint32_t), SignedOrUnsigned<k64, k32>>();
  VerifyIntCasts<MAX_LITERAL(uint64_t), Unsigned<k64>>();

  VerifyIntCasts<MIN_LITERAL(int8_t), Signed<k8>>();
  VerifyIntCasts<MIN_LITERAL(int16_t), Signed<k16>>();
  VerifyIntCasts<MIN_LITERAL(int32_t), Signed<k32>>();
  VerifyIntCasts<MIN_LITERAL(int64_t), Signed<k64>>();

  VerifyIntCasts<MIN_LITERAL(uint8_t), SignedOrUnsigned<k8>>();
  VerifyIntCasts<MIN_LITERAL(uint16_t), SignedOrUnsigned<k8>>();
  VerifyIntCasts<MIN_LITERAL(uint32_t), SignedOrUnsigned<k8>>();
  VerifyIntCasts<MIN_LITERAL(uint64_t), SignedOrUnsigned<k8>>();
}

//
// End of the TryCastInteger test
//

AddressSpace MakeFakeAddressSpace(uint64_t start, uint64_t size) {
  return AddressSpace::Embedded(reinterpret_cast<void*>(start), size);
}

MATCHER_P(IsAddress, addr, "") {
  return reinterpret_cast<uint64_t>(arg) == addr;
}

TEST(BoundsTest, MapOutOfAddressSpace_Success) {
  AddressSpace space = MakeFakeAddressSpace(128, 128);
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{0}, 8, 1),
              IsAddress(128));
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{0}, 8, 128 / 8),
              IsAddress(128));
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{127}, 1, 1),
              IsAddress(128 + 127));
}

TEST(BoundsTest, MapOutOfAddressSpace_OutOfBounds) {
  AddressSpace space = MakeFakeAddressSpace(128, 128);
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{128}, 8, 1),
              Eq(nullptr));
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{8}, 8, 128 / 8),
              Eq(nullptr));
}

TEST(BoundsTest, MapOutOfAddressSpace_ZeroSizeEdge) {
  AddressSpace space = MakeFakeAddressSpace(128, 128);
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{128}, 1, 0),
              IsAddress(128 + 128));
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{128}, 1, 1),
              Eq(nullptr));
}

TEST(BoundsTest, MapOutOfAddressSpace_HighAddress) {
  constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
  // Note that kMax is *not* a valid address; AddressSpace requires that 'one
  // past the end' is <= kMax.
  AddressSpace space = MakeFakeAddressSpace(kMax - 128, 128);

  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{0}, 8, 128 / 8),
              IsAddress(kMax - 128));
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{127}, 1, 1),
              IsAddress(kMax - 1));
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{128}, 1, 0),
              IsAddress(kMax));

  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{128}, 1, 1),
              Eq(nullptr));
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{0}, 1, 129),
              Eq(nullptr));
}

TEST(BoundsTest, MapOutOfAddressSpace_Overflow) {
  constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
  AddressSpace space = MakeFakeAddressSpace(0, kMax);

  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{kMax - 1}, 1, 1),
              IsAddress(kMax - 1));
  EXPECT_THAT(MapOutOfAddressSpace(space, ForeignPointer{kMax - 1}, 1, 2),
              Eq(nullptr));
}

}  // namespace fcp
