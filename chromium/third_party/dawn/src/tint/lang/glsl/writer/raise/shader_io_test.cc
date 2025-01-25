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

#include <utility>

#include "src/tint/lang/core/ir/transform/helper_test.h"
#include "src/tint/lang/core/type/struct.h"
#include "src/tint/lang/glsl/writer/raise/shader_io.h"

namespace tint::glsl::writer::raise {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

using GlslWriter_ShaderIOTest = core::ir::transform::TransformTest;

TEST_F(GlslWriter_ShaderIOTest, NoInputsOrOutputs) {
    auto* ep = b.Function("foo", ty.void_());
    ep->SetStage(core::ir::Function::PipelineStage::kCompute);
    ep->SetWorkgroupSize(1, 1, 1);

    b.Append(ep->Block(), [&] {  //
        b.Return(ep);
    });

    auto* src = R"(
%foo = @compute @workgroup_size(1, 1, 1) func():void {
  $B1: {
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, Parameters_NonStruct) {
    auto* ep = b.Function("foo", ty.void_());
    auto* front_facing = b.FunctionParam("front_facing", ty.bool_());
    front_facing->SetBuiltin(core::BuiltinValue::kFrontFacing);
    auto* position = b.FunctionParam("position", ty.vec4<f32>());
    position->SetBuiltin(core::BuiltinValue::kPosition);
    position->SetInvariant(true);
    auto* color1 = b.FunctionParam("color1", ty.f32());
    color1->SetLocation(0);
    auto* color2 = b.FunctionParam("color2", ty.f32());
    color2->SetLocation(1);
    color2->SetInterpolation(core::Interpolation{core::InterpolationType::kLinear,
                                                 core::InterpolationSampling::kSample});

    ep->SetParams({front_facing, position, color1, color2});
    ep->SetStage(core::ir::Function::PipelineStage::kFragment);

    b.Append(ep->Block(), [&] {
        auto* ifelse = b.If(front_facing);
        b.Append(ifelse->True(), [&] {
            b.Multiply(ty.vec4<f32>(), position, b.Add(ty.f32(), color1, color2));
            b.ExitIf(ifelse);
        });
        b.Return(ep);
    });

    auto* src = R"(
%foo = @fragment func(%front_facing:bool [@front_facing], %position:vec4<f32> [@invariant, @position], %color1:f32 [@location(0)], %color2:f32 [@location(1), @interpolate(linear, sample)]):void {
  $B1: {
    if %front_facing [t: $B2] {  # if_1
      $B2: {  # true
        %6:f32 = add %color1, %color2
        %7:vec4<f32> = mul %position, %6
        exit_if  # if_1
      }
    }
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
$B1: {  # root
  %gl_FrontFacing:ptr<__in, bool, read> = var @builtin(front_facing)
  %gl_FragCoord:ptr<__in, vec4<f32>, read> = var @invariant @builtin(position)
  %foo_loc0_Input:ptr<__in, f32, read> = var @location(0)
  %foo_loc1_Input:ptr<__in, f32, read> = var @location(1) @interpolate(linear, sample)
}

%foo_inner = func(%front_facing:bool, %position:vec4<f32>, %color1:f32, %color2:f32):void {
  $B2: {
    if %front_facing [t: $B3] {  # if_1
      $B3: {  # true
        %10:f32 = add %color1, %color2
        %11:vec4<f32> = mul %position, %10
        exit_if  # if_1
      }
    }
    ret
  }
}
%foo = @fragment func():void {
  $B4: {
    %13:bool = load %gl_FrontFacing
    %14:vec4<f32> = load %gl_FragCoord
    %15:f32 = load %foo_loc0_Input
    %16:f32 = load %foo_loc1_Input
    %17:void = call %foo_inner, %13, %14, %15, %16
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, Parameters_Struct) {
    auto* str_ty = ty.Struct(mod.symbols.New("Inputs"),
                             {
                                 {
                                     mod.symbols.New("front_facing"),
                                     ty.bool_(),
                                     core::IOAttributes{
                                         /* location */ std::nullopt,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ core::BuiltinValue::kFrontFacing,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                                 {
                                     mod.symbols.New("position"),
                                     ty.vec4<f32>(),
                                     core::IOAttributes{
                                         /* location */ std::nullopt,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ core::BuiltinValue::kPosition,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ true,
                                     },
                                 },
                                 {
                                     mod.symbols.New("color1"),
                                     ty.f32(),
                                     core::IOAttributes{
                                         /* location */ 0u,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ std::nullopt,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                                 {
                                     mod.symbols.New("color2"),
                                     ty.f32(),
                                     core::IOAttributes{
                                         /* location */ 1u,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ std::nullopt,
                                         /* interpolation */
                                         core::Interpolation{
                                             core::InterpolationType::kLinear,
                                             core::InterpolationSampling::kSample,
                                         },
                                         /* invariant */ false,
                                     },
                                 },
                             });

    auto* ep = b.Function("foo", ty.void_());
    auto* str_param = b.FunctionParam("inputs", str_ty);
    ep->SetParams({str_param});
    ep->SetStage(core::ir::Function::PipelineStage::kFragment);

    b.Append(ep->Block(), [&] {
        auto* ifelse = b.If(b.Access(ty.bool_(), str_param, 0_i));
        b.Append(ifelse->True(), [&] {
            auto* position = b.Access(ty.vec4<f32>(), str_param, 1_i);
            auto* color1 = b.Access(ty.f32(), str_param, 2_i);
            auto* color2 = b.Access(ty.f32(), str_param, 3_i);
            b.Multiply(ty.vec4<f32>(), position, b.Add(ty.f32(), color1, color2));
            b.ExitIf(ifelse);
        });
        b.Return(ep);
    });

    auto* src = R"(
Inputs = struct @align(16) {
  front_facing:bool @offset(0), @builtin(front_facing)
  position:vec4<f32> @offset(16), @invariant, @builtin(position)
  color1:f32 @offset(32), @location(0)
  color2:f32 @offset(36), @location(1), @interpolate(linear, sample)
}

%foo = @fragment func(%inputs:Inputs):void {
  $B1: {
    %3:bool = access %inputs, 0i
    if %3 [t: $B2] {  # if_1
      $B2: {  # true
        %4:vec4<f32> = access %inputs, 1i
        %5:f32 = access %inputs, 2i
        %6:f32 = access %inputs, 3i
        %7:f32 = add %5, %6
        %8:vec4<f32> = mul %4, %7
        exit_if  # if_1
      }
    }
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Inputs = struct @align(16) {
  front_facing:bool @offset(0)
  position:vec4<f32> @offset(16)
  color1:f32 @offset(32)
  color2:f32 @offset(36)
}

$B1: {  # root
  %gl_FrontFacing:ptr<__in, bool, read> = var @builtin(front_facing)
  %gl_FragCoord:ptr<__in, vec4<f32>, read> = var @invariant @builtin(position)
  %foo_loc0_Input:ptr<__in, f32, read> = var @location(0)
  %foo_loc1_Input:ptr<__in, f32, read> = var @location(1) @interpolate(linear, sample)
}

%foo_inner = func(%inputs:Inputs):void {
  $B2: {
    %7:bool = access %inputs, 0i
    if %7 [t: $B3] {  # if_1
      $B3: {  # true
        %8:vec4<f32> = access %inputs, 1i
        %9:f32 = access %inputs, 2i
        %10:f32 = access %inputs, 3i
        %11:f32 = add %9, %10
        %12:vec4<f32> = mul %8, %11
        exit_if  # if_1
      }
    }
    ret
  }
}
%foo = @fragment func():void {
  $B4: {
    %14:bool = load %gl_FrontFacing
    %15:vec4<f32> = load %gl_FragCoord
    %16:f32 = load %foo_loc0_Input
    %17:f32 = load %foo_loc1_Input
    %18:Inputs = construct %14, %15, %16, %17
    %19:void = call %foo_inner, %18
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, Parameters_Mixed) {
    auto* str_ty = ty.Struct(mod.symbols.New("Inputs"),
                             {
                                 {
                                     mod.symbols.New("position"),
                                     ty.vec4<f32>(),
                                     core::IOAttributes{
                                         /* location */ std::nullopt,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ core::BuiltinValue::kPosition,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ true,
                                     },
                                 },
                                 {
                                     mod.symbols.New("color1"),
                                     ty.f32(),
                                     core::IOAttributes{
                                         /* location */ 0u,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ std::nullopt,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                             });

    auto* ep = b.Function("foo", ty.void_());
    auto* front_facing = b.FunctionParam("front_facing", ty.bool_());
    front_facing->SetBuiltin(core::BuiltinValue::kFrontFacing);
    auto* str_param = b.FunctionParam("inputs", str_ty);
    auto* color2 = b.FunctionParam("color2", ty.f32());
    color2->SetLocation(1);
    color2->SetInterpolation(core::Interpolation{core::InterpolationType::kLinear,
                                                 core::InterpolationSampling::kSample});

    ep->SetParams({front_facing, str_param, color2});
    ep->SetStage(core::ir::Function::PipelineStage::kFragment);

    b.Append(ep->Block(), [&] {
        auto* ifelse = b.If(front_facing);
        b.Append(ifelse->True(), [&] {
            auto* position = b.Access(ty.vec4<f32>(), str_param, 0_i);
            auto* color1 = b.Access(ty.f32(), str_param, 1_i);
            b.Multiply(ty.vec4<f32>(), position, b.Add(ty.f32(), color1, color2));
            b.ExitIf(ifelse);
        });
        b.Return(ep);
    });

    auto* src = R"(
Inputs = struct @align(16) {
  position:vec4<f32> @offset(0), @invariant, @builtin(position)
  color1:f32 @offset(16), @location(0)
}

%foo = @fragment func(%front_facing:bool [@front_facing], %inputs:Inputs, %color2:f32 [@location(1), @interpolate(linear, sample)]):void {
  $B1: {
    if %front_facing [t: $B2] {  # if_1
      $B2: {  # true
        %5:vec4<f32> = access %inputs, 0i
        %6:f32 = access %inputs, 1i
        %7:f32 = add %6, %color2
        %8:vec4<f32> = mul %5, %7
        exit_if  # if_1
      }
    }
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Inputs = struct @align(16) {
  position:vec4<f32> @offset(0)
  color1:f32 @offset(16)
}

$B1: {  # root
  %gl_FrontFacing:ptr<__in, bool, read> = var @builtin(front_facing)
  %gl_FragCoord:ptr<__in, vec4<f32>, read> = var @invariant @builtin(position)
  %foo_loc0_Input:ptr<__in, f32, read> = var @location(0)
  %foo_loc1_Input:ptr<__in, f32, read> = var @location(1) @interpolate(linear, sample)
}

%foo_inner = func(%front_facing:bool, %inputs:Inputs, %color2:f32):void {
  $B2: {
    if %front_facing [t: $B3] {  # if_1
      $B3: {  # true
        %9:vec4<f32> = access %inputs, 0i
        %10:f32 = access %inputs, 1i
        %11:f32 = add %10, %color2
        %12:vec4<f32> = mul %9, %11
        exit_if  # if_1
      }
    }
    ret
  }
}
%foo = @fragment func():void {
  $B4: {
    %14:bool = load %gl_FrontFacing
    %15:vec4<f32> = load %gl_FragCoord
    %16:f32 = load %foo_loc0_Input
    %17:Inputs = construct %15, %16
    %18:f32 = load %foo_loc1_Input
    %19:void = call %foo_inner, %14, %17, %18
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, ReturnValue_NonStructBuiltin) {
    auto* ep = b.Function("foo", ty.vec4<f32>());
    ep->SetReturnBuiltin(core::BuiltinValue::kPosition);
    ep->SetReturnInvariant(true);
    ep->SetStage(core::ir::Function::PipelineStage::kVertex);

    b.Append(ep->Block(), [&] {  //
        b.Return(ep, b.Construct(ty.vec4<f32>(), 0.5_f));
    });

    auto* src = R"(
%foo = @vertex func():vec4<f32> [@invariant, @position] {
  $B1: {
    %2:vec4<f32> = construct 0.5f
    ret %2
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
$B1: {  # root
  %gl_Position:ptr<__out, vec4<f32>, write> = var @invariant @builtin(position)
  %gl_PointSize:ptr<__out, f32, write> = var @builtin(__point_size)
}

%foo_inner = func():vec4<f32> {
  $B2: {
    %4:vec4<f32> = construct 0.5f
    ret %4
  }
}
%foo = @vertex func():void {
  $B3: {
    %6:vec4<f32> = call %foo_inner
    store %gl_Position, %6
    %7:f32 = swizzle %gl_Position, y
    %8:f32 = negation %7
    store_vector_element %gl_Position, 1u, %8
    %9:f32 = swizzle %gl_Position, z
    %10:f32 = swizzle %gl_Position, w
    %11:f32 = mul 2.0f, %9
    %12:f32 = sub %11, %10
    store_vector_element %gl_Position, 2u, %12
    store %gl_PointSize, 1.0f
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, ReturnValue_NonStructLocation) {
    auto* ep = b.Function("foo", ty.vec4<f32>());
    ep->SetReturnLocation(1u);
    ep->SetStage(core::ir::Function::PipelineStage::kFragment);

    b.Append(ep->Block(), [&] {  //
        b.Return(ep, b.Construct(ty.vec4<f32>(), 0.5_f));
    });

    auto* src = R"(
%foo = @fragment func():vec4<f32> [@location(1)] {
  $B1: {
    %2:vec4<f32> = construct 0.5f
    ret %2
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
$B1: {  # root
  %foo_loc1_Output:ptr<__out, vec4<f32>, write> = var @location(1)
}

%foo_inner = func():vec4<f32> {
  $B2: {
    %3:vec4<f32> = construct 0.5f
    ret %3
  }
}
%foo = @fragment func():void {
  $B3: {
    %5:vec4<f32> = call %foo_inner
    store %foo_loc1_Output, %5
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, ReturnValue_Struct) {
    auto* str_ty = ty.Struct(mod.symbols.New("Outputs"),
                             {
                                 {
                                     mod.symbols.New("position"),
                                     ty.vec4<f32>(),
                                     core::IOAttributes{
                                         /* location */ std::nullopt,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ core::BuiltinValue::kPosition,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ true,
                                     },
                                 },
                                 {
                                     mod.symbols.New("color1"),
                                     ty.f32(),
                                     core::IOAttributes{
                                         /* location */ 0u,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ std::nullopt,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                                 {
                                     mod.symbols.New("color2"),
                                     ty.f32(),
                                     core::IOAttributes{
                                         /* location */ 1u,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ std::nullopt,
                                         /* interpolation */
                                         core::Interpolation{
                                             core::InterpolationType::kLinear,
                                             core::InterpolationSampling::kSample,
                                         },
                                         /* invariant */ false,
                                     },
                                 },
                             });

    auto* ep = b.Function("foo", str_ty);
    ep->SetStage(core::ir::Function::PipelineStage::kVertex);

    b.Append(ep->Block(), [&] {  //
        b.Return(ep, b.Construct(str_ty, b.Construct(ty.vec4<f32>(), 0_f), 0.25_f, 0.75_f));
    });

    auto* src = R"(
Outputs = struct @align(16) {
  position:vec4<f32> @offset(0), @invariant, @builtin(position)
  color1:f32 @offset(16), @location(0)
  color2:f32 @offset(20), @location(1), @interpolate(linear, sample)
}

%foo = @vertex func():Outputs {
  $B1: {
    %2:vec4<f32> = construct 0.0f
    %3:Outputs = construct %2, 0.25f, 0.75f
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Outputs = struct @align(16) {
  position:vec4<f32> @offset(0)
  color1:f32 @offset(16)
  color2:f32 @offset(20)
}

$B1: {  # root
  %gl_Position:ptr<__out, vec4<f32>, write> = var @invariant @builtin(position)
  %foo_loc0_Output:ptr<__out, f32, write> = var @location(0)
  %foo_loc1_Output:ptr<__out, f32, write> = var @location(1) @interpolate(linear, sample)
  %gl_PointSize:ptr<__out, f32, write> = var @builtin(__point_size)
}

%foo_inner = func():Outputs {
  $B2: {
    %6:vec4<f32> = construct 0.0f
    %7:Outputs = construct %6, 0.25f, 0.75f
    ret %7
  }
}
%foo = @vertex func():void {
  $B3: {
    %9:Outputs = call %foo_inner
    %10:vec4<f32> = access %9, 0u
    store %gl_Position, %10
    %11:f32 = swizzle %gl_Position, y
    %12:f32 = negation %11
    store_vector_element %gl_Position, 1u, %12
    %13:f32 = swizzle %gl_Position, z
    %14:f32 = swizzle %gl_Position, w
    %15:f32 = mul 2.0f, %13
    %16:f32 = sub %15, %14
    store_vector_element %gl_Position, 2u, %16
    %17:f32 = access %9, 1u
    store %foo_loc0_Output, %17
    %18:f32 = access %9, 2u
    store %foo_loc1_Output, %18
    store %gl_PointSize, 1.0f
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, ReturnValue_DualSourceBlending) {
    auto* str_ty =
        ty.Struct(mod.symbols.New("Output"), {
                                                 {
                                                     mod.symbols.New("color1"),
                                                     ty.f32(),
                                                     core::IOAttributes{
                                                         /* location */ 0u,
                                                         /* blend_src */ 0u,
                                                         /* color */ std::nullopt,
                                                         /* builtin */ std::nullopt,
                                                         /* interpolation */ std::nullopt,
                                                         /* invariant */ false,
                                                     },
                                                 },
                                                 {
                                                     mod.symbols.New("color2"),
                                                     ty.f32(),
                                                     core::IOAttributes{
                                                         /* location */ 0u,
                                                         /* blend_src */ 1u,
                                                         /* color */ std::nullopt,
                                                         /* builtin */ std::nullopt,
                                                         /* interpolation */ std::nullopt,
                                                         /* invariant */ false,
                                                     },
                                                 },
                                             });

    auto* ep = b.Function("foo", str_ty);
    ep->SetStage(core::ir::Function::PipelineStage::kFragment);

    b.Append(ep->Block(), [&] {  //
        b.Return(ep, b.Construct(str_ty, 0.25_f, 0.75_f));
    });

    auto* src = R"(
Output = struct @align(4) {
  color1:f32 @offset(0), @location(0)
  color2:f32 @offset(4), @location(0)
}

%foo = @fragment func():Output {
  $B1: {
    %2:Output = construct 0.25f, 0.75f
    ret %2
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Output = struct @align(4) {
  color1:f32 @offset(0)
  color2:f32 @offset(4)
}

$B1: {  # root
  %foo_loc0_idx0_Output:ptr<__out, f32, write> = var @location(0) @blend_src(0)
  %foo_loc0_idx1_Output:ptr<__out, f32, write> = var @location(0) @blend_src(1)
}

%foo_inner = func():Output {
  $B2: {
    %4:Output = construct 0.25f, 0.75f
    ret %4
  }
}
%foo = @fragment func():void {
  $B3: {
    %6:Output = call %foo_inner
    %7:f32 = access %6, 0u
    store %foo_loc0_idx0_Output, %7
    %8:f32 = access %6, 1u
    store %foo_loc0_idx1_Output, %8
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, Struct_SharedByVertexAndFragment) {
    auto* vec4f = ty.vec4<f32>();
    auto* str_ty = ty.Struct(mod.symbols.New("Interface"),
                             {
                                 {
                                     mod.symbols.New("position"),
                                     vec4f,
                                     core::IOAttributes{
                                         /* location */ std::nullopt,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ core::BuiltinValue::kPosition,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                                 {
                                     mod.symbols.New("color"),
                                     vec4f,
                                     core::IOAttributes{
                                         /* location */ 0u,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ std::nullopt,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                             });

    // Vertex shader.
    {
        auto* ep = b.Function("vert", str_ty);
        ep->SetStage(core::ir::Function::PipelineStage::kVertex);

        b.Append(ep->Block(), [&] {  //
            auto* position = b.Construct(vec4f, 0_f);
            auto* color = b.Construct(vec4f, 1_f);
            b.Return(ep, b.Construct(str_ty, position, color));
        });
    }

    // Fragment shader.
    {
        auto* ep = b.Function("frag", vec4f);
        auto* inputs = b.FunctionParam("inputs", str_ty);
        ep->SetStage(core::ir::Function::PipelineStage::kFragment);
        ep->SetParams({inputs});
        ep->SetReturnLocation(0u);

        b.Append(ep->Block(), [&] {  //
            auto* position = b.Access(vec4f, inputs, 0_u);
            auto* color = b.Access(vec4f, inputs, 1_u);
            b.Return(ep, b.Add(vec4f, position, color));
        });
    }

    auto* src = R"(
Interface = struct @align(16) {
  position:vec4<f32> @offset(0), @builtin(position)
  color:vec4<f32> @offset(16), @location(0)
}

%vert = @vertex func():Interface {
  $B1: {
    %2:vec4<f32> = construct 0.0f
    %3:vec4<f32> = construct 1.0f
    %4:Interface = construct %2, %3
    ret %4
  }
}
%frag = @fragment func(%inputs:Interface):vec4<f32> [@location(0)] {
  $B2: {
    %7:vec4<f32> = access %inputs, 0u
    %8:vec4<f32> = access %inputs, 1u
    %9:vec4<f32> = add %7, %8
    ret %9
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Interface = struct @align(16) {
  position:vec4<f32> @offset(0)
  color:vec4<f32> @offset(16)
}

$B1: {  # root
  %gl_Position:ptr<__out, vec4<f32>, write> = var @builtin(position)
  %vert_loc0_Output:ptr<__out, vec4<f32>, write> = var @location(0)
  %gl_PointSize:ptr<__out, f32, write> = var @builtin(__point_size)
  %gl_FragCoord:ptr<__in, vec4<f32>, read> = var @builtin(position)
  %frag_loc0_Input:ptr<__in, vec4<f32>, read> = var @location(0)
  %frag_loc0_Output:ptr<__out, vec4<f32>, write> = var @location(0)
}

%vert_inner = func():Interface {
  $B2: {
    %8:vec4<f32> = construct 0.0f
    %9:vec4<f32> = construct 1.0f
    %10:Interface = construct %8, %9
    ret %10
  }
}
%frag_inner = func(%inputs:Interface):vec4<f32> {
  $B3: {
    %13:vec4<f32> = access %inputs, 0u
    %14:vec4<f32> = access %inputs, 1u
    %15:vec4<f32> = add %13, %14
    ret %15
  }
}
%vert = @vertex func():void {
  $B4: {
    %17:Interface = call %vert_inner
    %18:vec4<f32> = access %17, 0u
    store %gl_Position, %18
    %19:f32 = swizzle %gl_Position, y
    %20:f32 = negation %19
    store_vector_element %gl_Position, 1u, %20
    %21:f32 = swizzle %gl_Position, z
    %22:f32 = swizzle %gl_Position, w
    %23:f32 = mul 2.0f, %21
    %24:f32 = sub %23, %22
    store_vector_element %gl_Position, 2u, %24
    %25:vec4<f32> = access %17, 1u
    store %vert_loc0_Output, %25
    store %gl_PointSize, 1.0f
    ret
  }
}
%frag = @fragment func():void {
  $B5: {
    %27:vec4<f32> = load %gl_FragCoord
    %28:vec4<f32> = load %frag_loc0_Input
    %29:Interface = construct %27, %28
    %30:vec4<f32> = call %frag_inner, %29
    store %frag_loc0_Output, %30
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, Struct_SharedWithBuffer) {
    auto* vec4f = ty.vec4<f32>();
    auto* str_ty = ty.Struct(mod.symbols.New("Outputs"),
                             {
                                 {
                                     mod.symbols.New("position"),
                                     vec4f,
                                     core::IOAttributes{
                                         /* location */ std::nullopt,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ core::BuiltinValue::kPosition,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                                 {
                                     mod.symbols.New("color"),
                                     vec4f,
                                     core::IOAttributes{
                                         /* location */ 0u,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ std::nullopt,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                             });

    auto* var = b.Var(ty.ptr(storage, str_ty, read));
    var->SetBindingPoint(0, 0);
    auto* buffer = mod.root_block->Append(var);

    auto* ep = b.Function("vert", str_ty);
    ep->SetStage(core::ir::Function::PipelineStage::kVertex);

    b.Append(ep->Block(), [&] {  //
        b.Return(ep, b.Load(buffer));
    });

    auto* src = R"(
Outputs = struct @align(16) {
  position:vec4<f32> @offset(0), @builtin(position)
  color:vec4<f32> @offset(16), @location(0)
}

$B1: {  # root
  %1:ptr<storage, Outputs, read> = var @binding_point(0, 0)
}

%vert = @vertex func():Outputs {
  $B2: {
    %3:Outputs = load %1
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Outputs = struct @align(16) {
  position:vec4<f32> @offset(0)
  color:vec4<f32> @offset(16)
}

$B1: {  # root
  %1:ptr<storage, Outputs, read> = var @binding_point(0, 0)
  %gl_Position:ptr<__out, vec4<f32>, write> = var @builtin(position)
  %vert_loc0_Output:ptr<__out, vec4<f32>, write> = var @location(0)
  %gl_PointSize:ptr<__out, f32, write> = var @builtin(__point_size)
}

%vert_inner = func():Outputs {
  $B2: {
    %6:Outputs = load %1
    ret %6
  }
}
%vert = @vertex func():void {
  $B3: {
    %8:Outputs = call %vert_inner
    %9:vec4<f32> = access %8, 0u
    store %gl_Position, %9
    %10:f32 = swizzle %gl_Position, y
    %11:f32 = negation %10
    store_vector_element %gl_Position, 1u, %11
    %12:f32 = swizzle %gl_Position, z
    %13:f32 = swizzle %gl_Position, w
    %14:f32 = mul 2.0f, %12
    %15:f32 = sub %14, %13
    store_vector_element %gl_Position, 2u, %15
    %16:vec4<f32> = access %8, 1u
    store %vert_loc0_Output, %16
    store %gl_PointSize, 1.0f
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

// Test that we change the type of the sample mask builtin to an array for SPIR-V.
TEST_F(GlslWriter_ShaderIOTest, SampleMask) {
    auto* str_ty = ty.Struct(mod.symbols.New("Outputs"),
                             {
                                 {
                                     mod.symbols.New("color"),
                                     ty.f32(),
                                     core::IOAttributes{
                                         /* location */ 0u,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ std::nullopt,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                                 {
                                     mod.symbols.New("mask"),
                                     ty.u32(),
                                     core::IOAttributes{
                                         /* location */ std::nullopt,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ core::BuiltinValue::kSampleMask,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                             });

    auto* mask_in = b.FunctionParam("mask_in", ty.u32());
    mask_in->SetBuiltin(core::BuiltinValue::kSampleMask);

    auto* ep = b.Function("foo", str_ty);
    ep->SetStage(core::ir::Function::PipelineStage::kFragment);
    ep->SetParams({mask_in});

    b.Append(ep->Block(), [&] {  //
        b.Return(ep, b.Construct(str_ty, 0.5_f, mask_in));
    });

    auto* src = R"(
Outputs = struct @align(4) {
  color:f32 @offset(0), @location(0)
  mask:u32 @offset(4), @builtin(sample_mask)
}

%foo = @fragment func(%mask_in:u32 [@sample_mask]):Outputs {
  $B1: {
    %3:Outputs = construct 0.5f, %mask_in
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Outputs = struct @align(4) {
  color:f32 @offset(0)
  mask:u32 @offset(4)
}

$B1: {  # root
  %gl_SampleMaskIn:ptr<__in, u32, read> = var @builtin(sample_mask)
  %foo_loc0_Output:ptr<__out, f32, write> = var @location(0)
  %gl_SampleMask:ptr<__out, u32, write> = var @builtin(sample_mask)
}

%foo_inner = func(%mask_in:u32):Outputs {
  $B2: {
    %6:Outputs = construct 0.5f, %mask_in
    ret %6
  }
}
%foo = @fragment func():void {
  $B3: {
    %8:u32 = load %gl_SampleMaskIn
    %9:Outputs = call %foo_inner, %8
    %10:f32 = access %9, 0u
    store %foo_loc0_Output, %10
    %11:u32 = access %9, 1u
    store %gl_SampleMask, %11
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

// Test that interpolation attributes are stripped from vertex inputs and fragment outputs.
TEST_F(GlslWriter_ShaderIOTest, InterpolationOnVertexInputOrFragmentOutput) {
    auto* str_ty =
        ty.Struct(mod.symbols.New("MyStruct"), {
                                                   {
                                                       mod.symbols.New("color"),
                                                       ty.f32(),
                                                       core::IOAttributes{
                                                           /* location */ 1u,
                                                           /* blend_src */ std::nullopt,
                                                           /* color */ std::nullopt,
                                                           /* builtin */ std::nullopt,
                                                           /* interpolation */
                                                           core::Interpolation{
                                                               core::InterpolationType::kLinear,
                                                               core::InterpolationSampling::kSample,
                                                           },
                                                           /* invariant */ false,
                                                       },
                                                   },
                                               });

    // Vertex shader.
    {
        auto* ep = b.Function("vert", ty.vec4<f32>());
        ep->SetReturnBuiltin(core::BuiltinValue::kPosition);
        ep->SetReturnInvariant(true);
        ep->SetStage(core::ir::Function::PipelineStage::kVertex);

        auto* str_param = b.FunctionParam("input", str_ty);
        auto* ival = b.FunctionParam("ival", ty.i32());
        ival->SetLocation(1);
        ival->SetInterpolation(core::Interpolation{core::InterpolationType::kFlat});
        ep->SetParams({str_param, ival});

        b.Append(ep->Block(), [&] {  //
            b.Return(ep, b.Construct(ty.vec4<f32>(), 0.5_f));
        });
    }

    // Fragment shader with struct output.
    {
        auto* ep = b.Function("frag1", str_ty);
        ep->SetStage(core::ir::Function::PipelineStage::kFragment);

        b.Append(ep->Block(), [&] {  //
            b.Return(ep, b.Construct(str_ty, 0.5_f));
        });
    }

    // Fragment shader with non-struct output.
    {
        auto* ep = b.Function("frag2", ty.i32());
        ep->SetStage(core::ir::Function::PipelineStage::kFragment);
        ep->SetReturnLocation(0);
        ep->SetReturnInterpolation(core::Interpolation{core::InterpolationType::kFlat});

        b.Append(ep->Block(), [&] {  //
            b.Return(ep, b.Constant(42_i));
        });
    }

    auto* src = R"(
MyStruct = struct @align(4) {
  color:f32 @offset(0), @location(1), @interpolate(linear, sample)
}

%vert = @vertex func(%input:MyStruct, %ival:i32 [@location(1), @interpolate(flat)]):vec4<f32> [@invariant, @position] {
  $B1: {
    %4:vec4<f32> = construct 0.5f
    ret %4
  }
}
%frag1 = @fragment func():MyStruct {
  $B2: {
    %6:MyStruct = construct 0.5f
    ret %6
  }
}
%frag2 = @fragment func():i32 [@location(0), @interpolate(flat)] {
  $B3: {
    ret 42i
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
MyStruct = struct @align(4) {
  color:f32 @offset(0)
}

$B1: {  # root
  %vert_loc1_Input:ptr<__in, f32, read> = var @location(1)
  %vert_loc1_Input_1:ptr<__in, i32, read> = var @location(1)  # %vert_loc1_Input_1: 'vert_loc1_Input'
  %gl_Position:ptr<__out, vec4<f32>, write> = var @invariant @builtin(position)
  %gl_PointSize:ptr<__out, f32, write> = var @builtin(__point_size)
  %frag1_loc1_Output:ptr<__out, f32, write> = var @location(1)
  %frag2_loc0_Output:ptr<__out, i32, write> = var @location(0)
}

%vert_inner = func(%input:MyStruct, %ival:i32):vec4<f32> {
  $B2: {
    %10:vec4<f32> = construct 0.5f
    ret %10
  }
}
%frag1_inner = func():MyStruct {
  $B3: {
    %12:MyStruct = construct 0.5f
    ret %12
  }
}
%frag2_inner = func():i32 {
  $B4: {
    ret 42i
  }
}
%vert = @vertex func():void {
  $B5: {
    %15:f32 = load %vert_loc1_Input
    %16:MyStruct = construct %15
    %17:i32 = load %vert_loc1_Input_1
    %18:vec4<f32> = call %vert_inner, %16, %17
    store %gl_Position, %18
    %19:f32 = swizzle %gl_Position, y
    %20:f32 = negation %19
    store_vector_element %gl_Position, 1u, %20
    %21:f32 = swizzle %gl_Position, z
    %22:f32 = swizzle %gl_Position, w
    %23:f32 = mul 2.0f, %21
    %24:f32 = sub %23, %22
    store_vector_element %gl_Position, 2u, %24
    store %gl_PointSize, 1.0f
    ret
  }
}
%frag1 = @fragment func():void {
  $B6: {
    %26:MyStruct = call %frag1_inner
    %27:f32 = access %26, 0u
    store %frag1_loc1_Output, %27
    ret
  }
}
%frag2 = @fragment func():void {
  $B7: {
    %29:i32 = call %frag2_inner
    store %frag2_loc0_Output, %29
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, DISABLED_ClampFragDepth) {
    auto* str_ty = ty.Struct(mod.symbols.New("Outputs"),
                             {
                                 {
                                     mod.symbols.New("color"),
                                     ty.f32(),
                                     core::IOAttributes{
                                         /* location */ 0u,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ std::nullopt,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                                 {
                                     mod.symbols.New("depth"),
                                     ty.f32(),
                                     core::IOAttributes{
                                         /* location */ std::nullopt,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ core::BuiltinValue::kFragDepth,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                             });

    auto* ep = b.Function("foo", str_ty);
    ep->SetStage(core::ir::Function::PipelineStage::kFragment);

    b.Append(ep->Block(), [&] {  //
        b.Return(ep, b.Construct(str_ty, 0.5_f, 2_f));
    });

    auto* src = R"(
Outputs = struct @align(4) {
  color:f32 @offset(0), @location(0)
  depth:f32 @offset(4), @builtin(frag_depth)
}

%foo = @fragment func():Outputs {
  $B1: {
    %2:Outputs = construct 0.5f, 2.0f
    ret %2
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Outputs = struct @align(4) {
  color:f32 @offset(0)
  depth:f32 @offset(4)
}

$B1: {  # root
  %foo_loc0_Output:ptr<__out, f32, write> = var @location(0)
  %gl_FragDepth:ptr<__out, f32, write> = var @builtin(frag_depth)
}

%foo_inner = func():Outputs {
  $B2: {
    %4:Outputs = construct 0.5f, 2.0f
    ret %4
  }
}
%foo = @fragment func():void {
  $B3: {
    %6:Outputs = call %foo_inner
    %7:f32 = access %6, 0u
    store %foo_loc0_Output, %7
    %8:f32 = access %6, 1u
    %9:f32 = clamp %8, 2.0f, 3.0f
    store %gl_FragDepth, %9
    ret
  }
}
)";

    ShaderIOConfig config;
    config.depth_range_offsets = {2, 3};
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

TEST_F(GlslWriter_ShaderIOTest, DISABLED_ClampFragDepth_MultipleFragmentShaders) {
    auto* str_ty = ty.Struct(mod.symbols.New("Outputs"),
                             {
                                 {
                                     mod.symbols.New("color"),
                                     ty.f32(),
                                     core::IOAttributes{
                                         /* location */ 0u,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ std::nullopt,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                                 {
                                     mod.symbols.New("depth"),
                                     ty.f32(),
                                     core::IOAttributes{
                                         /* location */ std::nullopt,
                                         /* blend_src */ std::nullopt,
                                         /* color */ std::nullopt,
                                         /* builtin */ core::BuiltinValue::kFragDepth,
                                         /* interpolation */ std::nullopt,
                                         /* invariant */ false,
                                     },
                                 },
                             });

    auto make_entry_point = [&](std::string_view name) {
        auto* ep = b.Function(name, str_ty);
        ep->SetStage(core::ir::Function::PipelineStage::kFragment);
        b.Append(ep->Block(), [&] {  //
            b.Return(ep, b.Construct(str_ty, 0.5_f, 2_f));
        });
    };
    make_entry_point("ep1");
    make_entry_point("ep2");
    make_entry_point("ep3");

    auto* src = R"(
Outputs = struct @align(4) {
  color:f32 @offset(0), @location(0)
  depth:f32 @offset(4), @builtin(frag_depth)
}

%ep1 = @fragment func():Outputs {
  $B1: {
    %2:Outputs = construct 0.5f, 2.0f
    ret %2
  }
}
%ep2 = @fragment func():Outputs {
  $B2: {
    %4:Outputs = construct 0.5f, 2.0f
    ret %4
  }
}
%ep3 = @fragment func():Outputs {
  $B3: {
    %6:Outputs = construct 0.5f, 2.0f
    ret %6
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Outputs = struct @align(4) {
  color:f32 @offset(0)
  depth:f32 @offset(4)
}

$B1: {  # root
  %ep1_loc0_Output:ptr<__out, f32, write> = var @location(0)
  %gl_FragDepth:ptr<__out, f32, write> = var @builtin(frag_depth)
  %ep2_loc0_Output:ptr<__out, f32, write> = var @location(0)
  %gl_FragDepth_1:ptr<__out, f32, write> = var @builtin(frag_depth)  # %gl_FragDepth_1: 'gl_FragDepth'
  %ep3_loc0_Output:ptr<__out, f32, write> = var @location(0)
  %gl_FragDepth_2:ptr<__out, f32, write> = var @builtin(frag_depth)  # %gl_FragDepth_2: 'gl_FragDepth'
}

%ep1_inner = func():Outputs {
  $B2: {
    %8:Outputs = construct 0.5f, 2.0f
    ret %8
  }
}
%ep2_inner = func():Outputs {
  $B3: {
    %10:Outputs = construct 0.5f, 2.0f
    ret %10
  }
}
%ep3_inner = func():Outputs {
  $B4: {
    %12:Outputs = construct 0.5f, 2.0f
    ret %12
  }
}
%ep1 = @fragment func():void {
  $B5: {
    %14:Outputs = call %ep1_inner
    %15:f32 = access %14, 0u
    store %ep1_loc0_Output, %15
    %16:f32 = access %14, 1u
    %17:f32 = clamp %16, 0.0f, 0.0f
    store %gl_FragDepth, %17
    ret
  }
}
%ep2 = @fragment func():void {
  $B6: {
    %19:Outputs = call %ep2_inner
    %20:f32 = access %19, 0u
    store %ep2_loc0_Output, %20
    %21:f32 = access %19, 1u
    %22:f32 = clamp %21, 0.0f, 0.0f
    store %gl_FragDepth_1, %22
    ret
  }
}
%ep3 = @fragment func():void {
  $B7: {
    %24:Outputs = call %ep3_inner
    %25:f32 = access %24, 0u
    store %ep3_loc0_Output, %25
    %26:f32 = access %24, 1u
    %27:f32 = clamp %26, 0.0f, 0.0f
    store %gl_FragDepth_2, %27
    ret
  }
}
)";

    ShaderIOConfig config;
    Run(ShaderIO, config);

    EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::glsl::writer::raise
