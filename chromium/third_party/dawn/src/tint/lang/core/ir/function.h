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

#ifndef SRC_TINT_LANG_CORE_IR_FUNCTION_H_
#define SRC_TINT_LANG_CORE_IR_FUNCTION_H_

#include <array>
#include <optional>
#include <utility>

#include "src/tint/lang/core/io_attributes.h"
#include "src/tint/lang/core/ir/function_param.h"
#include "src/tint/lang/core/ir/value.h"
#include "src/tint/lang/core/type/type.h"
#include "src/tint/utils/containers/const_propagating_ptr.h"
#include "src/tint/utils/ice/ice.h"

// Forward declarations
namespace tint::core::ir {
class Block;
class FunctionTerminator;
}  // namespace tint::core::ir

namespace tint::core::ir {

/// An IR representation of a function
class Function : public Castable<Function, Value> {
  public:
    /// The pipeline stage for an entry point
    enum class PipelineStage {
        /// Not a pipeline entry point
        kUndefined,
        /// Vertex
        kCompute,
        /// Fragment
        kFragment,
        /// Vertex
        kVertex,
    };

    /// Constructor
    Function();

    /// Constructor
    /// @param rt the function return type
    /// @param stage the function stage
    /// @param wg_size the workgroup_size
    Function(const core::type::Type* rt,
             PipelineStage stage = PipelineStage::kUndefined,
             std::optional<std::array<uint32_t, 3>> wg_size = {});
    ~Function() override;

    /// @copydoc Instruction::Clone()
    Function* Clone(CloneContext& ctx) override;

    /// Sets the function stage
    /// @param stage the stage to set
    void SetStage(PipelineStage stage) { pipeline_stage_ = stage; }

    /// @returns the function pipeline stage
    PipelineStage Stage() const { return pipeline_stage_; }

    /// Sets the workgroup size
    /// @param x the x size
    /// @param y the y size
    /// @param z the z size
    void SetWorkgroupSize(uint32_t x, uint32_t y, uint32_t z) { workgroup_size_ = {x, y, z}; }

    /// Sets the workgroup size
    /// @param size the new size
    void SetWorkgroupSize(std::array<uint32_t, 3> size) { workgroup_size_ = size; }

    /// Clears the workgroup size.
    void ClearWorkgroupSize() { workgroup_size_ = {}; }

    /// @returns the workgroup size information
    std::optional<std::array<uint32_t, 3>> WorkgroupSize() const { return workgroup_size_; }

    /// @param type the return type for the function
    void SetReturnType(const core::type::Type* type) { return_.type = type; }

    /// @returns the return type for the function
    const core::type::Type* ReturnType() const { return return_.type; }

    /// Sets the return IO attributes.
    /// @param attrs the attributes
    void SetReturnAttributes(const IOAttributes& attrs) { return_.attributes = attrs; }
    /// @returns the return IO attributes
    const IOAttributes& ReturnAttributes() const { return return_.attributes; }

    /// Sets the return attributes
    /// @param builtin the builtin to set
    void SetReturnBuiltin(BuiltinValue builtin) {
        TINT_ASSERT(!return_.attributes.builtin.has_value());
        return_.attributes.builtin = builtin;
    }
    /// @returns the return builtin attribute
    std::optional<BuiltinValue> ReturnBuiltin() const { return return_.attributes.builtin; }

    /// Sets the return location.
    /// @param loc the optional location to set
    void SetReturnLocation(std::optional<uint32_t> loc) { return_.attributes.location = loc; }

    /// @returns the return location
    std::optional<uint32_t> ReturnLocation() const { return return_.attributes.location; }

    /// Sets the return interpolation.
    /// @param interp the optional interpolation
    void SetReturnInterpolation(std::optional<core::Interpolation> interp) {
        return_.attributes.interpolation = interp;
    }

    /// @returns the return interpolation
    std::optional<Interpolation> ReturnInterpolation() const {
        return return_.attributes.interpolation;
    }

    /// Sets the return as invariant
    /// @param val the invariant value to set
    void SetReturnInvariant(bool val) { return_.attributes.invariant = val; }

    /// @returns the return invariant value
    bool ReturnInvariant() const { return return_.attributes.invariant; }

    /// Sets the function parameters
    /// @param params the function parameters
    void SetParams(VectorRef<FunctionParam*> params);

    /// Sets the function parameters
    /// @param params the function parameters
    void SetParams(std::initializer_list<FunctionParam*> params);

    /// Appends a new function parameter.
    /// @param param the function parameter to append
    void AppendParam(FunctionParam* param);

    /// @returns the function parameters
    const VectorRef<FunctionParam*> Params() { return params_; }

    /// @returns the function parameters
    VectorRef<const FunctionParam*> Params() const { return params_; }

    /// Sets the root block for the function
    /// @param target the root block
    void SetBlock(Block* target) {
        TINT_ASSERT(target != nullptr);
        block_ = target;
    }

    /// @returns the function root block
    ir::Block* Block() { return block_; }

    /// @returns the function root block
    const ir::Block* Block() const { return block_; }

    /// Destroys the function and all of its instructions.
    void Destroy() override;

  private:
    PipelineStage pipeline_stage_ = PipelineStage::kUndefined;
    std::optional<std::array<uint32_t, 3>> workgroup_size_;

    struct {
        const core::type::Type* type = nullptr;
        IOAttributes attributes = {};
    } return_;

    Vector<FunctionParam*, 1> params_;
    ConstPropagatingPtr<ir::Block> block_;
};

/// @param value the enum value
/// @returns the string for the given enum value
std::string_view ToString(Function::PipelineStage value);

/// @param out the stream to write to
/// @param value the Function::PipelineStage
/// @returns @p out so calls can be chained
template <typename STREAM, typename = traits::EnableIfIsOStream<STREAM>>
auto& operator<<(STREAM& out, Function::PipelineStage value) {
    return out << ToString(value);
}

}  // namespace tint::core::ir

#endif  // SRC_TINT_LANG_CORE_IR_FUNCTION_H_
