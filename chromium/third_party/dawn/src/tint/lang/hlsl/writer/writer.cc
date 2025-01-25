// Copyright 2020 The Dawn & Tint Authors
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

#include "src/tint/lang/hlsl/writer/writer.h"

#include <memory>
#include <utility>

#include "src/tint/lang/core/ir/function.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/hlsl/writer/ast_printer/ast_printer.h"
#include "src/tint/lang/hlsl/writer/printer/printer.h"
#include "src/tint/lang/hlsl/writer/raise/raise.h"
#include "src/tint/lang/wgsl/ast/pipeline_stage.h"
#include "src/tint/utils/ice/ice.h"

namespace tint::hlsl::writer {
namespace {

ast::PipelineStage ir_to_ast_stage(core::ir::Function::PipelineStage stage) {
    switch (stage) {
        case core::ir::Function::PipelineStage::kCompute:
            return ast::PipelineStage::kCompute;
        case core::ir::Function::PipelineStage::kFragment:
            return ast::PipelineStage::kFragment;
        case core::ir::Function::PipelineStage::kVertex:
            return ast::PipelineStage::kVertex;
        default:
            break;
    }
    TINT_UNREACHABLE();
}

}  // namespace

Result<Output> Generate(core::ir::Module& ir, const Options& options) {
    Output output;

    // Raise the core-dialect to HLSL-dialect
    auto res = Raise(ir, options);
    if (res != Success) {
        return res.Failure();
    }

    auto result = Print(ir);
    if (result != Success) {
        return result.Failure();
    }
    output.hlsl = result->hlsl;

    // Collect the list of entry points in the generated program.
    for (auto func : ir.functions) {
        if (func->Stage() != core::ir::Function::PipelineStage::kUndefined) {
            auto name = ir.NameOf(func).Name();
            output.entry_points.push_back({name, ir_to_ast_stage(func->Stage())});
        }
    }

    return output;
}

Result<Output> Generate(const Program& program, const Options& options) {
    if (!program.IsValid()) {
        return Failure{program.Diagnostics()};
    }

    // Sanitize the program.
    auto sanitized_result = Sanitize(program, options);
    if (!sanitized_result.program.IsValid()) {
        return Failure{sanitized_result.program.Diagnostics()};
    }

    // Generate the HLSL code.
    auto impl = std::make_unique<ASTPrinter>(sanitized_result.program);
    if (!impl->Generate()) {
        return Failure{impl->Diagnostics()};
    }

    Output output;
    output.hlsl = impl->Result();

    // Collect the list of entry points in the sanitized program.
    for (auto* func : sanitized_result.program.AST().Functions()) {
        if (func->IsEntryPoint()) {
            auto name = func->name->symbol.Name();
            output.entry_points.push_back({name, func->PipelineStage()});
        }
    }

    output.used_array_length_from_uniform_indices =
        std::move(sanitized_result.used_array_length_from_uniform_indices);

    return output;
}

}  // namespace tint::hlsl::writer
