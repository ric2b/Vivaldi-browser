// Copyright 2018 The Dawn & Tint Authors
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

#include "dawn/native/vulkan/ShaderModuleVk.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "dawn/common/HashUtils.h"
#include "dawn/common/MatchVariant.h"
#include "dawn/native/CacheRequest.h"
#include "dawn/native/PhysicalDevice.h"
#include "dawn/native/Serializable.h"
#include "dawn/native/TintUtils.h"
#include "dawn/native/vulkan/BindGroupLayoutVk.h"
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/FencedDeleter.h"
#include "dawn/native/vulkan/PhysicalDeviceVk.h"
#include "dawn/native/vulkan/PipelineLayoutVk.h"
#include "dawn/native/vulkan/UtilsVulkan.h"
#include "dawn/native/vulkan/VulkanError.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/metrics/HistogramMacros.h"
#include "dawn/platform/tracing/TraceEvent.h"
#include "partition_alloc/pointers/raw_ptr.h"
#include "tint/tint.h"

#ifdef DAWN_ENABLE_SPIRV_VALIDATION
#include "dawn/native/SpirvValidation.h"
#endif

namespace dawn::native::vulkan {

#define COMPILED_SPIRV_MEMBERS(X)   \
    X(std::vector<uint32_t>, spirv) \
    X(std::string, remappedEntryPoint)

// Represents the result and metadata for a SPIR-V compilation.
DAWN_SERIALIZABLE(struct, CompiledSpirv, COMPILED_SPIRV_MEMBERS){};
#undef COMPILED_SPIRV_MEMBERS

bool TransformedShaderModuleCacheKey::operator==(
    const TransformedShaderModuleCacheKey& other) const {
    if (layoutPtr != other.layoutPtr || entryPoint != other.entryPoint ||
        constants.size() != other.constants.size()) {
        return false;
    }
    if (!std::equal(constants.begin(), constants.end(), other.constants.begin())) {
        return false;
    }
    if (maxSubgroupSizeForFullSubgroups != other.maxSubgroupSizeForFullSubgroups) {
        return false;
    }
    if (emitPointSize != other.emitPointSize) {
        return false;
    }
    return true;
}

size_t TransformedShaderModuleCacheKeyHashFunc::operator()(
    const TransformedShaderModuleCacheKey& key) const {
    size_t hash = 0;
    HashCombine(&hash, key.layoutPtr, key.entryPoint, key.emitPointSize);
    for (const auto& entry : key.constants) {
        HashCombine(&hash, entry.first, entry.second);
    }
    return hash;
}

class ShaderModule::ConcurrentTransformedShaderModuleCache {
  public:
    explicit ConcurrentTransformedShaderModuleCache(Device* device) : mDevice(device) {}

    ~ConcurrentTransformedShaderModuleCache() {
        std::lock_guard<std::mutex> lock(mMutex);

        for (const auto& [_, moduleAndSpirv] : mTransformedShaderModuleCache) {
            mDevice->GetFencedDeleter()->DeleteWhenUnused(moduleAndSpirv.vkModule);
        }
    }

    std::optional<ModuleAndSpirv> Find(const TransformedShaderModuleCacheKey& key) {
        std::lock_guard<std::mutex> lock(mMutex);

        auto iter = mTransformedShaderModuleCache.find(key);
        if (iter != mTransformedShaderModuleCache.end()) {
            return iter->second.AsRefs();
        }
        return {};
    }
    ModuleAndSpirv AddOrGet(const TransformedShaderModuleCacheKey& key,
                            VkShaderModule module,
                            CompiledSpirv compilation,
                            bool hasInputAttachment) {
        DAWN_ASSERT(module != VK_NULL_HANDLE);
        std::lock_guard<std::mutex> lock(mMutex);

        auto iter = mTransformedShaderModuleCache.find(key);
        if (iter == mTransformedShaderModuleCache.end()) {
            bool added = false;
            std::tie(iter, added) = mTransformedShaderModuleCache.emplace(
                key, Entry{module, std::move(compilation.spirv),
                           std::move(compilation.remappedEntryPoint), hasInputAttachment});
            DAWN_ASSERT(added);
        } else {
            // No need to use FencedDeleter since this shader module was just created and does
            // not need to wait for queue operations to complete.
            // Also, use of fenced deleter here is not thread safe.
            mDevice->fn.DestroyShaderModule(mDevice->GetVkDevice(), module, nullptr);
        }
        return iter->second.AsRefs();
    }

  private:
    struct Entry {
        VkShaderModule vkModule;
        std::vector<uint32_t> spirv;
        std::string remappedEntryPoint;
        bool hasInputAttachment;

        ModuleAndSpirv AsRefs() const {
            return {
                vkModule,           spirv.data(), spirv.size(), remappedEntryPoint.c_str(),
                hasInputAttachment,
            };
        }
    };

    raw_ptr<Device> mDevice;
    std::mutex mMutex;
    absl::flat_hash_map<TransformedShaderModuleCacheKey,
                        Entry,
                        TransformedShaderModuleCacheKeyHashFunc>
        mTransformedShaderModuleCache;
};

// static
ResultOrError<Ref<ShaderModule>> ShaderModule::Create(
    Device* device,
    const UnpackedPtr<ShaderModuleDescriptor>& descriptor,
    const std::vector<tint::wgsl::Extension>& internalExtensions,
    ShaderModuleParseResult* parseResult,
    OwnedCompilationMessages* compilationMessages) {
    Ref<ShaderModule> module = AcquireRef(new ShaderModule(device, descriptor, internalExtensions));
    DAWN_TRY(module->Initialize(parseResult, compilationMessages));
    return module;
}

ShaderModule::ShaderModule(Device* device,
                           const UnpackedPtr<ShaderModuleDescriptor>& descriptor,
                           std::vector<tint::wgsl::Extension> internalExtensions)
    : ShaderModuleBase(device, descriptor, std::move(internalExtensions)),
      mTransformedShaderModuleCache(
          std::make_unique<ConcurrentTransformedShaderModuleCache>(device)) {}

MaybeError ShaderModule::Initialize(ShaderModuleParseResult* parseResult,
                                    OwnedCompilationMessages* compilationMessages) {
    ScopedTintICEHandler scopedICEHandler(GetDevice());
    return InitializeBase(parseResult, compilationMessages);
}

void ShaderModule::DestroyImpl() {
    ShaderModuleBase::DestroyImpl();
    // Remove reference to internal cache to trigger cleanup.
    mTransformedShaderModuleCache = nullptr;
}

ShaderModule::~ShaderModule() = default;

#if TINT_BUILD_SPV_WRITER

#define SPIRV_COMPILATION_REQUEST_MEMBERS(X)                                                     \
    X(SingleShaderStage, stage)                                                                  \
    X(const tint::Program*, inputProgram)                                                        \
    X(std::optional<tint::ast::transform::SubstituteOverride::Config>, substituteOverrideConfig) \
    X(LimitsForCompilationRequest, limits)                                                       \
    X(std::string_view, entryPointName)                                                          \
    X(bool, disableSymbolRenaming)                                                               \
    X(tint::spirv::writer::Options, tintOptions)                                                 \
    X(bool, use_tint_ir)                                                                         \
    X(CacheKey::UnsafeUnkeyedValue<dawn::platform::Platform*>, platform)                         \
    X(std::optional<uint32_t>, maxSubgroupSizeForFullSubgroups)

DAWN_MAKE_CACHE_REQUEST(SpirvCompilationRequest, SPIRV_COMPILATION_REQUEST_MEMBERS);
#undef SPIRV_COMPILATION_REQUEST_MEMBERS

#endif  // TINT_BUILD_SPV_WRITER

ResultOrError<ShaderModule::ModuleAndSpirv> ShaderModule::GetHandleAndSpirv(
    SingleShaderStage stage,
    const ProgrammableStage& programmableStage,
    const PipelineLayout* layout,
    bool clampFragDepth,
    bool emitPointSize,
    std::optional<uint32_t> maxSubgroupSizeForFullSubgroups) {
    TRACE_EVENT0(GetDevice()->GetPlatform(), General, "ShaderModuleVk::GetHandleAndSpirv");

    ScopedTintICEHandler scopedICEHandler(GetDevice());

    // Check to see if we have the handle and spirv cached already
    // TODO(chromium:345359083): Improve the computation of the cache key. For example, it isn't
    // ideal to use `reinterpret_cast<uintptr_t>(layout)` as the layout may be freed and
    // reallocated during the runtime.
    auto cacheKey = TransformedShaderModuleCacheKey{
        reinterpret_cast<uintptr_t>(layout), programmableStage.entryPoint.c_str(),
        programmableStage.constants, maxSubgroupSizeForFullSubgroups, emitPointSize};
    auto handleAndSpirv = mTransformedShaderModuleCache->Find(cacheKey);
    if (handleAndSpirv.has_value()) {
        return std::move(*handleAndSpirv);
    }

#if TINT_BUILD_SPV_WRITER
    // Creation of module and spirv is deferred to this point when using tint generator

    tint::spirv::writer::Bindings bindings;

    const BindingInfoArray& moduleBindingInfo =
        GetEntryPoint(programmableStage.entryPoint.c_str()).bindings;

    for (BindGroupIndex group : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
        const BindGroupLayout* bgl = ToBackend(layout->GetBindGroupLayout(group));

        for (const auto& currentModuleBindingInfo : moduleBindingInfo[group]) {
            // We cannot use structured binding here because lambda expressions can only capture
            // variables, while structured binding doesn't introduce variables.
            const auto& binding = currentModuleBindingInfo.first;
            const auto& shaderBindingInfo = currentModuleBindingInfo.second;

            tint::BindingPoint srcBindingPoint{static_cast<uint32_t>(group),
                                               static_cast<uint32_t>(binding)};

            tint::BindingPoint dstBindingPoint{
                static_cast<uint32_t>(group), static_cast<uint32_t>(bgl->GetBindingIndex(binding))};

            MatchVariant(
                shaderBindingInfo.bindingInfo,
                [&](const BufferBindingInfo& bindingInfo) {
                    switch (bindingInfo.type) {
                        case wgpu::BufferBindingType::Uniform:
                            bindings.uniform.emplace(
                                srcBindingPoint,
                                tint::spirv::writer::binding::Uniform{dstBindingPoint.group,
                                                                      dstBindingPoint.binding});
                            break;
                        case kInternalStorageBufferBinding:
                        case wgpu::BufferBindingType::Storage:
                        case wgpu::BufferBindingType::ReadOnlyStorage:
                            bindings.storage.emplace(
                                srcBindingPoint,
                                tint::spirv::writer::binding::Storage{dstBindingPoint.group,
                                                                      dstBindingPoint.binding});
                            break;
                        case wgpu::BufferBindingType::Undefined:
                            DAWN_UNREACHABLE();
                            break;
                    }
                },
                [&](const SamplerBindingInfo& bindingInfo) {
                    bindings.sampler.emplace(srcBindingPoint,
                                             tint::spirv::writer::binding::Sampler{
                                                 dstBindingPoint.group, dstBindingPoint.binding});
                },
                [&](const TextureBindingInfo& bindingInfo) {
                    bindings.texture.emplace(srcBindingPoint,
                                             tint::spirv::writer::binding::Texture{
                                                 dstBindingPoint.group, dstBindingPoint.binding});
                },
                [&](const StorageTextureBindingInfo& bindingInfo) {
                    bindings.storage_texture.emplace(
                        srcBindingPoint, tint::spirv::writer::binding::StorageTexture{
                                             dstBindingPoint.group, dstBindingPoint.binding});
                },
                [&](const ExternalTextureBindingInfo& bindingInfo) {
                    const auto& bindingMap = bgl->GetExternalTextureBindingExpansionMap();
                    const auto& expansion = bindingMap.find(binding);
                    DAWN_ASSERT(expansion != bindingMap.end());

                    const auto& bindingExpansion = expansion->second;
                    tint::spirv::writer::binding::BindingInfo plane0{
                        static_cast<uint32_t>(group),
                        static_cast<uint32_t>(bgl->GetBindingIndex(bindingExpansion.plane0))};
                    tint::spirv::writer::binding::BindingInfo plane1{
                        static_cast<uint32_t>(group),
                        static_cast<uint32_t>(bgl->GetBindingIndex(bindingExpansion.plane1))};
                    tint::spirv::writer::binding::BindingInfo metadata{
                        static_cast<uint32_t>(group),
                        static_cast<uint32_t>(bgl->GetBindingIndex(bindingExpansion.params))};

                    bindings.external_texture.emplace(
                        srcBindingPoint,
                        tint::spirv::writer::binding::ExternalTexture{metadata, plane0, plane1});
                },
                [&](const InputAttachmentBindingInfo& bindingInfo) {
                    bindings.input_attachment.emplace(
                        srcBindingPoint, tint::spirv::writer::binding::InputAttachment{
                                             dstBindingPoint.group, dstBindingPoint.binding});
                });
        }
    }

    const bool hasInputAttachment = !bindings.input_attachment.empty();

    std::optional<tint::ast::transform::SubstituteOverride::Config> substituteOverrideConfig;
    if (!programmableStage.metadata->overrides.empty()) {
        substituteOverrideConfig = BuildSubstituteOverridesTransformConfig(programmableStage);
    }

    SpirvCompilationRequest req = {};
    req.stage = stage;
    auto tintProgram = GetTintProgram();
    req.inputProgram = &(tintProgram->program);
    req.entryPointName = programmableStage.entryPoint;
    req.disableSymbolRenaming = GetDevice()->IsToggleEnabled(Toggle::DisableSymbolRenaming);
    req.platform = UnsafeUnkeyedValue(GetDevice()->GetPlatform());
    req.substituteOverrideConfig = std::move(substituteOverrideConfig);
    req.maxSubgroupSizeForFullSubgroups = maxSubgroupSizeForFullSubgroups;

    req.tintOptions.clamp_frag_depth = clampFragDepth;
    req.tintOptions.disable_robustness = !GetDevice()->IsRobustnessEnabled();
    req.tintOptions.emit_vertex_point_size = emitPointSize;
    req.tintOptions.disable_workgroup_init =
        GetDevice()->IsToggleEnabled(Toggle::DisableWorkgroupInit);
    req.tintOptions.use_zero_initialize_workgroup_memory_extension =
        GetDevice()->IsToggleEnabled(Toggle::VulkanUseZeroInitializeWorkgroupMemoryExtension);
    req.tintOptions.use_storage_input_output_16 =
        GetDevice()->IsToggleEnabled(Toggle::VulkanUseStorageInputOutput16);
    req.tintOptions.bindings = std::move(bindings);
    req.tintOptions.disable_image_robustness =
        GetDevice()->IsToggleEnabled(Toggle::VulkanUseImageRobustAccess2);
    // Currently we can disable index clamping on all runtime-sized arrays in Tint robustness
    // transform as unsized arrays can only be declared on storage address space.
    req.tintOptions.disable_runtime_sized_array_index_clamping =
        GetDevice()->IsToggleEnabled(Toggle::VulkanUseBufferRobustAccess2);
    req.tintOptions.polyfill_dot_4x8_packed =
        GetDevice()->IsToggleEnabled(Toggle::PolyFillPacked4x8DotProduct);
    req.use_tint_ir = GetDevice()->IsToggleEnabled(Toggle::UseTintIR);
    req.tintOptions.disable_polyfill_integer_div_mod =
        GetDevice()->IsToggleEnabled(Toggle::DisablePolyfillsOnIntegerDivisonAndModulo);

    // Set subgroup uniform control flow flag for subgroup experiment, if device has
    // Chromium-experimental-subgroup-uniform-control-flow feature. (dawn:464)
    if (GetDevice()->HasFeature(Feature::ChromiumExperimentalSubgroupUniformControlFlow)) {
        req.tintOptions.experimental_require_subgroup_uniform_control_flow = true;
    } else {
        req.tintOptions.experimental_require_subgroup_uniform_control_flow = false;
    }
    // Pass matrices to user functions by pointer on Qualcomm devices to workaround a known bug.
    // See crbug.com/tint/2045.
    if (ToBackend(GetDevice()->GetPhysicalDevice())->IsAndroidQualcomm()) {
        req.tintOptions.pass_matrix_by_pointer = true;
    }

    const CombinedLimits& limits = GetDevice()->GetLimits();
    req.limits = LimitsForCompilationRequest::Create(limits.v1);

    CacheResult<CompiledSpirv> compilation;
    DAWN_TRY_LOAD_OR_RUN(
        compilation, GetDevice(), std::move(req), CompiledSpirv::FromBlob,
        [](SpirvCompilationRequest r) -> ResultOrError<CompiledSpirv> {
            tint::ast::transform::Manager transformManager;
            tint::ast::transform::DataMap transformInputs;

            // Many Vulkan drivers can't handle multi-entrypoint shader modules.
            // Run before the renamer so that the entry point name matches `entryPointName` still.
            transformManager.append(std::make_unique<tint::ast::transform::SingleEntryPoint>());
            transformInputs.Add<tint::ast::transform::SingleEntryPoint::Config>(
                std::string(r.entryPointName));

            // Needs to run before all other transforms so that they can use builtin names safely.
            if (!r.disableSymbolRenaming) {
                transformManager.Add<tint::ast::transform::Renamer>();
            }

            if (r.substituteOverrideConfig) {
                // This needs to run after SingleEntryPoint transform which removes unused overrides
                // for current entry point.
                transformManager.Add<tint::ast::transform::SubstituteOverride>();
                transformInputs.Add<tint::ast::transform::SubstituteOverride::Config>(
                    std::move(r.substituteOverrideConfig).value());
            }

            tint::Program program;
            tint::ast::transform::DataMap transformOutputs;
            {
                TRACE_EVENT0(r.platform.UnsafeGetValue(), General, "RunTransforms");
                DAWN_TRY_ASSIGN(program,
                                RunTransforms(&transformManager, r.inputProgram, transformInputs,
                                              &transformOutputs, nullptr));
            }

            // Get the entry point name after the renamer pass.
            // TODO(dawn:2180): refactor out.
            std::string remappedEntryPoint;
            if (r.disableSymbolRenaming) {
                remappedEntryPoint = r.entryPointName;
            } else {
                auto* data = transformOutputs.Get<tint::ast::transform::Renamer::Data>();
                DAWN_ASSERT(data != nullptr);

                auto it = data->remappings.find(r.entryPointName.data());
                DAWN_ASSERT(it != data->remappings.end());
                remappedEntryPoint = it->second;
            }
            DAWN_ASSERT(remappedEntryPoint != "");

            // Validate workgroup size after program runs transforms.
            if (r.stage == SingleShaderStage::Compute) {
                Extent3D _;
                DAWN_TRY_ASSIGN(_, ValidateComputeStageWorkgroupSize(
                                       program, remappedEntryPoint.c_str(), r.limits,
                                       r.maxSubgroupSizeForFullSubgroups));
            }

            TRACE_EVENT0(r.platform.UnsafeGetValue(), General, "tint::spirv::writer::Generate()");
            tint::Result<tint::spirv::writer::Output> tintResult;
            if (r.use_tint_ir) {
                // Convert the AST program to an IR module.
                auto ir = tint::wgsl::reader::ProgramToLoweredIR(program);
                DAWN_INVALID_IF(ir != tint::Success,
                                "An error occurred while generating Tint IR\n%s",
                                ir.Failure().reason.Str());

                tintResult = tint::spirv::writer::Generate(ir.Get(), r.tintOptions);
            } else {
                tintResult = tint::spirv::writer::Generate(program, r.tintOptions);
            }
            DAWN_INVALID_IF(tintResult != tint::Success,
                            "An error occurred while generating SPIR-V\n%s",
                            tintResult.Failure().reason.Str());

            CompiledSpirv result;
            result.spirv = std::move(tintResult.Get().spirv);
            result.remappedEntryPoint = remappedEntryPoint;
            return result;
        },
        "Vulkan.CompileShaderToSPIRV");

#ifdef DAWN_ENABLE_SPIRV_VALIDATION
    DAWN_TRY(ValidateSpirv(GetDevice(), compilation->spirv.data(), compilation->spirv.size(),
                           GetDevice()->IsToggleEnabled(Toggle::DumpShaders)));
#endif

    VkShaderModuleCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.codeSize = compilation->spirv.size() * sizeof(uint32_t);
    createInfo.pCode = compilation->spirv.data();

    Device* device = ToBackend(GetDevice());

    VkShaderModule newHandle = VK_NULL_HANDLE;
    {
        TRACE_EVENT0(GetDevice()->GetPlatform(), General, "vkCreateShaderModule");
        DAWN_TRY(CheckVkSuccess(
            device->fn.CreateShaderModule(device->GetVkDevice(), &createInfo, nullptr, &*newHandle),
            "CreateShaderModule"));
    }

    ModuleAndSpirv moduleAndSpirv;
    if (newHandle != VK_NULL_HANDLE) {
        device->GetBlobCache()->EnsureStored(compilation);

        // Set the label on `newHandle` now, and not on `moduleAndSpirv.module` later
        // since `moduleAndSpirv.module` may be in use by multiple threads.
        SetDebugName(ToBackend(GetDevice()), newHandle, "Dawn_ShaderModule", GetLabel());
        moduleAndSpirv = mTransformedShaderModuleCache->AddOrGet(
            cacheKey, newHandle, compilation.Acquire(), hasInputAttachment);
    }

    return std::move(moduleAndSpirv);
#else
    return DAWN_INTERNAL_ERROR("TINT_BUILD_SPV_WRITER is not defined.");
#endif
}

}  // namespace dawn::native::vulkan
