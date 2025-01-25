// Copyright 2022 The Dawn & Tint Authors
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

#include "src/tint/lang/wgsl/ast/transform/clamp_frag_depth.h"

#include <utility>

#include "src/tint/lang/core/builtin_value.h"
#include "src/tint/lang/wgsl/ast/attribute.h"
#include "src/tint/lang/wgsl/ast/builtin_attribute.h"
#include "src/tint/lang/wgsl/ast/function.h"
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/ast/struct.h"
#include "src/tint/lang/wgsl/ast/transform/push_constant_helper.h"
#include "src/tint/lang/wgsl/program/clone_context.h"
#include "src/tint/lang/wgsl/program/program_builder.h"
#include "src/tint/lang/wgsl/resolver/resolve.h"
#include "src/tint/lang/wgsl/sem/function.h"
#include "src/tint/lang/wgsl/sem/statement.h"
#include "src/tint/lang/wgsl/sem/struct.h"
#include "src/tint/utils/containers/vector.h"
#include "src/tint/utils/macros/scoped_assignment.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::ClampFragDepth);
TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::ClampFragDepth::Config);

namespace tint::ast::transform {

/// PIMPL state for the transform
struct ClampFragDepth::State {
    /// The source program
    const Program& src;
    /// The target program builder
    ProgramBuilder b{};
    /// The clone context
    program::CloneContext ctx = {&b, &src, /* auto_clone_symbols */ true};
    /// The sem::Info of the program
    const sem::Info& sem = src.Sem();
    /// The symbols of the program
    const SymbolTable& sym = src.Symbols();

    /// Runs the transform
    /// @returns the new program or SkipTransform if the transform is not required
    Transform::ApplyResult Run(const DataMap& inputs) {
        const Config* cfg = inputs.Get<Config>();
        if (!cfg || !cfg->offsets.has_value()) {
            return SkipTransform;
        }

        PushConstantHelper push_constant_helper(ctx);

        // At least one entry-point needs clamping. Add the following to the module:
        //
        //   enable chromium_experimental_push_constant;
        //
        //   struct PushConstants {
        //       min_depth : f32,
        //       max_depth : f32,
        //   }
        //   var<push_constant> push_constants : PushConstants;
        //
        //   fn clamp_frag_depth(v : f32) -> f32 {
        //       return clamp(v, push_constants.min, push_constants.max_depth);
        //   }

        push_constant_helper.InsertMember("min_depth", b.ty.f32(), cfg->offsets->min);
        push_constant_helper.InsertMember("max_depth", b.ty.f32(), cfg->offsets->max);

        Symbol buffer_name = push_constant_helper.Run();

        auto base_fn_sym = b.Symbols().New("clamp_frag_depth");
        b.Func(base_fn_sym, Vector{b.Param("v", b.ty.f32())}, b.ty.f32(),
               Vector{b.Return(b.Call("clamp", "v", b.MemberAccessor(buffer_name, "min_depth"),
                                      b.MemberAccessor(buffer_name, "max_depth")))});

        // If true, the currently cloned function returns frag depth directly as a scalar
        bool returns_frag_depth_as_value = false;

        // If valid, the currently cloned function returns frag depth in a struct
        // The symbol is the name of the helper function to apply the depth clamping.
        Symbol returns_frag_depth_as_struct_helper;

        // Map of io struct to helper function to return the structure with the depth clamping
        // applied.
        Hashmap<const ast::Struct*, Symbol, 4u> io_structs_clamp_helpers;

        // Register a callback that will be called for each visted AST function.
        // This call wraps the cloning of the function's statements, and will assign to
        // `returns_frag_depth_as_value` or `returns_frag_depth_as_struct_helper` if the function's
        // return value requires depth clamping.
        ctx.ReplaceAll([&](const ast::Function* fn) {
            if (fn->PipelineStage() != ast::PipelineStage::kFragment) {
                return ctx.CloneWithoutTransform(fn);
            }

            if (ReturnsFragDepthAsValue(fn)) {
                TINT_SCOPED_ASSIGNMENT(returns_frag_depth_as_value, true);
                return ctx.CloneWithoutTransform(fn);
            }

            if (ReturnsFragDepthInStruct(fn)) {
                // At most once per I/O struct, add the conversion function:
                //
                //   fn clamp_frag_depth_S(s : S) -> S {
                //       return S(s.first, s.second, clamp_frag_depth(s.frag_depth), s.last);
                //   }
                auto* struct_ty = sem.Get(fn)->ReturnType()->As<sem::Struct>()->Declaration();
                auto helper = io_structs_clamp_helpers.GetOrAdd(struct_ty, [&] {
                    auto return_ty = fn->return_type;
                    auto fn_sym =
                        b.Symbols().New("clamp_frag_depth_" + struct_ty->name->symbol.Name());

                    Vector<const ast::Expression*, 8u> initializer_args;
                    for (auto* member : struct_ty->members) {
                        const ast::Expression* arg =
                            b.MemberAccessor("s", ctx.Clone(member->name->symbol));
                        if (ContainsFragDepth(member->attributes)) {
                            arg = b.Call(base_fn_sym, arg);
                        }
                        initializer_args.Push(arg);
                    }
                    Vector params{b.Param("s", ctx.Clone(return_ty))};
                    Vector body{
                        b.Return(b.Call(ctx.Clone(return_ty), std::move(initializer_args))),
                    };
                    b.Func(fn_sym, params, ctx.Clone(return_ty), body);
                    return fn_sym;
                });

                TINT_SCOPED_ASSIGNMENT(returns_frag_depth_as_struct_helper, helper);
                return ctx.CloneWithoutTransform(fn);
            }

            return ctx.CloneWithoutTransform(fn);
        });

        // Replace the return statements `return expr` with `return clamp_frag_depth(expr)`.
        ctx.ReplaceAll([&](const ast::ReturnStatement* stmt) -> const ast::ReturnStatement* {
            if (returns_frag_depth_as_value) {
                return b.Return(stmt->source, b.Call(base_fn_sym, ctx.Clone(stmt->value)));
            }
            if (returns_frag_depth_as_struct_helper.IsValid()) {
                return b.Return(stmt->source, b.Call(returns_frag_depth_as_struct_helper,
                                                     ctx.Clone(stmt->value)));
            }
            return nullptr;
        });

        ctx.Clone();
        return resolver::Resolve(b);
    }

  private:
    /// @param attrs the attributes to examine
    /// @returns true if @p attrs contains a `@builtin(frag_depth)` attribute
    bool ContainsFragDepth(VectorRef<const ast::Attribute*> attrs) {
        for (auto* attribute : attrs) {
            if (auto* builtin_attr = attribute->As<ast::BuiltinAttribute>()) {
                if (builtin_attr->builtin == core::BuiltinValue::kFragDepth) {
                    return true;
                }
            }
        }

        return false;
    }

    /// @param fn the function to examine
    /// @returns true if @p fn has a return type with a `@builtin(frag_depth)` attribute
    bool ReturnsFragDepthAsValue(const ast::Function* fn) {
        return ContainsFragDepth(fn->return_type_attributes);
    }

    /// @param fn the function to examine
    /// @returns true if @p fn has a return structure with a `@builtin(frag_depth)` attribute on one
    /// of the members
    bool ReturnsFragDepthInStruct(const ast::Function* fn) {
        if (auto* struct_ty = sem.Get(fn)->ReturnType()->As<sem::Struct>()) {
            for (auto* member : struct_ty->Members()) {
                if (ContainsFragDepth(member->Declaration()->attributes)) {
                    return true;
                }
            }
        }

        return false;
    }
};

ClampFragDepth::ClampFragDepth() = default;
ClampFragDepth::~ClampFragDepth() = default;

ast::transform::Transform::ApplyResult ClampFragDepth::Apply(const Program& src,
                                                             const ast::transform::DataMap& inputs,
                                                             ast::transform::DataMap&) const {
    return State{src}.Run(inputs);
}

ClampFragDepth::Config::Config() = default;
ClampFragDepth::Config::Config(std::optional<RangeOffsets> off) : offsets(off) {}

ClampFragDepth::Config::~Config() = default;

}  // namespace tint::ast::transform
