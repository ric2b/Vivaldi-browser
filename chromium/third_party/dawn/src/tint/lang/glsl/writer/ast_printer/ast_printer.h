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

#ifndef SRC_TINT_LANG_GLSL_WRITER_AST_PRINTER_AST_PRINTER_H_
#define SRC_TINT_LANG_GLSL_WRITER_AST_PRINTER_AST_PRINTER_H_

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "src/tint/lang/core/builtin_value.h"
#include "src/tint/lang/glsl/writer/common/version.h"
#include "src/tint/lang/wgsl/program/program_builder.h"
#include "src/tint/utils/containers/scope_stack.h"
#include "src/tint/utils/generator/text_generator.h"
#include "src/tint/utils/math/hash.h"
#include "src/tint/utils/text/string_stream.h"

// Forward declarations
namespace tint::sem {
class BuiltinFn;
class Call;
class ValueConstructor;
class ValueConversion;
}  // namespace tint::sem
namespace tint::glsl::writer {
struct Options;
}

namespace tint::glsl::writer {

/// The result of sanitizing a program for generation.
struct SanitizedResult {
    /// Constructor
    SanitizedResult();
    /// Destructor
    ~SanitizedResult();
    /// Move constructor
    SanitizedResult(SanitizedResult&&);

    /// The sanitized program.
    Program program;
};

/// Sanitize a program in preparation for generating GLSL.
/// @param program The program to sanitize
/// @param options The HLSL generator options.
/// @param entry_point the entry point to generate GLSL for
/// @returns the sanitized program and any supplementary information
SanitizedResult Sanitize(const Program& program,
                         const Options& options,
                         const std::string& entry_point);

/// Implementation class for GLSL generator
class ASTPrinter : public tint::TextGenerator {
  public:
    /// Constructor
    /// @param program the program to generate
    /// @param version the GLSL version to use
    ASTPrinter(const Program& program, const Version& version);
    ~ASTPrinter() override;

    /// Generates the GLSL shader
    /// @returns true on successful generation, false otherwise
    bool Generate();

    /// Record an extension directive within the generator
    /// @param ext the extension to record
    void RecordExtension(const ast::Enable* ext);
    /// Handles an index accessor expression
    /// @param out the output of the expression stream
    /// @param expr the expression to emit
    void EmitIndexAccessor(StringStream& out, const ast::IndexAccessorExpression* expr);
    /// Handles an assignment statement
    /// @param stmt the statement to emit
    void EmitAssign(const ast::AssignmentStatement* stmt);
    /// Handles emission of bitwise operators (&|) on bool scalars and vectors
    /// @param out the output of the expression stream
    /// @param expr the binary expression
    void EmitBitwiseBoolOp(StringStream& out, const ast::BinaryExpression* expr);
    /// Handles generating a binary expression
    /// @param out the output of the expression stream
    /// @param expr the binary expression
    void EmitFloatModulo(StringStream& out, const ast::BinaryExpression* expr);
    /// Handles generating the modulo operator on float vector operands
    /// @param out the output of the expression stream
    /// @param expr the binary expression
    void EmitBinary(StringStream& out, const ast::BinaryExpression* expr);
    /// Handles generating a bitcast expression
    /// @param out the output of the expression stream
    /// @param expr the expression
    void EmitVectorRelational(StringStream& out, const ast::BinaryExpression* expr);
    /// Handles a bitcast call expression
    /// @param out the output of the expression stream
    /// @param expr the expression
    void EmitBitcastCall(StringStream& out, const ast::CallExpression* expr);
    /// Emits a list of statements
    /// @param stmts the statement list
    void EmitStatements(VectorRef<const ast::Statement*> stmts);
    /// Emits a list of statements with an indentation
    /// @param stmts the statement list
    void EmitStatementsWithIndent(VectorRef<const ast::Statement*> stmts);
    /// Handles a block statement
    /// @param stmt the statement to emit
    void EmitBlock(const ast::BlockStatement* stmt);
    /// Handles a break statement
    /// @param stmt the statement to emit
    void EmitBreak(const ast::BreakStatement* stmt);
    /// Handles a break-if statement
    /// @param stmt the statement to emit
    void EmitBreakIf(const ast::BreakIfStatement* stmt);
    /// Handles generating a call expression
    /// @param out the output of the expression stream
    /// @param expr the call expression
    void EmitCall(StringStream& out, const ast::CallExpression* expr);
    /// Handles generating a function call expression
    /// @param out the output of the expression stream
    /// @param call the call expression
    /// @param fn the function being called
    void EmitFunctionCall(StringStream& out, const sem::Call* call, const sem::Function* fn);
    /// Handles generating a builtin call expression
    /// @param out the output of the expression stream
    /// @param call the call expression
    /// @param builtin the builtin being called
    void EmitBuiltinCall(StringStream& out, const sem::Call* call, const sem::BuiltinFn* builtin);
    /// Handles generating a value conversion expression
    /// @param out the output of the expression stream
    /// @param call the call expression
    /// @param conv the value conversion
    void EmitValueConversion(StringStream& out,
                             const sem::Call* call,
                             const sem::ValueConversion* conv);
    /// Handles generating a value constructor expression
    /// @param out the output of the expression stream
    /// @param call the call expression
    /// @param ctor the value constructor
    void EmitValueConstructor(StringStream& out,
                              const sem::Call* call,
                              const sem::ValueConstructor* ctor);
    /// Handles generating a barrier builtin call
    /// @param out the output of the expression stream
    /// @param builtin the semantic information for the barrier builtin
    void EmitBarrierCall(StringStream& out, const sem::BuiltinFn* builtin);
    /// Handles generating an atomic builtin call for a workgroup variable
    /// @param out the output of the expression stream
    /// @param expr the call expression
    /// @param builtin the semantic information for the atomic builtin
    void EmitWorkgroupAtomicCall(StringStream& out,
                                 const ast::CallExpression* expr,
                                 const sem::BuiltinFn* builtin);
    /// Handles generating an array.length() call
    /// @param out the output of the expression stream
    /// @param expr the call expression
    void EmitArrayLength(StringStream& out, const ast::CallExpression* expr);
    /// Handles generating a call to `bitfieldExtract`
    /// @param out the output of the expression stream
    /// @param expr the call expression
    void EmitExtractBits(StringStream& out, const ast::CallExpression* expr);
    /// Handles generating a call to `bitfieldInsert`
    /// @param out the output of the expression stream
    /// @param expr the call expression
    void EmitInsertBits(StringStream& out, const ast::CallExpression* expr);
    /// Emulates 'fma' on GLSL ES, where it is unsupported.
    /// @param out the output of the expression stream
    /// @param expr the fma() expression
    void EmitEmulatedFMA(StringStream& out, const ast::CallExpression* expr);
    /// Create a float literal zero AST node, and associated semantic nodes.
    /// @param stmt the statement which will own the semantic expression node
    /// @returns an AST expression representing 0.0f
    const ast::Expression* CreateF32Zero(const sem::Statement* stmt);

    /// Handles generating a call to a texture function (`textureSample`,
    /// `textureSampleGrad`, etc)
    /// @param out the output of the expression stream
    /// @param call the call expression
    /// @param builtin the semantic information for the texture builtin
    void EmitTextureCall(StringStream& out, const sem::Call* call, const sem::BuiltinFn* builtin);
    /// Handles generating a call to the `select()` builtin
    /// @param out the output of the expression stream
    /// @param expr the call expression
    void EmitCountOneBitsCall(StringStream& out, const ast::CallExpression* expr);
    /// Handles generating a call to the `countOneBits()` builtin
    /// @param out the output of the expression stream
    /// @param expr the call expression
    /// @param builtin the semantic information for the builtin
    void EmitSelectCall(StringStream& out,
                        const ast::CallExpression* expr,
                        const sem::BuiltinFn* builtin);
    /// Handles generating a call to the `dot()` builtin
    /// @param out the output of the expression stream
    /// @param expr the call expression
    /// @param builtin the semantic information for the builtin
    void EmitDotCall(StringStream& out,
                     const ast::CallExpression* expr,
                     const sem::BuiltinFn* builtin);
    /// Handles generating a call to the `modf()` builtin
    /// @param out the output of the expression stream
    /// @param expr the call expression
    /// @param builtin the semantic information for the builtin
    void EmitModfCall(StringStream& out,
                      const ast::CallExpression* expr,
                      const sem::BuiltinFn* builtin);
    /// Handles generating a call to the `frexp()` builtin
    /// @param out the output of the expression stream
    /// @param expr the call expression
    /// @param builtin the semantic information for the builtin
    void EmitFrexpCall(StringStream& out,
                       const ast::CallExpression* expr,
                       const sem::BuiltinFn* builtin);
    /// Handles generating a call to the `degrees()` builtin
    /// @param out the output of the expression stream
    /// @param expr the call expression
    /// @param builtin the semantic information for the builtin
    void EmitDegreesCall(StringStream& out,
                         const ast::CallExpression* expr,
                         const sem::BuiltinFn* builtin);
    /// Handles generating a call to the `radians()` builtin
    /// @param out the output of the expression stream
    /// @param expr the call expression
    /// @param builtin the semantic information for the builtin
    void EmitRadiansCall(StringStream& out,
                         const ast::CallExpression* expr,
                         const sem::BuiltinFn* builtin);
    /// Handles generating a call to the `quantizeToF16()` intrinsic
    /// @param out the output of the expression stream
    /// @param expr the call expression
    /// @param builtin the semantic information for the builtin
    void EmitQuantizeToF16Call(StringStream& out,
                               const ast::CallExpression* expr,
                               const sem::BuiltinFn* builtin);
    /// Handles a case statement
    /// @param stmt the statement
    void EmitCase(const ast::CaseStatement* stmt);
    /// Handles generating a discard statement
    /// @param stmt the discard statement
    void EmitDiscard(const ast::DiscardStatement* stmt);
    /// Handles a continue statement
    /// @param stmt the statement to emit
    void EmitContinue(const ast::ContinueStatement* stmt);
    /// Handles generate an Expression
    /// @param out the output of the expression stream
    /// @param expr the expression
    void EmitExpression(StringStream& out, const ast::Expression* expr);
    /// Handles generating a function
    /// @param func the function to generate
    void EmitFunction(const ast::Function* func);

    /// Handles emitting a global variable
    /// @param global the global variable
    void EmitGlobalVariable(const ast::Variable* global);

    /// Handles emitting a global variable with the uniform address space
    /// @param var the AST node for the 'var'
    /// @param sem the semantic node for the 'var'
    void EmitUniformVariable(const ast::Var* var, const sem::GlobalVariable* sem);

    /// Handles emitting a global variable with the storage address space
    /// @param var the AST node for the 'var'
    /// @param sem the semantic node for the 'var'
    void EmitStorageVariable(const ast::Var* var, const sem::GlobalVariable* sem);

    /// Handles emitting a global variable with the handle address space
    /// @param var the AST node for the 'var'
    /// @param sem the semantic node for the 'var'
    void EmitHandleVariable(const ast::Var* var, const sem::GlobalVariable* sem);

    /// Handles emitting a global variable with the private address space
    /// @param var the global variable
    void EmitPrivateVariable(const sem::Variable* var);

    /// Handles emitting a global variable with the workgroup address space
    /// @param var the global variable
    void EmitWorkgroupVariable(const sem::Variable* var);

    /// Handles emitting a global variable with the input or output address space
    /// @param var the global variable
    void EmitIOVariable(const sem::GlobalVariable* var);

    /// Handles emitting a global variable with the push_constant address space
    /// @param var the global variable
    void EmitPushConstant(const sem::GlobalVariable* var);

    /// Handles emitting interpolation qualifiers
    /// @param out the output of the expression stream
    /// @param attrs the attributes
    void EmitInterpolationQualifiers(StringStream& out, VectorRef<const ast::Attribute*> attrs);
    /// Handles emitting attributes
    /// @param out the output of the expression stream
    /// @param var the global variable semantics
    void EmitAttributes(StringStream& out, const sem::GlobalVariable* var);
    /// Handles emitting the entry point function
    /// @param func the entry point
    void EmitEntryPointFunction(const ast::Function* func);
    /// Handles an if statement
    /// @param stmt the statement to emit
    void EmitIf(const ast::IfStatement* stmt);
    /// Handles a constant value
    /// @param out the output stream
    /// @param constant the constant value to emit
    void EmitConstant(StringStream& out, const core::constant::Value* constant);
    /// Handles a literal
    /// @param out the output stream
    /// @param lit the literal to emit
    void EmitLiteral(StringStream& out, const ast::LiteralExpression* lit);
    /// Handles a loop statement
    /// @param stmt the statement to emit
    void EmitLoop(const ast::LoopStatement* stmt);
    /// Handles a for loop statement
    /// @param stmt the statement to emit
    void EmitForLoop(const ast::ForLoopStatement* stmt);
    /// Handles a while statement
    /// @param stmt the statement to emit
    void EmitWhile(const ast::WhileStatement* stmt);
    /// Handles generating an identifier expression
    /// @param out the output of the expression stream
    /// @param expr the identifier expression
    void EmitIdentifier(StringStream& out, const ast::IdentifierExpression* expr);
    /// Handles a member accessor expression
    /// @param out the output of the expression stream
    /// @param expr the member accessor expression
    void EmitMemberAccessor(StringStream& out, const ast::MemberAccessorExpression* expr);
    /// Handles return statements
    /// @param stmt the statement to emit
    void EmitReturn(const ast::ReturnStatement* stmt);
    /// Handles statement
    /// @param stmt the statement to emit
    void EmitStatement(const ast::Statement* stmt);
    /// Handles generating a switch statement
    /// @param stmt the statement to emit
    void EmitSwitch(const ast::SwitchStatement* stmt);
    /// Handles generating type
    /// @param out the output stream
    /// @param type the type to generate
    /// @param address_space the address space of the variable
    /// @param access the access control type of the variable
    /// @param name the name of the variable, used for array emission.
    /// @param name_printed (optional) if not nullptr and an array was printed
    /// then the boolean is set to true.
    void EmitType(StringStream& out,
                  const core::type::Type* type,
                  core::AddressSpace address_space,
                  core::Access access,
                  const std::string& name,
                  bool* name_printed = nullptr);
    /// Handles generating type and name
    /// @param out the output stream
    /// @param type the type to generate
    /// @param address_space the address space of the variable
    /// @param access the access control type of the variable
    /// @param name the name to emit
    void EmitTypeAndName(StringStream& out,
                         const core::type::Type* type,
                         core::AddressSpace address_space,
                         core::Access access,
                         const std::string& name);
    /// Handles generating a structure declaration. If the structure has already been emitted, then
    /// this function will simply return `true` without emitting anything.
    /// @param buffer the text buffer that the type declaration will be written to
    /// @param ty the struct to generate
    void EmitStructType(TextBuffer* buffer, const core::type::Struct* ty);
    /// Handles generating the members of a structure
    /// @param buffer the text buffer that the struct members will be written to
    /// @param ty the struct to generate
    void EmitStructMembers(TextBuffer* buffer, const core::type::Struct* ty);
    /// Handles a unary op expression
    /// @param out the output of the expression stream
    /// @param expr the expression to emit
    void EmitUnaryOp(StringStream& out, const ast::UnaryOpExpression* expr);
    /// Emits the zero value for the given type
    /// @param out the output stream
    /// @param type the type to emit the value for
    void EmitZeroValue(StringStream& out, const core::type::Type* type);
    /// Handles generating a 'var' declaration
    /// @param var the variable to generate
    void EmitVar(const ast::Var* var);
    /// Handles generating a 'let' declaration
    /// @param let the variable to generate
    void EmitLet(const ast::Let* let);
    /// Handles generating a module-scope 'let' declaration
    /// @param let the 'let' to emit
    void EmitProgramConstVariable(const ast::Variable* let);
    /// Handles generating a builtin method name
    /// @param builtin the semantic info for the builtin
    /// @returns the name or "" if not valid
    std::string generate_builtin_name(const sem::BuiltinFn* builtin);
    /// Converts a builtin to a gl_ string
    /// @param builtin the builtin to convert
    /// @param stage pipeline stage in which this builtin is used
    /// @returns the string name of the builtin or blank on error
    const char* builtin_to_string(core::BuiltinValue builtin, ast::PipelineStage stage);
    /// Converts a builtin to a core::type::Type appropriate for GLSL.
    /// @param builtin the builtin to convert
    /// @returns the appropriate semantic type or null on error.
    core::type::Type* builtin_type(core::BuiltinValue builtin);

  private:
    enum class VarType { kIn, kOut };

    struct EntryPointData {
        std::string struct_name;
        std::string var_name;
    };

    /// The map key for two semantic types.
    using BinaryOperandType =
        tint::UnorderedKeyWrapper<std::tuple<const core::type::Type*, const core::type::Type*>>;

    /// CallBuiltinHelper will call the builtin helper function, creating it
    /// if it hasn't been built already. If the builtin needs to be built then
    /// CallBuiltinHelper will generate the function signature and will call
    /// `build` to emit the body of the function.
    /// @param out the output of the expression stream
    /// @param call the call expression
    /// @param builtin the semantic information for the builtin
    /// @param build a function with the signature:
    ///        `bool(TextBuffer* buffer, const std::vector<std::string>& params)`
    ///        Where:
    ///          `buffer` is the body of the generated function
    ///          `params` is the name of all the generated function parameters
    template <typename F>
    void CallBuiltinHelper(StringStream& out,
                           const ast::CallExpression* call,
                           const sem::BuiltinFn* builtin,
                           F&& build);

    /// Create a uint type corresponding to the given bool or bool vector type.
    /// @param type the bool or bool vector type to convert
    /// @returns the corresponding uint type
    core::type::Type* BoolTypeToUint(const core::type::Type* type);

    /// @param s the structure
    /// @returns the name of the structure, taking special care of builtin structures that start
    /// with double underscores. If the structure is a builtin, then the returned name will be a
    /// unique name without the leading underscores.
    std::string StructName(const core::type::Struct* s);

    /// @return a new, unique identifier with the given prefix.
    /// @param prefix optional prefix to apply to the generated identifier. If empty "tint_symbol"
    /// will be used.
    std::string UniqueIdentifier(const std::string& prefix = "");

    /// Alias for builder_.TypeOf(ptr)
    template <typename T>
    auto TypeOf(T* ptr) {
        return builder_.TypeOf(ptr);
    }

    ProgramBuilder builder_;

    /// Helper functions emitted at the top of the output
    TextBuffer helpers_;

    /// Map of builtin structure to unique generated name
    std::unordered_map<const core::type::Struct*, std::string> builtin_struct_names_;
    std::function<void()> emit_continuing_;
    std::unordered_map<const sem::BuiltinFn*, std::string> builtins_;
    std::unordered_map<const core::type::Vector*, std::string> dynamic_vector_write_;
    std::unordered_map<const core::type::Vector*, std::string> int_dot_funcs_;
    std::unordered_map<BinaryOperandType, std::string> float_modulo_funcs_;
    // Polyfill functions for bitcast expression, BinaryOperandType indicates the source type and
    // the destination type
    std::unordered_map<BinaryOperandType, std::string> bitcast_funcs_;
    std::unordered_set<const core::type::Struct*> emitted_structs_;
    bool requires_oes_sample_variables_ = false;
    bool requires_default_precision_qualifier_ = false;
    bool requires_f16_extension_ = false;
    bool requires_dual_source_blending_extension_ = false;
    Version version_;
};

}  // namespace tint::glsl::writer

#endif  // SRC_TINT_LANG_GLSL_WRITER_AST_PRINTER_AST_PRINTER_H_
