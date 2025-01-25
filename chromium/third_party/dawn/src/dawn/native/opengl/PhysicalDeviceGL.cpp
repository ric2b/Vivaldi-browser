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

#include "dawn/native/opengl/PhysicalDeviceGL.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "dawn/common/GPUInfo.h"
#include "dawn/native/ChainUtils.h"
#include "dawn/native/Instance.h"
#include "dawn/native/opengl/ContextEGL.h"
#include "dawn/native/opengl/DeviceGL.h"
#include "dawn/native/opengl/DisplayEGL.h"
#include "dawn/native/opengl/SwapChainEGL.h"

namespace dawn::native::opengl {

namespace {

struct Vendor {
    const char* vendorName;
    uint32_t vendorId;
};

const Vendor kVendors[] = {{"ATI", gpu_info::kVendorID_AMD},
                           {"ARM", gpu_info::kVendorID_ARM},
                           {"Imagination", gpu_info::kVendorID_ImgTec},
                           {"Intel", gpu_info::kVendorID_Intel},
                           {"NVIDIA", gpu_info::kVendorID_Nvidia},
                           {"Qualcomm", gpu_info::kVendorID_Qualcomm}};

uint32_t GetVendorIdFromVendors(const char* vendor) {
    uint32_t vendorId = 0;
    for (const auto& it : kVendors) {
        // Matching vendor name with vendor string
        if (strstr(vendor, it.vendorName) != nullptr) {
            vendorId = it.vendorId;
            break;
        }
    }
    return vendorId;
}

uint32_t GetDeviceIdFromRender(std::string_view render) {
    uint32_t deviceId = 0;
    size_t pos = render.find("(0x");
    if (pos == std::string_view::npos) {
        pos = render.find("(0X");
    }
    if (pos == std::string_view::npos) {
        return deviceId;
    }
    render.remove_prefix(pos + 3);

    // The first character after the prefix must be hexadecimal, otherwise an invalid argument
    // exception is thrown.
    if (!render.empty() && std::isxdigit(static_cast<unsigned char>(*render.data()))) {
        deviceId = static_cast<uint32_t>(std::stoul(render.data(), nullptr, 16));
    }

    return deviceId;
}

}  // anonymous namespace

// static
ResultOrError<Ref<PhysicalDevice>> PhysicalDevice::Create(wgpu::BackendType backendType,
                                                          Ref<DisplayEGL> display) {
    const EGLFunctions& egl = display->egl;
    EGLDisplay eglDisplay = display->GetDisplay();

    // Create a temporary context and make it current during the creation of the PhysicalDevice so
    // that we can query the limits and other properties. Assumes that the limit are the same
    // irrespective of the context creation options.
    std::unique_ptr<ContextEGL> context;
    DAWN_TRY_ASSIGN(context, ContextEGL::Create(display, backendType, /*useRobustness*/ false,
                                                /*useANGLETextureSharing*/ false));

    EGLSurface prevDrawSurface = egl.GetCurrentSurface(EGL_DRAW);
    EGLSurface prevReadSurface = egl.GetCurrentSurface(EGL_READ);
    EGLContext prevContext = egl.GetCurrentContext();

    context->MakeCurrent();

    Ref<PhysicalDevice> physicalDevice =
        AcquireRef(new PhysicalDevice(backendType, std::move(display)));
    DAWN_TRY_WITH_CLEANUP(physicalDevice->Initialize(), {
        egl.MakeCurrent(eglDisplay, prevDrawSurface, prevReadSurface, prevContext);
    });

    egl.MakeCurrent(eglDisplay, prevDrawSurface, prevReadSurface, prevContext);

    return physicalDevice;
}

PhysicalDevice::PhysicalDevice(wgpu::BackendType backendType, Ref<DisplayEGL> display)
    : PhysicalDeviceBase(backendType), mDisplay(std::move(display)) {}

DisplayEGL* PhysicalDevice::GetDisplay() const {
    return mDisplay.Get();
}

bool PhysicalDevice::SupportsExternalImages() const {
    // Via dawn::native::opengl::WrapExternalEGLImage
    return GetBackendType() == wgpu::BackendType::OpenGLES;
}

MaybeError PhysicalDevice::InitializeImpl() {
    DAWN_TRY(mFunctions.Initialize(mDisplay->egl.GetProcAddress));

    // In some cases (like like of EGL_KHR_create_context) we don't know before this point that we
    // got a GL context that supports the required version. Check it now.
    switch (GetBackendType()) {
        case wgpu::BackendType::OpenGLES:
            DAWN_INVALID_IF(!mFunctions.IsAtLeastGLES(3, 1), "OpenGL ES 3.1 is required.");
            break;
        case wgpu::BackendType::OpenGL:
            DAWN_INVALID_IF(!mFunctions.IsAtLeastGL(4, 4), "Desktop OpenGL 4.4 is required.");
            break;
        default:
            DAWN_UNREACHABLE();
    }

    if (mFunctions.GetVersion().IsES()) {
        DAWN_ASSERT(GetBackendType() == wgpu::BackendType::OpenGLES);

        // WebGPU requires being able to render to f16 and being able to blend f16
        // which EXT_color_buffer_half_float provides.
        DAWN_INVALID_IF(!mFunctions.IsGLExtensionSupported("GL_EXT_color_buffer_half_float"),
                        "GL_EXT_color_buffer_half_float is required");

        // WebGPU requires being able to render to f32 but does not require being able to blend f32
        DAWN_INVALID_IF(!mFunctions.IsGLExtensionSupported("GL_EXT_color_buffer_float"),
                        "GL_EXT_color_buffer_float is required");
    } else {
        DAWN_ASSERT(GetBackendType() == wgpu::BackendType::OpenGL);
    }

    mName = reinterpret_cast<const char*>(mFunctions.GetString(GL_RENDERER));

    // Workaroud to find vendor id from vendor name
    const char* vendor = reinterpret_cast<const char*>(mFunctions.GetString(GL_VENDOR));
    mVendorId = GetVendorIdFromVendors(vendor);
    // Workaround to find device id from ANGLE render string
    if (mName.find("ANGLE") == 0) {
        mDeviceId = GetDeviceIdFromRender(mName);
    }

    mDriverDescription = std::string("OpenGL version ") +
                         reinterpret_cast<const char*>(mFunctions.GetString(GL_VERSION));

    if (mName.find("SwiftShader") != std::string::npos) {
        mAdapterType = wgpu::AdapterType::CPU;
    }

    return {};
}

void PhysicalDevice::InitializeSupportedFeaturesImpl() {
    EnableFeature(Feature::StaticSamplers);

    // TextureCompressionBC
    {
        // BC1, BC2 and BC3 are not supported in OpenGL or OpenGL ES core features.
        bool supportsS3TC =
            mFunctions.IsGLExtensionSupported("GL_EXT_texture_compression_s3tc") ||
            (mFunctions.IsGLExtensionSupported("GL_EXT_texture_compression_dxt1") &&
             mFunctions.IsGLExtensionSupported("GL_ANGLE_texture_compression_dxt3") &&
             mFunctions.IsGLExtensionSupported("GL_ANGLE_texture_compression_dxt5"));

        // COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT and
        // COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT requires both GL_EXT_texture_sRGB and
        // GL_EXT_texture_compression_s3tc on desktop OpenGL drivers.
        // (https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_sRGB.txt)
        bool supportsTextureSRGB = mFunctions.IsGLExtensionSupported("GL_EXT_texture_sRGB");

        // GL_EXT_texture_compression_s3tc_srgb is an extension in OpenGL ES.
        // NVidia GLES drivers don't support this extension, but they do support
        // GL_NV_sRGB_formats. (Note that GL_EXT_texture_sRGB does not exist on ES.
        // GL_EXT_sRGB does (core in ES 3.0), but it does not automatically provide S3TC
        // SRGB support even if S3TC is supported; see
        // https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_sRGB.txt.)
        bool supportsS3TCSRGB =
            mFunctions.IsGLExtensionSupported("GL_EXT_texture_compression_s3tc_srgb") ||
            mFunctions.IsGLExtensionSupported("GL_NV_sRGB_formats");

        // BC4 and BC5
        bool supportsRGTC = mFunctions.IsAtLeastGL(3, 0) ||
                            mFunctions.IsGLExtensionSupported("GL_ARB_texture_compression_rgtc") ||
                            mFunctions.IsGLExtensionSupported("GL_EXT_texture_compression_rgtc");

        // BC6 and BC7
        bool supportsBPTC = mFunctions.IsAtLeastGL(4, 2) ||
                            mFunctions.IsGLExtensionSupported("GL_ARB_texture_compression_bptc") ||
                            mFunctions.IsGLExtensionSupported("GL_EXT_texture_compression_bptc");

        if (supportsS3TC && (supportsTextureSRGB || supportsS3TCSRGB) && supportsRGTC &&
            supportsBPTC) {
            EnableFeature(dawn::native::Feature::TextureCompressionBC);
        }
    }

    if (mDisplay->egl.HasExt(EGLExt::DisplayTextureShareGroup)) {
        EnableFeature(dawn::native::Feature::ANGLETextureSharing);
    }

    // Non-zero baseInstance requires at least desktop OpenGL 4.2, and it is not supported in
    // OpenGL ES OpenGL:
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawElementsIndirect.xhtml
    // OpenGL ES:
    // https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glDrawElementsIndirect.xhtml
    if (mFunctions.IsAtLeastGL(4, 2)) {
        EnableFeature(Feature::IndirectFirstInstance);
    }

    // ShaderF16
    if (mFunctions.IsGLExtensionSupported("GL_AMD_gpu_shader_half_float")) {
        EnableFeature(Feature::ShaderF16);
    }

    // DualSourceBlending
    if (mFunctions.IsGLExtensionSupported("GL_EXT_blend_func_extended") ||
        mFunctions.IsAtLeastGL(3, 3)) {
        EnableFeature(Feature::DualSourceBlending);
    }

    // Unorm16TextureFormats, Snorm16TextureFormats and Norm16TextureFormats
    if (mFunctions.IsGLExtensionSupported("GL_EXT_texture_norm16")) {
        EnableFeature(Feature::Unorm16TextureFormats);
        EnableFeature(Feature::Snorm16TextureFormats);
        EnableFeature(Feature::Norm16TextureFormats);
    }
}

namespace {
GLint Get(const OpenGLFunctions& gl, GLenum pname) {
    GLint value;
    gl.GetIntegerv(pname, &value);
    return value;
}

GLint GetIndexed(const OpenGLFunctions& gl, GLenum pname, GLuint index) {
    GLint value;
    gl.GetIntegeri_v(pname, index, &value);
    return value;
}
}  // namespace

MaybeError PhysicalDevice::InitializeSupportedLimitsImpl(CombinedLimits* limits) {
    const OpenGLFunctions& gl = mFunctions;
    GetDefaultLimitsForSupportedFeatureLevel(&limits->v1);

    limits->v1.maxTextureDimension1D = limits->v1.maxTextureDimension2D =
        Get(gl, GL_MAX_TEXTURE_SIZE);
    limits->v1.maxTextureDimension3D = Get(gl, GL_MAX_3D_TEXTURE_SIZE);
    limits->v1.maxTextureArrayLayers = Get(gl, GL_MAX_ARRAY_TEXTURE_LAYERS);

    // Since we flatten bindings, leave maxBindGroups and maxBindingsPerBindGroup at the default.

    limits->v1.maxDynamicUniformBuffersPerPipelineLayout = Get(gl, GL_MAX_UNIFORM_BUFFER_BINDINGS);
    limits->v1.maxDynamicStorageBuffersPerPipelineLayout =
        Get(gl, GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS);
    limits->v1.maxSampledTexturesPerShaderStage =
        std::min(Get(gl, GL_MAX_TEXTURE_IMAGE_UNITS), Get(gl, GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS));
    limits->v1.maxSamplersPerShaderStage = Get(gl, GL_MAX_TEXTURE_IMAGE_UNITS);
    limits->v1.maxStorageBuffersPerShaderStage = Get(gl, GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS);
    // TODO(crbug.com/dawn/1834): Note that OpenGLES allows an implementation to have zero vertex
    // image uniforms, so this isn't technically correct for vertex shaders.
    limits->v1.maxStorageTexturesPerShaderStage = Get(gl, GL_MAX_FRAGMENT_IMAGE_UNIFORMS);

    limits->v1.maxUniformBuffersPerShaderStage = Get(gl, GL_MAX_UNIFORM_BUFFER_BINDINGS);
    limits->v1.maxUniformBufferBindingSize = Get(gl, GL_MAX_UNIFORM_BLOCK_SIZE);
    limits->v1.maxStorageBufferBindingSize = Get(gl, GL_MAX_SHADER_STORAGE_BLOCK_SIZE);

    limits->v1.minUniformBufferOffsetAlignment = Get(gl, GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
    limits->v1.minStorageBufferOffsetAlignment = Get(gl, GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT);
    limits->v1.maxVertexBuffers = Get(gl, GL_MAX_VERTEX_ATTRIB_BINDINGS);
    limits->v1.maxBufferSize = kAssumedMaxBufferSize;
    // The code that handles adding the index buffer offset to first_index
    // used in drawIndexedIndirect can not handle a max buffer size larger than 4gig.
    // See IndirectDrawValidationEncoder.cpp
    static_assert(kAssumedMaxBufferSize < 0x100000000u);

    limits->v1.maxVertexAttributes = Get(gl, GL_MAX_VERTEX_ATTRIBS);
    limits->v1.maxVertexBufferArrayStride = Get(gl, GL_MAX_VERTEX_ATTRIB_STRIDE);
    limits->v1.maxInterStageShaderComponents = Get(gl, GL_MAX_VARYING_COMPONENTS);
    limits->v1.maxInterStageShaderVariables = Get(gl, GL_MAX_VARYING_VECTORS);
    // TODO(dawn:685, dawn:1448): Support higher values as ANGLE compiler always generates
    // additional shader varyings (gl_PointSize and dx_Position) on ANGLE D3D backends.
    limits->v1.maxInterStageShaderComponents =
        std::min(limits->v1.maxInterStageShaderComponents, kMaxInterStageShaderComponents);
    limits->v1.maxInterStageShaderVariables =
        std::min(limits->v1.maxInterStageShaderVariables, kMaxInterStageShaderVariables);

    limits->v1.maxColorAttachments =
        std::min(Get(gl, GL_MAX_COLOR_ATTACHMENTS), Get(gl, GL_MAX_DRAW_BUFFERS));

    // TODO(crbug.com/dawn/1834): determine if GL has an equivalent value here.
    //    limits->v1.maxColorAttachmentBytesPerSample = WGPU_LIMIT_U32_UNDEFINED;

    limits->v1.maxComputeWorkgroupStorageSize = Get(gl, GL_MAX_COMPUTE_SHARED_MEMORY_SIZE);
    limits->v1.maxComputeInvocationsPerWorkgroup = Get(gl, GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS);
    limits->v1.maxComputeWorkgroupSizeX = GetIndexed(gl, GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0);
    limits->v1.maxComputeWorkgroupSizeY = GetIndexed(gl, GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1);
    limits->v1.maxComputeWorkgroupSizeZ = GetIndexed(gl, GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2);
    GLint v[3];
    v[0] = GetIndexed(gl, GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0);
    v[1] = GetIndexed(gl, GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1);
    v[2] = GetIndexed(gl, GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2);
    limits->v1.maxComputeWorkgroupsPerDimension = std::min(v[0], std::min(v[1], v[2]));
    return {};
}

void PhysicalDevice::SetupBackendAdapterToggles(dawn::platform::Platform* platform,
                                                TogglesState* adapterToggles) const {}

void PhysicalDevice::SetupBackendDeviceToggles(dawn::platform::Platform* platform,
                                               TogglesState* deviceToggles) const {
    const OpenGLFunctions& gl = mFunctions;

    // TODO(crbug.com/dawn/582): Use OES_draw_buffers_indexed where available.
    bool supportsIndexedDrawBuffers = gl.IsAtLeastGLES(3, 2) || gl.IsAtLeastGL(3, 0);

    bool supportsSnormRead =
        gl.IsAtLeastGL(4, 4) || gl.IsGLExtensionSupported("GL_EXT_render_snorm");

    // Desktop GL supports BGRA textures via swizzling in the driver; ES requires an extension.
    bool supportsBGRARead =
        gl.GetVersion().IsDesktop() || gl.IsGLExtensionSupported("GL_EXT_read_format_bgra");

    bool supportsSampleVariables = gl.IsAtLeastGL(4, 0) || gl.IsAtLeastGLES(3, 2) ||
                                   gl.IsGLExtensionSupported("GL_OES_sample_variables");

    // Decide whether glTexSubImage2D/3D accepts GL_STENCIL_INDEX or not.
    bool supportsStencilWriteTexture =
        gl.GetVersion().IsDesktop() || gl.IsGLExtensionSupported("GL_OES_texture_stencil8");

    // TODO(crbug.com/dawn/343): Investigate emulation.
    deviceToggles->Default(Toggle::DisableIndexedDrawBuffers, !supportsIndexedDrawBuffers);
    deviceToggles->Default(Toggle::DisableSampleVariables, !supportsSampleVariables);
    deviceToggles->Default(Toggle::FlushBeforeClientWaitSync, gl.GetVersion().IsES());
    // For OpenGL ES, we must use a placeholder fragment shader for vertex-only render pipeline.
    deviceToggles->Default(Toggle::UsePlaceholderFragmentInVertexOnlyPipeline,
                           gl.GetVersion().IsES());
    // For OpenGL/OpenGL ES, use compute shader blit to emulate depth16unorm texture to buffer
    // copies.
    deviceToggles->Default(Toggle::UseBlitForDepth16UnormTextureToBufferCopy, true);

    // For OpenGL ES, use compute shader blit to emulate depth32float texture to buffer copies.
    deviceToggles->Default(Toggle::UseBlitForDepth32FloatTextureToBufferCopy,
                           gl.GetVersion().IsES());

    // For OpenGL ES, use compute shader blit to emulate stencil texture to buffer copies.
    deviceToggles->Default(Toggle::UseBlitForStencilTextureToBufferCopy, gl.GetVersion().IsES());

    // For OpenGL ES, use compute shader blit to emulate snorm texture to buffer copies.
    deviceToggles->Default(Toggle::UseBlitForSnormTextureToBufferCopy,
                           gl.GetVersion().IsES() || !supportsSnormRead);

    // For OpenGL ES, use compute shader blit to emulate bgra8unorm texture to buffer copies.
    deviceToggles->Default(Toggle::UseBlitForBGRA8UnormTextureToBufferCopy, !supportsBGRARead);

    // For OpenGL ES, use compute shader blit to emulate rgb9e5ufloat texture to buffer copies.
    deviceToggles->Default(Toggle::UseBlitForRGB9E5UfloatTextureCopy, gl.GetVersion().IsES());

    // Use a blit to emulate stencil-only buffer-to-texture copies.
    deviceToggles->Default(Toggle::UseBlitForBufferToStencilTextureCopy, true);

    // Use a blit to emulate write to stencil textures.
    deviceToggles->Default(Toggle::UseBlitForStencilTextureWrite, !supportsStencilWriteTexture);

    // Use T2B and B2T copies to emulate a T2T copy between sRGB and non-sRGB textures.
    deviceToggles->Default(Toggle::UseT2B2TForSRGBTextureCopy, true);
}

ResultOrError<Ref<DeviceBase>> PhysicalDevice::CreateDeviceImpl(
    AdapterBase* adapter,
    const UnpackedPtr<DeviceDescriptor>& descriptor,
    const TogglesState& deviceToggles,
    Ref<DeviceBase::DeviceLostEvent>&& lostEvent) {
    bool useANGLETextureSharing = false;
    for (size_t i = 0; i < descriptor->requiredFeatureCount; ++i) {
        if (descriptor->requiredFeatures[i] == wgpu::FeatureName::ANGLETextureSharing) {
            useANGLETextureSharing = true;
        }
    }

    bool useRobustness = !deviceToggles.IsEnabled(Toggle::DisableRobustness);

    std::unique_ptr<ContextEGL> context;
    DAWN_TRY_ASSIGN(context, ContextEGL::Create(mDisplay, GetBackendType(), useRobustness,
                                                useANGLETextureSharing));

    return Device::Create(adapter, descriptor, mFunctions, std::move(context), deviceToggles,
                          std::move(lostEvent));
}

bool PhysicalDevice::SupportsFeatureLevel(FeatureLevel featureLevel) const {
    return featureLevel == FeatureLevel::Compatibility;
}

ResultOrError<PhysicalDeviceSurfaceCapabilities> PhysicalDevice::GetSurfaceCapabilities(
    InstanceBase*,
    const Surface*) const {
    PhysicalDeviceSurfaceCapabilities capabilities;

    capabilities.usages = wgpu::TextureUsage::RenderAttachment |
                          wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding |
                          wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst;

    for (wgpu::TextureFormat format : mDisplay->GetPotentialSurfaceFormats()) {
        if (mDisplay->ChooseConfig(EGL_WINDOW_BIT, format) != kNoConfig) {
            capabilities.formats.push_back(format);
        }
    }

    capabilities.presentModes = {
        wgpu::PresentMode::Fifo,
        wgpu::PresentMode::Immediate,
        wgpu::PresentMode::Mailbox,
    };

    capabilities.alphaModes = {
        wgpu::CompositeAlphaMode::Opaque,
    };

    return capabilities;
}

FeatureValidationResult PhysicalDevice::ValidateFeatureSupportedWithTogglesImpl(
    wgpu::FeatureName feature,
    const TogglesState& toggles) const {
    return {};
}

void PhysicalDevice::PopulateBackendProperties(UnpackedPtr<AdapterProperties>& properties) const {}

}  // namespace dawn::native::opengl
