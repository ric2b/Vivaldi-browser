/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnGraphicsPipeline.h"

#include "include/gpu/graphite/TextureInfo.h"
#include "include/private/base/SkTemplates.h"
#include "src/gpu/SkSLToBackend.h"
#include "src/gpu/Swizzle.h"
#include "src/gpu/graphite/Attribute.h"
#include "src/gpu/graphite/ContextUtils.h"
#include "src/gpu/graphite/GraphicsPipelineDesc.h"
#include "src/gpu/graphite/Log.h"
#include "src/gpu/graphite/RenderPassDesc.h"
#include "src/gpu/graphite/RendererProvider.h"
#include "src/gpu/graphite/UniformManager.h"
#include "src/gpu/graphite/dawn/DawnCaps.h"
#include "src/gpu/graphite/dawn/DawnErrorChecker.h"
#include "src/gpu/graphite/dawn/DawnGraphiteTypesPriv.h"
#include "src/gpu/graphite/dawn/DawnGraphiteUtilsPriv.h"
#include "src/gpu/graphite/dawn/DawnResourceProvider.h"
#include "src/gpu/graphite/dawn/DawnSharedContext.h"
#include "src/gpu/graphite/dawn/DawnUtilsPriv.h"
#include "src/sksl/SkSLProgramSettings.h"
#include "src/sksl/SkSLUtil.h"
#include "src/sksl/ir/SkSLProgram.h"

#include <vector>

namespace skgpu::graphite {

namespace {

inline wgpu::VertexFormat attribute_type_to_dawn(VertexAttribType type) {
    switch (type) {
        case VertexAttribType::kFloat:
            return wgpu::VertexFormat::Float32;
        case VertexAttribType::kFloat2:
            return wgpu::VertexFormat::Float32x2;
        case VertexAttribType::kFloat3:
            return wgpu::VertexFormat::Float32x3;
        case VertexAttribType::kFloat4:
            return wgpu::VertexFormat::Float32x4;
        case VertexAttribType::kHalf2:
            return wgpu::VertexFormat::Float16x2;
        case VertexAttribType::kHalf4:
            return wgpu::VertexFormat::Float16x4;
        case VertexAttribType::kInt2:
            return wgpu::VertexFormat::Sint32x2;
        case VertexAttribType::kInt3:
            return wgpu::VertexFormat::Sint32x3;
        case VertexAttribType::kInt4:
            return wgpu::VertexFormat::Sint32x4;
        case VertexAttribType::kByte2:
            return wgpu::VertexFormat::Sint8x2;
        case VertexAttribType::kByte4:
            return wgpu::VertexFormat::Sint8x4;
        case VertexAttribType::kUByte2:
            return wgpu::VertexFormat::Uint8x2;
        case VertexAttribType::kUByte4:
            return wgpu::VertexFormat::Uint8x4;
        case VertexAttribType::kUByte4_norm:
            return wgpu::VertexFormat::Unorm8x4;
        case VertexAttribType::kShort2:
            return wgpu::VertexFormat::Sint16x2;
        case VertexAttribType::kShort4:
            return wgpu::VertexFormat::Sint16x4;
        case VertexAttribType::kUShort2:
            return wgpu::VertexFormat::Uint16x2;
        case VertexAttribType::kUShort2_norm:
            return wgpu::VertexFormat::Unorm16x2;
        case VertexAttribType::kInt:
            return wgpu::VertexFormat::Sint32;
        case VertexAttribType::kUInt:
            return wgpu::VertexFormat::Uint32;
        case VertexAttribType::kUShort4_norm:
            return wgpu::VertexFormat::Unorm16x4;
        case VertexAttribType::kHalf:
        case VertexAttribType::kByte:
        case VertexAttribType::kUByte:
        case VertexAttribType::kUByte_norm:
        case VertexAttribType::kUShort_norm:
            // Not supported.
            break;
    }
    SkUNREACHABLE;
}

wgpu::CompareFunction compare_op_to_dawn(CompareOp op) {
    switch (op) {
        case CompareOp::kAlways:
            return wgpu::CompareFunction::Always;
        case CompareOp::kNever:
            return wgpu::CompareFunction::Never;
        case CompareOp::kGreater:
            return wgpu::CompareFunction::Greater;
        case CompareOp::kGEqual:
            return wgpu::CompareFunction::GreaterEqual;
        case CompareOp::kLess:
            return wgpu::CompareFunction::Less;
        case CompareOp::kLEqual:
            return wgpu::CompareFunction::LessEqual;
        case CompareOp::kEqual:
            return wgpu::CompareFunction::Equal;
        case CompareOp::kNotEqual:
            return wgpu::CompareFunction::NotEqual;
    }
    SkUNREACHABLE;
}

wgpu::StencilOperation stencil_op_to_dawn(StencilOp op) {
    switch (op) {
        case StencilOp::kKeep:
            return wgpu::StencilOperation::Keep;
        case StencilOp::kZero:
            return wgpu::StencilOperation::Zero;
        case StencilOp::kReplace:
            return wgpu::StencilOperation::Replace;
        case StencilOp::kInvert:
            return wgpu::StencilOperation::Invert;
        case StencilOp::kIncWrap:
            return wgpu::StencilOperation::IncrementWrap;
        case StencilOp::kDecWrap:
            return wgpu::StencilOperation::DecrementWrap;
        case StencilOp::kIncClamp:
            return wgpu::StencilOperation::IncrementClamp;
        case StencilOp::kDecClamp:
            return wgpu::StencilOperation::DecrementClamp;
    }
    SkUNREACHABLE;
}

wgpu::StencilFaceState stencil_face_to_dawn(DepthStencilSettings::Face face) {
    wgpu::StencilFaceState state;
    state.compare = compare_op_to_dawn(face.fCompareOp);
    state.failOp = stencil_op_to_dawn(face.fStencilFailOp);
    state.depthFailOp = stencil_op_to_dawn(face.fDepthFailOp);
    state.passOp = stencil_op_to_dawn(face.fDepthStencilPassOp);
    return state;
}

size_t create_vertex_attributes(SkSpan<const Attribute> attrs,
                                int shaderLocationOffset,
                                std::vector<wgpu::VertexAttribute>* out) {
    SkASSERT(out && out->empty());
    out->resize(attrs.size());
    size_t vertexAttributeOffset = 0;
    int attributeIndex = 0;
    for (const auto& attr : attrs) {
        wgpu::VertexAttribute& vertexAttribute =  (*out)[attributeIndex];
        vertexAttribute.format = attribute_type_to_dawn(attr.cpuType());
        vertexAttribute.offset = vertexAttributeOffset;
        vertexAttribute.shaderLocation = shaderLocationOffset + attributeIndex;
        vertexAttributeOffset += attr.sizeAlign4();
        attributeIndex++;
    }
    return vertexAttributeOffset;
}

// TODO: share this w/ Ganesh dawn backend?
static wgpu::BlendFactor blend_coeff_to_dawn_blend(const DawnCaps& caps, skgpu::BlendCoeff coeff) {
#if defined(__EMSCRIPTEN__)
#define VALUE_IF_DSB_OR_ZERO(VALUE) wgpu::BlendFactor::Zero
#else
#define VALUE_IF_DSB_OR_ZERO(VALUE) \
    ((caps.shaderCaps()->fDualSourceBlendingSupport) ? (VALUE) : wgpu::BlendFactor::Zero)
#endif
    switch (coeff) {
        case skgpu::BlendCoeff::kZero:
            return wgpu::BlendFactor::Zero;
        case skgpu::BlendCoeff::kOne:
            return wgpu::BlendFactor::One;
        case skgpu::BlendCoeff::kSC:
            return wgpu::BlendFactor::Src;
        case skgpu::BlendCoeff::kISC:
            return wgpu::BlendFactor::OneMinusSrc;
        case skgpu::BlendCoeff::kDC:
            return wgpu::BlendFactor::Dst;
        case skgpu::BlendCoeff::kIDC:
            return wgpu::BlendFactor::OneMinusDst;
        case skgpu::BlendCoeff::kSA:
            return wgpu::BlendFactor::SrcAlpha;
        case skgpu::BlendCoeff::kISA:
            return wgpu::BlendFactor::OneMinusSrcAlpha;
        case skgpu::BlendCoeff::kDA:
            return wgpu::BlendFactor::DstAlpha;
        case skgpu::BlendCoeff::kIDA:
            return wgpu::BlendFactor::OneMinusDstAlpha;
        case skgpu::BlendCoeff::kConstC:
            return wgpu::BlendFactor::Constant;
        case skgpu::BlendCoeff::kIConstC:
            return wgpu::BlendFactor::OneMinusConstant;
        case skgpu::BlendCoeff::kS2C:
            return VALUE_IF_DSB_OR_ZERO(wgpu::BlendFactor::Src1);
        case skgpu::BlendCoeff::kIS2C:
            return VALUE_IF_DSB_OR_ZERO(wgpu::BlendFactor::OneMinusSrc1);
        case skgpu::BlendCoeff::kS2A:
            return VALUE_IF_DSB_OR_ZERO(wgpu::BlendFactor::Src1Alpha);
        case skgpu::BlendCoeff::kIS2A:
            return VALUE_IF_DSB_OR_ZERO(wgpu::BlendFactor::OneMinusSrc1Alpha);
        case skgpu::BlendCoeff::kIllegal:
            return wgpu::BlendFactor::Zero;
    }
    SkUNREACHABLE;
#undef VALUE_IF_DSB_OR_ZERO
}

static wgpu::BlendFactor blend_coeff_to_dawn_blend_for_alpha(const DawnCaps& caps,
                                                             skgpu::BlendCoeff coeff) {
    switch (coeff) {
        // Force all srcColor used in alpha slot to alpha version.
        case skgpu::BlendCoeff::kSC:
            return wgpu::BlendFactor::SrcAlpha;
        case skgpu::BlendCoeff::kISC:
            return wgpu::BlendFactor::OneMinusSrcAlpha;
        case skgpu::BlendCoeff::kDC:
            return wgpu::BlendFactor::DstAlpha;
        case skgpu::BlendCoeff::kIDC:
            return wgpu::BlendFactor::OneMinusDstAlpha;
        default:
            return blend_coeff_to_dawn_blend(caps, coeff);
    }
}

// TODO: share this w/ Ganesh Metal backend?
static wgpu::BlendOperation blend_equation_to_dawn_blend_op(skgpu::BlendEquation equation) {
    static const wgpu::BlendOperation gTable[] = {
            wgpu::BlendOperation::Add,              // skgpu::BlendEquation::kAdd
            wgpu::BlendOperation::Subtract,         // skgpu::BlendEquation::kSubtract
            wgpu::BlendOperation::ReverseSubtract,  // skgpu::BlendEquation::kReverseSubtract
    };
    static_assert(std::size(gTable) == (int)skgpu::BlendEquation::kFirstAdvanced);
    static_assert(0 == (int)skgpu::BlendEquation::kAdd);
    static_assert(1 == (int)skgpu::BlendEquation::kSubtract);
    static_assert(2 == (int)skgpu::BlendEquation::kReverseSubtract);

    SkASSERT((unsigned)equation < skgpu::kBlendEquationCnt);
    return gTable[(int)equation];
}

struct AsyncPipelineCreationBase {
    wgpu::RenderPipeline fRenderPipeline;
    bool fFinished = false;
};

} // anonymous namespace

#if defined(__EMSCRIPTEN__)
// For wasm, we don't use async compilation.
struct DawnGraphicsPipeline::AsyncPipelineCreation : public AsyncPipelineCreationBase {};
#else
struct DawnGraphicsPipeline::AsyncPipelineCreation : public AsyncPipelineCreationBase {
    wgpu::Future fFuture;
};
#endif

#if !defined(__EMSCRIPTEN__)
using namespace ycbcrUtils;
// Fetches any immutable samplers and accumulates them into outImmutableSamplers. Returns false
// if there is a failure that triggers us to fail a draw. Acts as a no-op and returns true if a
// shader doesn't store any data (meaning immutable samplers are never used with the given shader).
bool gather_immutable_samplers(const SkSpan<uint32_t> samplerData,
                               DawnResourceProvider* resourceProvider,
                               skia_private::AutoTArray<sk_sp<DawnSampler>>& outImmutableSamplers) {
    // The quantity of int32s needed to represent immutable sampler data varies, so handle
    // incrementing i within the loop. Sampler data can be anywhere from 1-3 uint32s depending upon
    // whether a sampler is immutable or dynamic and whether it uses a known or external format.
    // Since sampler data size can vary per-sampler, also track sampler count for indexing into
    // outImmutableSamplers.
    size_t samplerIdx = 0;
    for (size_t i = 0; i < samplerData.size();) {
        // A first sampler value of 0 indicates that an image shader uses no immutable samplers.
        // Thus, we can push a nullptr on to immutableSamplers and continue iterating.
        if (samplerData[i] == 0) {
            i++;
            samplerIdx++;
            continue;
        }

        // If the data is non-zero, that means we are using an immutable sampler. Check
        // whether it uses a known or external format to determine how many uint32s
        // (samplerDataLength) we must consult to obtain all the data necessary to
        // query the resource provider for a real sampler.
        uint32_t immutableSamplerInfo = samplerData[i] >> SamplerDesc::kImmutableSamplerInfoShift;
        SkASSERT(immutableSamplerInfo != 0);
        bool usesExternalFormat =
                static_cast<bool>(immutableSamplerInfo & ycbcrUtils::kUseExternalFormatMask);
        const int samplerDataLength =
                usesExternalFormat ? kIntsNeededExternalFormat : kIntsNeededKnownFormat;

        // Gather samplerDataLength int32s from the data span. Use that to populate a
        // SamplerDesc which enables us to query the resource provider for a real sampler.
        SamplerDesc samplerDesc;
        memcpy(&samplerDesc, samplerData.begin() + i, samplerDataLength * sizeof(uint32_t));
        sk_sp<Sampler> immutableSampler =
                resourceProvider->findOrCreateCompatibleSampler(samplerDesc);
        if (!immutableSampler) {
            SKGPU_LOG_E("Failed to find or create immutable sampler for pipeline");
            return false;
        }

        sk_sp<DawnSampler> dawnImmutableSampler =
                sk_ref_sp<DawnSampler>(static_cast<DawnSampler*>(immutableSampler.get()));
        SkASSERT(dawnImmutableSampler);

        outImmutableSamplers[samplerIdx++] = std::move(dawnImmutableSampler);
        i += samplerDataLength;
    }
    // If there was any sampler data, then assert that we appropriately analyzed the correct number
    // of samplers.
    SkASSERT(samplerData.empty() || samplerIdx == outImmutableSamplers.size());
    return true;
}
#endif

// static
sk_sp<DawnGraphicsPipeline> DawnGraphicsPipeline::Make(const DawnSharedContext* sharedContext,
                                                       DawnResourceProvider* resourceProvider,
                                                       const RuntimeEffectDictionary* runtimeDict,
                                                       const GraphicsPipelineDesc& pipelineDesc,
                                                       const RenderPassDesc& renderPassDesc) {
    const DawnCaps& caps = *static_cast<const DawnCaps*>(sharedContext->caps());
    const auto& device = sharedContext->device();

    SkSL::Program::Interface vsInterface, fsInterface;

    SkSL::ProgramSettings settings;
    settings.fSharpenTextures = true;
    settings.fForceNoRTFlip = true;

    ShaderErrorHandler* errorHandler = caps.shaderErrorHandler();

    const RenderStep* step = sharedContext->rendererProvider()->lookup(pipelineDesc.renderStepID());
    const bool useStorageBuffers = caps.storageBufferSupport();

    std::string vsCode, fsCode;
    wgpu::ShaderModule fsModule, vsModule;

    // Some steps just render depth buffer but not color buffer, so the fragment
    // shader is null.
    UniquePaintParamsID paintID = pipelineDesc.paintParamsID();
    FragSkSLInfo fsSkSLInfo = BuildFragmentSkSL(&caps,
                                                sharedContext->shaderCodeDictionary(),
                                                runtimeDict,
                                                step,
                                                paintID,
                                                useStorageBuffers,
                                                renderPassDesc.fWriteSwizzle);
    std::string& fsSkSL = fsSkSLInfo.fSkSL;
    const BlendInfo& blendInfo = fsSkSLInfo.fBlendInfo;
    const bool localCoordsNeeded = fsSkSLInfo.fRequiresLocalCoords;
    const int numTexturesAndSamplers = fsSkSLInfo.fNumTexturesAndSamplers;

    bool hasFragmentSkSL = !fsSkSL.empty();
    if (hasFragmentSkSL) {
        if (!skgpu::SkSLToWGSL(caps.shaderCaps(),
                               fsSkSL,
                               SkSL::ProgramKind::kGraphiteFragment,
                               settings,
                               &fsCode,
                               &fsInterface,
                               errorHandler)) {
            return {};
        }
        if (!DawnCompileWGSLShaderModule(sharedContext, fsSkSLInfo.fLabel.c_str(), fsCode,
                                         &fsModule, errorHandler)) {
            return {};
        }
    }

    VertSkSLInfo vsSkSLInfo = BuildVertexSkSL(caps.resourceBindingRequirements(),
                                              step,
                                              useStorageBuffers,
                                              localCoordsNeeded);
    const std::string& vsSkSL = vsSkSLInfo.fSkSL;
    if (!skgpu::SkSLToWGSL(caps.shaderCaps(),
                           vsSkSL,
                           SkSL::ProgramKind::kGraphiteVertex,
                           settings,
                           &vsCode,
                           &vsInterface,
                           errorHandler)) {
        return {};
    }
    if (!DawnCompileWGSLShaderModule(sharedContext, vsSkSLInfo.fLabel.c_str(), vsCode,
                                     &vsModule, errorHandler)) {
        return {};
    }

    std::string pipelineLabel =
            GetPipelineLabel(sharedContext->shaderCodeDictionary(), renderPassDesc, step, paintID);
    wgpu::RenderPipelineDescriptor descriptor;
    // Always set the label for pipelines, dawn may need it for tracing.
    descriptor.label = pipelineLabel.c_str();

    // Fragment state
    skgpu::BlendEquation equation = blendInfo.fEquation;
    skgpu::BlendCoeff srcCoeff = blendInfo.fSrcBlend;
    skgpu::BlendCoeff dstCoeff = blendInfo.fDstBlend;
    bool blendOn = !skgpu::BlendShouldDisable(equation, srcCoeff, dstCoeff);

    wgpu::BlendState blend;
    if (blendOn) {
        blend.color.operation = blend_equation_to_dawn_blend_op(equation);
        blend.color.srcFactor = blend_coeff_to_dawn_blend(caps, srcCoeff);
        blend.color.dstFactor = blend_coeff_to_dawn_blend(caps, dstCoeff);
        blend.alpha.operation = blend_equation_to_dawn_blend_op(equation);
        blend.alpha.srcFactor = blend_coeff_to_dawn_blend_for_alpha(caps, srcCoeff);
        blend.alpha.dstFactor = blend_coeff_to_dawn_blend_for_alpha(caps, dstCoeff);
    }

    wgpu::ColorTargetState colorTarget;
    colorTarget.format =
            TextureInfos::GetDawnViewFormat(renderPassDesc.fColorAttachment.fTextureInfo);
    colorTarget.blend = blendOn ? &blend : nullptr;
    colorTarget.writeMask = blendInfo.fWritesColor && hasFragmentSkSL ? wgpu::ColorWriteMask::All
                                                                      : wgpu::ColorWriteMask::None;

#if !defined(__EMSCRIPTEN__)
    const bool loadMsaaFromResolve =
            renderPassDesc.fColorResolveAttachment.fTextureInfo.isValid() &&
            renderPassDesc.fColorResolveAttachment.fLoadOp == LoadOp::kLoad;
    // Special case: a render pass loading resolve texture requires additional settings for the
    // pipeline to make it compatible.
    wgpu::ColorTargetStateExpandResolveTextureDawn pipelineMSAALoadResolveTextureDesc;
    if (loadMsaaFromResolve && sharedContext->dawnCaps()->resolveTextureLoadOp().has_value()) {
        SkASSERT(device.HasFeature(wgpu::FeatureName::DawnLoadResolveTexture));
        colorTarget.nextInChain = &pipelineMSAALoadResolveTextureDesc;
        pipelineMSAALoadResolveTextureDesc.enabled = true;
    }
#endif

    wgpu::FragmentState fragment;
    // Dawn doesn't allow having a color attachment but without fragment shader, so have to use a
    // noop fragment shader, if fragment shader is null.
    fragment.module = hasFragmentSkSL ? std::move(fsModule) : sharedContext->noopFragment();
    fragment.entryPoint = "main";
    fragment.targetCount = 1;
    fragment.targets = &colorTarget;
    descriptor.fragment = &fragment;

    // Depth stencil state
    const auto& depthStencilSettings = step->depthStencilSettings();
    SkASSERT(depthStencilSettings.fDepthTestEnabled ||
             depthStencilSettings.fDepthCompareOp == CompareOp::kAlways);
    wgpu::DepthStencilState depthStencil;
    if (renderPassDesc.fDepthStencilAttachment.fTextureInfo.isValid()) {
        wgpu::TextureFormat dsFormat = TextureInfos::GetDawnViewFormat(
                renderPassDesc.fDepthStencilAttachment.fTextureInfo);
        depthStencil.format =
                DawnFormatIsDepthOrStencil(dsFormat) ? dsFormat : wgpu::TextureFormat::Undefined;
        if (depthStencilSettings.fDepthTestEnabled) {
            depthStencil.depthWriteEnabled = depthStencilSettings.fDepthWriteEnabled;
        }
        depthStencil.depthCompare = compare_op_to_dawn(depthStencilSettings.fDepthCompareOp);

        // Dawn validation fails if the stencil state is non-default and the
        // format doesn't have the stencil aspect.
        if (DawnFormatIsStencil(dsFormat) && depthStencilSettings.fStencilTestEnabled) {
            depthStencil.stencilFront = stencil_face_to_dawn(depthStencilSettings.fFrontStencil);
            depthStencil.stencilBack = stencil_face_to_dawn(depthStencilSettings.fBackStencil);
            depthStencil.stencilReadMask = depthStencilSettings.fFrontStencil.fReadMask;
            depthStencil.stencilWriteMask = depthStencilSettings.fFrontStencil.fWriteMask;
        }

        descriptor.depthStencil = &depthStencil;
    }

    // Determine the BindGroupLayouts that will be used to make up the pipeline layout.
    BindGroupLayouts groupLayouts;

    // The quantity of samplers = 1/2 the cumulative number of textures AND samplers. The count
    // reported by the generated SkSL already includes any texture/sampler required for dst reads
    // via texture copy so no additional logic is needed when preparing the BindGroupLayout.
    const int numSamplers = numTexturesAndSamplers / 2;
    // Determine and store any immutable samplers to be included in the pipeline layout. A sampler's
    // binding index can be determined by multiplying its index within the immutableSamplers array
    // by 2. Initialize all values to the default of nullptr, which acts as a spacer to indicate the
    // usage of a "regular" dynamic sampler.
    skia_private::AutoTArray<sk_sp<DawnSampler>> immutableSamplers(numSamplers);
    {
        SkASSERT(resourceProvider);
        groupLayouts[0] = resourceProvider->getOrCreateUniformBuffersBindGroupLayout();
        if (!groupLayouts[0]) {
            return {};
        }

        bool hasFragmentSamplers = hasFragmentSkSL && numTexturesAndSamplers > 0;
        if (hasFragmentSamplers) {
#if !defined(__EMSCRIPTEN__)
            // fsSkSLInfo.fData contains SamplerDesc information of any immutable samplers used by
            // this pipeline. Note that, for now, all data within fsSkSLInfo.fData is known to be
            // SamplerDesc info of immutable samplers represented as uint32s. However, other
            // snippets may one day utilize this fData to represent some other struct or info.
            // b/347072931 tracks the effort to tie data to snippetIDs which would inform us of the
            // expected data type.
            if (!gather_immutable_samplers(
                        {fsSkSLInfo.fData}, resourceProvider, immutableSamplers)) {
                return {};
            }
#endif
            // Optimize for the common case of a single texture + 1 dynamic sampler.
            if (numTexturesAndSamplers == 2 && !immutableSamplers[0]) {
                groupLayouts[1] =
                        resourceProvider->getOrCreateSingleTextureSamplerBindGroupLayout();
            } else {
                using BindGroupLayoutEntryList = std::vector<wgpu::BindGroupLayoutEntry>;
                BindGroupLayoutEntryList entries(numTexturesAndSamplers);
#if !defined(__EMSCRIPTEN__)
                // Static sampler layouts are passed in by address and therefore must stay valid
                // until the BindGroupLayoutDescriptor is created, so store them outside of
                // the loop that iterates over each BindGroupLayoutEntry.
                std::vector<wgpu::StaticSamplerBindingLayout> staticSamplerLayouts;
#endif

                for (int i = 0; i < numTexturesAndSamplers;) {
                    entries[i].binding = i;
                    entries[i].visibility = wgpu::ShaderStage::Fragment;
#if !defined(__EMSCRIPTEN__)
                    // When it's possible to use static samplers, check to see if we are using one
                    // for this entry. If so, add the sampler to the BindGroupLayoutEntry. Note that
                    // a sampler's index in immutableSamplers is equivalent to half of an entry's
                    // index within the entries container.
                    if (immutableSamplers[i / 2]) {
                        wgpu::StaticSamplerBindingLayout& immutableSamplerBinding =
                                staticSamplerLayouts.emplace_back();

                        immutableSamplerBinding.sampler = immutableSamplers[i / 2]->dawnSampler();
                        immutableSamplerBinding.sampledTextureBinding = i + 1;
                        entries[i].nextInChain = &immutableSamplerBinding;
                    } else {
#endif
                        entries[i].sampler.type = wgpu::SamplerBindingType::Filtering;
#if !defined(__EMSCRIPTEN__)
                    }
#endif
                    ++i;

                    entries[i].binding = i;
                    entries[i].visibility = wgpu::ShaderStage::Fragment;
                    entries[i].texture.sampleType = wgpu::TextureSampleType::Float;
                    entries[i].texture.viewDimension = wgpu::TextureViewDimension::e2D;
                    entries[i].texture.multisampled = false;
                    ++i;
                }

                wgpu::BindGroupLayoutDescriptor groupLayoutDesc;
                if (sharedContext->caps()->setBackendLabels()) {
                    groupLayoutDesc.label = vsSkSLInfo.fLabel.c_str();
                }
                groupLayoutDesc.entryCount = entries.size();
                groupLayoutDesc.entries = entries.data();
                groupLayouts[1] = device.CreateBindGroupLayout(&groupLayoutDesc);
            }
            if (!groupLayouts[1]) {
                return {};
            }
        }

        wgpu::PipelineLayoutDescriptor layoutDesc;
        if (sharedContext->caps()->setBackendLabels()) {
            layoutDesc.label = fsSkSLInfo.fLabel.c_str();
        }
        layoutDesc.bindGroupLayoutCount =
            hasFragmentSamplers ? groupLayouts.size() : groupLayouts.size() - 1;
        layoutDesc.bindGroupLayouts = groupLayouts.data();
        auto layout = device.CreatePipelineLayout(&layoutDesc);
        if (!layout) {
            return {};
        }
        descriptor.layout = std::move(layout);
    }

    // Vertex state
    std::array<wgpu::VertexBufferLayout, kNumVertexBuffers> vertexBufferLayouts;
    // Vertex buffer layout
    std::vector<wgpu::VertexAttribute> vertexAttributes;
    {
        auto arrayStride = create_vertex_attributes(step->vertexAttributes(),
                                                    0,
                                                    &vertexAttributes);
        auto& layout = vertexBufferLayouts[kVertexBufferIndex];
        if (arrayStride) {
            layout.arrayStride = arrayStride;
            layout.stepMode = wgpu::VertexStepMode::Vertex;
            layout.attributeCount = vertexAttributes.size();
            layout.attributes = vertexAttributes.data();
        } else {
            layout.arrayStride = 0;
            layout.stepMode = wgpu::VertexStepMode::VertexBufferNotUsed;
            layout.attributeCount = 0;
            layout.attributes = nullptr;
        }
    }

    // Instance buffer layout
    std::vector<wgpu::VertexAttribute> instanceAttributes;
    {
        auto arrayStride = create_vertex_attributes(step->instanceAttributes(),
                                                    step->vertexAttributes().size(),
                                                    &instanceAttributes);
        auto& layout = vertexBufferLayouts[kInstanceBufferIndex];
        if (arrayStride) {
            layout.arrayStride = arrayStride;
            layout.stepMode = wgpu::VertexStepMode::Instance;
            layout.attributeCount = instanceAttributes.size();
            layout.attributes = instanceAttributes.data();
        } else {
            layout.arrayStride = 0;
            layout.stepMode = wgpu::VertexStepMode::VertexBufferNotUsed;
            layout.attributeCount = 0;
            layout.attributes = nullptr;
        }
    }

    auto& vertex = descriptor.vertex;
    vertex.module = std::move(vsModule);
    vertex.entryPoint = "main";
    vertex.constantCount = 0;
    vertex.constants = nullptr;
    vertex.bufferCount = vertexBufferLayouts.size();
    vertex.buffers = vertexBufferLayouts.data();

    // Other state
    descriptor.primitive.frontFace = wgpu::FrontFace::CCW;
    descriptor.primitive.cullMode = wgpu::CullMode::None;
    switch (step->primitiveType()) {
        case PrimitiveType::kTriangles:
            descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
            break;
        case PrimitiveType::kTriangleStrip:
            descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
            descriptor.primitive.stripIndexFormat = wgpu::IndexFormat::Uint16;
            break;
        case PrimitiveType::kPoints:
            descriptor.primitive.topology = wgpu::PrimitiveTopology::PointList;
            break;
    }

    // Multisampled state
    descriptor.multisample.count = renderPassDesc.fSampleCount;
    descriptor.multisample.mask = 0xFFFFFFFF;
    descriptor.multisample.alphaToCoverageEnabled = false;

    auto asyncCreation = std::make_unique<AsyncPipelineCreation>();

    if (caps.useAsyncPipelineCreation()) {
#if defined(__EMSCRIPTEN__)
        // We shouldn't use CreateRenderPipelineAsync in wasm.
        SKGPU_LOG_F("CreateRenderPipelineAsync shouldn't be used in WASM");
#else
        asyncCreation->fFuture = device.CreateRenderPipelineAsync(
                &descriptor,
                wgpu::CallbackMode::WaitAnyOnly,
                [asyncCreationPtr = asyncCreation.get()](wgpu::CreatePipelineAsyncStatus status,
                                                         wgpu::RenderPipeline pipeline,
                                                         char const* message) {
                    if (status != wgpu::CreatePipelineAsyncStatus::Success) {
                        SKGPU_LOG_E("Failed to create render pipeline (%d): %s",
                                    static_cast<int>(status),
                                    message);
                        // invalidate AsyncPipelineCreation pointer to signal that this pipeline has
                        // failed.
                        asyncCreationPtr->fRenderPipeline = nullptr;
                    } else {
                        asyncCreationPtr->fRenderPipeline = std::move(pipeline);
                    }

                    asyncCreationPtr->fFinished = true;
                });
#endif
    } else {
        std::optional<DawnErrorChecker> errorChecker;
        if (sharedContext->dawnCaps()->allowScopedErrorChecks()) {
            errorChecker.emplace(sharedContext);
        }
        asyncCreation->fRenderPipeline = device.CreateRenderPipeline(&descriptor);
        asyncCreation->fFinished = true;

        if (errorChecker.has_value() && errorChecker->popErrorScopes() != DawnErrorType::kNoError) {
            asyncCreation->fRenderPipeline = nullptr;
        }
    }

    PipelineInfo pipelineInfo{vsSkSLInfo, fsSkSLInfo};
#if defined(GPU_TEST_UTILS)
    pipelineInfo.fNativeVertexShader = std::move(vsCode);
    pipelineInfo.fNativeFragmentShader = std::move(fsCode);
#endif
    return sk_sp<DawnGraphicsPipeline>(
            new DawnGraphicsPipeline(sharedContext,
                                     pipelineInfo,
                                     std::move(asyncCreation),
                                     std::move(groupLayouts),
                                     step->primitiveType(),
                                     depthStencilSettings.fStencilReferenceValue,
                                     std::move(immutableSamplers)));
}

DawnGraphicsPipeline::DawnGraphicsPipeline(
        const skgpu::graphite::SharedContext* sharedContext,
        const PipelineInfo& pipelineInfo,
        std::unique_ptr<AsyncPipelineCreation> asyncCreationInfo,
        BindGroupLayouts groupLayouts,
        PrimitiveType primitiveType,
        uint32_t refValue,
        skia_private::AutoTArray<sk_sp<DawnSampler>> immutableSamplers)
    : GraphicsPipeline(sharedContext, pipelineInfo)
    , fAsyncPipelineCreation(std::move(asyncCreationInfo))
    , fGroupLayouts(std::move(groupLayouts))
    , fPrimitiveType(primitiveType)
    , fStencilReferenceValue(refValue)
    , fImmutableSamplers(std::move(immutableSamplers)) {}

DawnGraphicsPipeline::~DawnGraphicsPipeline() {
    this->freeGpuData();
}

void DawnGraphicsPipeline::freeGpuData() {
    // Wait for async creation to finish before we can destroy this object.
    (void)this->dawnRenderPipeline();
    fAsyncPipelineCreation = nullptr;
}

const wgpu::RenderPipeline& DawnGraphicsPipeline::dawnRenderPipeline() const {
    if (!fAsyncPipelineCreation) {
        static const wgpu::RenderPipeline kNullPipeline = nullptr;
        return kNullPipeline;
    }
    if (fAsyncPipelineCreation->fFinished) {
        return fAsyncPipelineCreation->fRenderPipeline;
    }
#if defined(__EMSCRIPTEN__)
    // We shouldn't use CreateRenderPipelineAsync in wasm.
    SKGPU_LOG_F("CreateRenderPipelineAsync shouldn't be used in WASM");
#else
    wgpu::FutureWaitInfo waitInfo{};
    waitInfo.future = fAsyncPipelineCreation->fFuture;
    const auto& instance = static_cast<const DawnSharedContext*>(sharedContext())
                                   ->device()
                                   .GetAdapter()
                                   .GetInstance();

    [[maybe_unused]] auto status =
            instance.WaitAny(1, &waitInfo, /*timeoutNS=*/std::numeric_limits<uint64_t>::max());
    SkASSERT(status == wgpu::WaitStatus::Success);
    SkASSERT(waitInfo.completed);
#endif

    return fAsyncPipelineCreation->fRenderPipeline;
}

} // namespace skgpu::graphite
