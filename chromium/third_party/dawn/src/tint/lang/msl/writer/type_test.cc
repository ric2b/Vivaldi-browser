// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "gmock/gmock.h"

#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/depth_multisampled_texture.h"
#include "src/tint/lang/core/type/depth_texture.h"
#include "src/tint/lang/core/type/matrix.h"
#include "src/tint/lang/core/type/multisampled_texture.h"
#include "src/tint/lang/core/type/sampled_texture.h"
#include "src/tint/lang/core/type/storage_texture.h"
#include "src/tint/lang/core/type/struct.h"
#include "src/tint/lang/msl/writer/helper_test.h"
#include "src/tint/utils/text/string.h"

namespace tint::msl::writer {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

TEST_F(MslWriterTest, EmitType_Array) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.array<bool, 4>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + MetalArray() + R"(
void foo() {
  thread tint_array<bool, 4> a = {};
}
)");
}

TEST_F(MslWriterTest, EmitType_ArrayOfArray) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.array(ty.array<bool, 4>(), 5)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + MetalArray() + R"(
void foo() {
  thread tint_array<tint_array<bool, 4>, 5> a = {};
}
)");
}

TEST_F(MslWriterTest, EmitType_ArrayOfArrayOfArray) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a",
              ty.ptr(core::AddressSpace::kPrivate, ty.array(ty.array(ty.array<bool, 4>(), 5), 6)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + MetalArray() + R"(
void foo() {
  thread tint_array<tint_array<tint_array<bool, 4>, 5>, 6> a = {};
}
)");
}

TEST_F(MslWriterTest, EmitType_RuntimeArray) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.array<bool, 0>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + MetalArray() + R"(
void foo() {
  thread tint_array<bool, 1> a = {};
}
)");
}

TEST_F(MslWriterTest, EmitType_Bool) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.bool_()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  thread bool a = false;
}
)");
}

TEST_F(MslWriterTest, EmitType_F32) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.f32()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  thread float a = 0.0f;
}
)");
}

TEST_F(MslWriterTest, EmitType_F16) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.f16()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  thread half a = 0.0h;
}
)");
}

TEST_F(MslWriterTest, EmitType_I32) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.i32()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  thread int a = 0;
}
)");
}

TEST_F(MslWriterTest, EmitType_Matrix_F32) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.mat2x3<f32>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  thread float2x3 a = float2x3(0.0f);
}
)");
}

TEST_F(MslWriterTest, EmitType_Matrix_F16) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.mat2x3<f16>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  thread half2x3 a = half2x3(0.0h);
}
)");
}
TEST_F(MslWriterTest, EmitType_U32) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.u32()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  thread uint a = 0u;
}
)");
}

TEST_F(MslWriterTest, EmitType_Atomic_U32) {
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", ty.ptr(core::AddressSpace::kWorkgroup, ty.atomic<u32>()));
    func->SetParams({param});
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo(threadgroup atomic_uint* const a) {
}
)");
}

TEST_F(MslWriterTest, EmitType_Atomic_I32) {
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", ty.ptr(core::AddressSpace::kWorkgroup, ty.atomic<i32>()));
    func->SetParams({param});
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo(threadgroup atomic_int* const a) {
}
)");
}

TEST_F(MslWriterTest, EmitType_Vector) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.vec3<f32>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  thread float3 a = 0.0f;
}
)");
}

TEST_F(MslWriterTest, EmitType_VectorPacked) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kFunction, ty.packed_vec(ty.f32(), 3)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  packed_float3 a = 0.0f;
}
)");
}

TEST_F(MslWriterTest, EmitType_Void) {
    // Tested via the function return type.
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
}
)");
}

// TODO(dsinclair): How do we create a pointer type ... ?
TEST_F(MslWriterTest, DISABLED_EmitType_Pointer_Workgroup) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr<workgroup, f32, read_write>());
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  threadgroup float* a;
}
)");
}

// TODO(dsinclair): How do we create a pointer type ... ?
TEST_F(MslWriterTest, DISABLED_EmitType_Pointer_Const) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr<function, f32>());
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo() {
  const thread float* a = 0.0f;
}
)");
}

TEST_F(MslWriterTest, EmitType_Struct) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.i32()},
                                                  {mod.symbols.Register("b"), ty.f32()},
                                              });
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
struct S {
  int a;
  float b;
};

void foo() {
  thread S a = {};
}
)");
}

TEST_F(MslWriterTest, EmitType_Struct_Dedup) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.i32()},
                                                  {mod.symbols.Register("b"), ty.f32()},
                                              });
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Var("b", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
struct S {
  int a;
  float b;
};

void foo() {
  thread S a = {};
  thread S b = {};
}
)");
}

void FormatMSLField(StringStream& out,
                    const char* addr,
                    const char* type,
                    size_t array_count,
                    const char* name) {
    out << "  /* " << std::string(addr) << " */ ";
    if (array_count == 0) {
        out << type << " ";
    } else {
        out << "tint_array<" << type << ", " << std::to_string(array_count) << "> ";
    }
    out << name << ";\n";
}

#define CHECK_TYPE_SIZE_AND_ALIGN(TYPE, SIZE, ALIGN)      \
    static_assert(sizeof(TYPE) == SIZE, "Bad type size"); \
    static_assert(alignof(TYPE) == ALIGN, "Bad type alignment")

// Declare C++ types that match the size and alignment of the types of the same
// name in MSL.
#define DECLARE_TYPE(NAME, SIZE, ALIGN) \
    struct alignas(ALIGN) NAME {        \
        uint8_t _[SIZE];                \
    };                                  \
    CHECK_TYPE_SIZE_AND_ALIGN(NAME, SIZE, ALIGN)

// Size and alignments taken from the MSL spec:
// https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
DECLARE_TYPE(float2, 8, 8);
DECLARE_TYPE(float3, 12, 4);
DECLARE_TYPE(float4, 16, 16);
DECLARE_TYPE(float2x2, 16, 8);
DECLARE_TYPE(float2x3, 32, 16);
DECLARE_TYPE(float2x4, 32, 16);
DECLARE_TYPE(float3x2, 24, 8);
DECLARE_TYPE(float3x3, 48, 16);
DECLARE_TYPE(float3x4, 48, 16);
DECLARE_TYPE(float4x2, 32, 8);
DECLARE_TYPE(float4x3, 64, 16);
DECLARE_TYPE(float4x4, 64, 16);
DECLARE_TYPE(half2, 4, 4);
DECLARE_TYPE(packed_half3, 6, 2);
DECLARE_TYPE(half4, 8, 8);
DECLARE_TYPE(half2x2, 8, 4);
DECLARE_TYPE(half2x3, 16, 8);
DECLARE_TYPE(half2x4, 16, 8);
DECLARE_TYPE(half3x2, 12, 4);
DECLARE_TYPE(half3x3, 24, 8);
DECLARE_TYPE(half3x4, 24, 8);
DECLARE_TYPE(half4x2, 16, 4);
DECLARE_TYPE(half4x3, 32, 8);
DECLARE_TYPE(half4x4, 32, 8);
using uint = unsigned int;

struct MemberData {
    Symbol name;
    const core::type::Type* type;
    uint32_t size = 0;
    uint32_t align = 0;
};
core::type::Struct* MkStruct(core::ir::Module& mod,
                             core::type::Manager& ty,
                             std::string_view name,
                             VectorRef<MemberData> data) {
    Vector<const core::type::StructMember*, 26> members;
    uint32_t align = 0;
    uint32_t size = 0;
    for (uint32_t i = 0; i < data.Length(); ++i) {
        auto& d = data[i];

        uint32_t mem_align = d.align == 0 ? d.type->Align() : d.align;
        uint32_t mem_size = d.size == 0 ? d.type->Size() : d.size;

        uint32_t offset = tint::RoundUp(mem_align, size);
        members.Push(ty.Get<core::type::StructMember>(d.name, d.type, i, offset, mem_align,
                                                      mem_size, core::IOAttributes{}));

        align = std::max(align, mem_align);
        size = offset + mem_size;
    }

    return ty.Get<core::type::Struct>(mod.symbols.New(name), std::move(members), align,
                                      tint::RoundUp(align, size), size);
}

TEST_F(MslWriterTest, EmitType_Struct_Layout_NonComposites) {
    Vector<MemberData, 26> data = {{mod.symbols.Register("a"), ty.i32(), 32},        //
                                   {mod.symbols.Register("b"), ty.f32(), 128, 128},  //
                                   {mod.symbols.Register("c"), ty.vec2<f32>()},      //
                                   {mod.symbols.Register("d"), ty.u32()},            //
                                   {mod.symbols.Register("e"), ty.vec3<f32>()},      //
                                   {mod.symbols.Register("f"), ty.u32()},            //
                                   {mod.symbols.Register("g"), ty.vec4<f32>()},      //
                                   {mod.symbols.Register("h"), ty.u32()},            //
                                   {mod.symbols.Register("i"), ty.mat2x2<f32>()},    //
                                   {mod.symbols.Register("j"), ty.u32()},            //
                                   {mod.symbols.Register("k"), ty.mat2x3<f32>()},    //
                                   {mod.symbols.Register("l"), ty.u32()},            //
                                   {mod.symbols.Register("m"), ty.mat2x4<f32>()},    //
                                   {mod.symbols.Register("n"), ty.u32()},            //
                                   {mod.symbols.Register("o"), ty.mat3x2<f32>()},    //
                                   {mod.symbols.Register("p"), ty.u32()},            //
                                   {mod.symbols.Register("q"), ty.mat3x3<f32>()},    //
                                   {mod.symbols.Register("r"), ty.u32()},            //
                                   {mod.symbols.Register("s"), ty.mat3x4<f32>()},    //
                                   {mod.symbols.Register("t"), ty.u32()},            //
                                   {mod.symbols.Register("u"), ty.mat4x2<f32>()},    //
                                   {mod.symbols.Register("v"), ty.u32()},            //
                                   {mod.symbols.Register("w"), ty.mat4x3<f32>()},    //
                                   {mod.symbols.Register("x"), ty.u32()},            //
                                   {mod.symbols.Register("y"), ty.mat4x4<f32>()},    //
                                   {mod.symbols.Register("z"), ty.f32()}};

    auto* s = MkStruct(mod, ty, "S", data);
    s->AddUsage(core::AddressSpace::kStorage);

    // ALL_FIELDS() calls the macro FIELD(ADDR, TYPE, ARRAY_COUNT, NAME)
    // for each field of the structure s.
#define ALL_FIELDS()                       \
    FIELD(0x0000, int, 0, a)               \
    FIELD(0x0004, int8_t, 124, tint_pad)   \
    FIELD(0x0080, float, 0, b)             \
    FIELD(0x0084, int8_t, 124, tint_pad_1) \
    FIELD(0x0100, float2, 0, c)            \
    FIELD(0x0108, uint, 0, d)              \
    FIELD(0x010c, int8_t, 4, tint_pad_2)   \
    FIELD(0x0110, float3, 0, e)            \
    FIELD(0x011c, uint, 0, f)              \
    FIELD(0x0120, float4, 0, g)            \
    FIELD(0x0130, uint, 0, h)              \
    FIELD(0x0134, int8_t, 4, tint_pad_3)   \
    FIELD(0x0138, float2x2, 0, i)          \
    FIELD(0x0148, uint, 0, j)              \
    FIELD(0x014c, int8_t, 4, tint_pad_4)   \
    FIELD(0x0150, float2x3, 0, k)          \
    FIELD(0x0170, uint, 0, l)              \
    FIELD(0x0174, int8_t, 12, tint_pad_5)  \
    FIELD(0x0180, float2x4, 0, m)          \
    FIELD(0x01a0, uint, 0, n)              \
    FIELD(0x01a4, int8_t, 4, tint_pad_6)   \
    FIELD(0x01a8, float3x2, 0, o)          \
    FIELD(0x01c0, uint, 0, p)              \
    FIELD(0x01c4, int8_t, 12, tint_pad_7)  \
    FIELD(0x01d0, float3x3, 0, q)          \
    FIELD(0x0200, uint, 0, r)              \
    FIELD(0x0204, int8_t, 12, tint_pad_8)  \
    FIELD(0x0210, float3x4, 0, s)          \
    FIELD(0x0240, uint, 0, t)              \
    FIELD(0x0244, int8_t, 4, tint_pad_9)   \
    FIELD(0x0248, float4x2, 0, u)          \
    FIELD(0x0268, uint, 0, v)              \
    FIELD(0x026c, int8_t, 4, tint_pad_10)  \
    FIELD(0x0270, float4x3, 0, w)          \
    FIELD(0x02b0, uint, 0, x)              \
    FIELD(0x02b4, int8_t, 12, tint_pad_11) \
    FIELD(0x02c0, float4x4, 0, y)          \
    FIELD(0x0300, float, 0, z)             \
    FIELD(0x0304, int8_t, 124, tint_pad_12)

    // Check that the generated string is as expected.
    StringStream expect;
    expect << MetalHeader() << MetalArray() << "\nstruct S {\n";
#define FIELD(ADDR, TYPE, ARRAY_COUNT, NAME) \
    FormatMSLField(expect, #ADDR, #TYPE, ARRAY_COUNT, #NAME);
    ALL_FIELDS()
#undef FIELD
    expect << R"(};

void foo() {
  thread S a = {};
}
)";

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, expect.str());

    // 1.4 Metal and C++14
    // The Metal programming language is a C++14-based Specification with
    // extensions and restrictions. Refer to the C++14 Specification (also
    // known as the ISO/IEC JTC1/SC22/WG21 N4431 Language Specification) for a
    // detailed description of the language grammar.
    //
    // Tint is written in C++14, so use the compiler to verify the generated
    // layout is as expected for C++14 / MSL.
    {
        struct S {
#define FIELD(ADDR, TYPE, ARRAY_COUNT, NAME) std::array<TYPE, ARRAY_COUNT ? ARRAY_COUNT : 1> NAME;
            ALL_FIELDS()
#undef FIELD
        };

#define FIELD(ADDR, TYPE, ARRAY_COUNT, NAME) \
    EXPECT_EQ(ADDR, static_cast<int>(offsetof(S, NAME))) << "Field " << #NAME;
        ALL_FIELDS()
#undef FIELD
    }
#undef ALL_FIELDS
}

TEST_F(MslWriterTest, EmitType_Struct_Layout_Structures) {
    // inner_x: size(1024), align(512)
    Vector<MemberData, 2> inner_x_data = {{{mod.symbols.Register("a"), ty.i32()},  //
                                           {mod.symbols.Register("b"), ty.f32(), 0, 512}}};
    auto* inner_x = MkStruct(mod, ty, "inner_x", inner_x_data);

    // inner_y: size(516), align(4)
    Vector<MemberData, 2> inner_y_data = {{mod.symbols.Register("a"), ty.i32(), 512},
                                          {mod.symbols.Register("b"), ty.f32()}};

    auto* inner_y = MkStruct(mod, ty, "inner_y", inner_y_data);

    auto* s = ty.Struct(mod.symbols.New("S"), {{mod.symbols.Register("a"), ty.i32()},
                                               {mod.symbols.Register("b"), inner_x},
                                               {mod.symbols.Register("c"), ty.f32()},
                                               {mod.symbols.Register("d"), inner_y},
                                               {mod.symbols.Register("e"), ty.f32()}});
    const_cast<core::type::Struct*>(s)->AddUsage(core::AddressSpace::kStorage);

// ALL_FIELDS() calls the macro FIELD(ADDR, TYPE, ARRAY_COUNT, NAME)
// for each field of the structure s.
#define ALL_FIELDS()                     \
    FIELD(0x0000, int, 0, a)             \
    FIELD(0x0004, int8_t, 508, tint_pad) \
    FIELD(0x0200, inner_x, 0, b)         \
    FIELD(0x0600, float, 0, c)           \
    FIELD(0x0604, inner_y, 0, d)         \
    FIELD(0x0808, float, 0, e)           \
    FIELD(0x080c, int8_t, 500, tint_pad_1)

    // Check that the generated string is as expected.
    StringStream expect;
    expect << MetalHeader() << MetalArray() << R"(
struct inner_x {
  int a;
  float b;
};

struct inner_y {
  int a;
  float b;
};

)";

    expect << "struct S {\n";
#define FIELD(ADDR, TYPE, ARRAY_COUNT, NAME) \
    FormatMSLField(expect, #ADDR, #TYPE, ARRAY_COUNT, #NAME);
    ALL_FIELDS()
#undef FIELD
    expect << R"(};

void foo() {
  thread S a = {};
}
)";

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, expect.str());

    // 1.4 Metal and C++14
    // The Metal programming language is a C++14-based Specification with
    // extensions and restrictions. Refer to the C++14 Specification (also
    // known as the ISO/IEC JTC1/SC22/WG21 N4431 Language Specification) for a
    // detailed description of the language grammar.
    //
    // Tint is written in C++14, so use the compiler to verify the generated
    // layout is as expected for C++14 / MSL.
    {
        struct inner_x {
            uint32_t a;
            alignas(512) float b;
        };
        CHECK_TYPE_SIZE_AND_ALIGN(inner_x, 1024, 512);

        struct inner_y {
            uint32_t a[128];
            float b;
        };
        CHECK_TYPE_SIZE_AND_ALIGN(inner_y, 516, 4);

        struct S {
#define FIELD(ADDR, TYPE, ARRAY_COUNT, NAME) std::array<TYPE, ARRAY_COUNT ? ARRAY_COUNT : 1> NAME;
            ALL_FIELDS()
#undef FIELD
        };

#define FIELD(ADDR, TYPE, ARRAY_COUNT, NAME) \
    EXPECT_EQ(ADDR, static_cast<int>(offsetof(S, NAME))) << "Field " << #NAME;
        ALL_FIELDS()
#undef FIELD
    }

#undef ALL_FIELDS
}

TEST_F(MslWriterTest, EmitType_Struct_Layout_ArrayDefaultStride) {
    // inner: size(1024), align(512)
    Vector<MemberData, 2> inner_data = {{mod.symbols.Register("a"), ty.i32()},
                                        {mod.symbols.Register("b"), ty.f32(), 0, 512}};

    auto* inner = MkStruct(mod, ty, "inner", inner_data);

    // array_x: size(28), align(4)
    auto array_x = ty.array<f32, 7>();

    // array_y: size(4096), align(512)
    auto array_y = ty.array(inner, 4_u);

    // array_z: size(4), align(4)
    auto array_z = ty.array<f32>();

    auto* s = ty.Struct(mod.symbols.New("S"), {{mod.symbols.Register("a"), ty.i32()},
                                               {mod.symbols.Register("b"), array_x},
                                               {mod.symbols.Register("c"), ty.f32()},
                                               {mod.symbols.Register("d"), array_y},
                                               {mod.symbols.Register("e"), ty.f32()},
                                               {mod.symbols.Register("f"), array_z}});
    const_cast<core::type::Struct*>(s)->AddUsage(core::AddressSpace::kStorage);

    // ALL_FIELDS() calls the macro FIELD(ADDR, TYPE, ARRAY_COUNT, NAME)
    // for each field of the structure s.
#define ALL_FIELDS()                     \
    FIELD(0x0000, int, 0, a)             \
    FIELD(0x0004, float, 7, b)           \
    FIELD(0x0020, float, 0, c)           \
    FIELD(0x0024, int8_t, 476, tint_pad) \
    FIELD(0x0200, inner, 4, d)           \
    FIELD(0x1200, float, 0, e)           \
    FIELD(0x1204, float, 1, f)           \
    FIELD(0x1208, int8_t, 504, tint_pad_1)

    // Check that the generated string is as expected.
    StringStream expect;

    expect << MetalHeader() << MetalArray() << R"(
struct inner {
  int a;
  float b;
};

)";

    expect << "struct S {\n";
#define FIELD(ADDR, TYPE, ARRAY_COUNT, NAME) \
    FormatMSLField(expect, #ADDR, #TYPE, ARRAY_COUNT, #NAME);
    ALL_FIELDS()
#undef FIELD
    expect << R"(};

void foo() {
  thread S a = {};
}
)";

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, expect.str());

    // 1.4 Metal and C++14
    // The Metal programming language is a C++14-based Specification with
    // extensions and restrictions. Refer to the C++14 Specification (also
    // known as the ISO/IEC JTC1/SC22/WG21 N4431 Language Specification) for a
    // detailed description of the language grammar.
    //
    // Tint is written in C++14, so use the compiler to verify the generated
    // layout is as expected for C++14 / MSL.
    {
        struct inner {
            uint32_t a;
            alignas(512) float b;
        };
        CHECK_TYPE_SIZE_AND_ALIGN(inner, 1024, 512);

        // array_x: size(28), align(4)
        using array_x = std::array<float, 7>;
        CHECK_TYPE_SIZE_AND_ALIGN(array_x, 28, 4);

        // array_y: size(4096), align(512)
        using array_y = std::array<inner, 4>;
        CHECK_TYPE_SIZE_AND_ALIGN(array_y, 4096, 512);

        // array_z: size(4), align(4)
        using array_z = std::array<float, 1>;
        CHECK_TYPE_SIZE_AND_ALIGN(array_z, 4, 4);

        struct S {
#define FIELD(ADDR, TYPE, ARRAY_COUNT, NAME) std::array<TYPE, ARRAY_COUNT ? ARRAY_COUNT : 1> NAME;
            ALL_FIELDS()
#undef FIELD
        };

#define FIELD(ADDR, TYPE, ARRAY_COUNT, NAME) \
    EXPECT_EQ(ADDR, static_cast<int>(offsetof(S, NAME))) << "Field " << #NAME;
        ALL_FIELDS()
#undef FIELD
    }

#undef ALL_FIELDS
}

TEST_F(MslWriterTest, EmitType_Struct_Layout_ArrayVec3DefaultStride) {
    // array: size(64), align(16)
    auto array = ty.array<vec3<f32>, 4>();

    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.i32()},
                                                  {mod.symbols.Register("b"), array},
                                                  {mod.symbols.Register("c"), ty.i32()},
                                              });
    const_cast<core::type::Struct*>(s)->AddUsage(core::AddressSpace::kStorage);

    // ALL_FIELDS() calls the macro FIELD(ADDR, TYPE, ARRAY_COUNT, NAME)
    // for each field of the structure s.
#define ALL_FIELDS()                    \
    FIELD(0x0000, int, 0, a)            \
    FIELD(0x0004, int8_t, 12, tint_pad) \
    FIELD(0x0010, float3, 4, b)         \
    FIELD(0x0050, int, 0, c)            \
    FIELD(0x0054, int8_t, 12, tint_pad_1)

    // Check that the generated string is as expected.
    StringStream expect;

    expect << MetalHeader() << MetalArray() << "\nstruct S {\n";
#define FIELD(ADDR, TYPE, ARRAY_COUNT, NAME) \
    FormatMSLField(expect, #ADDR, #TYPE, ARRAY_COUNT, #NAME);
    ALL_FIELDS()
#undef FIELD
    expect << R"(};

void foo() {
  thread S a = {};
}
)";

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, expect.str());
}

TEST_F(MslWriterTest, AttemptTintPadSymbolCollision) {
    Vector<MemberData, 26> data = {// uses symbols tint_pad_[0..9] and tint_pad_[20..35]
                                   {mod.symbols.Register("tint_pad_2"), ty.i32(), 32},         //
                                   {mod.symbols.Register("tint_pad_20"), ty.f32(), 128, 128},  //
                                   {mod.symbols.Register("tint_pad_33"), ty.vec2<f32>()},      //
                                   {mod.symbols.Register("tint_pad_1"), ty.u32()},             //
                                   {mod.symbols.Register("tint_pad_3"), ty.vec3<f32>()},       //
                                   {mod.symbols.Register("tint_pad_7"), ty.u32()},             //
                                   {mod.symbols.Register("tint_pad_25"), ty.vec4<f32>()},      //
                                   {mod.symbols.Register("tint_pad_5"), ty.u32()},             //
                                   {mod.symbols.Register("tint_pad_27"), ty.mat2x2<f32>()},    //
                                   {mod.symbols.Register("tint_pad_24"), ty.u32()},            //
                                   {mod.symbols.Register("tint_pad_23"), ty.mat2x3<f32>()},    //
                                   {mod.symbols.Register("tint_pad"), ty.u32()},               //
                                   {mod.symbols.Register("tint_pad_8"), ty.mat2x4<f32>()},     //
                                   {mod.symbols.Register("tint_pad_26"), ty.u32()},            //
                                   {mod.symbols.Register("tint_pad_29"), ty.mat3x2<f32>()},    //
                                   {mod.symbols.Register("tint_pad_6"), ty.u32()},             //
                                   {mod.symbols.Register("tint_pad_22"), ty.mat3x3<f32>()},    //
                                   {mod.symbols.Register("tint_pad_32"), ty.u32()},            //
                                   {mod.symbols.Register("tint_pad_34"), ty.mat3x4<f32>()},    //
                                   {mod.symbols.Register("tint_pad_35"), ty.u32()},            //
                                   {mod.symbols.Register("tint_pad_30"), ty.mat4x2<f32>()},    //
                                   {mod.symbols.Register("tint_pad_9"), ty.u32()},             //
                                   {mod.symbols.Register("tint_pad_31"), ty.mat4x3<f32>()},    //
                                   {mod.symbols.Register("tint_pad_28"), ty.u32()},            //
                                   {mod.symbols.Register("tint_pad_4"), ty.mat4x4<f32>()},     //
                                   {mod.symbols.Register("tint_pad_21"), ty.f32()}};

    auto* s = MkStruct(mod, ty, "S", data);
    s->AddUsage(core::AddressSpace::kStorage);

    auto expect = MetalHeader() + MetalArray() + R"(
struct S {
  /* 0x0000 */ int tint_pad_2;
  /* 0x0004 */ tint_array<int8_t, 124> tint_pad_10;
  /* 0x0080 */ float tint_pad_20;
  /* 0x0084 */ tint_array<int8_t, 124> tint_pad_11;
  /* 0x0100 */ float2 tint_pad_33;
  /* 0x0108 */ uint tint_pad_1;
  /* 0x010c */ tint_array<int8_t, 4> tint_pad_12;
  /* 0x0110 */ float3 tint_pad_3;
  /* 0x011c */ uint tint_pad_7;
  /* 0x0120 */ float4 tint_pad_25;
  /* 0x0130 */ uint tint_pad_5;
  /* 0x0134 */ tint_array<int8_t, 4> tint_pad_13;
  /* 0x0138 */ float2x2 tint_pad_27;
  /* 0x0148 */ uint tint_pad_24;
  /* 0x014c */ tint_array<int8_t, 4> tint_pad_14;
  /* 0x0150 */ float2x3 tint_pad_23;
  /* 0x0170 */ uint tint_pad;
  /* 0x0174 */ tint_array<int8_t, 12> tint_pad_15;
  /* 0x0180 */ float2x4 tint_pad_8;
  /* 0x01a0 */ uint tint_pad_26;
  /* 0x01a4 */ tint_array<int8_t, 4> tint_pad_16;
  /* 0x01a8 */ float3x2 tint_pad_29;
  /* 0x01c0 */ uint tint_pad_6;
  /* 0x01c4 */ tint_array<int8_t, 12> tint_pad_17;
  /* 0x01d0 */ float3x3 tint_pad_22;
  /* 0x0200 */ uint tint_pad_32;
  /* 0x0204 */ tint_array<int8_t, 12> tint_pad_18;
  /* 0x0210 */ float3x4 tint_pad_34;
  /* 0x0240 */ uint tint_pad_35;
  /* 0x0244 */ tint_array<int8_t, 4> tint_pad_19;
  /* 0x0248 */ float4x2 tint_pad_30;
  /* 0x0268 */ uint tint_pad_9;
  /* 0x026c */ tint_array<int8_t, 4> tint_pad_36;
  /* 0x0270 */ float4x3 tint_pad_31;
  /* 0x02b0 */ uint tint_pad_28;
  /* 0x02b4 */ tint_array<int8_t, 12> tint_pad_37;
  /* 0x02c0 */ float4x4 tint_pad_4;
  /* 0x0300 */ float tint_pad_21;
  /* 0x0304 */ tint_array<int8_t, 124> tint_pad_38;
};

void foo() {
  thread S a = {};
}
)";

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, expect);
}

TEST_F(MslWriterTest, EmitType_Sampler) {
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", ty.sampler());
    func->SetParams({param});
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo(sampler a) {
}
)");
}

TEST_F(MslWriterTest, EmitType_SamplerComparison) {
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", ty.comparison_sampler());
    func->SetParams({param});
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo(sampler a) {
}
)");
}

struct MslDepthTextureData {
    core::type::TextureDimension dim;
    std::string result;
};
inline std::ostream& operator<<(std::ostream& out, MslDepthTextureData data) {
    StringStream str;
    str << data.dim;
    out << str.str();
    return out;
}
using MslWriterDepthTexturesTest = MslWriterTestWithParam<MslDepthTextureData>;
TEST_P(MslWriterDepthTexturesTest, Emit) {
    auto params = GetParam();

    auto* t = ty.Get<core::type::DepthTexture>(params.dim);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", t);
    func->SetParams({param});
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo()" + params.result +
                               R"( a) {
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    MslWriterTest,
    MslWriterDepthTexturesTest,
    testing::Values(MslDepthTextureData{core::type::TextureDimension::k2d,
                                        "depth2d<float, access::sample>"},
                    MslDepthTextureData{core::type::TextureDimension::k2dArray,
                                        "depth2d_array<float, access::sample>"},
                    MslDepthTextureData{core::type::TextureDimension::kCube,
                                        "depthcube<float, access::sample>"},
                    MslDepthTextureData{core::type::TextureDimension::kCubeArray,
                                        "depthcube_array<float, access::sample>"}));

TEST_F(MslWriterTest, EmitType_DepthMultisampledTexture) {
    auto* t = ty.Get<core::type::DepthMultisampledTexture>(core::type::TextureDimension::k2d);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", t);
    func->SetParams({param});
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo(depth2d_ms<float, access::read> a) {
}
)");
}

struct MslTextureData {
    core::type::TextureDimension dim;
    std::string result;
};
inline std::ostream& operator<<(std::ostream& out, MslTextureData data) {
    StringStream str;
    str << data.dim;
    out << str.str();
    return out;
}
using MslWriterSampledtexturesTest = MslWriterTestWithParam<MslTextureData>;
TEST_P(MslWriterSampledtexturesTest, Emit) {
    auto params = GetParam();

    auto* t = ty.Get<core::type::SampledTexture>(params.dim, ty.f32());
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", t);
    func->SetParams({param});
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo()" + params.result +
                               R"( a) {
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    MslWriterTest,
    MslWriterSampledtexturesTest,
    testing::Values(
        MslTextureData{core::type::TextureDimension::k1d, "texture1d<float, access::sample>"},
        MslTextureData{core::type::TextureDimension::k2d, "texture2d<float, access::sample>"},
        MslTextureData{core::type::TextureDimension::k2dArray,
                       "texture2d_array<float, access::sample>"},
        MslTextureData{core::type::TextureDimension::k3d, "texture3d<float, access::sample>"},
        MslTextureData{core::type::TextureDimension::kCube, "texturecube<float, access::sample>"},
        MslTextureData{core::type::TextureDimension::kCubeArray,
                       "texturecube_array<float, access::sample>"}));

TEST_F(MslWriterTest, EmitType_MultisampledTexture) {
    auto* ms = ty.Get<core::type::MultisampledTexture>(core::type::TextureDimension::k2d, ty.u32());
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", ms);
    func->SetParams({param});
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo(texture2d_ms<uint, access::read> a) {
}
)");
}

struct MslStorageTextureData {
    core::type::TextureDimension dim;
    std::string result;
};
inline std::ostream& operator<<(std::ostream& out, MslStorageTextureData data) {
    StringStream str;
    str << data.dim;
    return out << str.str();
}
using MslWriterStorageTexturesTest = MslWriterTestWithParam<MslStorageTextureData>;
TEST_P(MslWriterStorageTexturesTest, Emit) {
    auto params = GetParam();

    auto* f32 = const_cast<core::type::F32*>(ty.f32());
    auto s = ty.Get<core::type::StorageTexture>(params.dim, core::TexelFormat::kR32Float,
                                                core::Access::kWrite, f32);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", s);
    func->SetParams({param});
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.msl;
    EXPECT_EQ(output_.msl, MetalHeader() + R"(
void foo()" + params.result +
                               R"( a) {
}
)");
}
INSTANTIATE_TEST_SUITE_P(MslWriterTest,
                         MslWriterStorageTexturesTest,
                         testing::Values(MslStorageTextureData{core::type::TextureDimension::k1d,
                                                               "texture1d<float, access::write>"},
                                         MslStorageTextureData{core::type::TextureDimension::k2d,
                                                               "texture2d<float, access::write>"},
                                         MslStorageTextureData{
                                             core::type::TextureDimension::k2dArray,
                                             "texture2d_array<float, access::write>"},
                                         MslStorageTextureData{core::type::TextureDimension::k3d,
                                                               "texture3d<float, access::write>"}));

}  // namespace
}  // namespace tint::msl::writer
