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

#include "gmock/gmock.h"

#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/depth_multisampled_texture.h"
#include "src/tint/lang/core/type/depth_texture.h"
#include "src/tint/lang/core/type/matrix.h"
#include "src/tint/lang/core/type/multisampled_texture.h"
#include "src/tint/lang/core/type/sampled_texture.h"
#include "src/tint/lang/core/type/storage_texture.h"
#include "src/tint/lang/core/type/struct.h"
#include "src/tint/lang/glsl/writer/helper_test.h"
#include "src/tint/utils/text/string.h"

namespace tint::glsl::writer {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

TEST_F(GlslWriterTest, EmitType_Array) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.array<bool, 4>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  bool a[4] = bool[4](false, false, false, false);
}
)");
}

TEST_F(GlslWriterTest, EmitType_ArrayOfArray) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.array(ty.array<bool, 4>(), 5)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  bool a[5][4] = bool[5][4](bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false));
}
)");
}

TEST_F(GlslWriterTest, EmitType_ArrayOfArrayOfArray) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a",
              ty.ptr(core::AddressSpace::kPrivate, ty.array(ty.array(ty.array<bool, 4>(), 5), 6)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  bool a[6][5][4] = bool[6][5][4](bool[5][4](bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false)), bool[5][4](bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false)), bool[5][4](bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false)), bool[5][4](bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false)), bool[5][4](bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false)), bool[5][4](bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false), bool[4](false, false, false, false)));
}
)");
}

TEST_F(GlslWriterTest, EmitType_StructArrayVec) {
    auto* Inner =
        ty.Struct(mod.symbols.New("Inner"), {
                                                {mod.symbols.New("t"), ty.array<vec3<f32>, 5>()},
                                            });
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, Inner));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

struct Inner {
  vec3 t[5];
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  Inner a = Inner(vec3[5](vec3(0.0f), vec3(0.0f), vec3(0.0f), vec3(0.0f), vec3(0.0f)));
}
)");
}

TEST_F(GlslWriterTest, EmitType_Bool) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.bool_()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  bool a = false;
}
)");
}

TEST_F(GlslWriterTest, EmitType_F32) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.f32()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  float a = 0.0f;
}
)");
}

TEST_F(GlslWriterTest, EmitType_F16) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.f16()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  float16_t a = 0.0hf;
}
)");
}

TEST_F(GlslWriterTest, EmitType_I32) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.i32()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  int a = 0;
}
)");
}

TEST_F(GlslWriterTest, EmitType_Matrix_F32) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.mat2x3<f32>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  mat2x3 a = mat2x3(vec3(0.0f), vec3(0.0f));
}
)");
}

TEST_F(GlslWriterTest, EmitType_MatrixSquare_F32) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.mat2x2<f32>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  mat2 a = mat2(vec2(0.0f), vec2(0.0f));
}
)");
}

TEST_F(GlslWriterTest, EmitType_Matrix_F16) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.mat2x3<f16>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f16mat2x3 a = f16mat2x3(f16vec3(0.0hf), f16vec3(0.0hf));
}
)");
}

TEST_F(GlslWriterTest, EmitType_U32) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.u32()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  uint a = 0u;
}
)");
}

TEST_F(GlslWriterTest, EmitType_Atomic_U32) {
    b.Append(b.ir.root_block, [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kWorkgroup, ty.atomic<u32>()))->Result(0);
    });
    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
shared uint a;
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}

TEST_F(GlslWriterTest, EmitType_Atomic_I32) {
    b.Append(b.ir.root_block, [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kWorkgroup, ty.atomic<i32>()))->Result(0);
    });
    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
shared int a;
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}

TEST_F(GlslWriterTest, EmitType_Vector_F32) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.vec3<f32>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  vec3 a = vec3(0.0f);
}
)");
}

TEST_F(GlslWriterTest, EmitType_Vector_F16) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.vec3<f16>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(#extension GL_AMD_gpu_shader_half_float: require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f16vec3 a = f16vec3(0.0hf);
}
)");
}

TEST_F(GlslWriterTest, EmitType_Vector_I32) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.vec2<i32>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  ivec2 a = ivec2(0);
}
)");
}

TEST_F(GlslWriterTest, EmitType_Vector_U32) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.vec4<u32>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  uvec4 a = uvec4(0u);
}
)");
}

TEST_F(GlslWriterTest, EmitType_Vector_bool) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.vec3<bool>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  bvec3 a = bvec3(false);
}
)");
}

TEST_F(GlslWriterTest, EmitType_Void) {
    // Tested via the function return type.
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] { b.Return(func); });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}

TEST_F(GlslWriterTest, EmitType_Struct) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.i32()},
                                                  {mod.symbols.Register("b"), ty.f32()},
                                              });
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

struct S {
  int a;
  float b;
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  S a = S(0, 0.0f);
}
)");
}

TEST_F(GlslWriterTest, EmitType_Struct_Dedup) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.i32()},
                                                  {mod.symbols.Register("b"), ty.f32()},
                                              });
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    b.Append(func->Block(), [&] {
        func->SetWorkgroupSize(1, 1, 1);
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Var("b", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

struct S {
  int a;
  float b;
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  S a = S(0, 0.0f);
  S b = S(0, 0.0f);
}
)");
}

TEST_F(GlslWriterTest, EmitType_Struct_Nested) {
    auto* inner =
        ty.Struct(mod.symbols.New("Inner"), {
                                                {mod.symbols.Register("x"), ty.u32()},
                                                {mod.symbols.Register("y"), ty.vec4<f32>()},
                                            });

    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.i32()},
                                                  {mod.symbols.Register("b"), inner},
                                              });
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(

struct Inner {
  uint x;
  vec4 y;
};

struct S {
  int a;
  Inner b;
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  S a = S(0, Inner(0u, vec4(0.0f)));
}
)");
}

// TODO(dsinclair): Add sampler support
TEST_F(GlslWriterTest, DISABLED_EmitType_Sampler) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    auto* param = b.FunctionParam("a", ty.sampler());
    func->SetParams({param});
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] { b.Return(func); });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}

// TODO(dsinclair): Add sampler comparison support
TEST_F(GlslWriterTest, DISABLED_EmitType_SamplerComparison) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    auto* param = b.FunctionParam("a", ty.comparison_sampler());
    func->SetParams({param});
    func->SetWorkgroupSize(1, 1, 1);
    b.Append(func->Block(), [&] { b.Return(func); });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}

struct GlslDepthTextureData {
    core::type::TextureDimension dim;
    std::string result;
};
inline std::ostream& operator<<(std::ostream& out, GlslDepthTextureData data) {
    StringStream str;
    str << data.dim;
    out << str.str();
    return out;
}
using GlslWriterDepthTextureESTest = GlslWriterTestWithParam<GlslDepthTextureData>;
TEST_P(GlslWriterDepthTextureESTest, Emit) {
    auto params = GetParam();

    auto* t = ty.Get<core::type::DepthTexture>(params.dim);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", t);
    func->SetParams({param});
    b.Append(func->Block(), [&] { b.Return(func); });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
void foo(highp )" + params.result +
                                R"( a) {
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    GlslWriterTest,
    GlslWriterDepthTextureESTest,
    testing::Values(
        GlslDepthTextureData{core::type::TextureDimension::k2d, "sampler2DShadow"},
        GlslDepthTextureData{core::type::TextureDimension::k2dArray, "sampler2DArrayShadow"},
        GlslDepthTextureData{core::type::TextureDimension::kCube, "samplerCubeShadow"}));

using GlslWriterDepthTextureNonESTest = GlslWriterTestWithParam<GlslDepthTextureData>;
TEST_P(GlslWriterDepthTextureNonESTest, Emit) {
    auto params = GetParam();

    auto* t = ty.Get<core::type::DepthTexture>(params.dim);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", t);
    func->SetParams({param});
    b.Append(func->Block(), [&] { b.Return(func); });

    Options opts{};
    opts.version = Version(Version::Standard::kDesktop, 4, 6);
    ASSERT_TRUE(Generate(opts)) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, R"(#version 460

void foo(highp )" + params.result +
                                R"( a) {
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    GlslWriterTest,
    GlslWriterDepthTextureNonESTest,
    testing::Values(
        GlslDepthTextureData{core::type::TextureDimension::k2d, "sampler2DShadow"},
        GlslDepthTextureData{core::type::TextureDimension::k2dArray, "sampler2DArrayShadow"},
        GlslDepthTextureData{core::type::TextureDimension::kCube, "samplerCubeShadow"},
        GlslDepthTextureData{core::type::TextureDimension::kCubeArray, "samplerCubeArrayShadow"}));

TEST_F(GlslWriterTest, EmitType_DepthMultisampledTexture) {
    auto* t = ty.Get<core::type::DepthMultisampledTexture>(core::type::TextureDimension::k2d);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", t);
    func->SetParams({param});
    b.Append(func->Block(), [&] { b.Return(func); });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
void foo(highp sampler2DMS a) {
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}

enum class TextureDataType : uint8_t {
    kF32,
    kU32,
    kI32,
};

struct GlslTextureData {
    core::type::TextureDimension dim;
    TextureDataType datatype;
    std::string result;
};
inline std::ostream& operator<<(std::ostream& out, GlslTextureData data) {
    StringStream str;
    str << data.dim;
    out << str.str();
    return out;
}
using GlslWriterSampledTextureESTest = GlslWriterTestWithParam<GlslTextureData>;
TEST_P(GlslWriterSampledTextureESTest, Emit) {
    auto params = GetParam();

    const core::type::Type* subtype = nullptr;
    switch (params.datatype) {
        case TextureDataType::kF32:
            subtype = ty.f32();
            break;
        case TextureDataType::kI32:
            subtype = ty.i32();
            break;
        case TextureDataType::kU32:
            subtype = ty.u32();
            break;
    }

    auto* t = ty.Get<core::type::SampledTexture>(params.dim, subtype);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", t);
    func->SetParams({param});
    b.Append(func->Block(), [&] { b.Return(func); });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
void foo(highp )" + params.result +
                                R"( a) {
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    GlslWriterTest,
    GlslWriterSampledTextureESTest,
    testing::Values(
        GlslTextureData{core::type::TextureDimension::k2d, TextureDataType::kF32, "sampler2D"},
        GlslTextureData{core::type::TextureDimension::k2dArray, TextureDataType::kF32,
                        "sampler2DArray"},
        GlslTextureData{core::type::TextureDimension::k3d, TextureDataType::kF32, "sampler3D"},
        GlslTextureData{core::type::TextureDimension::kCube, TextureDataType::kF32, "samplerCube"},

        GlslTextureData{core::type::TextureDimension::k2d, TextureDataType::kI32, "isampler2D"},
        GlslTextureData{core::type::TextureDimension::k2dArray, TextureDataType::kI32,
                        "isampler2DArray"},
        GlslTextureData{core::type::TextureDimension::k3d, TextureDataType::kI32, "isampler3D"},
        GlslTextureData{core::type::TextureDimension::kCube, TextureDataType::kI32, "isamplerCube"},

        GlslTextureData{core::type::TextureDimension::k2d, TextureDataType::kU32, "usampler2D"},
        GlslTextureData{core::type::TextureDimension::k2dArray, TextureDataType::kU32,
                        "usampler2DArray"},
        GlslTextureData{core::type::TextureDimension::k3d, TextureDataType::kU32, "usampler3D"},
        GlslTextureData{core::type::TextureDimension::kCube, TextureDataType::kU32,
                        "usamplerCube"}));

using GlslWriterSampledTextureNonESTest = GlslWriterTestWithParam<GlslTextureData>;
TEST_P(GlslWriterSampledTextureNonESTest, Emit) {
    auto params = GetParam();

    const core::type::Type* subtype = nullptr;
    switch (params.datatype) {
        case TextureDataType::kF32:
            subtype = ty.f32();
            break;
        case TextureDataType::kI32:
            subtype = ty.i32();
            break;
        case TextureDataType::kU32:
            subtype = ty.u32();
            break;
    }

    auto* t = ty.Get<core::type::SampledTexture>(params.dim, subtype);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", t);
    func->SetParams({param});
    b.Append(func->Block(), [&] { b.Return(func); });

    Options opts{};
    opts.version = Version(Version::Standard::kDesktop, 4, 6);
    ASSERT_TRUE(Generate(opts)) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, R"(#version 460

void foo(highp )" + params.result +
                                R"( a) {
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    GlslWriterTest,
    GlslWriterSampledTextureNonESTest,
    testing::Values(
        GlslTextureData{core::type::TextureDimension::k1d, TextureDataType::kF32, "sampler1D"},
        GlslTextureData{core::type::TextureDimension::k2d, TextureDataType::kF32, "sampler2D"},
        GlslTextureData{core::type::TextureDimension::k2dArray, TextureDataType::kF32,
                        "sampler2DArray"},
        GlslTextureData{core::type::TextureDimension::k3d, TextureDataType::kF32, "sampler3D"},
        GlslTextureData{core::type::TextureDimension::kCube, TextureDataType::kF32, "samplerCube"},
        GlslTextureData{core::type::TextureDimension::kCubeArray, TextureDataType::kF32,
                        "samplerCubeArray"},

        GlslTextureData{core::type::TextureDimension::k1d, TextureDataType::kI32, "isampler1D"},
        GlslTextureData{core::type::TextureDimension::k2d, TextureDataType::kI32, "isampler2D"},
        GlslTextureData{core::type::TextureDimension::k2dArray, TextureDataType::kI32,
                        "isampler2DArray"},
        GlslTextureData{core::type::TextureDimension::k3d, TextureDataType::kI32, "isampler3D"},
        GlslTextureData{core::type::TextureDimension::kCube, TextureDataType::kI32, "isamplerCube"},
        GlslTextureData{core::type::TextureDimension::kCubeArray, TextureDataType::kI32,
                        "isamplerCubeArray"},

        GlslTextureData{core::type::TextureDimension::k1d, TextureDataType::kU32, "usampler1D"},
        GlslTextureData{core::type::TextureDimension::k2d, TextureDataType::kU32, "usampler2D"},
        GlslTextureData{core::type::TextureDimension::k2dArray, TextureDataType::kU32,
                        "usampler2DArray"},
        GlslTextureData{core::type::TextureDimension::k3d, TextureDataType::kU32, "usampler3D"},
        GlslTextureData{core::type::TextureDimension::kCube, TextureDataType::kU32, "usamplerCube"},
        GlslTextureData{core::type::TextureDimension::kCubeArray, TextureDataType::kU32,
                        "usamplerCubeArray"}));

using GlslWriterMultisampledTextureESTest = GlslWriterTestWithParam<GlslTextureData>;
TEST_P(GlslWriterMultisampledTextureESTest, Emit) {
    auto params = GetParam();

    const core::type::Type* subtype = nullptr;
    switch (params.datatype) {
        case TextureDataType::kF32:
            subtype = ty.f32();
            break;
        case TextureDataType::kI32:
            subtype = ty.i32();
            break;
        case TextureDataType::kU32:
            subtype = ty.u32();
            break;
    }

    auto* ms = ty.Get<core::type::MultisampledTexture>(params.dim, subtype);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", ms);
    func->SetParams({param});
    b.Append(func->Block(), [&] { b.Return(func); });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
void foo(highp )" + params.result +
                                R"( a) {
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}
INSTANTIATE_TEST_SUITE_P(GlslWriterTest,
                         GlslWriterMultisampledTextureESTest,
                         testing::Values(GlslTextureData{core::type::TextureDimension::k2d,
                                                         TextureDataType::kF32, "sampler2DMS"},
                                         GlslTextureData{core::type::TextureDimension::k2d,
                                                         TextureDataType::kI32, "isampler2DMS"},
                                         GlslTextureData{core::type::TextureDimension::k2d,
                                                         TextureDataType::kU32, "usampler2DMS"}));

using GlslWriterMultisampledTextureNonESTest = GlslWriterTestWithParam<GlslTextureData>;
TEST_P(GlslWriterMultisampledTextureNonESTest, Emit) {
    auto params = GetParam();

    const core::type::Type* subtype = nullptr;
    switch (params.datatype) {
        case TextureDataType::kF32:
            subtype = ty.f32();
            break;
        case TextureDataType::kI32:
            subtype = ty.i32();
            break;
        case TextureDataType::kU32:
            subtype = ty.u32();
            break;
    }
    auto* ms = ty.Get<core::type::MultisampledTexture>(params.dim, subtype);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", ms);
    func->SetParams({param});
    b.Append(func->Block(), [&] { b.Return(func); });

    Options opts{};
    opts.version = Version(Version::Standard::kDesktop, 4, 6);
    ASSERT_TRUE(Generate(opts)) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, R"(#version 460

void foo(highp )" + params.result +
                                R"( a) {
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    GlslWriterTest,
    GlslWriterMultisampledTextureNonESTest,
    testing::Values(
        GlslTextureData{core::type::TextureDimension::k2d, TextureDataType::kF32, "sampler2DMS"},
        GlslTextureData{core::type::TextureDimension::k2dArray, TextureDataType::kF32,
                        "sampler2DMSArray"},

        GlslTextureData{core::type::TextureDimension::k2d, TextureDataType::kI32, "isampler2DMS"},
        GlslTextureData{core::type::TextureDimension::k2dArray, TextureDataType::kI32,
                        "isampler2DMSArray"},

        GlslTextureData{core::type::TextureDimension::k2d, TextureDataType::kU32, "usampler2DMS"},
        GlslTextureData{core::type::TextureDimension::k2dArray, TextureDataType::kU32,
                        "usampler2DMSArray"}));

struct GlslStorageTextureData {
    core::type::TextureDimension dim;
    core::Access access;
    TextureDataType datatype;
    std::string result;
};
inline std::ostream& operator<<(std::ostream& out, GlslStorageTextureData data) {
    StringStream str;
    str << data.dim << " " << data.access << " " << static_cast<uint8_t>(data.datatype);
    return out << str.str();
}
using GlslWriterStorageTextureESTest = GlslWriterTestWithParam<GlslStorageTextureData>;
TEST_P(GlslWriterStorageTextureESTest, Emit) {
    auto params = GetParam();

    const core::type::Type* subtype = nullptr;
    switch (params.datatype) {
        case TextureDataType::kF32:
            subtype = ty.f32();
            break;
        case TextureDataType::kI32:
            subtype = ty.i32();
            break;
        case TextureDataType::kU32:
            subtype = ty.u32();
            break;
    }
    auto s = ty.Get<core::type::StorageTexture>(params.dim, core::TexelFormat::kR32Float,
                                                params.access, subtype);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", s);
    func->SetParams({param});
    b.Append(func->Block(), [&] { b.Return(func); });

    ASSERT_TRUE(Generate()) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, GlslHeader() + R"(
void foo(highp )" + params.result +
                                R"( a) {
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    GlslWriterTest,
    GlslWriterStorageTextureESTest,
    testing::Values(
        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kRead,
                               TextureDataType::kF32, "readonly image2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kRead,
                               TextureDataType::kF32, "readonly image2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kRead,
                               TextureDataType::kF32, "readonly image3D"},

        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kWrite,
                               TextureDataType::kF32, "writeonly image2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kWrite,
                               TextureDataType::kF32, "writeonly image2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kWrite,
                               TextureDataType::kF32, "writeonly image3D"},

        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kReadWrite,
                               TextureDataType::kF32, "image2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kReadWrite,
                               TextureDataType::kF32, "image2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kReadWrite,
                               TextureDataType::kF32, "image3D"},

        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kRead,
                               TextureDataType::kI32, "readonly iimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kRead,
                               TextureDataType::kI32, "readonly iimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kRead,
                               TextureDataType::kI32, "readonly iimage3D"},

        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kWrite,
                               TextureDataType::kI32, "writeonly iimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kWrite,
                               TextureDataType::kI32, "writeonly iimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kWrite,
                               TextureDataType::kI32, "writeonly iimage3D"},

        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kReadWrite,
                               TextureDataType::kI32, "iimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kReadWrite,
                               TextureDataType::kI32, "iimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kReadWrite,
                               TextureDataType::kI32, "iimage3D"},

        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kRead,
                               TextureDataType::kU32, "readonly uimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kRead,
                               TextureDataType::kU32, "readonly uimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kRead,
                               TextureDataType::kU32, "readonly uimage3D"},

        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kWrite,
                               TextureDataType::kU32, "writeonly uimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kWrite,
                               TextureDataType::kU32, "writeonly uimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kWrite,
                               TextureDataType::kU32, "writeonly uimage3D"},

        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kReadWrite,
                               TextureDataType::kU32, "uimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kReadWrite,
                               TextureDataType::kU32, "uimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kReadWrite,
                               TextureDataType::kU32, "uimage3D"}));

using GlslWriterStorageTextureNonESTest = GlslWriterTestWithParam<GlslStorageTextureData>;
TEST_P(GlslWriterStorageTextureNonESTest, Emit) {
    auto params = GetParam();

    const core::type::Type* subtype = nullptr;
    switch (params.datatype) {
        case TextureDataType::kF32:
            subtype = ty.f32();
            break;
        case TextureDataType::kI32:
            subtype = ty.i32();
            break;
        case TextureDataType::kU32:
            subtype = ty.u32();
            break;
    }
    auto s = ty.Get<core::type::StorageTexture>(params.dim, core::TexelFormat::kR32Float,
                                                params.access, subtype);
    auto* func = b.Function("foo", ty.void_());
    auto* param = b.FunctionParam("a", s);
    func->SetParams({param});
    b.Append(func->Block(), [&] { b.Return(func); });

    Options opts{};
    opts.version = Version(Version::Standard::kDesktop, 4, 6);
    ASSERT_TRUE(Generate(opts)) << err_ << output_.glsl;
    EXPECT_EQ(output_.glsl, R"(#version 460

void foo(highp )" + params.result +
                                R"( a) {
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    GlslWriterTest,
    GlslWriterStorageTextureNonESTest,
    testing::Values(
        GlslStorageTextureData{core::type::TextureDimension::k1d, core::Access::kRead,
                               TextureDataType::kF32, "readonly image1D"},
        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kRead,
                               TextureDataType::kF32, "readonly image2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kRead,
                               TextureDataType::kF32, "readonly image2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kRead,
                               TextureDataType::kF32, "readonly image3D"},
        GlslStorageTextureData{core::type::TextureDimension::kCube, core::Access::kRead,
                               TextureDataType::kF32, "readonly imageCube"},
        GlslStorageTextureData{core::type::TextureDimension::kCubeArray, core::Access::kRead,
                               TextureDataType::kF32, "readonly imageCubeArray"},

        GlslStorageTextureData{core::type::TextureDimension::k1d, core::Access::kWrite,
                               TextureDataType::kF32, "writeonly image1D"},
        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kWrite,
                               TextureDataType::kF32, "writeonly image2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kWrite,
                               TextureDataType::kF32, "writeonly image2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kWrite,
                               TextureDataType::kF32, "writeonly image3D"},
        GlslStorageTextureData{core::type::TextureDimension::kCube, core::Access::kWrite,
                               TextureDataType::kF32, "writeonly imageCube"},
        GlslStorageTextureData{core::type::TextureDimension::kCubeArray, core::Access::kWrite,
                               TextureDataType::kF32, "writeonly imageCubeArray"},

        GlslStorageTextureData{core::type::TextureDimension::k1d, core::Access::kReadWrite,
                               TextureDataType::kF32, "image1D"},
        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kReadWrite,
                               TextureDataType::kF32, "image2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kReadWrite,
                               TextureDataType::kF32, "image2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kReadWrite,
                               TextureDataType::kF32, "image3D"},
        GlslStorageTextureData{core::type::TextureDimension::kCube, core::Access::kReadWrite,
                               TextureDataType::kF32, "imageCube"},
        GlslStorageTextureData{core::type::TextureDimension::kCubeArray, core::Access::kReadWrite,
                               TextureDataType::kF32, "imageCubeArray"},

        GlslStorageTextureData{core::type::TextureDimension::k1d, core::Access::kRead,
                               TextureDataType::kI32, "readonly iimage1D"},
        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kRead,
                               TextureDataType::kI32, "readonly iimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kRead,
                               TextureDataType::kI32, "readonly iimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kRead,
                               TextureDataType::kI32, "readonly iimage3D"},
        GlslStorageTextureData{core::type::TextureDimension::kCube, core::Access::kRead,
                               TextureDataType::kI32, "readonly iimageCube"},
        GlslStorageTextureData{core::type::TextureDimension::kCubeArray, core::Access::kRead,
                               TextureDataType::kI32, "readonly iimageCubeArray"},

        GlslStorageTextureData{core::type::TextureDimension::k1d, core::Access::kWrite,
                               TextureDataType::kI32, "writeonly iimage1D"},
        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kWrite,
                               TextureDataType::kI32, "writeonly iimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kWrite,
                               TextureDataType::kI32, "writeonly iimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kWrite,
                               TextureDataType::kI32, "writeonly iimage3D"},
        GlslStorageTextureData{core::type::TextureDimension::kCube, core::Access::kWrite,
                               TextureDataType::kI32, "writeonly iimageCube"},
        GlslStorageTextureData{core::type::TextureDimension::kCubeArray, core::Access::kWrite,
                               TextureDataType::kI32, "writeonly iimageCubeArray"},

        GlslStorageTextureData{core::type::TextureDimension::k1d, core::Access::kReadWrite,
                               TextureDataType::kI32, "iimage1D"},
        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kReadWrite,
                               TextureDataType::kI32, "iimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kReadWrite,
                               TextureDataType::kI32, "iimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kReadWrite,
                               TextureDataType::kI32, "iimage3D"},
        GlslStorageTextureData{core::type::TextureDimension::kCube, core::Access::kReadWrite,
                               TextureDataType::kI32, "iimageCube"},
        GlslStorageTextureData{core::type::TextureDimension::kCubeArray, core::Access::kReadWrite,
                               TextureDataType::kI32, "iimageCubeArray"},

        GlslStorageTextureData{core::type::TextureDimension::k1d, core::Access::kRead,
                               TextureDataType::kU32, "readonly uimage1D"},
        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kRead,
                               TextureDataType::kU32, "readonly uimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kRead,
                               TextureDataType::kU32, "readonly uimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kRead,
                               TextureDataType::kU32, "readonly uimage3D"},
        GlslStorageTextureData{core::type::TextureDimension::kCube, core::Access::kRead,
                               TextureDataType::kU32, "readonly uimageCube"},
        GlslStorageTextureData{core::type::TextureDimension::kCubeArray, core::Access::kRead,
                               TextureDataType::kU32, "readonly uimageCubeArray"},

        GlslStorageTextureData{core::type::TextureDimension::k1d, core::Access::kWrite,
                               TextureDataType::kU32, "writeonly uimage1D"},
        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kWrite,
                               TextureDataType::kU32, "writeonly uimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kWrite,
                               TextureDataType::kU32, "writeonly uimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kWrite,
                               TextureDataType::kU32, "writeonly uimage3D"},
        GlslStorageTextureData{core::type::TextureDimension::kCube, core::Access::kWrite,
                               TextureDataType::kU32, "writeonly uimageCube"},
        GlslStorageTextureData{core::type::TextureDimension::kCubeArray, core::Access::kWrite,
                               TextureDataType::kU32, "writeonly uimageCubeArray"},

        GlslStorageTextureData{core::type::TextureDimension::k1d, core::Access::kReadWrite,
                               TextureDataType::kU32, "uimage1D"},
        GlslStorageTextureData{core::type::TextureDimension::k2d, core::Access::kReadWrite,
                               TextureDataType::kU32, "uimage2D"},
        GlslStorageTextureData{core::type::TextureDimension::k2dArray, core::Access::kReadWrite,
                               TextureDataType::kU32, "uimage2DArray"},
        GlslStorageTextureData{core::type::TextureDimension::k3d, core::Access::kReadWrite,
                               TextureDataType::kU32, "uimage3D"},
        GlslStorageTextureData{core::type::TextureDimension::kCube, core::Access::kReadWrite,
                               TextureDataType::kU32, "uimageCube"},
        GlslStorageTextureData{core::type::TextureDimension::kCubeArray, core::Access::kReadWrite,
                               TextureDataType::kU32, "uimageCubeArray"}));

}  // namespace
}  // namespace tint::glsl::writer
