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

#include <string>
#include <tuple>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "src/tint/lang/core/address_space.h"
#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/function_param.h"
#include "src/tint/lang/core/ir/ir_helper_test.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/lang/core/number.h"
#include "src/tint/lang/core/type/manager.h"
#include "src/tint/lang/core/type/matrix.h"
#include "src/tint/lang/core/type/memory_view.h"
#include "src/tint/lang/core/type/pointer.h"
#include "src/tint/lang/core/type/reference.h"
#include "src/tint/lang/core/type/struct.h"
#include "src/tint/utils/text/string.h"

namespace tint::core::ir {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

using IR_ValidatorTest = IRTestHelper;

TEST_F(IR_ValidatorTest, RootBlock_Var) {
    mod.root_block->Append(b.Var(ty.ptr<private_, i32>()));
    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, RootBlock_NonVar) {
    auto* l = b.Loop();
    l->Body()->Append(b.Continue(l));

    mod.root_block->Append(l);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:2:3 error: loop: root block: invalid instruction: tint::core::ir::Loop
  loop [b: $B2] {  # loop_1
  ^^^^^^^^^^^^^

:1:1 note: in block
$B1: {  # root
^^^

note: # Disassembly
$B1: {  # root
  loop [b: $B2] {  # loop_1
    $B2: {  # body
      continue  # -> $B3
    }
  }
}

)");
}

TEST_F(IR_ValidatorTest, RootBlock_Let) {
    mod.root_block->Append(b.Let("a", 1_f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:2:12 error: let: root block: invalid instruction: tint::core::ir::Let
  %a:f32 = let 1.0f
           ^^^

:1:1 note: in block
$B1: {  # root
^^^

note: # Disassembly
$B1: {  # root
  %a:f32 = let 1.0f
}

)");
}

TEST_F(IR_ValidatorTest, RootBlock_LetWithAllowModuleScopeLets) {
    mod.root_block->Append(b.Let("a", 1_f));

    auto res = ir::Validate(mod, Capabilities{Capability::kAllowModuleScopeLets});
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, RootBlock_Construct) {
    mod.root_block->Append(b.Construct(ty.vec2<f32>(), 1_f, 2_f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:2:18 error: construct: root block: invalid instruction: tint::core::ir::Construct
  %1:vec2<f32> = construct 1.0f, 2.0f
                 ^^^^^^^^^

:1:1 note: in block
$B1: {  # root
^^^

note: # Disassembly
$B1: {  # root
  %1:vec2<f32> = construct 1.0f, 2.0f
}

)");
}

TEST_F(IR_ValidatorTest, RootBlock_ConstructWithAllowModuleScopeLets) {
    mod.root_block->Append(b.Construct(ty.vec2<f32>(), 1_f, 2_f));

    auto res = ir::Validate(mod, Capabilities{Capability::kAllowModuleScopeLets});
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, RootBlock_VarBlockMismatch) {
    auto* var = b.Var(ty.ptr<private_, i32>());
    mod.root_block->Append(var);

    auto* f = b.Function("f", ty.void_());
    f->Block()->Append(b.Return(f));
    var->SetBlock(f->Block());

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:2:38 error: var: instruction in root block does not have root block as parent
  %1:ptr<private, i32, read_write> = var
                                     ^^^

:1:1 note: in block
$B1: {  # root
^^^

note: # Disassembly
$B1: {  # root
  %1:ptr<private, i32, read_write> = var
}

%f = func():void {
  $B2: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Function) {
    auto* f = b.Function("my_func", ty.void_());

    f->SetParams({b.FunctionParam(ty.i32()), b.FunctionParam(ty.f32())});
    f->Block()->Append(b.Return(f));

    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, Function_Duplicate) {
    auto* f = b.Function("my_func", ty.void_());
    // Function would auto-push by the builder, so this adds a duplicate
    mod.functions.Push(f);

    f->SetParams({b.FunctionParam(ty.i32()), b.FunctionParam(ty.f32())});
    f->Block()->Append(b.Return(f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:1:1 error: function %my_func added to module multiple times
%my_func = func(%2:i32, %3:f32):void {
^^^^^^^^

note: # Disassembly
%my_func = func(%2:i32, %3:f32):void {
  $B1: {
    ret
  }
}
%my_func = func(%2:i32, %3:f32):void {
  $B1: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Function_DeadParameter) {
    auto* f = b.Function("my_func", ty.void_());
    auto* p = b.FunctionParam("my_param", ty.f32());
    f->SetParams({p});
    f->Block()->Append(b.Return(f));

    p->Destroy();

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:1:17 error: destroyed parameter found in function parameter list
%my_func = func(%my_param:f32):void {
                ^^^^^^^^^^^^^

note: # Disassembly
%my_func = func(%my_param:f32):void {
  $B1: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Function_ParameterWithNullFunction) {
    auto* f = b.Function("my_func", ty.void_());
    auto* p = b.FunctionParam("my_param", ty.f32());
    f->SetParams({p});
    f->Block()->Append(b.Return(f));

    p->SetFunction(nullptr);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:1:17 error: function parameter has nullptr parent function
%my_func = func(%my_param:f32):void {
                ^^^^^^^^^^^^^

note: # Disassembly
%my_func = func(%my_param:f32):void {
  $B1: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Function_ParameterUsedInMultipleFunctions) {
    auto* p = b.FunctionParam("my_param", ty.f32());
    auto* f1 = b.Function("my_func1", ty.void_());
    auto* f2 = b.Function("my_func2", ty.void_());
    f1->SetParams({p});
    f2->SetParams({p});
    f1->Block()->Append(b.Return(f1));
    f2->Block()->Append(b.Return(f2));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:1:18 error: function parameter has incorrect parent function
%my_func1 = func(%my_param:f32):void {
                 ^^^^^^^^^^^^^

:6:1 note: parent function declared here
%my_func2 = func(%my_param:f32):void {
^^^^^^^^^

note: # Disassembly
%my_func1 = func(%my_param:f32):void {
  $B1: {
    ret
  }
}
%my_func2 = func(%my_param:f32):void {
  $B2: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Function_ParameterWithNullType) {
    auto* f = b.Function("my_func", ty.void_());
    auto* p = b.FunctionParam("my_param", nullptr);
    f->SetParams({p});
    f->Block()->Append(b.Return(f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:1:17 error: function parameter has nullptr type
%my_func = func(%my_param:undef):void {
                ^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func(%my_param:undef):void {
  $B1: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Function_MissingWorkgroupSize) {
    auto* f = b.Function("f", ty.void_(), Function::PipelineStage::kCompute);
    b.Append(f->Block(), [&] { b.Return(f); });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:1:1 error: compute entry point requires workgroup size attribute
%f = @compute func():void {
^^

note: # Disassembly
%f = @compute func():void {
  $B1: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, CallToFunctionOutsideModule) {
    auto* f = b.Function("f", ty.void_());
    auto* g = b.Function("g", ty.void_());
    mod.functions.Pop();  // Remove g

    b.Append(f->Block(), [&] {
        b.Call(g);
        b.Return(f);
    });
    b.Append(g->Block(), [&] { b.Return(g); });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:20 error: call: %g is not part of the module
    %2:void = call %g
                   ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%f = func():void {
  $B1: {
    %2:void = call %g
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, CallToEntryPointFunction) {
    auto* f = b.Function("f", ty.void_());
    auto* g = b.Function("g", ty.void_(), Function::PipelineStage::kCompute);
    g->SetWorkgroupSize(1, 1, 1);

    b.Append(f->Block(), [&] {
        b.Call(g);
        b.Return(f);
    });
    b.Append(g->Block(), [&] { b.Return(g); });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:20 error: call: call target must not have a pipeline stage
    %2:void = call %g
                   ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%f = func():void {
  $B1: {
    %2:void = call %g
    ret
  }
}
%g = @compute @workgroup_size(1, 1, 1) func():void {
  $B2: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, CallToFunctionTooFewArguments) {
    auto* g = b.Function("g", ty.void_());
    g->SetParams({b.FunctionParam<i32>(), b.FunctionParam<i32>()});
    b.Append(g->Block(), [&] { b.Return(g); });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        b.Call(g, 42_i);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:20 error: call: function has 2 parameters, but call provides 1 arguments
    %5:void = call %g, 42i
                   ^^

:7:3 note: in block
  $B2: {
  ^^^

note: # Disassembly
%g = func(%2:i32, %3:i32):void {
  $B1: {
    ret
  }
}
%f = func():void {
  $B2: {
    %5:void = call %g, 42i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, CallToFunctionTooManyArguments) {
    auto* g = b.Function("g", ty.void_());
    g->SetParams({b.FunctionParam<i32>(), b.FunctionParam<i32>()});
    b.Append(g->Block(), [&] { b.Return(g); });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        b.Call(g, 1_i, 2_i, 3_i);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:20 error: call: function has 2 parameters, but call provides 3 arguments
    %5:void = call %g, 1i, 2i, 3i
                   ^^

:7:3 note: in block
  $B2: {
  ^^^

note: # Disassembly
%g = func(%2:i32, %3:i32):void {
  $B1: {
    ret
  }
}
%f = func():void {
  $B2: {
    %5:void = call %g, 1i, 2i, 3i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, CallToFunctionWrongArgType) {
    auto* g = b.Function("g", ty.void_());
    g->SetParams({b.FunctionParam<i32>(), b.FunctionParam<i32>(), b.FunctionParam<i32>()});
    b.Append(g->Block(), [&] { b.Return(g); });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        b.Call(g, 1_i, 2_f, 3_i);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:8:28 error: call: function parameter 1 is of type 'i32', but argument is of type 'f32'
    %6:void = call %g, 1i, 2.0f, 3i
                           ^^^^

:7:3 note: in block
  $B2: {
  ^^^

note: # Disassembly
%g = func(%2:i32, %3:i32, %4:i32):void {
  $B1: {
    ret
  }
}
%f = func():void {
  $B2: {
    %6:void = call %g, 1i, 2.0f, 3i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, CallToFunctionNullArg) {
    auto* g = b.Function("g", ty.void_());
    g->SetParams({b.FunctionParam<i32>()});
    b.Append(g->Block(), [&] { b.Return(g); });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        b.Call(g, nullptr);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:24 error: call: operand is undefined
    %4:void = call %g, undef
                       ^^^^^

:7:3 note: in block
  $B2: {
  ^^^

note: # Disassembly
%g = func(%2:i32):void {
  $B1: {
    ret
  }
}
%f = func():void {
  $B2: {
    %4:void = call %g, undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, CallToNullFunction) {
    auto* g = b.Function("g", ty.void_());
    b.Append(g->Block(), [&] { b.Return(g); });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        auto* c = b.Call(g);
        c->SetOperands(Vector{static_cast<ir::Value*>(nullptr)});
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:20 error: call: operand is undefined
    %3:void = call undef
                   ^^^^^

:7:3 note: in block
  $B2: {
  ^^^

note: # Disassembly
%g = func():void {
  $B1: {
    ret
  }
}
%f = func():void {
  $B2: {
    %3:void = call undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, CallToFunctionNoResult) {
    auto* g = b.Function("g", ty.void_());
    b.Append(g->Block(), [&] { b.Return(g); });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        auto* c = b.Call(g);
        c->ClearResults();
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:13 error: call: expected exactly 1 results, got 0
    undef = call %g
            ^^^^

:7:3 note: in block
  $B2: {
  ^^^

note: # Disassembly
%g = func():void {
  $B1: {
    ret
  }
}
%f = func():void {
  $B2: {
    undef = call %g
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, CallToFunctionNoOperands) {
    auto* g = b.Function("g", ty.void_());
    b.Append(g->Block(), [&] { b.Return(g); });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        auto* c = b.Call(g);
        c->ClearOperands();
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:15 error: call: expected at least 1 operands, got 0
    %3:void = call undef
              ^^^^

:7:3 note: in block
  $B2: {
  ^^^

note: # Disassembly
%g = func():void {
  $B1: {
    ret
  }
}
%f = func():void {
  $B2: {
    %3:void = call undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, CallToNonFunctionTarget) {
    auto* g = b.Function("g", ty.void_());
    mod.functions.Pop();  // Remove g, since it isn't actually going to be used, it is just needed
                          // to create the UserCall before mangling it

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        auto* c = b.Call(g);
        c->SetOperands(Vector{b.Value(0_i)});
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:20 error: call: target not defined or not a function
    %2:void = call 0i
                   ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%f = func():void {
  $B1: {
    %2:void = call 0i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Construct_Struct_ZeroValue) {
    auto* str_ty = ty.Struct(mod.symbols.New("MyStruct"), {
                                                              {mod.symbols.New("a"), ty.i32()},
                                                              {mod.symbols.New("b"), ty.u32()},
                                                          });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        b.Construct(str_ty);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success) << res.Failure();
}

TEST_F(IR_ValidatorTest, Construct_Struct_ValidArgs) {
    auto* str_ty = ty.Struct(mod.symbols.New("MyStruct"), {
                                                              {mod.symbols.New("a"), ty.i32()},
                                                              {mod.symbols.New("b"), ty.u32()},
                                                          });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        b.Construct(str_ty, 1_i, 2_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success) << res.Failure();
}

TEST_F(IR_ValidatorTest, Construct_Struct_NotEnoughArgs) {
    auto* str_ty = ty.Struct(mod.symbols.New("MyStruct"), {
                                                              {mod.symbols.New("a"), ty.i32()},
                                                              {mod.symbols.New("b"), ty.u32()},
                                                          });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        b.Construct(str_ty, 1_i);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:19 error: construct: structure has 2 members, but construct provides 1 arguments
    %2:MyStruct = construct 1i
                  ^^^^^^^^^

:7:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
MyStruct = struct @align(4) {
  a:i32 @offset(0)
  b:u32 @offset(4)
}

%f = func():void {
  $B1: {
    %2:MyStruct = construct 1i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Construct_Struct_TooManyArgs) {
    auto* str_ty = ty.Struct(mod.symbols.New("MyStruct"), {
                                                              {mod.symbols.New("a"), ty.i32()},
                                                              {mod.symbols.New("b"), ty.u32()},
                                                          });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        b.Construct(str_ty, 1_i, 2_u, 3_i);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:19 error: construct: structure has 2 members, but construct provides 3 arguments
    %2:MyStruct = construct 1i, 2u, 3i
                  ^^^^^^^^^

:7:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
MyStruct = struct @align(4) {
  a:i32 @offset(0)
  b:u32 @offset(4)
}

%f = func():void {
  $B1: {
    %2:MyStruct = construct 1i, 2u, 3i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Construct_Struct_WrongArgType) {
    auto* str_ty = ty.Struct(mod.symbols.New("MyStruct"), {
                                                              {mod.symbols.New("a"), ty.i32()},
                                                              {mod.symbols.New("b"), ty.u32()},
                                                          });

    auto* f = b.Function("f", ty.void_());
    b.Append(f->Block(), [&] {
        b.Construct(str_ty, 1_i, 2_i);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:8:33 error: construct: structure member 1 is of type 'u32', but argument is of type 'i32'
    %2:MyStruct = construct 1i, 2i
                                ^^

:7:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
MyStruct = struct @align(4) {
  a:i32 @offset(0)
  b:u32 @offset(4)
}

%f = func():void {
  $B1: {
    %2:MyStruct = construct 1i, 2i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Block_NoTerminator) {
    b.Function("my_func", ty.void_());

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:2:3 error: block does not end in a terminator instruction
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
  }
}
)");
}

TEST_F(IR_ValidatorTest, Block_VarBlockMismatch) {
    auto* var = b.Var(ty.ptr<function, i32>());

    auto* f = b.Function("f", ty.void_());
    f->Block()->Append(var);
    f->Block()->Append(b.Return(f));

    auto* g = b.Function("g", ty.void_());
    g->Block()->Append(b.Return(g));

    var->SetBlock(g->Block());

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:41 error: var: block instruction does not have same block as parent
    %2:ptr<function, i32, read_write> = var
                                        ^^^

:2:3 note: in block
  $B1: {
  ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%f = func():void {
  $B1: {
    %2:ptr<function, i32, read_write> = var
    ret
  }
}
%g = func():void {
  $B2: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Block_DeadParameter) {
    auto* f = b.Function("my_func", ty.void_());

    auto* p = b.BlockParam("my_param", ty.f32());
    b.Append(f->Block(), [&] {
        auto* l = b.Loop();
        b.Append(l->Initializer(), [&] { b.NextIteration(l, nullptr); });
        l->Body()->SetParams({p});
        b.Append(l->Body(), [&] { b.ExitLoop(l); });
        b.Return(f);
    });

    p->Destroy();

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:12 error: destroyed parameter found in block parameter list
      $B3 (%my_param:f32): {  # body
           ^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3] {  # loop_1
      $B2: {  # initializer
        next_iteration undef  # -> $B3
      }
      $B3 (%my_param:f32): {  # body
        exit_loop  # loop_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Block_ParameterWithNullBlock) {
    auto* f = b.Function("my_func", ty.void_());

    auto* p = b.BlockParam("my_param", ty.f32());
    b.Append(f->Block(), [&] {
        auto* l = b.Loop();
        b.Append(l->Initializer(), [&] { b.NextIteration(l, nullptr); });
        l->Body()->SetParams({p});
        b.Append(l->Body(), [&] { b.ExitLoop(l); });
        b.Return(f);
    });

    p->SetBlock(nullptr);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:12 error: block parameter has nullptr parent block
      $B3 (%my_param:f32): {  # body
           ^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3] {  # loop_1
      $B2: {  # initializer
        next_iteration undef  # -> $B3
      }
      $B3 (%my_param:f32): {  # body
        exit_loop  # loop_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Block_ParameterUsedInMultipleBlocks) {
    auto* f = b.Function("my_func", ty.void_());

    auto* p = b.BlockParam("my_param", ty.f32());
    b.Append(f->Block(), [&] {
        auto* l = b.Loop();
        b.Append(l->Initializer(), [&] { b.NextIteration(l, nullptr); });
        l->Body()->SetParams({p});
        b.Append(l->Body(), [&] { b.Continue(l, p); });
        l->Continuing()->SetParams({p});
        b.Append(l->Continuing(), [&] { b.NextIteration(l, p); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:12 error: block parameter has incorrect parent block
      $B3 (%my_param:f32): {  # body
           ^^^^^^^^^

:10:7 note: parent block declared here
      $B4 (%my_param:f32): {  # continuing
      ^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3, c: $B4] {  # loop_1
      $B2: {  # initializer
        next_iteration undef  # -> $B3
      }
      $B3 (%my_param:f32): {  # body
        continue %my_param  # -> $B4
      }
      $B4 (%my_param:f32): {  # continuing
        next_iteration %my_param  # -> $B3
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_NoOperands) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.vec3<f32>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        auto* access = b.Access(ty.f32(), obj, 0_i);
        access->ClearOperands();
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:14 error: access: expected at least 1 operands, got 0
    %3:f32 = access
             ^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:vec3<f32>):void {
  $B1: {
    %3:f32 = access
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_NoResults) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.vec3<f32>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        auto* access = b.Access(ty.f32(), obj, 0_i);
        access->ClearResults();
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:13 error: access: expected exactly 1 results, got 0
    undef = access %2, 0i
            ^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:vec3<f32>):void {
  $B1: {
    undef = access %2, 0i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_NullObject) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        b.Access(ty.f32(), nullptr);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:21 error: access: operand is undefined
    %2:f32 = access undef
                    ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:f32 = access undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_NullIndex) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.vec3<f32>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.f32(), obj, nullptr);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:25 error: access: operand is undefined
    %3:f32 = access %2, undef
                        ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:vec3<f32>):void {
  $B1: {
    %3:f32 = access %2, undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_NegativeIndex) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.vec3<f32>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.f32(), obj, -1_i);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:25 error: access: constant index must be positive, got -1
    %3:f32 = access %2, -1i
                        ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:vec3<f32>):void {
  $B1: {
    %3:f32 = access %2, -1i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_OOB_Index_Value) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.mat3x2<f32>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.f32(), obj, 1_u, 3_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:29 error: access: index out of bounds for type 'vec2<f32>'
    %3:f32 = access %2, 1u, 3u
                            ^^

:2:3 note: in block
  $B1: {
  ^^^

:3:29 note: acceptable range: [0..1]
    %3:f32 = access %2, 1u, 3u
                            ^^

note: # Disassembly
%my_func = func(%2:mat3x2<f32>):void {
  $B1: {
    %3:f32 = access %2, 1u, 3u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_OOB_Index_Ptr) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<private_, array<array<f32, 2>, 3>>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.ptr<private_, f32>(), obj, 1_u, 3_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:3:55 error: access: index out of bounds for type 'ptr<private, array<f32, 2>, read_write>'
    %3:ptr<private, f32, read_write> = access %2, 1u, 3u
                                                      ^^

:2:3 note: in block
  $B1: {
  ^^^

:3:55 note: acceptable range: [0..1]
    %3:ptr<private, f32, read_write> = access %2, 1u, 3u
                                                      ^^

note: # Disassembly
%my_func = func(%2:ptr<private, array<array<f32, 2>, 3>, read_write>):void {
  $B1: {
    %3:ptr<private, f32, read_write> = access %2, 1u, 3u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_StaticallyUnindexableType_Value) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.f32());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.f32(), obj, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:25 error: access: type 'f32' cannot be indexed
    %3:f32 = access %2, 1u
                        ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:f32):void {
  $B1: {
    %3:f32 = access %2, 1u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_StaticallyUnindexableType_Ptr) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<private_, f32>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.ptr<private_, f32>(), obj, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:51 error: access: type 'ptr<private, f32, read_write>' cannot be indexed
    %3:ptr<private, f32, read_write> = access %2, 1u
                                                  ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:ptr<private, f32, read_write>):void {
  $B1: {
    %3:ptr<private, f32, read_write> = access %2, 1u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_DynamicallyUnindexableType_Value) {
    auto* str_ty = ty.Struct(mod.symbols.New("MyStruct"), {
                                                              {mod.symbols.New("a"), ty.i32()},
                                                              {mod.symbols.New("b"), ty.i32()},
                                                          });

    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(str_ty);
    auto* idx = b.FunctionParam(ty.i32());
    f->SetParams({obj, idx});

    b.Append(f->Block(), [&] {
        b.Access(ty.i32(), obj, idx);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:25 error: access: type 'MyStruct' cannot be dynamically indexed
    %4:i32 = access %2, %3
                        ^^

:7:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
MyStruct = struct @align(4) {
  a:i32 @offset(0)
  b:i32 @offset(4)
}

%my_func = func(%2:MyStruct, %3:i32):void {
  $B1: {
    %4:i32 = access %2, %3
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_DynamicallyUnindexableType_Ptr) {
    auto* str_ty = ty.Struct(mod.symbols.New("MyStruct"), {
                                                              {mod.symbols.New("a"), ty.i32()},
                                                              {mod.symbols.New("b"), ty.i32()},
                                                          });

    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<private_, read_write>(str_ty));
    auto* idx = b.FunctionParam(ty.i32());
    f->SetParams({obj, idx});

    b.Append(f->Block(), [&] {
        b.Access(ty.i32(), obj, idx);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:8:25 error: access: type 'ptr<private, MyStruct, read_write>' cannot be dynamically indexed
    %4:i32 = access %2, %3
                        ^^

:7:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
MyStruct = struct @align(4) {
  a:i32 @offset(0)
  b:i32 @offset(4)
}

%my_func = func(%2:ptr<private, MyStruct, read_write>, %3:i32):void {
  $B1: {
    %4:i32 = access %2, %3
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_Incorrect_Type_Value_Value) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.mat3x2<f32>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.i32(), obj, 1_u, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:3:14 error: access: result of access chain is type 'f32' but instruction type is 'i32'
    %3:i32 = access %2, 1u, 1u
             ^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:mat3x2<f32>):void {
  $B1: {
    %3:i32 = access %2, 1u, 1u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_Incorrect_Type_Ptr_Ptr) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<private_, array<array<f32, 2>, 3>>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.ptr<private_, i32>(), obj, 1_u, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:3:40 error: access: result of access chain is type 'ptr<private, f32, read_write>' but instruction type is 'ptr<private, i32, read_write>'
    %3:ptr<private, i32, read_write> = access %2, 1u, 1u
                                       ^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:ptr<private, array<array<f32, 2>, 3>, read_write>):void {
  $B1: {
    %3:ptr<private, i32, read_write> = access %2, 1u, 1u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_Incorrect_Type_Ptr_Value) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<private_, array<array<f32, 2>, 3>>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.f32(), obj, 1_u, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:3:14 error: access: result of access chain is type 'ptr<private, f32, read_write>' but instruction type is 'f32'
    %3:f32 = access %2, 1u, 1u
             ^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:ptr<private, array<array<f32, 2>, 3>, read_write>):void {
  $B1: {
    %3:f32 = access %2, 1u, 1u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_IndexVectorPtr) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<private_, vec3<f32>>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.f32(), obj, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:25 error: access: cannot obtain address of vector element
    %3:f32 = access %2, 1u
                        ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:ptr<private, vec3<f32>, read_write>):void {
  $B1: {
    %3:f32 = access %2, 1u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_IndexVectorPtr_WithCapability) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<private_, vec3<f32>>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.ptr<private_, f32>(), obj, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod, Capabilities{Capability::kAllowVectorElementPointer});
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, Access_IndexVectorPtr_ViaMatrixPtr) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<private_, mat3x2<f32>>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.f32(), obj, 1_u, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:29 error: access: cannot obtain address of vector element
    %3:f32 = access %2, 1u, 1u
                            ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:ptr<private, mat3x2<f32>, read_write>):void {
  $B1: {
    %3:f32 = access %2, 1u, 1u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_IndexVectorPtr_ViaMatrixPtr_WithCapability) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<private_, mat3x2<f32>>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.ptr<private_, f32>(), obj, 1_u, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod, Capabilities{Capability::kAllowVectorElementPointer});
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, Access_Incorrect_Ptr_AddressSpace) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<storage, array<f32, 2>, read>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.ptr<uniform, f32, read>(), obj, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:3:34 error: access: result of access chain is type 'ptr<storage, f32, read>' but instruction type is 'ptr<uniform, f32, read>'
    %3:ptr<uniform, f32, read> = access %2, 1u
                                 ^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:ptr<storage, array<f32, 2>, read>):void {
  $B1: {
    %3:ptr<uniform, f32, read> = access %2, 1u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_Incorrect_Ptr_Access) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.ptr<storage, array<f32, 2>, read>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.ptr<storage, f32, read_write>(), obj, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:3:40 error: access: result of access chain is type 'ptr<storage, f32, read>' but instruction type is 'ptr<storage, f32, read_write>'
    %3:ptr<storage, f32, read_write> = access %2, 1u
                                       ^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func(%2:ptr<storage, array<f32, 2>, read>):void {
  $B1: {
    %3:ptr<storage, f32, read_write> = access %2, 1u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Access_IndexVector) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.vec3<f32>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.f32(), obj, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, Access_IndexVector_ViaMatrix) {
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam(ty.mat3x2<f32>());
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ty.f32(), obj, 1_u, 1_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, Access_ExtractPointerFromStruct) {
    auto* ptr = ty.ptr<private_, i32>();
    Vector<type::Manager::StructMemberDesc, 1> members{
        type::Manager::StructMemberDesc{mod.symbols.New("a"), ptr},
    };
    auto* str = ty.Struct(mod.symbols.New("MyStruct"), std::move(members));
    auto* f = b.Function("my_func", ty.void_());
    auto* obj = b.FunctionParam("obj", str);
    f->SetParams({obj});

    b.Append(f->Block(), [&] {
        b.Access(ptr, obj, 0_u);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, Block_TerminatorInMiddle) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Return(f);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:5 error: return: must be the last instruction in the block
    ret
    ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    ret
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, If_EmptyFalse) {
    auto* f = b.Function("my_func", ty.void_());

    auto* if_ = b.If(true);
    if_->True()->Append(b.Return(f));

    f->Block()->Append(if_);
    f->Block()->Append(b.Return(f));

    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, If_EmptyTrue) {
    auto* f = b.Function("my_func", ty.void_());

    auto* if_ = b.If(true);
    if_->False()->Append(b.Return(f));

    f->Block()->Append(if_);
    f->Block()->Append(b.Return(f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:4:7 error: block does not end in a terminator instruction
      $B2: {  # true
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    if true [t: $B2, f: $B3] {  # if_1
      $B2: {  # true
      }
      $B3: {  # false
        ret
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, If_ConditionIsBool) {
    auto* f = b.Function("my_func", ty.void_());

    auto* if_ = b.If(1_i);
    if_->True()->Append(b.Return(f));
    if_->False()->Append(b.Return(f));

    f->Block()->Append(if_);
    f->Block()->Append(b.Return(f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:8 error: if: condition type must be 'bool'
    if 1i [t: $B2, f: $B3] {  # if_1
       ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    if 1i [t: $B2, f: $B3] {  # if_1
      $B2: {  # true
        ret
      }
      $B3: {  # false
        ret
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, If_ConditionIsNullptr) {
    auto* f = b.Function("my_func", ty.void_());

    auto* if_ = b.If(nullptr);
    if_->True()->Append(b.Return(f));
    if_->False()->Append(b.Return(f));

    f->Block()->Append(if_);
    f->Block()->Append(b.Return(f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:8 error: if: operand is undefined
    if undef [t: $B2, f: $B3] {  # if_1
       ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    if undef [t: $B2, f: $B3] {  # if_1
      $B2: {  # true
        ret
      }
      $B3: {  # false
        ret
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, If_NullResult) {
    auto* f = b.Function("my_func", ty.void_());

    auto* if_ = b.If(true);
    if_->True()->Append(b.Return(f));
    if_->False()->Append(b.Return(f));

    if_->SetResults(Vector<InstructionResult*, 1>{nullptr});

    f->Block()->Append(if_);
    f->Block()->Append(b.Return(f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:5 error: if: result is undefined
    undef = if true [t: $B2, f: $B3] {  # if_1
    ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    undef = if true [t: $B2, f: $B3] {  # if_1
      $B2: {  # true
        ret
      }
      $B3: {  # false
        ret
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Loop_OnlyBody) {
    auto* f = b.Function("my_func", ty.void_());

    auto* l = b.Loop();
    l->Body()->Append(b.ExitLoop(l));

    auto sb = b.Append(f->Block());
    sb.Append(l);
    sb.Return(f);

    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, Loop_EmptyBody) {
    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Append(b.Loop());
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:4:7 error: block does not end in a terminator instruction
      $B2: {  # body
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2] {  # loop_1
      $B2: {  # body
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Var_RootBlock_NullResult) {
    auto* v = mod.allocators.instructions.Create<ir::Var>(nullptr);
    v->SetInitializer(b.Constant(0_i));
    mod.root_block->Append(v);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:2:3 error: var: result is undefined
  undef = var, 0i
  ^^^^^

:1:1 note: in block
$B1: {  # root
^^^

:2:3 error: var: result is undefined
  undef = var, 0i
  ^^^^^

:1:1 note: in block
$B1: {  # root
^^^

note: # Disassembly
$B1: {  # root
  undef = var, 0i
}

)");
}

TEST_F(IR_ValidatorTest, Var_Function_NullResult) {
    auto* v = mod.allocators.instructions.Create<ir::Var>(nullptr);
    v->SetInitializer(b.Constant(0_i));

    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Append(v);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:5 error: var: result is undefined
    undef = var, 0i
    ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

:3:5 error: var: result is undefined
    undef = var, 0i
    ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    undef = var, 0i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Var_Function_NoResult) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* v = b.Var<function, f32>();
        v->SetInitializer(b.Constant(1_i));
        v->ClearResults();
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:13 error: var: expected exactly 1 results, got 0
    undef = var, 1i
            ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    undef = var, 1i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Var_Function_UnexpectedInputAttachmentIndex) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* v = b.Var<function, f32>();
        v->SetInputAttachmentIndex(0);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:41 error: var: '@input_attachment_index' is not valid for non-handle var
    %2:ptr<function, f32, read_write> = var @input_attachment_index(0)
                                        ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, f32, read_write> = var @input_attachment_index(0)
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Var_Private_UnexpectedInputAttachmentIndex) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* v = b.Var<private_, f32>();

        v->SetInputAttachmentIndex(0);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:40 error: var: '@input_attachment_index' is not valid for non-handle var
    %2:ptr<private, f32, read_write> = var @input_attachment_index(0)
                                       ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<private, f32, read_write> = var @input_attachment_index(0)
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Var_PushConstant_UnexpectedInputAttachmentIndex) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* v = b.Var<push_constant, f32>();
        v->SetInputAttachmentIndex(0);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:40 error: var: '@input_attachment_index' is not valid for non-handle var
    %2:ptr<push_constant, f32, read> = var @input_attachment_index(0)
                                       ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<push_constant, f32, read> = var @input_attachment_index(0)
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Var_Storage_UnexpectedInputAttachmentIndex) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* v = b.Var<storage, f32>();
        v->SetBindingPoint(0, 0);
        v->SetInputAttachmentIndex(0);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:40 error: var: '@input_attachment_index' is not valid for non-handle var
    %2:ptr<storage, f32, read_write> = var @binding_point(0, 0) @input_attachment_index(0)
                                       ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<storage, f32, read_write> = var @binding_point(0, 0) @input_attachment_index(0)
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Var_Uniform_UnexpectedInputAttachmentIndex) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* v = b.Var<uniform, f32>();
        v->SetBindingPoint(0, 0);
        v->SetInputAttachmentIndex(0);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:34 error: var: '@input_attachment_index' is not valid for non-handle var
    %2:ptr<uniform, f32, read> = var @binding_point(0, 0) @input_attachment_index(0)
                                 ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<uniform, f32, read> = var @binding_point(0, 0) @input_attachment_index(0)
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Var_Workgroup_UnexpectedInputAttachmentIndex) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* v = b.Var<workgroup, f32>();
        v->SetInputAttachmentIndex(0);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:42 error: var: '@input_attachment_index' is not valid for non-handle var
    %2:ptr<workgroup, f32, read_write> = var @input_attachment_index(0)
                                         ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<workgroup, f32, read_write> = var @input_attachment_index(0)
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Var_Init_WrongType) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* v = b.Var<function, f32>();
        v->SetInitializer(b.Constant(1_i));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:41 error: var: initializer type 'i32' does not match store type 'f32'
    %2:ptr<function, f32, read_write> = var, 1i
                                        ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, f32, read_write> = var, 1i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Var_HandleMissingBindingPoint) {
    auto* v = b.Var(ty.ptr<handle, i32>());
    mod.root_block->Append(v);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:2:31 error: var: resource variable missing binding points
  %1:ptr<handle, i32, read> = var
                              ^^^

:1:1 note: in block
$B1: {  # root
^^^

note: # Disassembly
$B1: {  # root
  %1:ptr<handle, i32, read> = var
}

)");
}

TEST_F(IR_ValidatorTest, Var_StorageMissingBindingPoint) {
    auto* v = b.Var(ty.ptr<storage, i32>());
    mod.root_block->Append(v);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:2:38 error: var: resource variable missing binding points
  %1:ptr<storage, i32, read_write> = var
                                     ^^^

:1:1 note: in block
$B1: {  # root
^^^

note: # Disassembly
$B1: {  # root
  %1:ptr<storage, i32, read_write> = var
}

)");
}

TEST_F(IR_ValidatorTest, Var_UniformMissingBindingPoint) {
    auto* v = b.Var(ty.ptr<uniform, i32>());
    mod.root_block->Append(v);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:2:32 error: var: resource variable missing binding points
  %1:ptr<uniform, i32, read> = var
                               ^^^

:1:1 note: in block
$B1: {  # root
^^^

note: # Disassembly
$B1: {  # root
  %1:ptr<uniform, i32, read> = var
}

)");
}

TEST_F(IR_ValidatorTest, Let_NullResult) {
    auto* v = mod.allocators.instructions.Create<ir::Let>(nullptr, b.Constant(1_i));

    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Append(v);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:5 error: let: result is undefined
    undef = let 1i
    ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    undef = let 1i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Let_NullValue) {
    auto* v = mod.allocators.instructions.Create<ir::Let>(b.InstructionResult(ty.f32()), nullptr);

    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Append(v);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:18 error: let: operand is undefined
    %2:f32 = let undef
                 ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:f32 = let undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Let_WrongType) {
    auto* v =
        mod.allocators.instructions.Create<ir::Let>(b.InstructionResult(ty.f32()), b.Constant(1_i));

    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Append(v);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:14 error: let: result type 'f32' does not match value type 'i32'
    %2:f32 = let 1i
             ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:f32 = let 1i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Instruction_AppendedDead) {
    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    auto* v = sb.Var(ty.ptr<function, f32>());
    auto* ret = sb.Return(f);

    v->Destroy();
    v->InsertBefore(ret);

    auto addr = tint::ToString(v);
    auto arrows = std::string(addr.length(), '^');

    std::string expected = R"(:3:5 error: var: destroyed instruction found in instruction list
    <destroyed tint::core::ir::Var $ADDRESS>
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^$ARROWS^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    <destroyed tint::core::ir::Var $ADDRESS>
    ret
  }
}
)";

    expected = tint::ReplaceAll(expected, "$ADDRESS", addr);
    expected = tint::ReplaceAll(expected, "$ARROWS", arrows);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), expected);
}

TEST_F(IR_ValidatorTest, Instruction_NullInstruction) {
    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    auto* v = sb.Var(ty.ptr<function, f32>());
    sb.Return(f);

    v->Result(0)->SetInstruction(nullptr);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:5 error: var: instruction of result is undefined
    %2:ptr<function, f32, read_write> = var
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, f32, read_write> = var
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Instruction_DeadOperand) {
    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    auto* v = sb.Var(ty.ptr<function, f32>());
    sb.Return(f);

    auto* result = sb.InstructionResult(ty.f32());
    result->Destroy();
    v->SetInitializer(result);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:46 error: var: operand is not alive
    %2:ptr<function, f32, read_write> = var, %3
                                             ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, f32, read_write> = var, %3
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Instruction_OperandUsageRemoved) {
    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    auto* v = sb.Var(ty.ptr<function, f32>());
    sb.Return(f);

    auto* result = sb.InstructionResult(ty.f32());
    v->SetInitializer(result);
    result->RemoveUsage({v, 0u});

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:46 error: var: operand missing usage
    %2:ptr<function, f32, read_write> = var, %3
                                             ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, f32, read_write> = var, %3
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Instruction_OrphanedInstruction) {
    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    auto* v = sb.Var(ty.ptr<function, f32>());
    auto* load = sb.Load(v);
    sb.Return(f);

    load->Remove();

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(error: load: orphaned instruction: load
note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, f32, read_write> = var
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Binary_LHS_Nullptr) {
    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Add(ty.i32(), nullptr, sb.Constant(2_i));
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:18 error: binary: operand is undefined
    %2:i32 = add undef, 2i
                 ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32 = add undef, 2i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Binary_RHS_Nullptr) {
    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Add(ty.i32(), sb.Constant(2_i), nullptr);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:22 error: binary: operand is undefined
    %2:i32 = add 2i, undef
                     ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32 = add 2i, undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Binary_Result_Nullptr) {
    auto* bin = mod.allocators.instructions.Create<ir::CoreBinary>(
        nullptr, BinaryOp::kAdd, b.Constant(3_i), b.Constant(2_i));

    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Append(bin);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:5 error: binary: result is undefined
    undef = add 3i, 2i
    ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    undef = add 3i, 2i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Unary_Value_Nullptr) {
    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Negation(ty.i32(), nullptr);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:23 error: unary: operand is undefined
    %2:i32 = negation undef
                      ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32 = negation undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Unary_Result_Nullptr) {
    auto* bin = mod.allocators.instructions.Create<ir::CoreUnary>(nullptr, UnaryOp::kNegation,
                                                                  b.Constant(2_i));

    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Append(bin);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:5 error: unary: result is undefined
    undef = negation 2i
    ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    undef = negation 2i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Unary_ResultTypeNotMatchValueType) {
    auto* bin = b.Complement(ty.f32(), 2_i);

    auto* f = b.Function("my_func", ty.void_());

    auto sb = b.Append(f->Block());
    sb.Append(bin);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:3:5 error: unary: result value type 'f32' does not match complement result type 'i32'
    %2:f32 = complement 2i
    ^^^^^^^^^^^^^^^^^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:f32 = complement 2i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitIf) {
    auto* if_ = b.If(true);
    if_->True()->Append(b.ExitIf(if_));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(if_);
    sb.Return(f);

    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, ExitIf_NullIf) {
    auto* if_ = b.If(true);
    if_->True()->Append(mod.allocators.instructions.Create<ExitIf>(nullptr));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(if_);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:5:9 error: exit_if: has no parent control instruction
        exit_if  # undef
        ^^^^^^^

:4:7 note: in block
      $B2: {  # true
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    if true [t: $B2] {  # if_1
      $B2: {  # true
        exit_if  # undef
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitIf_LessOperandsThenIfParams) {
    auto* if_ = b.If(true);
    if_->True()->Append(b.ExitIf(if_, 1_i));

    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    if_->SetResults(Vector{r1, r2});

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(if_);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: exit_if: provides 1 value but 'if' expects 2 values
        exit_if 1i  # if_1
        ^^^^^^^^^^

:4:7 note: in block
      $B2: {  # true
      ^^^

:3:5 note: 'if' declared here
    %2:i32, %3:f32 = if true [t: $B2] {  # if_1
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:f32 = if true [t: $B2] {  # if_1
      $B2: {  # true
        exit_if 1i  # if_1
      }
      # implicit false block: exit_if undef, undef
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitIf_MoreOperandsThenIfParams) {
    auto* if_ = b.If(true);
    if_->True()->Append(b.ExitIf(if_, 1_i, 2_f, 3_i));

    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    if_->SetResults(Vector{r1, r2});

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(if_);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: exit_if: provides 3 values but 'if' expects 2 values
        exit_if 1i, 2.0f, 3i  # if_1
        ^^^^^^^^^^^^^^^^^^^^

:4:7 note: in block
      $B2: {  # true
      ^^^

:3:5 note: 'if' declared here
    %2:i32, %3:f32 = if true [t: $B2] {  # if_1
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:f32 = if true [t: $B2] {  # if_1
      $B2: {  # true
        exit_if 1i, 2.0f, 3i  # if_1
      }
      # implicit false block: exit_if undef, undef
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitIf_WithResult) {
    auto* if_ = b.If(true);
    if_->True()->Append(b.ExitIf(if_, 1_i, 2_f));

    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    if_->SetResults(Vector{r1, r2});

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(if_);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, ExitIf_IncorrectResultType) {
    auto* if_ = b.If(true);
    if_->True()->Append(b.ExitIf(if_, 1_i, 2_i));

    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    if_->SetResults(Vector{r1, r2});

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(if_);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:21 error: exit_if: operand with type 'i32' does not match 'if' target type 'f32'
        exit_if 1i, 2i  # if_1
                    ^^

:4:7 note: in block
      $B2: {  # true
      ^^^

:3:13 note: %3 declared here
    %2:i32, %3:f32 = if true [t: $B2] {  # if_1
            ^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:f32 = if true [t: $B2] {  # if_1
      $B2: {  # true
        exit_if 1i, 2i  # if_1
      }
      # implicit false block: exit_if undef, undef
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitIf_NotInParentIf) {
    auto* f = b.Function("my_func", ty.void_());

    auto* if_ = b.If(true);
    if_->True()->Append(b.Return(f));

    auto sb = b.Append(f->Block());
    sb.Append(if_);
    sb.ExitIf(if_);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:5 error: exit_if: found outside all control instructions
    exit_if  # if_1
    ^^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    if true [t: $B2] {  # if_1
      $B2: {  # true
        ret
      }
    }
    exit_if  # if_1
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitIf_InvalidJumpsOverIf) {
    auto* f = b.Function("my_func", ty.void_());

    auto* if_inner = b.If(true);

    auto* if_outer = b.If(true);
    b.Append(if_outer->True(), [&] {
        b.Append(if_inner);
        b.ExitIf(if_outer);
    });

    b.Append(if_inner->True(), [&] { b.ExitIf(if_outer); });

    b.Append(f->Block(), [&] {
        b.Append(if_outer);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:13 error: exit_if: if target jumps over other control instructions
            exit_if  # if_1
            ^^^^^^^

:6:11 note: in block
          $B3: {  # true
          ^^^

:5:9 note: first control instruction jumped
        if true [t: $B3] {  # if_2
        ^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    if true [t: $B2] {  # if_1
      $B2: {  # true
        if true [t: $B3] {  # if_2
          $B3: {  # true
            exit_if  # if_1
          }
        }
        exit_if  # if_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitIf_InvalidJumpOverSwitch) {
    auto* f = b.Function("my_func", ty.void_());

    auto* switch_inner = b.Switch(1_i);

    auto* if_outer = b.If(true);
    b.Append(if_outer->True(), [&] {
        b.Append(switch_inner);
        b.ExitIf(if_outer);
    });

    auto* c = b.Case(switch_inner, {b.Constant(1_i), nullptr});
    b.Append(c, [&] { b.ExitIf(if_outer); });

    b.Append(f->Block(), [&] {
        b.Append(if_outer);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:13 error: exit_if: if target jumps over other control instructions
            exit_if  # if_1
            ^^^^^^^

:6:11 note: in block
          $B3: {  # case
          ^^^

:5:9 note: first control instruction jumped
        switch 1i [c: (1i default, $B3)] {  # switch_1
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    if true [t: $B2] {  # if_1
      $B2: {  # true
        switch 1i [c: (1i default, $B3)] {  # switch_1
          $B3: {  # case
            exit_if  # if_1
          }
        }
        exit_if  # if_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitIf_InvalidJumpOverLoop) {
    auto* f = b.Function("my_func", ty.void_());

    auto* loop = b.Loop();

    auto* if_outer = b.If(true);
    b.Append(if_outer->True(), [&] {
        b.Append(loop);
        b.ExitIf(if_outer);
    });

    b.Append(loop->Body(), [&] { b.ExitIf(if_outer); });

    b.Append(f->Block(), [&] {
        b.Append(if_outer);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:13 error: exit_if: if target jumps over other control instructions
            exit_if  # if_1
            ^^^^^^^

:6:11 note: in block
          $B3: {  # body
          ^^^

:5:9 note: first control instruction jumped
        loop [b: $B3] {  # loop_1
        ^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    if true [t: $B2] {  # if_1
      $B2: {  # true
        loop [b: $B3] {  # loop_1
          $B3: {  # body
            exit_if  # if_1
          }
        }
        exit_if  # if_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitSwitch) {
    auto* switch_ = b.Switch(1_i);

    auto* def = b.DefaultCase(switch_);
    def->Append(b.ExitSwitch(switch_));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(switch_);
    sb.Return(f);

    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, ExitSwitch_NullSwitch) {
    auto* switch_ = b.Switch(1_i);

    auto* def = b.DefaultCase(switch_);
    def->Append(mod.allocators.instructions.Create<ExitSwitch>(nullptr));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(switch_);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: exit_switch: has no parent control instruction
        exit_switch  # undef
        ^^^^^^^^^^^

:4:7 note: in block
      $B2: {  # case
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    switch 1i [c: (default, $B2)] {  # switch_1
      $B2: {  # case
        exit_switch  # undef
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitSwitch_LessOperandsThenSwitchParams) {
    auto* switch_ = b.Switch(1_i);

    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    switch_->SetResults(Vector{r1, r2});

    auto* def = b.DefaultCase(switch_);
    def->Append(b.ExitSwitch(switch_, 1_i));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(switch_);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: exit_switch: provides 1 value but 'switch' expects 2 values
        exit_switch 1i  # switch_1
        ^^^^^^^^^^^^^^

:4:7 note: in block
      $B2: {  # case
      ^^^

:3:5 note: 'switch' declared here
    %2:i32, %3:f32 = switch 1i [c: (default, $B2)] {  # switch_1
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:f32 = switch 1i [c: (default, $B2)] {  # switch_1
      $B2: {  # case
        exit_switch 1i  # switch_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitSwitch_MoreOperandsThenSwitchParams) {
    auto* switch_ = b.Switch(1_i);
    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    switch_->SetResults(Vector{r1, r2});

    auto* def = b.DefaultCase(switch_);
    def->Append(b.ExitSwitch(switch_, 1_i, 2_f, 3_i));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(switch_);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: exit_switch: provides 3 values but 'switch' expects 2 values
        exit_switch 1i, 2.0f, 3i  # switch_1
        ^^^^^^^^^^^^^^^^^^^^^^^^

:4:7 note: in block
      $B2: {  # case
      ^^^

:3:5 note: 'switch' declared here
    %2:i32, %3:f32 = switch 1i [c: (default, $B2)] {  # switch_1
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:f32 = switch 1i [c: (default, $B2)] {  # switch_1
      $B2: {  # case
        exit_switch 1i, 2.0f, 3i  # switch_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitSwitch_WithResult) {
    auto* switch_ = b.Switch(1_i);
    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    switch_->SetResults(Vector{r1, r2});

    auto* def = b.DefaultCase(switch_);
    def->Append(b.ExitSwitch(switch_, 1_i, 2_f));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(switch_);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, ExitSwitch_IncorrectResultType) {
    auto* switch_ = b.Switch(1_i);
    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    switch_->SetResults(Vector{r1, r2});

    auto* def = b.DefaultCase(switch_);
    def->Append(b.ExitSwitch(switch_, 1_i, 2_i));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(switch_);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:5:25 error: exit_switch: operand with type 'i32' does not match 'switch' target type 'f32'
        exit_switch 1i, 2i  # switch_1
                        ^^

:4:7 note: in block
      $B2: {  # case
      ^^^

:3:13 note: %3 declared here
    %2:i32, %3:f32 = switch 1i [c: (default, $B2)] {  # switch_1
            ^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:f32 = switch 1i [c: (default, $B2)] {  # switch_1
      $B2: {  # case
        exit_switch 1i, 2i  # switch_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitSwitch_NotInParentSwitch) {
    auto* switch_ = b.Switch(1_i);

    auto* f = b.Function("my_func", ty.void_());

    auto* def = b.DefaultCase(switch_);
    def->Append(b.Return(f));

    auto sb = b.Append(f->Block());
    sb.Append(switch_);

    auto* if_ = sb.Append(b.If(true));
    b.Append(if_->True(), [&] { b.ExitSwitch(switch_); });
    sb.Append(b.Return(f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:10:9 error: exit_switch: switch not found in parent control instructions
        exit_switch  # switch_1
        ^^^^^^^^^^^

:9:7 note: in block
      $B3: {  # true
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    switch 1i [c: (default, $B2)] {  # switch_1
      $B2: {  # case
        ret
      }
    }
    if true [t: $B3] {  # if_1
      $B3: {  # true
        exit_switch  # switch_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitSwitch_JumpsOverIfs) {
    // switch(true) {
    //   default: {
    //     if (true) {
    //      if (false) {
    //         break;
    //       }
    //     }
    //     break;
    //  }
    auto* switch_ = b.Switch(1_i);

    auto* f = b.Function("my_func", ty.void_());

    auto* def = b.DefaultCase(switch_);
    b.Append(def, [&] {
        auto* if_ = b.If(true);
        b.Append(if_->True(), [&] {
            auto* inner_if_ = b.If(false);
            b.Append(inner_if_->True(), [&] { b.ExitSwitch(switch_); });
            b.Return(f);
        });
        b.ExitSwitch(switch_);
    });

    auto sb = b.Append(f->Block());
    sb.Append(switch_);
    sb.Return(f);

    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, ExitSwitch_InvalidJumpOverSwitch) {
    auto* switch_ = b.Switch(1_i);

    auto* def = b.DefaultCase(switch_);
    b.Append(def, [&] {
        auto* inner = b.Switch(0_i);
        b.ExitSwitch(switch_);

        auto* inner_def = b.DefaultCase(inner);
        b.Append(inner_def, [&] { b.ExitSwitch(switch_); });
    });

    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(switch_);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:13 error: exit_switch: switch target jumps over other control instructions
            exit_switch  # switch_1
            ^^^^^^^^^^^

:6:11 note: in block
          $B3: {  # case
          ^^^

:5:9 note: first control instruction jumped
        switch 0i [c: (default, $B3)] {  # switch_2
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    switch 1i [c: (default, $B2)] {  # switch_1
      $B2: {  # case
        switch 0i [c: (default, $B3)] {  # switch_2
          $B3: {  # case
            exit_switch  # switch_1
          }
        }
        exit_switch  # switch_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitSwitch_InvalidJumpOverLoop) {
    auto* switch_ = b.Switch(1_i);

    auto* def = b.DefaultCase(switch_);
    b.Append(def, [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.ExitSwitch(switch_); });
        b.ExitSwitch(switch_);
    });

    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(switch_);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:13 error: exit_switch: switch target jumps over other control instructions
            exit_switch  # switch_1
            ^^^^^^^^^^^

:6:11 note: in block
          $B3: {  # body
          ^^^

:5:9 note: first control instruction jumped
        loop [b: $B3] {  # loop_1
        ^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    switch 1i [c: (default, $B2)] {  # switch_1
      $B2: {  # case
        loop [b: $B3] {  # loop_1
          $B3: {  # body
            exit_switch  # switch_1
          }
        }
        exit_switch  # switch_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Continue_OutsideOfLoop) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.Continue(loop);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:5 error: continue: called outside of associated loop
    continue  # -> $B3
    ^^^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2] {  # loop_1
      $B2: {  # body
        exit_loop  # loop_1
      }
    }
    continue  # -> $B3
  }
}
)");
}

TEST_F(IR_ValidatorTest, Continue_InLoopInit) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Initializer(), [&] { b.Continue(loop); });
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: continue: must only be called from loop body
        continue  # -> $B4
        ^^^^^^^^

:4:7 note: in block
      $B2: {  # initializer
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3] {  # loop_1
      $B2: {  # initializer
        continue  # -> $B4
      }
      $B3: {  # body
        exit_loop  # loop_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Continue_InLoopBody) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.Continue(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, Continue_InLoopContinuing) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.Append(loop->Continuing(), [&] { b.Continue(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:9 error: continue: must only be called from loop body
        continue  # -> $B3
        ^^^^^^^^

:7:7 note: in block
      $B3: {  # continuing
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        exit_loop  # loop_1
      }
      $B3: {  # continuing
        continue  # -> $B3
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Continue_UnexpectedValues) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.Continue(loop, 1_i, 2_f); });
        b.Append(loop->Continuing(), [&] { b.BreakIf(loop, true); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: continue: provides 2 values but 'loop' block $B3 expects 0 values
        continue 1i, 2.0f  # -> $B3
        ^^^^^^^^^^^^^^^^^

:4:7 note: in block
      $B2: {  # body
      ^^^

:7:7 note: 'loop' block $B3 declared here
      $B3: {  # continuing
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        continue 1i, 2.0f  # -> $B3
      }
      $B3: {  # continuing
        break_if true  # -> [t: exit_loop loop_1, f: $B2]
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Continue_MissingValues) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->Continuing()->SetParams({b.BlockParam<i32>(), b.BlockParam<i32>()});
        b.Append(loop->Body(), [&] { b.Continue(loop); });
        b.Append(loop->Continuing(), [&] { b.BreakIf(loop, true); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: continue: provides 0 values but 'loop' block $B3 expects 2 values
        continue  # -> $B3
        ^^^^^^^^

:4:7 note: in block
      $B2: {  # body
      ^^^

:7:7 note: 'loop' block $B3 declared here
      $B3 (%2:i32, %3:i32): {  # continuing
      ^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        continue  # -> $B3
      }
      $B3 (%2:i32, %3:i32): {  # continuing
        break_if true  # -> [t: exit_loop loop_1, f: $B2]
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Continue_MismatchedTypes) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->Continuing()->SetParams(
            {b.BlockParam<i32>(), b.BlockParam<f32>(), b.BlockParam<u32>(), b.BlockParam<bool>()});
        b.Append(loop->Body(), [&] { b.Continue(loop, 1_i, 2_i, 3_f, false); });
        b.Append(loop->Continuing(), [&] { b.BreakIf(loop, true); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:5:22 error: continue: operand with type 'i32' does not match 'loop' block $B3 target type 'f32'
        continue 1i, 2i, 3.0f, false  # -> $B3
                     ^^

:4:7 note: in block
      $B2: {  # body
      ^^^

:7:20 note: %3 declared here
      $B3 (%2:i32, %3:f32, %4:u32, %5:bool): {  # continuing
                   ^^

:5:26 error: continue: operand with type 'f32' does not match 'loop' block $B3 target type 'u32'
        continue 1i, 2i, 3.0f, false  # -> $B3
                         ^^^^

:4:7 note: in block
      $B2: {  # body
      ^^^

:7:28 note: %4 declared here
      $B3 (%2:i32, %3:f32, %4:u32, %5:bool): {  # continuing
                           ^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        continue 1i, 2i, 3.0f, false  # -> $B3
      }
      $B3 (%2:i32, %3:f32, %4:u32, %5:bool): {  # continuing
        break_if true  # -> [t: exit_loop loop_1, f: $B2]
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Continue_MatchedTypes) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->Continuing()->SetParams(
            {b.BlockParam<i32>(), b.BlockParam<f32>(), b.BlockParam<u32>(), b.BlockParam<bool>()});
        b.Append(loop->Body(), [&] { b.Continue(loop, 1_i, 2_f, 3_u, false); });
        b.Append(loop->Continuing(), [&] { b.BreakIf(loop, true); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, NextIteration_OutsideOfLoop) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.NextIteration(loop);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:5 error: next_iteration: called outside of associated loop
    next_iteration  # -> $B2
    ^^^^^^^^^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2] {  # loop_1
      $B2: {  # body
        exit_loop  # loop_1
      }
    }
    next_iteration  # -> $B2
  }
}
)");
}

TEST_F(IR_ValidatorTest, NextIteration_InLoopInit) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Initializer(), [&] { b.NextIteration(loop); });
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, NextIteration_InLoopBody) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.NextIteration(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: next_iteration: must only be called from loop initializer or continuing
        next_iteration  # -> $B2
        ^^^^^^^^^^^^^^

:4:7 note: in block
      $B2: {  # body
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2] {  # loop_1
      $B2: {  # body
        next_iteration  # -> $B2
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, NextIteration_InLoopContinuing) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.Append(loop->Continuing(), [&] { b.NextIteration(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, NextIteration_UnexpectedValues) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Initializer(), [&] { b.NextIteration(loop, 1_i, 2_f); });
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: next_iteration: provides 2 values but 'loop' block $B3 expects 0 values
        next_iteration 1i, 2.0f  # -> $B3
        ^^^^^^^^^^^^^^^^^^^^^^^

:4:7 note: in block
      $B2: {  # initializer
      ^^^

:7:7 note: 'loop' block $B3 declared here
      $B3: {  # body
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3] {  # loop_1
      $B2: {  # initializer
        next_iteration 1i, 2.0f  # -> $B3
      }
      $B3: {  # body
        exit_loop  # loop_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, NextIteration_MissingValues) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->Body()->SetParams({b.BlockParam<i32>(), b.BlockParam<i32>()});
        b.Append(loop->Initializer(), [&] { b.NextIteration(loop); });
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: next_iteration: provides 0 values but 'loop' block $B3 expects 2 values
        next_iteration  # -> $B3
        ^^^^^^^^^^^^^^

:4:7 note: in block
      $B2: {  # initializer
      ^^^

:7:7 note: 'loop' block $B3 declared here
      $B3 (%2:i32, %3:i32): {  # body
      ^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3] {  # loop_1
      $B2: {  # initializer
        next_iteration  # -> $B3
      }
      $B3 (%2:i32, %3:i32): {  # body
        exit_loop  # loop_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, NextIteration_MismatchedTypes) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->Body()->SetParams(
            {b.BlockParam<i32>(), b.BlockParam<f32>(), b.BlockParam<u32>(), b.BlockParam<bool>()});
        b.Append(loop->Initializer(), [&] { b.NextIteration(loop, 1_i, 2_i, 3_f, false); });
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:5:28 error: next_iteration: operand with type 'i32' does not match 'loop' block $B3 target type 'f32'
        next_iteration 1i, 2i, 3.0f, false  # -> $B3
                           ^^

:4:7 note: in block
      $B2: {  # initializer
      ^^^

:7:20 note: %3 declared here
      $B3 (%2:i32, %3:f32, %4:u32, %5:bool): {  # body
                   ^^

:5:32 error: next_iteration: operand with type 'f32' does not match 'loop' block $B3 target type 'u32'
        next_iteration 1i, 2i, 3.0f, false  # -> $B3
                               ^^^^

:4:7 note: in block
      $B2: {  # initializer
      ^^^

:7:28 note: %4 declared here
      $B3 (%2:i32, %3:f32, %4:u32, %5:bool): {  # body
                           ^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3] {  # loop_1
      $B2: {  # initializer
        next_iteration 1i, 2i, 3.0f, false  # -> $B3
      }
      $B3 (%2:i32, %3:f32, %4:u32, %5:bool): {  # body
        exit_loop  # loop_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, NextIteration_MatchedTypes) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->Body()->SetParams(
            {b.BlockParam<i32>(), b.BlockParam<f32>(), b.BlockParam<u32>(), b.BlockParam<bool>()});
        b.Append(loop->Initializer(), [&] { b.NextIteration(loop, 1_i, 2_f, 3_u, false); });
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, LoopBodyParamsWithoutInitializer) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->Body()->SetParams({b.BlockParam<i32>(), b.BlockParam<i32>()});
        b.Append(loop->Body(), [&] { b.ExitLoop(loop); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:5 error: loop: loop with body block parameters must have an initializer
    loop [b: $B2] {  # loop_1
    ^^^^^^^^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2] {  # loop_1
      $B2 (%2:i32, %3:i32): {  # body
        exit_loop  # loop_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ContinuingUseValueBeforeContinue) {
    auto* f = b.Function("my_func", ty.void_());
    auto* value = b.Let("value", 1_i);
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] {
            b.Append(value);
            b.Append(b.If(true)->True(), [&] { b.Continue(loop); });
            b.ExitLoop(loop);
        });
        b.Append(loop->Continuing(), [&] {
            b.Let("use", value);
            b.NextIteration(loop);
        });
        b.Return(f);
    });

    ASSERT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, ContinuingUseValueAfterContinue) {
    auto* f = b.Function("my_func", ty.void_());
    auto* value = b.Let("value", 1_i);
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] {
            b.Append(b.If(true)->True(), [&] { b.Continue(loop); });
            b.Append(value);
            b.ExitLoop(loop);
        });
        b.Append(loop->Continuing(), [&] {
            b.Let("use", value);
            b.NextIteration(loop);
        });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:14:24 error: let: %value cannot be used in continuing block as it is declared after the first 'continue' in the loop's body
        %use:i32 = let %value
                       ^^^^^^

:4:7 note: in block
      $B2: {  # body
      ^^^

:10:9 note: %value declared here
        %value:i32 = let 1i
        ^^^^^^^^^^

:7:13 note: loop body's first 'continue'
            continue  # -> $B3
            ^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        if true [t: $B4] {  # if_1
          $B4: {  # true
            continue  # -> $B3
          }
        }
        %value:i32 = let 1i
        exit_loop  # loop_1
      }
      $B3: {  # continuing
        %use:i32 = let %value
        next_iteration  # -> $B2
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, BreakIf_NextIterUnexpectedValues) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.Continue(loop); });
        b.Append(loop->Continuing(), [&] { b.BreakIf(loop, true, b.Values(1_i, 2_i), Empty); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:9 error: break_if: provides 2 values but 'loop' block $B2 expects 0 values
        break_if true next_iteration: [ 1i, 2i ]  # -> [t: exit_loop loop_1, f: $B2]
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:7:7 note: in block
      $B3: {  # continuing
      ^^^

:4:7 note: 'loop' block $B2 declared here
      $B2: {  # body
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        continue  # -> $B3
      }
      $B3: {  # continuing
        break_if true next_iteration: [ 1i, 2i ]  # -> [t: exit_loop loop_1, f: $B2]
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, BreakIf_NextIterMissingValues) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->Body()->SetParams({b.BlockParam<i32>(), b.BlockParam<i32>()});
        b.Append(loop->Initializer(), [&] { b.NextIteration(loop, nullptr, nullptr); });
        b.Append(loop->Body(), [&] { b.Continue(loop); });
        b.Append(loop->Continuing(), [&] { b.BreakIf(loop, true, Empty, Empty); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:11:9 error: break_if: provides 0 values but 'loop' block $B3 expects 2 values
        break_if true  # -> [t: exit_loop loop_1, f: $B3]
        ^^^^^^^^^^^^^

:10:7 note: in block
      $B4: {  # continuing
      ^^^

:7:7 note: 'loop' block $B3 declared here
      $B3 (%2:i32, %3:i32): {  # body
      ^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3, c: $B4] {  # loop_1
      $B2: {  # initializer
        next_iteration undef, undef  # -> $B3
      }
      $B3 (%2:i32, %3:i32): {  # body
        continue  # -> $B4
      }
      $B4: {  # continuing
        break_if true  # -> [t: exit_loop loop_1, f: $B3]
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, BreakIf_NextIterMismatchedTypes) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->Body()->SetParams(
            {b.BlockParam<i32>(), b.BlockParam<f32>(), b.BlockParam<u32>(), b.BlockParam<bool>()});
        b.Append(loop->Initializer(),
                 [&] { b.NextIteration(loop, nullptr, nullptr, nullptr, nullptr); });
        b.Append(loop->Body(), [&] { b.Continue(loop); });
        b.Append(loop->Continuing(),
                 [&] { b.BreakIf(loop, true, b.Values(1_i, 2_i, 3_f, false), Empty); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:11:45 error: break_if: operand with type 'i32' does not match 'loop' block $B3 target type 'f32'
        break_if true next_iteration: [ 1i, 2i, 3.0f, false ]  # -> [t: exit_loop loop_1, f: $B3]
                                            ^^

:10:7 note: in block
      $B4: {  # continuing
      ^^^

:7:20 note: %3 declared here
      $B3 (%2:i32, %3:f32, %4:u32, %5:bool): {  # body
                   ^^

:11:49 error: break_if: operand with type 'f32' does not match 'loop' block $B3 target type 'u32'
        break_if true next_iteration: [ 1i, 2i, 3.0f, false ]  # -> [t: exit_loop loop_1, f: $B3]
                                                ^^^^

:10:7 note: in block
      $B4: {  # continuing
      ^^^

:7:28 note: %4 declared here
      $B3 (%2:i32, %3:f32, %4:u32, %5:bool): {  # body
                           ^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3, c: $B4] {  # loop_1
      $B2: {  # initializer
        next_iteration undef, undef, undef, undef  # -> $B3
      }
      $B3 (%2:i32, %3:f32, %4:u32, %5:bool): {  # body
        continue  # -> $B4
      }
      $B4: {  # continuing
        break_if true next_iteration: [ 1i, 2i, 3.0f, false ]  # -> [t: exit_loop loop_1, f: $B3]
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, BreakIf_NextIterMatchedTypes) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->Body()->SetParams(
            {b.BlockParam<i32>(), b.BlockParam<f32>(), b.BlockParam<u32>(), b.BlockParam<bool>()});
        b.Append(loop->Initializer(),
                 [&] { b.NextIteration(loop, nullptr, nullptr, nullptr, nullptr); });
        b.Append(loop->Body(), [&] { b.Continue(loop); });
        b.Append(loop->Continuing(),
                 [&] { b.BreakIf(loop, true, b.Values(1_i, 2_f, 3_u, false), Empty); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, BreakIf_ExitUnexpectedValues) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.Continue(loop); });
        b.Append(loop->Continuing(), [&] { b.BreakIf(loop, true, Empty, b.Values(1_i, 2_i)); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:9 error: break_if: provides 2 values but 'loop' expects 0 values
        break_if true exit_loop: [ 1i, 2i ]  # -> [t: exit_loop loop_1, f: $B2]
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:7:7 note: in block
      $B3: {  # continuing
      ^^^

:3:5 note: 'loop' declared here
    loop [b: $B2, c: $B3] {  # loop_1
    ^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        continue  # -> $B3
      }
      $B3: {  # continuing
        break_if true exit_loop: [ 1i, 2i ]  # -> [t: exit_loop loop_1, f: $B2]
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, BreakIf_ExitMissingValues) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->SetResults(b.InstructionResult<i32>(), b.InstructionResult<i32>());
        b.Append(loop->Body(), [&] { b.Continue(loop); });
        b.Append(loop->Continuing(), [&] { b.BreakIf(loop, true, Empty, Empty); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:9 error: break_if: provides 0 values but 'loop' expects 2 values
        break_if true  # -> [t: exit_loop loop_1, f: $B2]
        ^^^^^^^^^^^^^

:7:7 note: in block
      $B3: {  # continuing
      ^^^

:3:5 note: 'loop' declared here
    %2:i32, %3:i32 = loop [b: $B2, c: $B3] {  # loop_1
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:i32 = loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        continue  # -> $B3
      }
      $B3: {  # continuing
        break_if true  # -> [t: exit_loop loop_1, f: $B2]
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, BreakIf_ExitMismatchedTypes) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->SetResults(b.InstructionResult<i32>(), b.InstructionResult<f32>(),
                         b.InstructionResult<u32>(), b.InstructionResult<bool>());
        b.Append(loop->Body(), [&] { b.Continue(loop); });
        b.Append(loop->Continuing(),
                 [&] { b.BreakIf(loop, true, Empty, b.Values(1_i, 2_i, 3_f, false)); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:8:40 error: break_if: operand with type 'i32' does not match 'loop' target type 'f32'
        break_if true exit_loop: [ 1i, 2i, 3.0f, false ]  # -> [t: exit_loop loop_1, f: $B2]
                                       ^^

:7:7 note: in block
      $B3: {  # continuing
      ^^^

:3:13 note: %3 declared here
    %2:i32, %3:f32, %4:u32, %5:bool = loop [b: $B2, c: $B3] {  # loop_1
            ^^^^^^

:8:44 error: break_if: operand with type 'f32' does not match 'loop' target type 'u32'
        break_if true exit_loop: [ 1i, 2i, 3.0f, false ]  # -> [t: exit_loop loop_1, f: $B2]
                                           ^^^^

:7:7 note: in block
      $B3: {  # continuing
      ^^^

:3:21 note: %4 declared here
    %2:i32, %3:f32, %4:u32, %5:bool = loop [b: $B2, c: $B3] {  # loop_1
                    ^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:f32, %4:u32, %5:bool = loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        continue  # -> $B3
      }
      $B3: {  # continuing
        break_if true exit_loop: [ 1i, 2i, 3.0f, false ]  # -> [t: exit_loop loop_1, f: $B2]
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, BreakIf_ExitMatchedTypes) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {
        auto* loop = b.Loop();
        loop->SetResults(b.InstructionResult<i32>(), b.InstructionResult<f32>(),
                         b.InstructionResult<u32>(), b.InstructionResult<bool>());
        b.Append(loop->Body(), [&] { b.Continue(loop); });
        b.Append(loop->Continuing(),
                 [&] { b.BreakIf(loop, true, Empty, b.Values(1_i, 2_f, 3_u, false)); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, ExitLoop) {
    auto* loop = b.Loop();
    loop->Continuing()->Append(b.NextIteration(loop));
    loop->Body()->Append(b.ExitLoop(loop));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(loop);
    sb.Return(f);

    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, ExitLoop_NullLoop) {
    auto* loop = b.Loop();
    loop->Continuing()->Append(b.NextIteration(loop));
    loop->Body()->Append(mod.allocators.instructions.Create<ExitLoop>(nullptr));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(loop);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: exit_loop: has no parent control instruction
        exit_loop  # undef
        ^^^^^^^^^

:4:7 note: in block
      $B2: {  # body
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        exit_loop  # undef
      }
      $B3: {  # continuing
        next_iteration  # -> $B2
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitLoop_LessOperandsThenLoopParams) {
    auto* loop = b.Loop();
    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    loop->SetResults(Vector{r1, r2});

    loop->Continuing()->Append(b.NextIteration(loop));
    loop->Body()->Append(b.ExitLoop(loop, 1_i));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(loop);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: exit_loop: provides 1 value but 'loop' expects 2 values
        exit_loop 1i  # loop_1
        ^^^^^^^^^^^^

:4:7 note: in block
      $B2: {  # body
      ^^^

:3:5 note: 'loop' declared here
    %2:i32, %3:f32 = loop [b: $B2, c: $B3] {  # loop_1
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:f32 = loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        exit_loop 1i  # loop_1
      }
      $B3: {  # continuing
        next_iteration  # -> $B2
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitLoop_MoreOperandsThenLoopParams) {
    auto* loop = b.Loop();
    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    loop->SetResults(Vector{r1, r2});

    loop->Continuing()->Append(b.NextIteration(loop));
    loop->Body()->Append(b.ExitLoop(loop, 1_i, 2_f, 3_i));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(loop);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: exit_loop: provides 3 values but 'loop' expects 2 values
        exit_loop 1i, 2.0f, 3i  # loop_1
        ^^^^^^^^^^^^^^^^^^^^^^

:4:7 note: in block
      $B2: {  # body
      ^^^

:3:5 note: 'loop' declared here
    %2:i32, %3:f32 = loop [b: $B2, c: $B3] {  # loop_1
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:f32 = loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        exit_loop 1i, 2.0f, 3i  # loop_1
      }
      $B3: {  # continuing
        next_iteration  # -> $B2
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitLoop_WithResult) {
    auto* loop = b.Loop();
    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    loop->SetResults(Vector{r1, r2});

    loop->Continuing()->Append(b.NextIteration(loop));
    loop->Body()->Append(b.ExitLoop(loop, 1_i, 2_f));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(loop);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_EQ(res, Success);
}

TEST_F(IR_ValidatorTest, ExitLoop_IncorrectResultType) {
    auto* loop = b.Loop();
    auto* r1 = b.InstructionResult(ty.i32());
    auto* r2 = b.InstructionResult(ty.f32());
    loop->SetResults(Vector{r1, r2});

    loop->Continuing()->Append(b.NextIteration(loop));
    loop->Body()->Append(b.ExitLoop(loop, 1_i, 2_i));

    auto* f = b.Function("my_func", ty.void_());
    auto sb = b.Append(f->Block());
    sb.Append(loop);
    sb.Return(f);

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:5:23 error: exit_loop: operand with type 'i32' does not match 'loop' target type 'f32'
        exit_loop 1i, 2i  # loop_1
                      ^^

:4:7 note: in block
      $B2: {  # body
      ^^^

:3:13 note: %3 declared here
    %2:i32, %3:f32 = loop [b: $B2, c: $B3] {  # loop_1
            ^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32, %3:f32 = loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        exit_loop 1i, 2i  # loop_1
      }
      $B3: {  # continuing
        next_iteration  # -> $B2
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitLoop_NotInParentLoop) {
    auto* f = b.Function("my_func", ty.void_());

    auto* loop = b.Loop();
    loop->Continuing()->Append(b.NextIteration(loop));
    loop->Body()->Append(b.Return(f));

    auto sb = b.Append(f->Block());
    sb.Append(loop);

    auto* if_ = sb.Append(b.If(true));
    b.Append(if_->True(), [&] { b.ExitLoop(loop); });
    sb.Append(b.Return(f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:13:9 error: exit_loop: loop not found in parent control instructions
        exit_loop  # loop_1
        ^^^^^^^^^

:12:7 note: in block
      $B4: {  # true
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        ret
      }
      $B3: {  # continuing
        next_iteration  # -> $B2
      }
    }
    if true [t: $B4] {  # if_1
      $B4: {  # true
        exit_loop  # loop_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitLoop_JumpsOverIfs) {
    // loop {
    //   if (true) {
    //    if (false) {
    //       break;
    //     }
    //   }
    //   break;
    // }
    auto* loop = b.Loop();
    loop->Continuing()->Append(b.NextIteration(loop));

    auto* f = b.Function("my_func", ty.void_());

    b.Append(loop->Body(), [&] {
        auto* if_ = b.If(true);
        b.Append(if_->True(), [&] {
            auto* inner_if_ = b.If(false);
            b.Append(inner_if_->True(), [&] { b.ExitLoop(loop); });
            b.Return(f);
        });
        b.ExitLoop(loop);
    });

    auto sb = b.Append(f->Block());
    sb.Append(loop);
    sb.Return(f);

    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, ExitLoop_InvalidJumpOverSwitch) {
    auto* loop = b.Loop();
    loop->Continuing()->Append(b.NextIteration(loop));

    b.Append(loop->Body(), [&] {
        auto* inner = b.Switch(1_i);
        b.ExitLoop(loop);

        auto* inner_def = b.DefaultCase(inner);
        b.Append(inner_def, [&] { b.ExitLoop(loop); });
    });

    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(loop);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:13 error: exit_loop: loop target jumps over other control instructions
            exit_loop  # loop_1
            ^^^^^^^^^

:6:11 note: in block
          $B4: {  # case
          ^^^

:5:9 note: first control instruction jumped
        switch 1i [c: (default, $B4)] {  # switch_1
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        switch 1i [c: (default, $B4)] {  # switch_1
          $B4: {  # case
            exit_loop  # loop_1
          }
        }
        exit_loop  # loop_1
      }
      $B3: {  # continuing
        next_iteration  # -> $B2
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitLoop_InvalidJumpOverLoop) {
    auto* outer_loop = b.Loop();

    outer_loop->Continuing()->Append(b.NextIteration(outer_loop));

    b.Append(outer_loop->Body(), [&] {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] { b.ExitLoop(outer_loop); });
        b.ExitLoop(outer_loop);
    });

    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(outer_loop);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:13 error: exit_loop: loop target jumps over other control instructions
            exit_loop  # loop_1
            ^^^^^^^^^

:6:11 note: in block
          $B4: {  # body
          ^^^

:5:9 note: first control instruction jumped
        loop [b: $B4] {  # loop_2
        ^^^^^^^^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        loop [b: $B4] {  # loop_2
          $B4: {  # body
            exit_loop  # loop_1
          }
        }
        exit_loop  # loop_1
      }
      $B3: {  # continuing
        next_iteration  # -> $B2
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitLoop_InvalidInsideContinuing) {
    auto* loop = b.Loop();

    loop->Continuing()->Append(b.ExitLoop(loop));
    loop->Body()->Append(b.Continue(loop));

    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(loop);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:8:9 error: exit_loop: loop exit jumps out of continuing block
        exit_loop  # loop_1
        ^^^^^^^^^

:7:7 note: in block
      $B3: {  # continuing
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        continue  # -> $B3
      }
      $B3: {  # continuing
        exit_loop  # loop_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitLoop_InvalidInsideContinuingNested) {
    auto* loop = b.Loop();

    b.Append(loop->Continuing(), [&]() {
        auto* if_ = b.If(true);
        b.Append(if_->True(), [&]() { b.ExitLoop(loop); });
        b.NextIteration(loop);
    });

    b.Append(loop->Body(), [&] { b.Continue(loop); });

    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(loop);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:10:13 error: exit_loop: loop exit jumps out of continuing block
            exit_loop  # loop_1
            ^^^^^^^^^

:9:11 note: in block
          $B4: {  # true
          ^^^

:7:7 note: in continuing block
      $B3: {  # continuing
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [b: $B2, c: $B3] {  # loop_1
      $B2: {  # body
        continue  # -> $B3
      }
      $B3: {  # continuing
        if true [t: $B4] {  # if_1
          $B4: {  # true
            exit_loop  # loop_1
          }
        }
        next_iteration  # -> $B2
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitLoop_InvalidInsideInitializer) {
    auto* loop = b.Loop();

    loop->Initializer()->Append(b.ExitLoop(loop));
    loop->Continuing()->Append(b.NextIteration(loop));

    b.Append(loop->Body(), [&] { b.Continue(loop); });

    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(loop);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:9 error: exit_loop: loop exit not permitted in loop initializer
        exit_loop  # loop_1
        ^^^^^^^^^

:4:7 note: in block
      $B2: {  # initializer
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3, c: $B4] {  # loop_1
      $B2: {  # initializer
        exit_loop  # loop_1
      }
      $B3: {  # body
        continue  # -> $B4
      }
      $B4: {  # continuing
        next_iteration  # -> $B3
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, ExitLoop_InvalidInsideInitializerNested) {
    auto* loop = b.Loop();

    b.Append(loop->Initializer(), [&]() {
        auto* if_ = b.If(true);
        b.Append(if_->True(), [&]() { b.ExitLoop(loop); });
        b.NextIteration(loop);
    });
    loop->Continuing()->Append(b.NextIteration(loop));

    b.Append(loop->Body(), [&] { b.Continue(loop); });

    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(loop);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:7:13 error: exit_loop: loop exit not permitted in loop initializer
            exit_loop  # loop_1
            ^^^^^^^^^

:6:11 note: in block
          $B5: {  # true
          ^^^

:4:7 note: in initializer block
      $B2: {  # initializer
      ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    loop [i: $B2, b: $B3, c: $B4] {  # loop_1
      $B2: {  # initializer
        if true [t: $B5] {  # if_1
          $B5: {  # true
            exit_loop  # loop_1
          }
        }
        next_iteration  # -> $B3
      }
      $B3: {  # body
        continue  # -> $B4
      }
      $B4: {  # continuing
        next_iteration  # -> $B3
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Return) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {  //
        b.Return(f);
    });

    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, Return_WithValue) {
    auto* f = b.Function("my_func", ty.i32());
    b.Append(f->Block(), [&] {  //
        b.Return(f, 42_i);
    });

    EXPECT_EQ(ir::Validate(mod), Success);
}

TEST_F(IR_ValidatorTest, Return_NullFunction) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {  //
        b.Return(nullptr);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:5 error: return: undefined function
    ret
    ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Return_UnexpectedValue) {
    auto* f = b.Function("my_func", ty.void_());
    b.Append(f->Block(), [&] {  //
        b.Return(f, 42_i);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:5 error: return: unexpected return value
    ret 42i
    ^^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    ret 42i
  }
}
)");
}

TEST_F(IR_ValidatorTest, Return_MissingValue) {
    auto* f = b.Function("my_func", ty.i32());
    b.Append(f->Block(), [&] {  //
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:5 error: return: expected return value
    ret
    ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():i32 {
  $B1: {
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Return_WrongValueType) {
    auto* f = b.Function("my_func", ty.i32());
    b.Append(f->Block(), [&] {  //
        b.Return(f, 42_f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(
        res.Failure().reason.Str(),
        R"(:3:5 error: return: return value type 'f32' does not match function return type 'i32'
    ret 42.0f
    ^^^^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():i32 {
  $B1: {
    ret 42.0f
  }
}
)");
}

TEST_F(IR_ValidatorTest, Load_NullFrom) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(
            mod.allocators.instructions.Create<ir::Load>(b.InstructionResult(ty.i32()), nullptr));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:19 error: load: operand is undefined
    %2:i32 = load undef
                  ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32 = load undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Load_SourceNotMemoryView) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* let = b.Let("l", 1_i);
        b.Append(mod.allocators.instructions.Create<ir::Load>(b.InstructionResult(ty.f32()),
                                                              let->Result(0)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:4:19 error: load: load source operand is not a memory view
    %3:f32 = load %l
                  ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %l:i32 = let 1i
    %3:f32 = load %l
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Load_TypeMismatch) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* var = b.Var(ty.ptr<function, i32>());
        b.Append(mod.allocators.instructions.Create<ir::Load>(b.InstructionResult(ty.f32()),
                                                              var->Result(0)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:4:19 error: load: result type 'f32' does not match source store type 'i32'
    %3:f32 = load %2
                  ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, i32, read_write> = var
    %3:f32 = load %2
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Load_MissingResult) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* var = b.Var(ty.ptr<function, i32>());
        auto* load = mod.allocators.instructions.Create<ir::Load>(nullptr, var->Result(0));
        load->ClearResults();
        b.Append(load);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:4:13 error: load: expected exactly 1 results, got 0
    undef = load %2
            ^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, i32, read_write> = var
    undef = load %2
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Store_NullTo) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(mod.allocators.instructions.Create<ir::Store>(nullptr, b.Constant(42_i)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:11 error: store: operand is undefined
    store undef, 42i
          ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    store undef, 42i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Store_NullFrom) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* var = b.Var(ty.ptr<function, i32>());
        b.Append(mod.allocators.instructions.Create<ir::Store>(var->Result(0), nullptr));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:4:15 error: store: operand is undefined
    store %2, undef
              ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, i32, read_write> = var
    store %2, undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Store_NullToAndFrom) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(mod.allocators.instructions.Create<ir::Store>(nullptr, nullptr));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:11 error: store: operand is undefined
    store undef, undef
          ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

:3:18 error: store: operand is undefined
    store undef, undef
                 ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    store undef, undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Store_NonEmptyResult) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* var = b.Var(ty.ptr<function, i32>());
        auto* store =
            mod.allocators.instructions.Create<ir::Store>(var->Result(0), b.Constant(42_i));
        store->SetResults(Vector{b.InstructionResult(ty.i32())});
        b.Append(store);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:4:5 error: store: expected exactly 0 results, got 1
    store %2, 42i
    ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, i32, read_write> = var
    store %2, 42i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Store_TargetNotMemoryView) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* let = b.Let("l", 1_i);
        b.Append(mod.allocators.instructions.Create<ir::Store>(let->Result(0), b.Constant(42_u)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:4:11 error: store: store target operand is not a memory view
    store %l, 42u
          ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %l:i32 = let 1i
    store %l, 42u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Store_TypeMismatch) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* var = b.Var(ty.ptr<function, i32>());
        b.Append(mod.allocators.instructions.Create<ir::Store>(var->Result(0), b.Constant(42_u)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:4:15 error: store: value type 'u32' does not match store type 'i32'
    store %2, 42u
              ^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, i32, read_write> = var
    store %2, 42u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Store_NoStoreType) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* result = b.InstructionResult(ty.u32());
        result->SetType(nullptr);
        b.Append(mod.allocators.instructions.Create<ir::Store>(result, b.Constant(42_u)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:11 error: store: %2 is not in scope
    store %2, 42u
          ^^

:2:3 note: in block
  $B1: {
  ^^^

:3:11 error: store: store target operand is not a memory view
    store %2, 42u
          ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    store %2, 42u
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Store_NoValueType) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* var = b.Var(ty.ptr<function, i32>());
        auto* val = b.Construct(ty.u32(), 42_u);
        val->Result(0)->SetType(nullptr);

        b.Append(mod.allocators.instructions.Create<ir::Store>(var->Result(0), val->Result(0)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:5:15 error: store: value type must not be null
    store %2, %3
              ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, i32, read_write> = var
    %3:null = construct 42u
    store %2, %3
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, LoadVectorElement_NullResult) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* var = b.Var(ty.ptr<function, vec3<f32>>());
        b.Append(mod.allocators.instructions.Create<ir::LoadVectorElement>(nullptr, var->Result(0),
                                                                           b.Constant(1_i)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:4:5 error: load_vector_element: result is undefined
    undef = load_vector_element %2, 1i
    ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, vec3<f32>, read_write> = var
    undef = load_vector_element %2, 1i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, LoadVectorElement_NullFrom) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(mod.allocators.instructions.Create<ir::LoadVectorElement>(
            b.InstructionResult(ty.f32()), nullptr, b.Constant(1_i)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:34 error: load_vector_element: operand is undefined
    %2:f32 = load_vector_element undef, 1i
                                 ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:f32 = load_vector_element undef, 1i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, LoadVectorElement_NullIndex) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* var = b.Var(ty.ptr<function, vec3<f32>>());
        b.Append(mod.allocators.instructions.Create<ir::LoadVectorElement>(
            b.InstructionResult(ty.f32()), var->Result(0), nullptr));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:4:38 error: load_vector_element: operand is undefined
    %3:f32 = load_vector_element %2, undef
                                     ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, vec3<f32>, read_write> = var
    %3:f32 = load_vector_element %2, undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, StoreVectorElement_NullTo) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Append(mod.allocators.instructions.Create<ir::StoreVectorElement>(
            nullptr, b.Constant(1_i), b.Constant(2_i)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:3:26 error: store_vector_element: operand is undefined
    store_vector_element undef, 1i, 2i
                         ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    store_vector_element undef, 1i, 2i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, StoreVectorElement_NullIndex) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* var = b.Var(ty.ptr<function, vec3<f32>>());
        b.Append(mod.allocators.instructions.Create<ir::StoreVectorElement>(var->Result(0), nullptr,
                                                                            b.Constant(2_i)));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:4:30 error: store_vector_element: operand is undefined
    store_vector_element %2, undef, 2i
                             ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

:4:37 error: store_vector_element: value type 'i32' does not match vector pointer element type 'f32'
    store_vector_element %2, undef, 2i
                                    ^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, vec3<f32>, read_write> = var
    store_vector_element %2, undef, 2i
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, StoreVectorElement_NullValue) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* var = b.Var(ty.ptr<function, vec3<f32>>());
        b.Append(mod.allocators.instructions.Create<ir::StoreVectorElement>(
            var->Result(0), b.Constant(1_i), nullptr));
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(:4:34 error: store_vector_element: operand is undefined
    store_vector_element %2, 1i, undef
                                 ^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:ptr<function, vec3<f32>, read_write> = var
    store_vector_element %2, 1i, undef
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Scoping_UseBeforeDecl) {
    auto* f = b.Function("my_func", ty.void_());

    auto* y = b.Add<i32>(2_i, 3_i);
    auto* x = b.Add<i32>(y, 1_i);

    f->Block()->Append(x);
    f->Block()->Append(y);
    f->Block()->Append(b.Return(f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:18 error: binary: %3 is not in scope
    %2:i32 = add %3, 1i
                 ^^

:2:3 note: in block
  $B1: {
  ^^^

:4:5 note: %3 declared here
    %3:i32 = add 2i, 3i
    ^^^^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %2:i32 = add %3, 1i
    %3:i32 = add 2i, 3i
    ret
  }
}
)");
}

template <typename T>
static const type::Type* TypeBuilder(type::Manager& m) {
    return m.Get<T>();
}
template <typename T>
static const type::Type* RefTypeBuilder(type::Manager& m) {
    return m.ref<AddressSpace::kFunction, T>();
}
using TypeBuilderFn = decltype(&TypeBuilder<i32>);

using IR_ValidatorRefTypeTest = IRTestParamHelper<std::tuple</* holds_ref */ bool,
                                                             /* refs_allowed */ bool,
                                                             /* type_builder */ TypeBuilderFn>>;

TEST_P(IR_ValidatorRefTypeTest, Var) {
    bool holds_ref = std::get<0>(GetParam());
    bool refs_allowed = std::get<1>(GetParam());
    auto* type = std::get<2>(GetParam())(ty);

    auto* fn = b.Function("my_func", ty.void_());
    b.Append(fn->Block(), [&] {
        if (auto* view = type->As<type::MemoryView>()) {
            b.Var(view);
        } else {
            b.Var(ty.ptr<function>(type));
        }

        b.Return(fn);
    });

    Capabilities caps;
    if (refs_allowed) {
        caps.Add(Capability::kAllowRefTypes);
    }
    auto res = ir::Validate(mod, caps);
    if (!holds_ref || refs_allowed) {
        ASSERT_EQ(res, Success) << res.Failure();
    } else {
        ASSERT_NE(res, Success);
        EXPECT_THAT(res.Failure().reason.Str(),
                    testing::HasSubstr("3:5 error: var: reference type is not permitted"));
    }
}

TEST_P(IR_ValidatorRefTypeTest, FnParam) {
    bool holds_ref = std::get<0>(GetParam());
    bool refs_allowed = std::get<1>(GetParam());
    auto* type = std::get<2>(GetParam())(ty);

    auto* fn = b.Function("my_func", ty.void_());
    fn->SetParams(Vector{b.FunctionParam(type)});
    b.Append(fn->Block(), [&] { b.Return(fn); });

    Capabilities caps;
    if (refs_allowed) {
        caps.Add(Capability::kAllowRefTypes);
    }
    auto res = ir::Validate(mod, caps);
    if (!holds_ref) {
        ASSERT_EQ(res, Success) << res.Failure();
    } else {
        ASSERT_NE(res, Success);
        EXPECT_THAT(res.Failure().reason.Str(),
                    testing::HasSubstr("references are not permitted as parameter types"));
    }
}

TEST_P(IR_ValidatorRefTypeTest, FnRet) {
    bool holds_ref = std::get<0>(GetParam());
    bool refs_allowed = std::get<1>(GetParam());
    auto* type = std::get<2>(GetParam())(ty);

    auto* fn = b.Function("my_func", type);
    b.Append(fn->Block(), [&] { b.Unreachable(); });

    Capabilities caps;
    if (refs_allowed) {
        caps.Add(Capability::kAllowRefTypes);
    }
    auto res = ir::Validate(mod, caps);
    if (!holds_ref) {
        ASSERT_EQ(res, Success) << res.Failure();
    } else {
        ASSERT_NE(res, Success);
        EXPECT_THAT(res.Failure().reason.Str(),
                    testing::HasSubstr("references are not permitted as return types"));
    }
}

INSTANTIATE_TEST_SUITE_P(NonRefTypes,
                         IR_ValidatorRefTypeTest,
                         testing::Combine(/* holds_ref */ testing::Values(false),
                                          /* refs_allowed */ testing::Values(false, true),
                                          /* type_builder */
                                          testing::Values(TypeBuilder<i32>,
                                                          TypeBuilder<bool>,
                                                          TypeBuilder<vec4<f32>>,
                                                          TypeBuilder<array<f32, 3>>)));

INSTANTIATE_TEST_SUITE_P(RefTypes,
                         IR_ValidatorRefTypeTest,
                         testing::Combine(/* holds_ref */ testing::Values(true),
                                          /* refs_allowed */ testing::Values(false, true),
                                          /* type_builder */
                                          testing::Values(RefTypeBuilder<i32>,
                                                          RefTypeBuilder<bool>,
                                                          RefTypeBuilder<vec4<f32>>)));

TEST_F(IR_ValidatorTest, Switch_NoCondition) {
    auto* f = b.Function("my_func", ty.void_());

    auto* s = b.ir.allocators.instructions.Create<ir::Switch>();
    f->Block()->Append(s);
    b.Append(b.DefaultCase(s), [&] { b.ExitSwitch(s); });
    f->Block()->Append(b.Return(f));

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(), R"(error: switch: operand is undefined
:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    switch undef [c: (default, $B2)] {  # switch_1
      $B2: {  # case
        exit_switch  # switch_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Switch_ConditionPointer) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* s = b.Switch(b.Var("a", b.Zero<i32>()));
        b.Append(b.DefaultCase(s), [&] { b.ExitSwitch(s); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(error: switch: condition type must be an integer scalar
:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    %a:ptr<function, i32, read_write> = var, 0i
    switch %a [c: (default, $B2)] {  # switch_1
      $B2: {  # case
        exit_switch  # switch_1
      }
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Switch_NoCases) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        b.Switch(1_i);
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:5 error: switch: missing default case for switch
    switch 1i [] {  # switch_1
    ^^^^^^^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    switch 1i [] {  # switch_1
    }
    ret
  }
}
)");
}

TEST_F(IR_ValidatorTest, Switch_NoDefaultCase) {
    auto* f = b.Function("my_func", ty.void_());

    b.Append(f->Block(), [&] {
        auto* s = b.Switch(1_i);
        b.Append(b.Case(s, {b.Constant(0_i)}), [&] { b.ExitSwitch(s); });
        b.Return(f);
    });

    auto res = ir::Validate(mod);
    ASSERT_NE(res, Success);
    EXPECT_EQ(res.Failure().reason.Str(),
              R"(:3:5 error: switch: missing default case for switch
    switch 1i [c: (0i, $B2)] {  # switch_1
    ^^^^^^^^^^^^^^^^^^^^^^^^

:2:3 note: in block
  $B1: {
  ^^^

note: # Disassembly
%my_func = func():void {
  $B1: {
    switch 1i [c: (0i, $B2)] {  # switch_1
      $B2: {  # case
        exit_switch  # switch_1
      }
    }
    ret
  }
}
)");
}

}  // namespace
}  // namespace tint::core::ir
