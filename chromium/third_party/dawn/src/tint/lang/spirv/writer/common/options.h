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

#ifndef SRC_TINT_LANG_SPIRV_WRITER_COMMON_OPTIONS_H_
#define SRC_TINT_LANG_SPIRV_WRITER_COMMON_OPTIONS_H_

#include <unordered_map>

#include "src/tint/api/common/binding_point.h"
#include "src/tint/utils/reflection/reflection.h"

namespace tint::spirv::writer {
namespace binding {

/// Generic binding point
struct BindingInfo {
    /// The group
    uint32_t group = 0;
    /// The binding
    uint32_t binding = 0;

    /// Equality operator
    /// @param rhs the BindingInfo to compare against
    /// @returns true if this BindingInfo is equal to `rhs`
    inline bool operator==(const BindingInfo& rhs) const {
        return group == rhs.group && binding == rhs.binding;
    }
    /// Inequality operator
    /// @param rhs the BindingInfo to compare against
    /// @returns true if this BindingInfo is not equal to `rhs`
    inline bool operator!=(const BindingInfo& rhs) const { return !(*this == rhs); }

    /// @returns the hash code of the BindingInfo
    tint::HashCode HashCode() const { return Hash(group, binding); }

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(BindingInfo, group, binding);
};

using Uniform = BindingInfo;
using Storage = BindingInfo;
using Texture = BindingInfo;
using StorageTexture = BindingInfo;
using Sampler = BindingInfo;
using InputAttachment = BindingInfo;

/// An external texture
struct ExternalTexture {
    /// Metadata
    BindingInfo metadata{};
    /// Plane0 binding data
    BindingInfo plane0{};
    /// Plane1 binding data
    BindingInfo plane1{};

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(ExternalTexture, metadata, plane0, plane1);
};

}  // namespace binding

// Maps the WGSL binding point to the SPIR-V group,binding for uniforms
using UniformBindings = std::unordered_map<BindingPoint, binding::Uniform>;
// Maps the WGSL binding point to the SPIR-V group,binding for storage
using StorageBindings = std::unordered_map<BindingPoint, binding::Storage>;
// Maps the WGSL binding point to the SPIR-V group,binding for textures
using TextureBindings = std::unordered_map<BindingPoint, binding::Texture>;
// Maps the WGSL binding point to the SPIR-V group,binding for storage textures
using StorageTextureBindings = std::unordered_map<BindingPoint, binding::StorageTexture>;
// Maps the WGSL binding point to the SPIR-V group,binding for samplers
using SamplerBindings = std::unordered_map<BindingPoint, binding::Sampler>;
// Maps the WGSL binding point to the plane0, plane1, and metadata information for external textures
using ExternalTextureBindings = std::unordered_map<BindingPoint, binding::ExternalTexture>;
// Maps the WGSL binding point to the SPIR-V group,binding for input attachments
using InputAttachmentBindings = std::unordered_map<BindingPoint, binding::InputAttachment>;

/// Binding information
struct Bindings {
    /// Uniform bindings
    UniformBindings uniform{};
    /// Storage bindings
    StorageBindings storage{};
    /// Texture bindings
    TextureBindings texture{};
    /// Storage texture bindings
    StorageTextureBindings storage_texture{};
    /// Sampler bindings
    SamplerBindings sampler{};
    /// External bindings
    ExternalTextureBindings external_texture{};
    /// Input attachment bindings
    InputAttachmentBindings input_attachment{};

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(Bindings,
                 uniform,
                 storage,
                 texture,
                 storage_texture,
                 sampler,
                 external_texture,
                 input_attachment);
};

/// Configuration options used for generating SPIR-V.
struct Options {
    /// The bindings
    Bindings bindings;

    /// Set to `true` to disable software robustness that prevents out-of-bounds accesses.
    bool disable_robustness = false;

    /// Set to `true` to skip robustness transform on textures.
    bool disable_image_robustness = false;

    /// Set to `true` to disable index clamping on the runtime-sized arrays in robustness transform.
    bool disable_runtime_sized_array_index_clamping = false;

    /// Set to `true` to disable workgroup memory zero initialization
    bool disable_workgroup_init = false;

    /// Set to `true` to initialize workgroup memory with OpConstantNull when
    /// VK_KHR_zero_initialize_workgroup_memory is enabled.
    bool use_zero_initialize_workgroup_memory_extension = false;

    /// Set to `true` to use the StorageInputOutput16 capability for shader IO that uses f16 types.
    bool use_storage_input_output_16 = true;

    /// Set to `true` to generate a PointSize builtin and have it set to 1.0
    /// from all vertex shaders in the module.
    bool emit_vertex_point_size = true;

    /// Set to `true` to clamp frag depth
    bool clamp_frag_depth = false;

    /// Set to `true` to always pass matrices to user functions by pointer instead of by value.
    bool pass_matrix_by_pointer = false;

    /// Set to `true` to require `SPV_KHR_subgroup_uniform_control_flow` extension and
    /// `SubgroupUniformControlFlowKHR` execution mode for compute stage entry points in generated
    /// SPIRV module. Issue: dawn:464
    bool experimental_require_subgroup_uniform_control_flow = false;

    /// Set to `true` to generate polyfill for `dot4I8Packed` and `dot4U8Packed` builtins
    bool polyfill_dot_4x8_packed = false;

    /// Set to `true` to disable the polyfills on integer division and modulo.
    bool disable_polyfill_integer_div_mod = false;

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(Options,
                 bindings,
                 disable_robustness,
                 disable_image_robustness,
                 disable_runtime_sized_array_index_clamping,
                 disable_workgroup_init,
                 use_zero_initialize_workgroup_memory_extension,
                 use_storage_input_output_16,
                 emit_vertex_point_size,
                 clamp_frag_depth,
                 pass_matrix_by_pointer,
                 experimental_require_subgroup_uniform_control_flow,
                 polyfill_dot_4x8_packed,
                 disable_polyfill_integer_div_mod);
};

}  // namespace tint::spirv::writer

#endif  // SRC_TINT_LANG_SPIRV_WRITER_COMMON_OPTIONS_H_
