// Copyright 2021 The Dawn & Tint Authors
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

#include "src/tint/lang/msl/writer/ast_raise/module_scope_var_to_entry_point_param.h"

#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "src/tint/lang/wgsl/ast/disable_validation_attribute.h"
#include "src/tint/lang/wgsl/program/clone_context.h"
#include "src/tint/lang/wgsl/program/program_builder.h"
#include "src/tint/lang/wgsl/resolver/resolve.h"
#include "src/tint/lang/wgsl/sem/call.h"
#include "src/tint/lang/wgsl/sem/function.h"
#include "src/tint/lang/wgsl/sem/module.h"
#include "src/tint/lang/wgsl/sem/statement.h"
#include "src/tint/lang/wgsl/sem/struct.h"
#include "src/tint/lang/wgsl/sem/variable.h"
#include "src/tint/utils/text/string.h"

TINT_INSTANTIATE_TYPEINFO(tint::msl::writer::ModuleScopeVarToEntryPointParam);

using namespace tint::core::fluent_types;  // NOLINT

namespace tint::msl::writer {
namespace {

using StructMemberList = tint::Vector<const ast::StructMember*, 8>;

// The name of the struct member for arrays that are wrapped in structures.
const char* kWrappedArrayMemberName = "arr";

bool ShouldRun(const Program& program) {
    for (auto* decl : program.AST().GlobalDeclarations()) {
        if (decl->Is<ast::Variable>()) {
            return true;
        }
    }
    return false;
}

// Returns `true` if `type` is or contains a matrix type.
bool ContainsMatrix(const core::type::Type* type) {
    type = type->UnwrapRef();
    if (type->Is<core::type::Matrix>()) {
        return true;
    } else if (auto* ary = type->As<core::type::Array>()) {
        return ContainsMatrix(ary->ElemType());
    } else if (auto* str = type->As<core::type::Struct>()) {
        for (auto* member : str->Members()) {
            if (ContainsMatrix(member->Type())) {
                return true;
            }
        }
    }
    return false;
}
}  // namespace

/// PIMPL state for the transform
struct ModuleScopeVarToEntryPointParam::State {
    /// The clone context.
    program::CloneContext& ctx;

    /// Constructor
    /// @param context the clone context
    explicit State(program::CloneContext& context) : ctx(context) {}

    /// Clone any struct types that are contained in `ty` (including `ty` itself),
    /// and add it to the global declarations now, so that they precede new global
    /// declarations that need to reference them.
    /// @param ty the type to clone
    void CloneStructTypes(const core::type::Type* ty) {
        if (auto* str = ty->As<sem::Struct>()) {
            if (!cloned_structs_.emplace(str).second) {
                // The struct has already been cloned.
                return;
            }

            // Recurse into members.
            for (auto* member : str->Members()) {
                CloneStructTypes(member->Type());
            }

            // Clone the struct and add it to the global declaration list.
            // Remove the old declaration.
            auto* ast_str = str->Declaration();
            ctx.dst->AST().AddTypeDecl(ctx.Clone(ast_str));
            ctx.Remove(ctx.src->AST().GlobalDeclarations(), ast_str);
        } else if (auto* arr = ty->As<core::type::Array>()) {
            CloneStructTypes(arr->ElemType());
        }
    }

    /// Process a variable `var` that is referenced in the entry point function `func`.
    /// This will redeclare the variable as a function parameter, possibly as a pointer.
    /// Some workgroup variables will be redeclared as a member inside a workgroup structure.
    /// @param func the entry point function
    /// @param var the variable
    /// @param new_var_symbol the symbol to use for the replacement
    /// @param workgroup_param helper function to get a symbol to a workgroup struct parameter
    /// @param workgroup_parameter_members reference to a list of a workgroup struct members
    /// @param is_pointer output signalling whether the replacement is a pointer
    /// @param is_wrapped output signalling whether the replacement is wrapped in a struct
    void ProcessVariableInEntryPoint(const ast::Function* func,
                                     const sem::Variable* var,
                                     Symbol new_var_symbol,
                                     std::function<Symbol()> workgroup_param,
                                     StructMemberList& workgroup_parameter_members,
                                     bool& is_pointer,
                                     bool& is_wrapped) {
        auto* ty = var->Type()->UnwrapRef();

        // Helper to create an AST node for the store type of the variable.
        auto store_type = [&] { return CreateASTTypeFor(ctx, ty); };

        core::AddressSpace sc = var->AddressSpace();
        switch (sc) {
            case core::AddressSpace::kHandle: {
                // For a texture or sampler variable, redeclare it as an entry point parameter.
                // Disable entry point parameter validation.
                auto* disable_validation =
                    ctx.dst->Disable(ast::DisabledValidation::kEntryPointParameter);
                auto attrs = ctx.Clone(var->Declaration()->attributes);
                attrs.Push(disable_validation);
                auto* param = ctx.dst->Param(new_var_symbol, store_type(), attrs);
                ctx.InsertFront(func->params, param);

                break;
            }
            case core::AddressSpace::kStorage:
            case core::AddressSpace::kUniform: {
                // Variables into the Storage and Uniform address spaces are redeclared as entry
                // point parameters with a pointer type.
                auto attributes = ctx.Clone(var->Declaration()->attributes);
                attributes.Push(ctx.dst->Disable(ast::DisabledValidation::kEntryPointParameter));
                attributes.Push(ctx.dst->Disable(ast::DisabledValidation::kIgnoreAddressSpace));

                auto param_type = store_type();
                if (auto* arr = ty->As<core::type::Array>();
                    arr && arr->Count()->Is<core::type::RuntimeArrayCount>()) {
                    // Wrap runtime-sized arrays in structures, so that we can declare pointers to
                    // them. Ideally we'd just emit the array itself as a pointer, but this is not
                    // representable in Tint's AST.
                    CloneStructTypes(ty);
                    auto* wrapper = ctx.dst->Structure(
                        ctx.dst->Sym(), tint::Vector{
                                            ctx.dst->Member(kWrappedArrayMemberName, param_type),
                                        });
                    param_type = ctx.dst->ty.Of(wrapper);
                    is_wrapped = true;
                }

                param_type = sc == core::AddressSpace::kStorage
                                 ? ctx.dst->ty.ptr(sc, param_type, var->Access())
                                 : ctx.dst->ty.ptr(sc, param_type);
                auto* param = ctx.dst->Param(new_var_symbol, param_type, attributes);
                ctx.InsertFront(func->params, param);
                is_pointer = true;

                break;
            }
            case core::AddressSpace::kWorkgroup: {
                if (ContainsMatrix(var->Type())) {
                    // Due to a bug in the MSL compiler, we use a threadgroup memory argument for
                    // any workgroup allocation that contains a matrix. See crbug.com/tint/938.
                    // TODO(jrprice): Do this for all other workgroup variables too.

                    // Create a member in the workgroup parameter struct.
                    auto member = ctx.Clone(var->Declaration()->name->symbol);
                    workgroup_parameter_members.Push(ctx.dst->Member(member, store_type()));
                    CloneStructTypes(var->Type()->UnwrapRef());

                    // Create a function-scope variable that is a pointer to the member.
                    auto* member_ptr = ctx.dst->AddressOf(
                        ctx.dst->MemberAccessor(ctx.dst->Deref(workgroup_param()), member));
                    auto* local_var = ctx.dst->Let(new_var_symbol, member_ptr);
                    ctx.InsertFront(func->body->statements, ctx.dst->Decl(local_var));
                    is_pointer = true;
                } else {
                    auto* disable_validation =
                        ctx.dst->Disable(ast::DisabledValidation::kIgnoreAddressSpace);
                    auto* initializer = ctx.Clone(var->Declaration()->initializer);
                    auto* local_var = ctx.dst->Var(new_var_symbol, store_type(), sc, initializer,
                                                   tint::Vector{disable_validation});
                    ctx.InsertFront(func->body->statements, ctx.dst->Decl(local_var));
                }
                break;
            }
            case core::AddressSpace::kPixelLocal:
                break;  // Ignore
            default: {
                TINT_ICE() << "unhandled module-scope address space (" << sc << ")";
            }
        }
    }

    /// Process a variable `var` that is referenced in the user-defined function `func`.
    /// This will redeclare the variable as a function parameter, possibly as a pointer.
    /// @param func the user-defined function
    /// @param var the variable
    /// @param new_var_symbol the symbol to use for the replacement
    /// @param is_pointer output signalling whether the replacement is a pointer or not
    void ProcessVariableInUserFunction(const ast::Function* func,
                                       const sem::Variable* var,
                                       Symbol new_var_symbol,
                                       bool& is_pointer) {
        auto* ty = var->Type()->UnwrapRef();
        auto param_type = CreateASTTypeFor(ctx, ty);
        auto sc = var->AddressSpace();
        switch (sc) {
            case core::AddressSpace::kPrivate:
                // Private variables are passed all together in a struct.
                return;
            case core::AddressSpace::kStorage:
            case core::AddressSpace::kUniform:
            case core::AddressSpace::kHandle:
            case core::AddressSpace::kWorkgroup:
                break;
            case core::AddressSpace::kPushConstant: {
                ctx.dst->Diagnostics().AddError(Source{})
                    << "unhandled module-scope address space (" << sc << ")";
                break;
            }
            default: {
                TINT_ICE() << "unhandled module-scope address space (" << sc << ")";
            }
        }

        // Use a pointer for non-handle types.
        tint::Vector<const ast::Attribute*, 2> attributes;
        if (!ty->IsHandle()) {
            param_type = sc == core::AddressSpace::kStorage
                             ? ctx.dst->ty.ptr(sc, param_type, var->Access())
                             : ctx.dst->ty.ptr(sc, param_type);
            is_pointer = true;

            // Disable validation of the parameter's address space and of arguments passed to it.
            attributes.Push(ctx.dst->Disable(ast::DisabledValidation::kIgnoreAddressSpace));
            attributes.Push(
                ctx.dst->Disable(ast::DisabledValidation::kIgnoreInvalidPointerArgument));
        }

        // Redeclare the variable as a parameter.
        ctx.InsertBack(func->params,
                       ctx.dst->Param(new_var_symbol, param_type, std::move(attributes)));
    }

    /// Replace all uses of `var` in `func` with references to `new_var`.
    /// @param func the function
    /// @param var the variable to replace
    /// @param new_var the symbol to use for replacement
    /// @param is_pointer true if `new_var` is a pointer to the new variable
    /// @param member_name if valid, the name of the struct member that holds this variable
    void ReplaceUsesInFunction(const ast::Function* func,
                               const sem::Variable* var,
                               Symbol new_var,
                               bool is_pointer,
                               Symbol member_name) {
        for (auto* user : var->Users()) {
            if (user->Stmt()->Function()->Declaration() == func) {
                const ast::Expression* expr = ctx.dst->Expr(new_var);
                if (is_pointer) {
                    // If this identifier is used by an address-of operator, just remove the
                    // address-of instead of adding a deref, since we already have a pointer.
                    auto* ident = user->Declaration()->As<ast::IdentifierExpression>();
                    if (ident_to_address_of_.count(ident) && !member_name.IsValid()) {
                        ctx.Replace(ident_to_address_of_[ident], expr);
                        continue;
                    }

                    expr = ctx.dst->Deref(expr);
                }
                if (member_name.IsValid()) {
                    // Get the member from the containing structure.
                    expr = ctx.dst->MemberAccessor(expr, member_name);
                }
                ctx.Replace(user->Declaration(), expr);
            }
        }
    }

    /// Process the module.
    void Process() {
        // Predetermine the list of function calls that need to be replaced.
        using CallList = tint::Vector<const ast::CallExpression*, 8>;
        std::unordered_map<const ast::Function*, CallList> calls_to_replace;

        tint::Vector<const ast::Function*, 8> functions_to_process;

        // Collect private variables into a single structure.
        StructMemberList private_struct_members;
        tint::Vector<std::function<const ast::AssignmentStatement*()>, 4> private_initializers;
        std::unordered_set<const ast::Function*> uses_privates;

        // Build a list of functions that transitively reference any module-scope variables.
        for (auto* decl : ctx.src->Sem().Module()->DependencyOrderedDeclarations()) {
            if (auto* var = decl->As<ast::Var>()) {
                auto* sem_var = ctx.src->Sem().Get(var);
                if (sem_var->AddressSpace() == core::AddressSpace::kPrivate) {
                    // Create a member in the private variable struct.
                    auto* ty = sem_var->Type()->UnwrapRef();
                    auto name = ctx.Clone(var->name->symbol);
                    private_struct_members.Push(ctx.dst->Member(name, CreateASTTypeFor(ctx, ty)));
                    CloneStructTypes(ty);

                    // Create a statement to assign the initializer if present.
                    if (var->initializer) {
                        private_initializers.Push([&, name, var] {
                            return ctx.dst->Assign(
                                ctx.dst->MemberAccessor(PrivateStructVariableName(), name),
                                ctx.Clone(var->initializer));
                        });
                    }
                }
                continue;
            }

            auto* func_ast = decl->As<ast::Function>();
            if (!func_ast) {
                continue;
            }

            auto* func_sem = ctx.src->Sem().Get(func_ast);

            bool needs_processing = false;
            for (auto* var : func_sem->TransitivelyReferencedGlobals()) {
                if (var->AddressSpace() != core::AddressSpace::kUndefined) {
                    if (var->AddressSpace() == core::AddressSpace::kPrivate) {
                        uses_privates.insert(func_ast);
                    }
                    needs_processing = true;
                }
            }
            if (needs_processing) {
                functions_to_process.Push(func_ast);

                // Find all of the calls to this function that will need to be replaced.
                for (auto* call : func_sem->CallSites()) {
                    calls_to_replace[call->Stmt()->Function()->Declaration()].Push(
                        call->Declaration());
                }
            }
        }

        if (!private_struct_members.IsEmpty()) {
            // Create the private variable struct.
            ctx.dst->Structure(PrivateStructName(), std::move(private_struct_members));
        }

        // Build a list of `&ident` expressions. We'll use this later to avoid generating
        // expressions of the form `&*ident`, which break WGSL validation rules when this expression
        // is passed to a function.
        // TODO(jrprice): We should add support for bidirectional SEM tree traversal so that we can
        // do this on the fly instead.
        for (auto* node : ctx.src->ASTNodes().Objects()) {
            auto* address_of = node->As<ast::UnaryOpExpression>();
            if (!address_of || address_of->op != core::UnaryOp::kAddressOf) {
                continue;
            }
            if (auto* ident = address_of->expr->As<ast::IdentifierExpression>()) {
                ident_to_address_of_[ident] = address_of;
            }
        }

        for (auto* func_ast : functions_to_process) {
            auto* func_sem = ctx.src->Sem().Get(func_ast);
            bool is_entry_point = func_ast->IsEntryPoint();
            bool needs_pointer_aliasing = false;

            // Map module-scope variables onto their replacement.
            struct NewVar {
                Symbol symbol;
                bool is_pointer;
                bool is_wrapped;
            };
            std::unordered_map<const sem::Variable*, NewVar> var_to_newvar;

            // We aggregate all workgroup variables into a struct to avoid hitting MSL's limit for
            // threadgroup memory arguments.
            Symbol workgroup_parameter_symbol;
            StructMemberList workgroup_parameter_members;
            auto workgroup_param = [&] {
                if (!workgroup_parameter_symbol.IsValid()) {
                    workgroup_parameter_symbol = ctx.dst->Sym();
                }
                return workgroup_parameter_symbol;
            };

            // If this function references any private variables, it needs to take the private
            // variable struct as a parameter (or declare it, if it is an entry point function).
            if (uses_privates.count(func_ast)) {
                if (is_entry_point) {
                    // Create a local declaration for the private variable struct.
                    auto* var = ctx.dst->Var(
                        PrivateStructVariableName(), ctx.dst->ty(PrivateStructName()),
                        core::AddressSpace::kPrivate,
                        tint::Vector{
                            ctx.dst->Disable(ast::DisabledValidation::kIgnoreAddressSpace),
                        });
                    ctx.InsertFront(func_ast->body->statements, ctx.dst->Decl(var));

                    // Initialize the members of that struct with the original initializers.
                    for (auto init : private_initializers) {
                        ctx.InsertFront(func_ast->body->statements, init());
                    }
                } else {
                    // Create a parameter that is a pointer to the private variable struct.
                    auto ptr = ctx.dst->ty.ptr<private_>(ctx.dst->ty(PrivateStructName()));
                    auto* param = ctx.dst->Param(PrivateStructVariableName(), ptr);
                    ctx.InsertBack(func_ast->params, param);
                }
            }

            // Process and redeclare all variables referenced by the function.
            for (auto* var : func_sem->TransitivelyReferencedGlobals()) {
                if (var->AddressSpace() == core::AddressSpace::kUndefined) {
                    continue;
                }
                if (var->AddressSpace() == core::AddressSpace::kPrivate) {
                    // Private variable are collected into a single struct that is passed by
                    // pointer (handled above), so we just need to replace the uses here.
                    ReplaceUsesInFunction(func_ast, var, PrivateStructVariableName(),
                                          /* is_pointer */ !is_entry_point,
                                          ctx.Clone(var->Declaration()->name->symbol));
                    continue;
                }

                // This is the symbol for the variable that replaces the module-scope var.
                auto new_var_symbol = ctx.dst->Sym();

                // Track whether the new variable is a pointer or not.
                bool is_pointer = false;

                // Track whether the new variable was wrapped in a struct or not.
                bool is_wrapped = false;

                // Process the variable to redeclare it as a parameter or local variable.
                if (is_entry_point) {
                    ProcessVariableInEntryPoint(func_ast, var, new_var_symbol, workgroup_param,
                                                workgroup_parameter_members, is_pointer,
                                                is_wrapped);
                } else {
                    ProcessVariableInUserFunction(func_ast, var, new_var_symbol, is_pointer);
                    if (var->AddressSpace() == core::AddressSpace::kWorkgroup) {
                        needs_pointer_aliasing = true;
                    }
                }

                // Record the replacement symbol.
                var_to_newvar[var] = {new_var_symbol, is_pointer, is_wrapped};

                // Replace all uses of the module-scope variable.
                ReplaceUsesInFunction(
                    func_ast, var, new_var_symbol, is_pointer,
                    is_wrapped ? ctx.dst->Sym(kWrappedArrayMemberName) : Symbol());
            }

            // Allow pointer aliasing if needed.
            if (needs_pointer_aliasing) {
                ctx.InsertBack(func_ast->attributes,
                               ctx.dst->Disable(ast::DisabledValidation::kIgnorePointerAliasing));
            }

            if (!workgroup_parameter_members.IsEmpty()) {
                // Create the workgroup memory parameter.
                // The parameter is a struct that contains members for each workgroup variable.
                auto* str =
                    ctx.dst->Structure(ctx.dst->Sym(), std::move(workgroup_parameter_members));
                auto param_type = ctx.dst->ty.ptr(workgroup, ctx.dst->ty.Of(str));
                auto* param = ctx.dst->Param(
                    workgroup_param(), param_type,
                    tint::Vector{
                        ctx.dst->Disable(ast::DisabledValidation::kEntryPointParameter),
                        ctx.dst->Disable(ast::DisabledValidation::kIgnoreAddressSpace),
                    });
                ctx.InsertFront(func_ast->params, param);
            }

            // Pass the variables as pointers to any functions that need them.
            for (auto* call : calls_to_replace[func_ast]) {
                auto* call_sem = ctx.src->Sem().Get(call)->Unwrap()->As<sem::Call>();
                auto* target_sem = call_sem->Target()->As<sem::Function>();

                // Pass the private variable struct pointer if needed.
                if (uses_privates.count(target_sem->Declaration())) {
                    const ast::Expression* arg = ctx.dst->Expr(PrivateStructVariableName());
                    if (is_entry_point) {
                        arg = ctx.dst->AddressOf(arg);
                    }
                    ctx.InsertBack(call->args, arg);
                }

                // Add new arguments for any variables that are needed by the callee.
                // For entry points, pass non-handle types as pointers.
                for (auto* target_var : target_sem->TransitivelyReferencedGlobals()) {
                    auto sc = target_var->AddressSpace();
                    if (sc == core::AddressSpace::kUndefined) {
                        continue;
                    }

                    auto it = var_to_newvar.find(target_var);
                    if (it == var_to_newvar.end()) {
                        // No replacement was created for this function.
                        continue;
                    }

                    auto new_var = it->second;
                    bool IsHandle = target_var->Type()->UnwrapRef()->IsHandle();
                    const ast::Expression* arg = ctx.dst->Expr(new_var.symbol);
                    if (new_var.is_wrapped) {
                        // The variable is wrapped in a struct, so we need to pass a pointer to the
                        // struct member instead.
                        arg = ctx.dst->AddressOf(
                            ctx.dst->MemberAccessor(ctx.dst->Deref(arg), kWrappedArrayMemberName));
                    } else if (is_entry_point && !IsHandle && !new_var.is_pointer) {
                        // We need to pass a pointer and we don't already have one, so take
                        // the address of the new variable.
                        arg = ctx.dst->AddressOf(arg);
                    }
                    ctx.InsertBack(call->args, arg);
                }
            }
        }

        // Now remove all module-scope variables with these address spaces.
        for (auto* var_ast : ctx.src->AST().GlobalVariables()) {
            auto* var_sem = ctx.src->Sem().Get(var_ast);
            if (var_sem->AddressSpace() != core::AddressSpace::kUndefined) {
                ctx.Remove(ctx.src->AST().GlobalDeclarations(), var_ast);
            }
        }
    }

    /// @returns the name of the structure that contains all of the module-scope private variables
    Symbol PrivateStructName() {
        if (!private_struct_name.IsValid()) {
            private_struct_name = ctx.dst->Symbols().New("tint_private_vars_struct");
        }
        return private_struct_name;
    }

    /// @returns the name of the variable that contains all of the module-scope private variables
    Symbol PrivateStructVariableName() {
        if (!private_struct_variable_name.IsValid()) {
            private_struct_variable_name = ctx.dst->Symbols().New("tint_private_vars");
        }
        return private_struct_variable_name;
    }

  private:
    // The structures that have already been cloned by this transform.
    std::unordered_set<const sem::Struct*> cloned_structs_;

    // Map from identifier expression to the address-of expression that uses it.
    std::unordered_map<const ast::IdentifierExpression*, const ast::UnaryOpExpression*>
        ident_to_address_of_;

    // The name of the structure that contains all the module-scope private variables.
    Symbol private_struct_name;

    // The name of the structure variable that contains all the module-scope private variables.
    Symbol private_struct_variable_name;
};

ModuleScopeVarToEntryPointParam::ModuleScopeVarToEntryPointParam() = default;

ModuleScopeVarToEntryPointParam::~ModuleScopeVarToEntryPointParam() = default;

ast::transform::Transform::ApplyResult ModuleScopeVarToEntryPointParam::Apply(
    const Program& src,
    const ast::transform::DataMap&,
    ast::transform::DataMap&) const {
    if (!ShouldRun(src)) {
        return SkipTransform;
    }

    ProgramBuilder b;
    program::CloneContext ctx{&b, &src, /* auto_clone_symbols */ true};
    State state{ctx};
    state.Process();

    ctx.Clone();
    return resolver::Resolve(b);
}

}  // namespace tint::msl::writer
