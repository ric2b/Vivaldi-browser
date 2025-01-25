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

#include "src/tint/cmd/fuzz/wgsl/fuzz.h"
#include "src/tint/lang/wgsl/ast/transform/offset_first_index.h"

namespace tint::ast::transform {
namespace {

bool CanRun(const Program& program,
            const fuzz::wgsl::Context& context,
            const OffsetFirstIndex::Config& config) {
    if (context.program_properties.Contains(
            fuzz::wgsl::ProgramProperties::kAddressSpacesShadowed) ||
        context.program_properties.Contains(fuzz::wgsl::ProgramProperties::kBuiltinTypesShadowed)) {
        return false;  // OffsetFirstIndex assumes the Renamer transform has been run
    }

    if ((config.first_instance_offset.value_or(0) & 3) != 0 ||
        (config.first_vertex_offset.value_or(0) & 3) != 0) {
        return false;  // Misaligned
    }

    if (config.first_instance_offset.has_value() &&
        config.first_instance_offset == config.first_vertex_offset) {
        return false;  // Offset collision
    }
    return true;
}

void OffsetFirstIndexFuzzer(const Program& program,
                            const fuzz::wgsl::Context& context,
                            const OffsetFirstIndex::Config& config) {
    if (!CanRun(program, context, config)) {
        return;
    }

    DataMap inputs;
    inputs.Add<OffsetFirstIndex::Config>(std::move(config));

    DataMap outputs;
    if (auto result = OffsetFirstIndex{}.Apply(program, inputs, outputs)) {
        if (!result->IsValid()) {
            TINT_ICE() << "OffsetFirstIndex returned invalid program:\n"
                       << Program::printer(*result) << "\n"
                       << result->Diagnostics();
        }
    }
}

}  // namespace
}  // namespace tint::ast::transform

TINT_WGSL_PROGRAM_FUZZER(tint::ast::transform::OffsetFirstIndexFuzzer);
