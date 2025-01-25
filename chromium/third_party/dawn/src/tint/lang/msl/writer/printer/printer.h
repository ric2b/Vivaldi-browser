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

#ifndef SRC_TINT_LANG_MSL_WRITER_PRINTER_PRINTER_H_
#define SRC_TINT_LANG_MSL_WRITER_PRINTER_PRINTER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "src/tint/utils/result/result.h"

// Forward declarations
namespace tint::core::ir {
class Module;
}  // namespace tint::core::ir

namespace tint::msl::writer {

/// The output produced when printing MSL.
struct PrintResult {
    /// Constructor
    PrintResult();

    /// Destructor
    ~PrintResult();

    /// Copy constructor
    PrintResult(const PrintResult&);

    /// Copy assignment
    /// @returns this
    PrintResult& operator=(const PrintResult&);

    /// The generated MSL.
    std::string msl = "";

    /// `true` if an invariant attribute was generated.
    bool has_invariant_attribute = false;

    /// A map from entry point name to a list of dynamic workgroup allocations.
    /// Each element of the vector is the size of the workgroup allocation that should be created
    /// for that index.
    std::unordered_map<std::string, std::vector<uint32_t>> workgroup_allocations;
};

/// @param module the Tint IR module to generate
/// @returns the result of printing the MSL shader on success, or failure
Result<PrintResult> Print(core::ir::Module& module);

}  // namespace tint::msl::writer

#endif  // SRC_TINT_LANG_MSL_WRITER_PRINTER_PRINTER_H_
