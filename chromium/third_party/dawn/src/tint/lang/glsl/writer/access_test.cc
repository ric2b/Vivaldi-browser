// Copyright 2024 The Dawn & Tint Authors
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

#include "src/tint/lang/glsl/writer/helper_test.h"

#include "gmock/gmock.h"

using namespace tint::core::number_suffixes;  // NOLINT
using namespace tint::core::fluent_types;     // NOLINT

namespace tint::glsl::writer {
namespace {

TEST_F(GlslWriterTest, AccessArray) {
    auto* func = b.Function("a", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);

    b.Append(func->Block(), [&] {
        auto* v = b.Var("v", b.Zero<array<f32, 3>>());
        b.Let("x", b.Load(b.Access(ty.ptr<function, f32>(), v, 1_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  float v[3] = float[3](0.0f, 0.0f, 0.0f);
  float x = v[1u];
}
)");
}

TEST_F(GlslWriterTest, AccessStruct) {
    Vector members{
        ty.Get<core::type::StructMember>(b.ir.symbols.New("a"), ty.i32(), 0u, 0u, 4u, 4u,
                                         core::IOAttributes{}),
        ty.Get<core::type::StructMember>(b.ir.symbols.New("b"), ty.f32(), 1u, 4u, 4u, 4u,
                                         core::IOAttributes{}),
    };
    auto* strct = ty.Struct(b.ir.symbols.New("S"), std::move(members));

    auto* f = b.Function("a", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    f->SetWorkgroupSize(1, 1, 1);

    b.Append(f->Block(), [&] {
        auto* v = b.Var("v", b.Zero(strct));
        b.Let("x", b.Load(b.Access(ty.ptr<function, f32>(), v, 1_u)));
        b.Return(f);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

struct S {
  int a;
  float b;
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  S v = S(0, 0.0f);
  float x = v.b;
}
)");
}

TEST_F(GlslWriterTest, AccessVector) {
    auto* func = b.Function("a", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);

    b.Append(func->Block(), [&] {
        auto* v = b.Var("v", b.Zero<vec3<f32>>());
        b.Let("x", b.LoadVectorElement(v, 1_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  vec3 v = vec3(0.0f);
  float x = v.y;
}
)");
}

TEST_F(GlslWriterTest, AccessMatrix) {
    auto* func = b.Function("a", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);

    b.Append(func->Block(), [&] {
        auto* v = b.Var("v", b.Zero<mat4x4<f32>>());
        auto* v1 = b.Access(ty.ptr<function, vec4<f32>>(), v, 1_u);
        b.Let("x", b.LoadVectorElement(v1, 2_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  mat4 v = mat4(vec4(0.0f), vec4(0.0f), vec4(0.0f), vec4(0.0f));
  float x = v[1u].z;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreVectorElementConstantIndex) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        auto* vec_var = b.Var("vec", ty.ptr<function, vec4<i32>>());
        b.StoreVectorElement(vec_var, 1_u, b.Constant(42_i));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
void foo() {
  ivec4 vec = ivec4(0);
  vec[1u] = 42;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}

// TODO(dsinclair): Needs ir::Convert
TEST_F(GlslWriterTest, DISABLED_AccessStoreVectorElementDynamicIndex) {
    auto* idx = b.FunctionParam("idx", ty.i32());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({idx});
    b.Append(func->Block(), [&] {
        auto* vec_var = b.Var("vec", ty.ptr<function, vec4<i32>>());
        b.StoreVectorElement(vec_var, idx, b.Constant(42_i));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

)");
}

TEST_F(GlslWriterTest, AccessNested) {
    Vector members_a{
        ty.Get<core::type::StructMember>(b.ir.symbols.New("d"), ty.i32(), 0u, 0u, 4u, 4u,
                                         core::IOAttributes{}),
        ty.Get<core::type::StructMember>(b.ir.symbols.New("e"), ty.array<f32, 3>(), 1u, 4u, 4u, 4u,
                                         core::IOAttributes{}),
    };
    auto* a_strct = ty.Struct(b.ir.symbols.New("A"), std::move(members_a));

    Vector members_s{
        ty.Get<core::type::StructMember>(b.ir.symbols.New("a"), ty.i32(), 0u, 0u, 4u, 4u,
                                         core::IOAttributes{}),
        ty.Get<core::type::StructMember>(b.ir.symbols.New("b"), ty.f32(), 1u, 4u, 4u, 4u,
                                         core::IOAttributes{}),
        ty.Get<core::type::StructMember>(b.ir.symbols.New("c"), a_strct, 2u, 8u, 8u, 8u,
                                         core::IOAttributes{}),
    };
    auto* s_strct = ty.Struct(b.ir.symbols.New("S"), std::move(members_s));

    auto* f = b.Function("a", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    f->SetWorkgroupSize(1, 1, 1);

    b.Append(f->Block(), [&] {
        auto* v = b.Var("v", b.Zero(s_strct));
        b.Let("x", b.Load(b.Access(ty.ptr<function, f32>(), v, 2_u, 1_u, 1_i)));
        b.Return(f);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

struct A {
  int d;
  float e[3];
};

struct S {
  int a;
  float b;
  A c;
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  S v = S(0, 0.0f, A(0, float[3](0.0f, 0.0f, 0.0f)));
  float x = v.c.e[1u];
}
)");
}

TEST_F(GlslWriterTest, AccessSwizzle) {
    auto* f = b.Function("a", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    f->SetWorkgroupSize(1, 1, 1);

    b.Append(f->Block(), [&] {
        auto* v = b.Var("v", b.Zero<vec3<f32>>());
        b.Let("b", b.Swizzle(ty.f32(), v, {1u}));
        b.Return(f);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  vec3 v = vec3(0.0f);
  float b = v.y;
}
)");
}

TEST_F(GlslWriterTest, AccessSwizzleMulti) {
    auto* f = b.Function("a", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    f->SetWorkgroupSize(1, 1, 1);

    b.Append(f->Block(), [&] {
        auto* v = b.Var("v", b.Zero<vec4<f32>>());
        b.Let("b", b.Swizzle(ty.vec4<f32>(), v, {3u, 2u, 1u, 0u}));
        b.Return(f);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  vec4 v = vec4(0.0f);
  vec4 b = v.wzyx;
}
)");
}

TEST_F(GlslWriterTest, AccessStorageVector) {
    auto* var = b.Var<storage, vec4<f32>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.LoadVectorElement(var, 0_u));
        b.Let("c", b.LoadVectorElement(var, 1_u));
        b.Let("d", b.LoadVectorElement(var, 2_u));
        b.Let("e", b.LoadVectorElement(var, 3_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  vec4 tint_symbol;
} v_1;
void main() {
  vec4 a = v_1.tint_symbol;
  float b = v_1.tint_symbol.x;
  float c = v_1.tint_symbol.y;
  float d = v_1.tint_symbol.z;
  float e = v_1.tint_symbol.w;
}
)");
}

TEST_F(GlslWriterTest, AccessStorageVectorF16) {
    auto* var = b.Var<storage, vec4<f16>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.LoadVectorElement(var, 0_u));
        b.Let("c", b.LoadVectorElement(var, 1_u));
        b.Let("d", b.LoadVectorElement(var, 2_u));
        b.Let("e", b.LoadVectorElement(var, 3_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  f16vec4 tint_symbol;
} v_1;
void main() {
  f16vec4 a = v_1.tint_symbol;
  float16_t b = v_1.tint_symbol.x;
  float16_t c = v_1.tint_symbol.y;
  float16_t d = v_1.tint_symbol.z;
  float16_t e = v_1.tint_symbol.w;
}
)");
}

TEST_F(GlslWriterTest, AccessStorageMatrix) {
    auto* var = b.Var<storage, mat4x4<f32>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<storage, vec4<f32>, core::Access::kRead>(), var, 3_u)));
        b.Let("c", b.LoadVectorElement(
                       b.Access(ty.ptr<storage, vec4<f32>, core::Access::kRead>(), var, 1_u), 2_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  mat4 tint_symbol;
} v_1;
void main() {
  mat4 a = v_1.tint_symbol;
  vec4 b = v_1.tint_symbol[3u];
  float c = v_1.tint_symbol[1u].z;
}
)");
}

TEST_F(GlslWriterTest, AccessStorageArray) {
    auto* var = b.Var<storage, array<vec3<f32>, 5>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<storage, vec3<f32>, core::Access::kRead>(), var, 3_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  vec3 tint_symbol[5];
} v_1;
void main() {
  vec3 a[5] = v_1.tint_symbol;
  vec3 b = v_1.tint_symbol[3u];
}
)");
}

TEST_F(GlslWriterTest, AccessStorageStruct) {
    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.f32()},
                                                });

    auto* var = b.Var("v", storage, SB, core::Access::kRead);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<storage, f32, core::Access::kRead>(), var, 1_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct SB {
  int a;
  float b;
};

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  SB tint_symbol;
} v_1;
void main() {
  SB a = v_1.tint_symbol;
  float b = v_1.tint_symbol.b;
}
)");
}

TEST_F(GlslWriterTest, AccessStorageNested) {
    auto* Inner =
        ty.Struct(mod.symbols.New("Inner"), {
                                                {mod.symbols.New("s"), ty.mat3x3<f32>()},
                                                {mod.symbols.New("t"), ty.array<vec3<f32>, 5>()},
                                            });
    auto* Outer = ty.Struct(mod.symbols.New("Outer"), {
                                                          {mod.symbols.New("x"), ty.f32()},
                                                          {mod.symbols.New("y"), Inner},
                                                      });

    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), Outer},
                                                });

    auto* var = b.Var("v", storage, SB, core::Access::kRead);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.LoadVectorElement(b.Access(ty.ptr<storage, vec3<f32>, core::Access::kRead>(),
                                                var, 1_u, 1_u, 1_u, 3_u),
                                       2_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct Inner {
  mat3 s;
  vec3 t[5];
};

struct Outer {
  float x;
  Inner y;
};

struct SB {
  int a;
  Outer b;
};

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  SB tint_symbol;
} v_1;
void main() {
  SB a = v_1.tint_symbol;
  float b = v_1.tint_symbol.b.y.t[3u].z;
}
)");
}

TEST_F(GlslWriterTest, AccessStorageStoreVector) {
    auto* var = b.Var<storage, vec4<f32>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.StoreVectorElement(var, 0_u, 2_f);
        b.StoreVectorElement(var, 1_u, 4_f);
        b.StoreVectorElement(var, 2_u, 8_f);
        b.StoreVectorElement(var, 3_u, 16_f);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  vec4 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol[0u] = 2.0f;
  v_1.tint_symbol[1u] = 4.0f;
  v_1.tint_symbol[2u] = 8.0f;
  v_1.tint_symbol[3u] = 16.0f;
}
)");
}

TEST_F(GlslWriterTest, AccessDirectVariable) {
    auto* var1 = b.Var<storage, vec4<f32>, core::Access::kRead>("v1");
    var1->SetBindingPoint(0, 0);
    b.ir.root_block->Append(var1);

    auto* var2 = b.Var<storage, vec4<f32>, core::Access::kRead>("v2");
    var2->SetBindingPoint(0, 1);
    b.ir.root_block->Append(var2);

    auto* p = b.FunctionParam("x", ty.ptr<storage, vec4<f32>, core::Access::kRead>());
    auto* bar = b.Function("bar", ty.void_());
    bar->SetParams({p});
    b.Append(bar->Block(), [&] {
        b.Let("a", b.LoadVectorElement(p, 1_u));
        b.Return(bar);
    });

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Call(bar, var1);
        b.Call(bar, var2);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  vec4 tint_symbol;
} v;
layout(binding = 1, std430)
buffer tint_symbol_3_1_ssbo {
  vec4 tint_symbol_2;
} v_1;
void bar() {
  float a = v.tint_symbol.y;
}
void bar_1() {
  float a = v_1.tint_symbol_2.y;
}
void main() {
  bar();
  bar_1();
}
)");
}

TEST_F(GlslWriterTest, AccessChainFromUnnamedAccessChain) {
    auto* Inner = ty.Struct(mod.symbols.New("Inner"), {
                                                          {mod.symbols.New("c"), ty.f32()},
                                                          {mod.symbols.New("d"), ty.u32()},
                                                      });
    auto* sb = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), Inner},
                                                });

    auto* var = b.Var("v", storage, sb, core::Access::kReadWrite);
    var->SetBindingPoint(0, 0);
    b.ir.root_block->Append(var);

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* x = b.Access(ty.ptr(storage, sb, core::Access::kReadWrite), var);
        auto* y = b.Access(ty.ptr(storage, Inner, core::Access::kReadWrite), x->Result(0), 1_u);
        b.Let("b", b.Load(b.Access(ty.ptr(storage, ty.u32(), core::Access::kReadWrite),
                                   y->Result(0), 1_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct Inner {
  float c;
  uint d;
};

struct SB {
  int a;
  Inner b;
};

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  SB tint_symbol;
} v_1;
void main() {
  uint b = v_1.tint_symbol.b.d;
}
)");
}

// TODO(dsinclair): Requires let pointer translation
TEST_F(GlslWriterTest, DISABLED_AccessChainFromLetAccessChain) {
    auto* Inner = ty.Struct(mod.symbols.New("Inner"), {
                                                          {mod.symbols.New("c"), ty.f32()},
                                                      });
    auto* sb = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), Inner},
                                                });

    auto* var = b.Var("v", storage, sb, core::Access::kReadWrite);
    var->SetBindingPoint(0, 0);
    b.ir.root_block->Append(var);

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* x = b.Let("x", var);
        auto* y = b.Let(
            "y", b.Access(ty.ptr(storage, Inner, core::Access::kReadWrite), x->Result(0), 1_u));
        auto* z = b.Let(
            "z", b.Access(ty.ptr(storage, ty.f32(), core::Access::kReadWrite), y->Result(0), 0_u));
        b.Let("a", b.Load(z));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

)");
}

// TODO(dsinclair): Support arrayLength
TEST_F(GlslWriterTest, DISABLED_AccessComplexDynamicAccessChain) {
    auto* S1 = ty.Struct(mod.symbols.New("S1"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.vec3<f32>()},
                                                    {mod.symbols.New("c"), ty.i32()},
                                                });
    auto* S2 = ty.Struct(mod.symbols.New("S2"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.array(S1, 3)},
                                                    {mod.symbols.New("c"), ty.i32()},
                                                });

    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.runtime_array(S2)},
                                                });

    auto* var = b.Var("sb", storage, SB, core::Access::kReadWrite);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* i = b.Load(b.Var("i", 4_i));
        auto* j = b.Load(b.Var("j", 1_u));
        auto* k = b.Load(b.Var("k", 2_i));
        // let x : f32 = sb.b[i].b[j].b[k];
        b.Let("x",
              b.LoadVectorElement(
                  b.Access(ty.ptr<storage, vec3<f32>, read_write>(), var, 1_u, i, 1_u, j, 1_u), k));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

)");
}

// TODO(dsinclair): Support arrayLength
TEST_F(GlslWriterTest, DISABLED_AccessComplexDynamicAccessChainSplit) {
    auto* S1 = ty.Struct(mod.symbols.New("S1"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.vec3<f32>()},
                                                    {mod.symbols.New("c"), ty.i32()},
                                                });
    auto* S2 = ty.Struct(mod.symbols.New("S2"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.array(S1, 3)},
                                                    {mod.symbols.New("c"), ty.i32()},
                                                });

    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.runtime_array(S2)},
                                                });

    auto* var = b.Var("sb", storage, SB, core::Access::kReadWrite);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* j = b.Load(b.Var("j", 1_u));
        b.Let("x", b.LoadVectorElement(b.Access(ty.ptr<storage, vec3<f32>, read_write>(), var, 1_u,
                                                4_u, 1_u, j, 1_u),
                                       2_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

)");
}

TEST_F(GlslWriterTest, AccessUniformChainFromUnnamedAccessChain) {
    auto* Inner = ty.Struct(mod.symbols.New("Inner"), {
                                                          {mod.symbols.New("c"), ty.f32()},
                                                          {mod.symbols.New("d"), ty.u32()},
                                                      });

    tint::Vector<const core::type::StructMember*, 2> members;
    members.Push(ty.Get<core::type::StructMember>(mod.symbols.New("a"), ty.i32(), 0u, 0u, 4u,
                                                  ty.i32()->Size(), core::IOAttributes{}));
    members.Push(ty.Get<core::type::StructMember>(mod.symbols.New("b"), Inner, 1u, 16u, 16u,
                                                  Inner->Size(), core::IOAttributes{}));
    auto* sb = ty.Struct(mod.symbols.New("SB"), members);

    auto* var = b.Var("v", uniform, sb, core::Access::kRead);
    var->SetBindingPoint(0, 0);
    b.ir.root_block->Append(var);

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* x = b.Access(ty.ptr(uniform, sb, core::Access::kRead), var);
        auto* y = b.Access(ty.ptr(uniform, Inner, core::Access::kRead), x->Result(0), 1_u);
        b.Let("b",
              b.Load(b.Access(ty.ptr(uniform, ty.u32(), core::Access::kRead), y->Result(0), 1_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct Inner {
  float c;
  uint d;
};

struct SB {
  int a;
  Inner b;
};

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  SB tint_symbol;
} v_1;
void main() {
  uint b = v_1.tint_symbol.b.d;
}
)");
}

// TODO(dsinclair): Handle let pointers
TEST_F(GlslWriterTest, DISABLED_AccessUniformChainFromLetAccessChain) {
    auto* Inner = ty.Struct(mod.symbols.New("Inner"), {
                                                          {mod.symbols.New("c"), ty.f32()},
                                                      });
    tint::Vector<const core::type::StructMember*, 2> members;
    members.Push(ty.Get<core::type::StructMember>(mod.symbols.New("a"), ty.i32(), 0u, 0u, 4u,
                                                  ty.i32()->Size(), core::IOAttributes{}));
    members.Push(ty.Get<core::type::StructMember>(mod.symbols.New("b"), Inner, 1u, 16u, 16u,
                                                  Inner->Size(), core::IOAttributes{}));
    auto* sb = ty.Struct(mod.symbols.New("SB"), members);

    auto* var = b.Var("v", uniform, sb, core::Access::kRead);
    var->SetBindingPoint(0, 0);
    b.ir.root_block->Append(var);

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* x = b.Let("x", var);
        auto* y =
            b.Let("y", b.Access(ty.ptr(uniform, Inner, core::Access::kRead), x->Result(0), 1_u));
        auto* z =
            b.Let("z", b.Access(ty.ptr(uniform, ty.f32(), core::Access::kRead), y->Result(0), 0_u));
        b.Let("a", b.Load(z));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

)");
}

TEST_F(GlslWriterTest, AccessUniformScalar) {
    auto* var = b.Var<uniform, f32, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  float tint_symbol;
} v_1;
void main() {
  float a = v_1.tint_symbol;
}
)");
}

TEST_F(GlslWriterTest, AccessUniformScalarF16) {
    auto* var = b.Var<uniform, f16, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  float16_t tint_symbol;
} v_1;
void main() {
  float16_t a = v_1.tint_symbol;
}
)");
}

TEST_F(GlslWriterTest, AccessUniformVector) {
    auto* var = b.Var<uniform, vec4<f32>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.LoadVectorElement(var, 0_u));
        b.Let("c", b.LoadVectorElement(var, 1_u));
        b.Let("d", b.LoadVectorElement(var, 2_u));
        b.Let("e", b.LoadVectorElement(var, 3_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  vec4 tint_symbol;
} v_1;
void main() {
  vec4 a = v_1.tint_symbol;
  float b = v_1.tint_symbol.x;
  float c = v_1.tint_symbol.y;
  float d = v_1.tint_symbol.z;
  float e = v_1.tint_symbol.w;
}
)");
}

TEST_F(GlslWriterTest, AccessUniformVectorF16) {
    auto* var = b.Var<uniform, vec4<f16>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* x = b.Var("x", 1_u);
        b.Let("a", b.Load(var));
        b.Let("b", b.LoadVectorElement(var, 0_u));
        b.Let("c", b.LoadVectorElement(var, b.Load(x)));
        b.Let("d", b.LoadVectorElement(var, 2_u));
        b.Let("e", b.LoadVectorElement(var, 3_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  f16vec4 tint_symbol;
} v_1;
void main() {
  uint x = 1u;
  f16vec4 a = v_1.tint_symbol;
  float16_t b = v_1.tint_symbol.x;
  float16_t c = v_1.tint_symbol[min(x, 3u)];
  float16_t d = v_1.tint_symbol.z;
  float16_t e = v_1.tint_symbol.w;
}
)");
}

TEST_F(GlslWriterTest, AccessUniformMatrix) {
    auto* var = b.Var<uniform, mat4x4<f32>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<uniform, vec4<f32>, core::Access::kRead>(), var, 3_u)));
        b.Let("c", b.LoadVectorElement(
                       b.Access(ty.ptr<uniform, vec4<f32>, core::Access::kRead>(), var, 1_u), 2_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  mat4 tint_symbol;
} v_1;
void main() {
  mat4 a = v_1.tint_symbol;
  vec4 b = v_1.tint_symbol[3u];
  float c = v_1.tint_symbol[1u].z;
}
)");
}

TEST_F(GlslWriterTest, AccessUniformMatrix2x3) {
    auto* var = b.Var<uniform, mat2x3<f32>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<uniform, vec3<f32>, core::Access::kRead>(), var, 1_u)));
        b.Let("c", b.LoadVectorElement(
                       b.Access(ty.ptr<uniform, vec3<f32>, core::Access::kRead>(), var, 1_u), 2_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_std140_1_ubo {
  vec3 tint_symbol_col0;
  vec3 tint_symbol_col1;
} v_1;
void main() {
  mat2x3 a = mat2x3(v_1.tint_symbol_col0, v_1.tint_symbol_col1);
  vec3 b = mat2x3(v_1.tint_symbol_col0, v_1.tint_symbol_col1)[1u];
  float c = mat2x3(v_1.tint_symbol_col0, v_1.tint_symbol_col1)[1u][2u];
}
)");
}

TEST_F(GlslWriterTest, AccessUniformMat2x3F16) {
    auto* var = b.Var<uniform, mat2x3<f16>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);
    b.ir.root_block->Append(var);

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr(uniform, ty.vec3<f16>()), var, 1_u)));
        b.Let("c", b.LoadVectorElement(b.Access(ty.ptr(uniform, ty.vec3<f16>()), var, 1_u), 2_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_std140_1_ubo {
  f16vec3 tint_symbol_col0;
  f16vec3 tint_symbol_col1;
} v_1;
void main() {
  f16mat2x3 a = f16mat2x3(v_1.tint_symbol_col0, v_1.tint_symbol_col1);
  f16vec3 b = f16mat2x3(v_1.tint_symbol_col0, v_1.tint_symbol_col1)[1u];
  float16_t c = f16mat2x3(v_1.tint_symbol_col0, v_1.tint_symbol_col1)[1u][2u];
}
)");
}

TEST_F(GlslWriterTest, AccessUniformMatrix3x2) {
    auto* var = b.Var<uniform, mat3x2<f32>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<uniform, vec2<f32>, core::Access::kRead>(), var, 1_u)));
        b.Let("c", b.LoadVectorElement(
                       b.Access(ty.ptr<uniform, vec2<f32>, core::Access::kRead>(), var, 1_u), 1_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_std140_1_ubo {
  vec2 tint_symbol_col0;
  vec2 tint_symbol_col1;
  vec2 tint_symbol_col2;
} v_1;
void main() {
  mat3x2 a = mat3x2(v_1.tint_symbol_col0, v_1.tint_symbol_col1, v_1.tint_symbol_col2);
  vec2 b = mat3x2(v_1.tint_symbol_col0, v_1.tint_symbol_col1, v_1.tint_symbol_col2)[1u];
  float c = mat3x2(v_1.tint_symbol_col0, v_1.tint_symbol_col1, v_1.tint_symbol_col2)[1u][1u];
}
)");
}

TEST_F(GlslWriterTest, AccessUniformMatrix2x2) {
    auto* var = b.Var<uniform, mat2x2<f32>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<uniform, vec2<f32>, core::Access::kRead>(), var, 1_u)));
        b.Let("c", b.LoadVectorElement(
                       b.Access(ty.ptr<uniform, vec2<f32>, core::Access::kRead>(), var, 1_u), 1_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_std140_1_ubo {
  vec2 tint_symbol_col0;
  vec2 tint_symbol_col1;
} v_1;
void main() {
  mat2 a = mat2(v_1.tint_symbol_col0, v_1.tint_symbol_col1);
  vec2 b = mat2(v_1.tint_symbol_col0, v_1.tint_symbol_col1)[1u];
  float c = mat2(v_1.tint_symbol_col0, v_1.tint_symbol_col1)[1u][1u];
}
)");
}

TEST_F(GlslWriterTest, AccessUniformMatrix2x2F16) {
    auto* var = b.Var<uniform, mat2x2<f16>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<uniform, vec2<f16>, core::Access::kRead>(), var, 1_u)));
        b.Let("c", b.LoadVectorElement(
                       b.Access(ty.ptr<uniform, vec2<f16>, core::Access::kRead>(), var, 1_u), 1_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_std140_1_ubo {
  f16vec2 tint_symbol_col0;
  f16vec2 tint_symbol_col1;
} v_1;
void main() {
  f16mat2 a = f16mat2(v_1.tint_symbol_col0, v_1.tint_symbol_col1);
  f16vec2 b = f16mat2(v_1.tint_symbol_col0, v_1.tint_symbol_col1)[1u];
  float16_t c = f16mat2(v_1.tint_symbol_col0, v_1.tint_symbol_col1)[1u][1u];
}
)");
}

TEST_F(GlslWriterTest, AccessUniformArray) {
    auto* var = b.Var<uniform, array<vec3<f32>, 5>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<uniform, vec3<f32>, core::Access::kRead>(), var, 3_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  vec3 tint_symbol[5];
} v_1;
void main() {
  vec3 a[5] = v_1.tint_symbol;
  vec3 b = v_1.tint_symbol[3u];
}
)");
}

TEST_F(GlslWriterTest, AccessUniformArrayF16) {
    auto* var = b.Var<uniform, array<vec3<f16>, 5>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<uniform, vec3<f16>, core::Access::kRead>(), var, 3_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  f16vec3 tint_symbol[5];
} v_1;
void main() {
  f16vec3 a[5] = v_1.tint_symbol;
  f16vec3 b = v_1.tint_symbol[3u];
}
)");
}

TEST_F(GlslWriterTest, AccessUniformArrayWhichCanHaveSizesOtherThenFive) {
    auto* var = b.Var<uniform, array<vec3<f32>, 42>, core::Access::kRead>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<uniform, vec3<f32>, core::Access::kRead>(), var, 3_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  vec3 tint_symbol[42];
} v_1;
void main() {
  vec3 a[42] = v_1.tint_symbol;
  vec3 b = v_1.tint_symbol[3u];
}
)");
}

TEST_F(GlslWriterTest, AccessUniformStruct) {
    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.f32()},
                                                });

    auto* var = b.Var("v", uniform, SB, core::Access::kRead);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<uniform, f32, core::Access::kRead>(), var, 1_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct SB {
  int a;
  float b;
};

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  SB tint_symbol;
} v_1;
void main() {
  SB a = v_1.tint_symbol;
  float b = v_1.tint_symbol.b;
}
)");
}

TEST_F(GlslWriterTest, AccessUniformStructF16) {
    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.f16()},
                                                });

    auto* var = b.Var("v", uniform, SB, core::Access::kRead);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.Load(b.Access(ty.ptr<uniform, f16, core::Access::kRead>(), var, 1_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;


struct SB {
  int a;
  float16_t b;
};

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  SB tint_symbol;
} v_1;
void main() {
  SB a = v_1.tint_symbol;
  float16_t b = v_1.tint_symbol.b;
}
)");
}

TEST_F(GlslWriterTest, AccessUniformStructNested) {
    auto* Inner =
        ty.Struct(mod.symbols.New("Inner"), {
                                                {mod.symbols.New("s"), ty.mat3x3<f32>()},
                                                {mod.symbols.New("t"), ty.array<vec3<f32>, 5>()},
                                            });
    auto* Outer = ty.Struct(mod.symbols.New("Outer"), {
                                                          {mod.symbols.New("x"), ty.f32()},
                                                          {mod.symbols.New("y"), Inner},
                                                      });

    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), Outer},
                                                });

    auto* var = b.Var("v", uniform, SB, core::Access::kRead);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Load(var));
        b.Let("b", b.LoadVectorElement(b.Access(ty.ptr<uniform, vec3<f32>, core::Access::kRead>(),
                                                var, 1_u, 1_u, 1_u, 3_u),
                                       2_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct Inner_std140 {
  vec3 s_col0;
  vec3 s_col1;
  vec3 s_col2;
  vec3 t[5];
};

struct Outer_std140 {
  float x;
  Inner_std140 y;
};

struct SB_std140 {
  int a;
  Outer_std140 b;
};

struct Inner {
  mat3 s;
  vec3 t[5];
};

struct Outer {
  float x;
  Inner y;
};

struct SB {
  int a;
  Outer b;
};

layout(binding = 0, std140)
uniform tint_symbol_1_std140_1_ubo {
  SB_std140 tint_symbol;
} v_1;
Inner tint_convert_Inner(Inner_std140 tint_input) {
  return Inner(mat3(tint_input.s_col0, tint_input.s_col1, tint_input.s_col2), tint_input.t);
}
Outer tint_convert_Outer(Outer_std140 tint_input) {
  return Outer(tint_input.x, tint_convert_Inner(tint_input.y));
}
SB tint_convert_SB(SB_std140 tint_input) {
  return SB(tint_input.a, tint_convert_Outer(tint_input.b));
}
void main() {
  SB a = tint_convert_SB(v_1.tint_symbol);
  float b = v_1.tint_symbol.b.y.t[3u].z;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreScalar) {
    auto* var = b.Var<storage, f32, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, f32, core::Access::kReadWrite>(), var), 2_f);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  float tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol = 2.0f;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreScalarF16) {
    auto* var = b.Var<storage, f16, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, f16, core::Access::kReadWrite>(), var), 2_h);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  float16_t tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol = 2.0hf;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreVectorElement) {
    auto* var = b.Var<storage, vec3<f32>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.StoreVectorElement(b.Access(ty.ptr<storage, vec3<f32>, core::Access::kReadWrite>(), var),
                             1_u, 2_f);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  vec3 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol[1u] = 2.0f;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreVectorElementF16) {
    auto* var = b.Var<storage, vec3<f16>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.StoreVectorElement(b.Access(ty.ptr<storage, vec3<f16>, core::Access::kReadWrite>(), var),
                             1_u, 2_h);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  f16vec3 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol[1u] = 2.0hf;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreVector) {
    auto* var = b.Var<storage, vec3<f32>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, vec3<f32>, core::Access::kReadWrite>(), var),
                b.Composite(ty.vec3<f32>(), 2_f, 3_f, 4_f));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  vec3 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol = vec3(2.0f, 3.0f, 4.0f);
}
)");
}

TEST_F(GlslWriterTest, AccessStoreVectorF16) {
    auto* var = b.Var<storage, vec3<f16>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, vec3<f16>, core::Access::kReadWrite>(), var),
                b.Composite(ty.vec3<f16>(), 2_h, 3_h, 4_h));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  f16vec3 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol = f16vec3(2.0hf, 3.0hf, 4.0hf);
}
)");
}

TEST_F(GlslWriterTest, AccessStoreMatrixElement) {
    auto* var = b.Var<storage, mat4x4<f32>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.StoreVectorElement(
            b.Access(ty.ptr<storage, vec4<f32>, core::Access::kReadWrite>(), var, 1_u), 2_u, 5_f);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  mat4 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol[1u][2u] = 5.0f;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreMatrixElementF16) {
    auto* var = b.Var<storage, mat3x2<f16>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.StoreVectorElement(
            b.Access(ty.ptr<storage, vec2<f16>, core::Access::kReadWrite>(), var, 2_u), 1_u, 5_h);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  f16mat3x2 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol[2u][1u] = 5.0hf;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreMatrixColumn) {
    auto* var = b.Var<storage, mat4x4<f32>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, vec4<f32>, core::Access::kReadWrite>(), var, 1_u),
                b.Splat<vec4<f32>>(5_f));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  mat4 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol[1u] = vec4(5.0f);
}
)");
}

TEST_F(GlslWriterTest, AccessStoreMatrixColumnF16) {
    auto* var = b.Var<storage, mat2x3<f16>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, vec3<f16>, core::Access::kReadWrite>(), var, 1_u),
                b.Splat<vec3<f16>>(5_h));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  f16mat2x3 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol[1u] = f16vec3(5.0hf);
}
)");
}

TEST_F(GlslWriterTest, AccessStoreMatrix) {
    auto* var = b.Var<storage, mat4x4<f32>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(var, b.Zero<mat4x4<f32>>());
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  mat4 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol = mat4(vec4(0.0f), vec4(0.0f), vec4(0.0f), vec4(0.0f));
}
)");
}

TEST_F(GlslWriterTest, AccessStoreMatrixF16) {
    auto* var = b.Var<storage, mat4x4<f16>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(var, b.Zero<mat4x4<f16>>());
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  f16mat4 tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol = f16mat4(f16vec4(0.0hf), f16vec4(0.0hf), f16vec4(0.0hf), f16vec4(0.0hf));
}
)");
}

TEST_F(GlslWriterTest, AccessStoreArrayElement) {
    auto* var = b.Var<storage, array<f32, 5>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, f32, core::Access::kReadWrite>(), var, 3_u), 1_f);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  float tint_symbol[5];
} v_1;
void main() {
  v_1.tint_symbol[3u] = 1.0f;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreArrayElementF16) {
    auto* var = b.Var<storage, array<f16, 5>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, f16, core::Access::kReadWrite>(), var, 3_u), 1_h);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  float16_t tint_symbol[5];
} v_1;
void main() {
  v_1.tint_symbol[3u] = 1.0hf;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreArray) {
    auto* var = b.Var<storage, array<vec3<f32>, 5>, core::Access::kReadWrite>("v");
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* ary = b.Let("ary", b.Zero<array<vec3<f32>, 5>>());
        b.Store(var, ary);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  vec3 tint_symbol[5];
} v_1;
void tint_store_and_preserve_padding(inout vec3 target[5], vec3 value_param[5]) {
  {
    uint v_2 = 0u;
    v_2 = 0u;
    while(true) {
      uint v_3 = v_2;
      if ((v_3 >= 5u)) {
        break;
      }
      target[v_3] = value_param[v_3];
      {
        v_2 = (v_3 + 1u);
      }
      continue;
    }
  }
}
void main() {
  vec3 ary[5] = vec3[5](vec3(0.0f), vec3(0.0f), vec3(0.0f), vec3(0.0f), vec3(0.0f));
  tint_store_and_preserve_padding(v_1.tint_symbol, ary);
}
)");
}

TEST_F(GlslWriterTest, AccessStoreStructMember) {
    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.f32()},
                                                });

    auto* var = b.Var("v", storage, SB, core::Access::kReadWrite);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, f32, core::Access::kReadWrite>(), var, 1_u), 3_f);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct SB {
  int a;
  float b;
};

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  SB tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol.b = 3.0f;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreStructMemberF16) {
    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.f16()},
                                                });

    auto* var = b.Var("v", storage, SB, core::Access::kReadWrite);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, f16, core::Access::kReadWrite>(), var, 1_u), 3_h);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;


struct SB {
  int a;
  float16_t b;
};

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  SB tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol.b = 3.0hf;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreStructNested) {
    auto* Inner =
        ty.Struct(mod.symbols.New("Inner"), {
                                                {mod.symbols.New("s"), ty.mat3x3<f32>()},
                                                {mod.symbols.New("t"), ty.array<vec3<f32>, 5>()},
                                            });
    auto* Outer = ty.Struct(mod.symbols.New("Outer"), {
                                                          {mod.symbols.New("x"), ty.f32()},
                                                          {mod.symbols.New("y"), Inner},
                                                      });

    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), Outer},
                                                });

    auto* var = b.Var("v", storage, SB, core::Access::kReadWrite);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Store(b.Access(ty.ptr<storage, f32, core::Access::kReadWrite>(), var, 1_u, 0_u), 2_f);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct Inner {
  mat3 s;
  vec3 t[5];
};

struct Outer {
  float x;
  Inner y;
};

struct SB {
  int a;
  Outer b;
};

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  SB tint_symbol;
} v_1;
void main() {
  v_1.tint_symbol.b.x = 2.0f;
}
)");
}

TEST_F(GlslWriterTest, AccessStoreStruct) {
    auto* Inner = ty.Struct(mod.symbols.New("Inner"), {
                                                          {mod.symbols.New("s"), ty.f32()},
                                                          {mod.symbols.New("t"), ty.vec3<f32>()},
                                                      });
    auto* Outer = ty.Struct(mod.symbols.New("Outer"), {
                                                          {mod.symbols.New("x"), ty.f32()},
                                                          {mod.symbols.New("y"), Inner},
                                                      });

    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), Outer},
                                                });

    auto* var = b.Var("v", storage, SB, core::Access::kReadWrite);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* s = b.Let("s", b.Zero(SB));
        b.Store(var, s);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct Inner {
  float s;
  vec3 t;
};

struct Outer {
  float x;
  Inner y;
};

struct SB {
  int a;
  Outer b;
};

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  SB tint_symbol;
} v_1;
void tint_store_and_preserve_padding_2(inout Inner target, Inner value_param) {
  target.s = value_param.s;
  target.t = value_param.t;
}
void tint_store_and_preserve_padding_1(inout Outer target, Outer value_param) {
  target.x = value_param.x;
  tint_store_and_preserve_padding_2(target.y, value_param.y);
}
void tint_store_and_preserve_padding(inout SB target, SB value_param) {
  target.a = value_param.a;
  tint_store_and_preserve_padding_1(target.b, value_param.b);
}
void main() {
  SB s = SB(0, Outer(0.0f, Inner(0.0f, vec3(0.0f))));
  tint_store_and_preserve_padding(v_1.tint_symbol, s);
}
)");
}

TEST_F(GlslWriterTest, AccessStoreStructComplex) {
    auto* Inner =
        ty.Struct(mod.symbols.New("Inner"), {
                                                {mod.symbols.New("s"), ty.mat3x3<f32>()},
                                                {mod.symbols.New("t"), ty.array<vec3<f32>, 5>()},
                                            });
    auto* Outer = ty.Struct(mod.symbols.New("Outer"), {
                                                          {mod.symbols.New("x"), ty.f32()},
                                                          {mod.symbols.New("y"), Inner},
                                                      });

    auto* SB = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), Outer},
                                                });

    auto* var = b.Var("v", storage, SB, core::Access::kReadWrite);
    var->SetBindingPoint(0, 0);

    b.ir.root_block->Append(var);
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* s = b.Let("s", b.Zero(SB));
        b.Store(var, s);
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct Inner {
  mat3 s;
  vec3 t[5];
};

struct Outer {
  float x;
  Inner y;
};

struct SB {
  int a;
  Outer b;
};

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  SB tint_symbol;
} v_1;
void tint_store_and_preserve_padding_4(inout vec3 target[5], vec3 value_param[5]) {
  {
    uint v_2 = 0u;
    v_2 = 0u;
    while(true) {
      uint v_3 = v_2;
      if ((v_3 >= 5u)) {
        break;
      }
      target[v_3] = value_param[v_3];
      {
        v_2 = (v_3 + 1u);
      }
      continue;
    }
  }
}
void tint_store_and_preserve_padding_3(inout mat3 target, mat3 value_param) {
  target[0u] = value_param[0u];
  target[1u] = value_param[1u];
  target[2u] = value_param[2u];
}
void tint_store_and_preserve_padding_2(inout Inner target, Inner value_param) {
  tint_store_and_preserve_padding_3(target.s, value_param.s);
  tint_store_and_preserve_padding_4(target.t, value_param.t);
}
void tint_store_and_preserve_padding_1(inout Outer target, Outer value_param) {
  target.x = value_param.x;
  tint_store_and_preserve_padding_2(target.y, value_param.y);
}
void tint_store_and_preserve_padding(inout SB target, SB value_param) {
  target.a = value_param.a;
  tint_store_and_preserve_padding_1(target.b, value_param.b);
}
void main() {
  SB s = SB(0, Outer(0.0f, Inner(mat3(vec3(0.0f), vec3(0.0f), vec3(0.0f)), vec3[5](vec3(0.0f), vec3(0.0f), vec3(0.0f), vec3(0.0f), vec3(0.0f)))));
  tint_store_and_preserve_padding(v_1.tint_symbol, s);
}
)");
}

TEST_F(GlslWriterTest, AccessChainReused) {
    auto* sb = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("a"), ty.i32()},
                                                    {mod.symbols.New("b"), ty.vec3<f32>()},
                                                });

    auto* var = b.Var("v", storage, sb, core::Access::kReadWrite);
    var->SetBindingPoint(0, 0);
    b.ir.root_block->Append(var);

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* x = b.Access(ty.ptr(storage, ty.vec3<f32>(), core::Access::kReadWrite), var, 1_u);
        b.Let("b", b.LoadVectorElement(x, 1_u));
        b.Let("c", b.LoadVectorElement(x, 2_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct SB {
  int a;
  vec3 b;
};

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  SB tint_symbol;
} v_1;
void main() {
  float b = v_1.tint_symbol.b.y;
  float c = v_1.tint_symbol.b.z;
}
)");
}

TEST_F(GlslWriterTest, AccessUniformChainReused) {
    auto* sb = ty.Struct(mod.symbols.New("SB"), {
                                                    {mod.symbols.New("c"), ty.f32()},
                                                    {mod.symbols.New("d"), ty.vec3<f32>()},
                                                });

    auto* var = b.Var("v", uniform, sb, core::Access::kRead);
    var->SetBindingPoint(0, 0);
    b.ir.root_block->Append(var);

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        auto* x = b.Access(ty.ptr(uniform, ty.vec3<f32>(), core::Access::kRead), var, 1_u);
        b.Let("b", b.LoadVectorElement(x, 1_u));
        b.Let("c", b.LoadVectorElement(x, 2_u));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(precision highp float;
precision highp int;


struct SB {
  float c;
  vec3 d;
};

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  SB tint_symbol;
} v_1;
void main() {
  float b = v_1.tint_symbol.d.y;
  float c = v_1.tint_symbol.d.z;
}
)");
}

}  // namespace
}  // namespace tint::glsl::writer
