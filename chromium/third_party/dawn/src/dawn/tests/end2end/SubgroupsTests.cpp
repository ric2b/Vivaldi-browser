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

#include <cstdint>
#include <string>
#include <vector>

#include "dawn/common/GPUInfo.h"
#include "dawn/common/Math.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

template <class Params>
class SubgroupsTestsBase : public DawnTestWithParams<Params> {
  public:
    using DawnTestWithParams<Params>::GetParam;
    using DawnTestWithParams<Params>::SupportsFeatures;

  protected:
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        // Always require related features if available.
        std::vector<wgpu::FeatureName> requiredFeatures;
        if (SupportsFeatures({wgpu::FeatureName::ShaderF16})) {
            mRequiredShaderF16Feature = true;
            requiredFeatures.push_back(wgpu::FeatureName::ShaderF16);
        }

        // Require either ChromiumExperimentalSubgroups or Subgroups/F16, but not both of them, so
        // that we can test the code path not involving ChromiumExperimentalSubgroups.
        if (GetParam().mUseChromiumExperimentalSubgroups) {
            if (SupportsFeatures({wgpu::FeatureName::ChromiumExperimentalSubgroups})) {
                mRequiredChromiumExperimentalSubgroups = true;
                requiredFeatures.push_back(wgpu::FeatureName::ChromiumExperimentalSubgroups);
            }
        } else {
            if (SupportsFeatures({wgpu::FeatureName::Subgroups})) {
                mRequiredSubgroupsFeature = true;
                requiredFeatures.push_back(wgpu::FeatureName::Subgroups);
            }
            if (SupportsFeatures({wgpu::FeatureName::SubgroupsF16})) {
                // SubgroupsF16 feature could be supported only if ShaderF16 and Subgroups features
                // are also supported.
                DAWN_ASSERT(mRequiredShaderF16Feature && mRequiredSubgroupsFeature);
                mRequiredSubgroupsF16Feature = true;
                requiredFeatures.push_back(wgpu::FeatureName::SubgroupsF16);
            }
        }

        mSubgroupsF16SupportedByBackend = SupportsFeatures({wgpu::FeatureName::SubgroupsF16});

        return requiredFeatures;
    }

    // Helper function that write enable directives for all required features into WGSL code
    std::stringstream& EnableExtensions(std::stringstream& code) {
        if (mRequiredShaderF16Feature) {
            code << "enable f16;";
        }
        if (GetParam().mUseChromiumExperimentalSubgroups) {
            code << "enable chromium_experimental_subgroups;";
        } else {
            if (mRequiredSubgroupsFeature) {
                code << "enable subgroups;";
            }
            if (mRequiredSubgroupsF16Feature) {
                code << "enable subgroups_f16;";
            }
        }
        return code;
    }

    bool IsShaderF16EnabledInWGSL() const { return mRequiredShaderF16Feature; }
    bool IsSubgroupsEnabledInWGSL() const {
        return mRequiredSubgroupsFeature || mRequiredChromiumExperimentalSubgroups;
    }
    bool IsSubgroupsF16EnabledInWGSL() const {
        return mRequiredSubgroupsF16Feature || mRequiredChromiumExperimentalSubgroups;
    }
    bool IsChromiumExperimentalSubgroupsRequired() const {
        return mRequiredChromiumExperimentalSubgroups;
    }
    bool IsSubgroupsF16SupportedByBackend() const { return mSubgroupsF16SupportedByBackend; }

  private:
    bool mRequiredShaderF16Feature = false;
    bool mRequiredSubgroupsFeature = false;
    bool mRequiredSubgroupsF16Feature = false;
    bool mRequiredChromiumExperimentalSubgroups = false;
    // Indicates that backend actually supports using subgroups functions with f16 types. Note that
    // using ChromiumExperimentalSubgroups allows subgroups_f16 extension in WGSL, but does not
    // ensure that backend supports using it.
    bool mSubgroupsF16SupportedByBackend = false;
};

using UseChromiumExperimentalSubgroups = bool;
DAWN_TEST_PARAM_STRUCT(SubgroupsShaderTestsParams, UseChromiumExperimentalSubgroups);

class SubgroupsShaderTests
    : public SubgroupsTestsBase<SubgroupsShaderTestsParams> {
  protected:
    // Testing reading subgroup_size. The shader declares a workgroup size of [workgroupSize, 1, 1],
    // in which each invocation read the workgroup_size built-in value and write back to output
    // buffer. It is expected that all output workgroup_size are equal and valid, i.e. between 1~128
    // and is a power of 2.
    void TestReadSubgroupSize(uint32_t workgroupSize) {
        auto shaderModule = CreateShaderModuleForReadSubgroupSize(workgroupSize);

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = shaderModule;
        auto pipeline = device.CreateComputePipeline(&csDesc);

        uint32_t outputBufferSizeInBytes = workgroupSize * sizeof(uint32_t);
        wgpu::BufferDescriptor outputBufferDesc;
        outputBufferDesc.size = outputBufferSizeInBytes;
        outputBufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
        wgpu::Buffer outputBuffer = device.CreateBuffer(&outputBufferDesc);

        wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                         {
                                                             {0, outputBuffer},
                                                         });

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.DispatchWorkgroups(1);
        pass.End();
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        EXPECT_BUFFER(outputBuffer, 0, outputBufferSizeInBytes,
                      new ExpectReadSubgroupSizeOutput(workgroupSize));
    }

  private:
    // Helper function that create shader module for testing reading subgroup_size. The shader
    // declares a workgroup size of [workgroupSize, 1, 1], in which each invocation read the
    // workgroup_size built-in value and write back to output buffer. It is expected that all
    // output workgroup_size are equal and valid, i.e. between 1~128 and is a power of 2.
    wgpu::ShaderModule CreateShaderModuleForReadSubgroupSize(uint32_t workgroupSize) {
        DAWN_ASSERT((1 <= workgroupSize) && (workgroupSize <= 256));
        std::stringstream code;
        EnableExtensions(code) << R"(
const workgroupSize = )" << workgroupSize
                               << R"(u;

@group(0) @binding(0) var<storage, read_write> output : array<u32, workgroupSize>;

@compute @workgroup_size(workgroupSize, 1, 1)
fn main(
    @builtin(local_invocation_id) local_id : vec3u,
    @builtin(subgroup_size) sg_size : u32
) {
    output[local_id.x] = sg_size;
}
)";
        return utils::CreateShaderModule(device, code.str().c_str());
    }

    class ExpectReadSubgroupSizeOutput : public dawn::detail::Expectation {
      public:
        explicit ExpectReadSubgroupSizeOutput(uint32_t workgroupSize)
            : mWorkgroupSize(workgroupSize) {}
        testing::AssertionResult Check(const void* data, size_t size) override {
            DAWN_ASSERT(size == sizeof(int32_t) * mWorkgroupSize);
            const uint32_t* actual = static_cast<const uint32_t*>(data);

            const uint32_t& outputSubgroupSizeAt0 = actual[0];
            // Validate that output subgroup_size is valid
            if (!(
                    // subgroup_size should be at least 1
                    (1 <= outputSubgroupSizeAt0) &&
                    // subgroup_size should be no larger than 128
                    (outputSubgroupSizeAt0 <= 128) &&
                    // subgroup_size should be a power of 2
                    ((outputSubgroupSizeAt0 & (outputSubgroupSizeAt0 - 1)) == 0))) {
                testing::AssertionResult result = testing::AssertionFailure()
                                                  << "Got invalid subgroup_size output: "
                                                  << outputSubgroupSizeAt0;
                return result;
            }
            // Validate that subgroup_size of all invocation are identical.
            for (uint32_t i = 1; i < mWorkgroupSize; i++) {
                const uint32_t& outputSubgroupSize = actual[i];
                if (outputSubgroupSize != outputSubgroupSizeAt0) {
                    testing::AssertionResult result = testing::AssertionFailure()
                                                      << "Got inconsistent subgroup_size output: "
                                                         "subgroup_size of invocation 0 is "
                                                      << outputSubgroupSizeAt0
                                                      << ", while invocation " << i << " is "
                                                      << outputSubgroupSize;
                    return result;
                }
            }

            return testing::AssertionSuccess();
        }

      private:
        uint32_t mWorkgroupSize;
    };
};

// Test that subgroup_size builtin attribute read by each invocation is valid and identical for any
// workgroup size between 1 and 256.
TEST_P(SubgroupsShaderTests, ReadSubgroupSize) {
    DAWN_TEST_UNSUPPORTED_IF(!IsSubgroupsEnabledInWGSL());

    for (uint32_t workgroupSize : {1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256}) {
        TestReadSubgroupSize(workgroupSize);
    }
}

// DawnTestBase::CreateDeviceImpl always enables allow_unsafe_apis toggle.
DAWN_INSTANTIATE_TEST_P(SubgroupsShaderTests,
                        {D3D12Backend(), D3D12Backend({}, {"use_dxc"}), MetalBackend(),
                         VulkanBackend()},
                        {false, true}  // UseChromiumExperimentalSubgroups
);

class SubgroupsShaderTestsFragment : public SubgroupsTestsBase<SubgroupsShaderTestsParams> {
  protected:
    // Testing reading subgroup_size in fragment shader. There is no workgroup size here and
    // subgroup_size is varying.
    void FragmentSubgroupSizeTest() {
        wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, R"(
            @vertex
            fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4f {
                var pos = array(
                    vec2f(-1.0, -1.0),
                    vec2f(-1.0,  1.0),
                    vec2f( 1.0, -1.0),
                    vec2f( 1.0,  1.0),
                    vec2f(-1.0,  1.0),
                    vec2f( 1.0, -1.0));
                return vec4f(pos[VertexIndex], 0.5, 1.0);
            })");

        std::stringstream fsCode;
        {
            EnableExtensions(fsCode);
            fsCode << R"(
            @group(0) @binding(0) var<storage, read_write> output : array<u32>;
            @fragment fn main(@builtin(subgroup_size) sg_size : u32) -> @location(0) vec4f {
                output[0] = sg_size;
                return vec4f(0.0, 1.0, 0.0, 1.0);
            })";
        }

        wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, fsCode.str().c_str());

        utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 100, 100);
        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.vertex.module = vsModule;
        descriptor.cFragment.module = fsModule;
        descriptor.cTargets[0].format = renderPass.colorFormat;

        auto pipeline = device.CreateRenderPipeline(&descriptor);

        constexpr uint32_t kArrayNumElements = 1u;
        uint32_t outputBufferSizeInBytes = sizeof(uint32_t) * kArrayNumElements;
        wgpu::BufferDescriptor outputBufferDesc;
        outputBufferDesc.size = outputBufferSizeInBytes;
        outputBufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
        wgpu::Buffer outputBuffer = device.CreateBuffer(&outputBufferDesc);
        wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                         {
                                                             {0, outputBuffer},
                                                         });

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.Draw(6);
            pass.End();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        // Check that the fragment shader ran.
        EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kGreen, renderPass.color, 0, 0);
        EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kGreen, renderPass.color, 0, 99);
        EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kGreen, renderPass.color, 99, 0);
        EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kGreen, renderPass.color, 99, 99);

        EXPECT_BUFFER(outputBuffer, 0, outputBufferSizeInBytes, new ExpectReadSubgroupSizeOutput());
    }

  private:
    class ExpectReadSubgroupSizeOutput : public dawn::detail::Expectation {
      public:
        ExpectReadSubgroupSizeOutput() {}
        testing::AssertionResult Check(const void* data, size_t size) override {
            DAWN_ASSERT(size == sizeof(int32_t));
            const uint32_t* actual = static_cast<const uint32_t*>(data);
            const uint32_t& outputSubgroupSizeAt0 = actual[0];
            // Subgroup size can vary across fragment invocation (unlike compute) but we could still
            // improve this check by using the min and max subgroup size for the device.
            if (!(
                    // subgroup_size should be at least 1
                    (1 <= outputSubgroupSizeAt0) &&
                    // subgroup_size should be no larger than 128
                    (outputSubgroupSizeAt0 <= 128) &&
                    // subgroup_size should be a power of 2
                    (IsPowerOfTwo(outputSubgroupSizeAt0)))) {
                testing::AssertionResult result = testing::AssertionFailure()
                                                  << "Got invalid subgroup_size output: "
                                                  << outputSubgroupSizeAt0;
                return result;
            }
            return testing::AssertionSuccess();
        }
    };
};

// Test that subgroup_size builtin attribute read by each invocation is valid and identical for any
// workgroup size between 1 and 256.
TEST_P(SubgroupsShaderTestsFragment, ReadSubgroupSize) {
    DAWN_TEST_UNSUPPORTED_IF(!IsSubgroupsEnabledInWGSL());
    FragmentSubgroupSizeTest();
}

// DawnTestBase::CreateDeviceImpl always enables allow_unsafe_apis toggle.
DAWN_INSTANTIATE_TEST_P(SubgroupsShaderTestsFragment,
                        {D3D12Backend(), D3D12Backend({}, {"use_dxc"}), MetalBackend(),
                         VulkanBackend()},
                        {false, true}  // UseChromiumExperimentalSubgroups
);

enum class BroadcastType {
    I32,
    U32,
    F32,
    F16,
};

std::ostream& operator<<(std::ostream& o, BroadcastType broadcastType) {
    switch (broadcastType) {
        case BroadcastType::I32:
            o << "i32";
            break;
        case BroadcastType::U32:
            o << "u32";
            break;
        case BroadcastType::F32:
            o << "f32";
            break;
        case BroadcastType::F16:
            o << "f16";
            break;
    }
    return o;
}

// Indicate which kind of value is the register of invocation 0 set to in subgroupBroadcast tests,
// and it will be broadcast to its subgroup.
enum class SubgroupBroadcastValueOfInvocation0 {
    Constant,      // Initialize reg of invocation 0 to
                   // SubgroupBroadcastConstantValueForInvocation0
    SubgroupSize,  // Initialize reg of invocation 0 to the value of subgroup_size
};

std::ostream& operator<<(std::ostream& o,
                         SubgroupBroadcastValueOfInvocation0 subgroupBroadcastValueOfInvocation0) {
    switch (subgroupBroadcastValueOfInvocation0) {
        case SubgroupBroadcastValueOfInvocation0::Constant:
            o << "Constant";
            break;
        case SubgroupBroadcastValueOfInvocation0::SubgroupSize:
            o << "SubgroupSize";
            break;
    }
    return o;
}

using UseChromiumExperimentalSubgroups = bool;
DAWN_TEST_PARAM_STRUCT(SubgroupsBroadcastTestsParams,
                       UseChromiumExperimentalSubgroups,
                       BroadcastType,
                       SubgroupBroadcastValueOfInvocation0);

// These two constants should be different so that the broadcast results from invocation 0 can be
// distinguished from other invocations, and both should not be 0 so that the broadcast results can
// be distinguished from zero-initialized empty buffer. They should also be exactly-representable in
// f16 type so we can expect the exact result values for f16 tests.
constexpr int32_t SubgroupBroadcastConstantValueForInvocation0 = 1;
constexpr int32_t SubgroupRegisterInitializer = 555;

class SubgroupsBroadcastTests
    : public SubgroupsTestsBase<SubgroupsBroadcastTestsParams> {
  protected:
    // Testing subgroup broadcasting. The shader declares a workgroup size of [workgroupSize, 1, 1],
    // in which each invocation hold a register initialized to SubgroupRegisterInitializer, then
    // sets the register of invocation 0 to SubgroupBroadcastConstantValueForInvocation0 or value of
    // subgroup_size, broadcasts the register's value of subgroup_id 0 for all subgroups, and writes
    // back each invocation's register to buffer broadcastOutput. After dispatching, it is expected
    // that broadcastOutput contains exactly [subgroup_size] elements being of
    // SubgroupBroadcastConstantValueForInvocation0 of value [subgroup_size] and all other elements
    // being SubgroupRegisterInitializer. Note that although we assume invocation 0 of the workgroup
    // has a subgroup_id of 0 in its subgroup, we don't assume any other particular subgroups layout
    // property.
    void TestBroadcastSubgroupSize(uint32_t workgroupSize) {
        auto shaderModule = CreateShaderModuleForBroadcastSubgroupSize(workgroupSize);

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = shaderModule;
        auto pipeline = device.CreateComputePipeline(&csDesc);

        uint32_t outputBufferSizeInBytes = (1 + workgroupSize) * sizeof(uint32_t);
        wgpu::BufferDescriptor outputBufferDesc;
        outputBufferDesc.size = outputBufferSizeInBytes;
        outputBufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
        wgpu::Buffer outputBuffer = device.CreateBuffer(&outputBufferDesc);

        wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                         {
                                                             {0, outputBuffer},
                                                         });

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.DispatchWorkgroups(1);
        pass.End();
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        EXPECT_BUFFER(outputBuffer, 0, outputBufferSizeInBytes,
                      new ExpectBroadcastSubgroupSizeOutput(workgroupSize));
    }

  private:
    // Helper function that create shader module for testing broadcasting subgroup_size. The shader
    // declares a workgroup size of [workgroupSize, 1, 1], in which each invocation hold a register
    // initialized to SubgroupRegisterInitializer, then sets the register of invocation 0 to
    // SubgroupBroadcastConstantValueForInvocation0 or value of subgroup_size, broadcasts the
    // register's value of subgroup_id 0 for all subgroups, and writes back each invocation's
    // register to buffer broadcastOutput.
    wgpu::ShaderModule CreateShaderModuleForBroadcastSubgroupSize(uint32_t workgroupSize) {
        DAWN_ASSERT((1 <= workgroupSize) && (workgroupSize <= 256));
        std::stringstream code;
        EnableExtensions(code) << R"(
const workgroupSize = )" << workgroupSize
                               << R"(u;
alias BroadcastType = )" << GetParam().mBroadcastType
                               << R"(;

struct Output {
    subgroupSizeOutput : u32,
    broadcastOutput : array<i32, workgroupSize>,
};
@group(0) @binding(0) var<storage, read_write> output : Output;

@compute @workgroup_size(workgroupSize, 1, 1)
fn main(
    @builtin(local_invocation_id) local_id : vec3u,
    @builtin(subgroup_size) sg_size : u32
) {
    // Initialize the register of BroadcastType to SubgroupRegisterInitializer.
    var reg: BroadcastType = BroadcastType()"
                               << SubgroupRegisterInitializer << R"();
    // Set the register value to subgroup size for invocation 0, and also output the subgroup size.
    if (all(local_id == vec3u())) {
        reg = BroadcastType()";
        switch (GetParam().mSubgroupBroadcastValueOfInvocation0) {
            case SubgroupBroadcastValueOfInvocation0::Constant: {
                code << SubgroupBroadcastConstantValueForInvocation0;
                break;
            }
            case SubgroupBroadcastValueOfInvocation0::SubgroupSize: {
                code << "sg_size";
                break;
            }
        }
        code << R"();
        output.subgroupSizeOutput = sg_size;
    }
    workgroupBarrier();
    // Broadcast the register value of subgroup_id 0 in each subgroup.
    reg = subgroupBroadcast(reg, 0u);
    // Write back the register value in i32.
    output.broadcastOutput[local_id.x] = i32(reg);
}
)";
        return utils::CreateShaderModule(device, code.str().c_str());
    }

    class ExpectBroadcastSubgroupSizeOutput : public dawn::detail::Expectation {
      public:
        explicit ExpectBroadcastSubgroupSizeOutput(uint32_t workgroupSize)
            : mWorkgroupSize(workgroupSize) {}
        testing::AssertionResult Check(const void* data, size_t size) override {
            DAWN_ASSERT(size == sizeof(int32_t) * (1 + mWorkgroupSize));
            const int32_t* actual = static_cast<const int32_t*>(data);

            int32_t outputSubgroupSize = actual[0];
            if (!(
                    // subgroup_size should be at least 1
                    (1 <= outputSubgroupSize) &&
                    // subgroup_size should be no larger than 128
                    (outputSubgroupSize <= 128) &&
                    // subgroup_size should be a power of 2
                    ((outputSubgroupSize & (outputSubgroupSize - 1)) == 0))) {
                testing::AssertionResult result = testing::AssertionFailure()
                                                  << "Got invalid subgroup_size output: "
                                                  << outputSubgroupSize;
                return result;
            }

            int32_t valueFromInvocation0;
            switch (GetParam().mSubgroupBroadcastValueOfInvocation0) {
                case SubgroupBroadcastValueOfInvocation0::Constant: {
                    valueFromInvocation0 = SubgroupBroadcastConstantValueForInvocation0;
                    break;
                }
                case SubgroupBroadcastValueOfInvocation0::SubgroupSize: {
                    valueFromInvocation0 = outputSubgroupSize;
                    break;
                }
            }

            // Expected that broadcastOutput contains exactly [subgroup_size] elements being of
            // value [subgroup_size] and all other elements being -1 (placeholder). Note that
            // although we assume invocation 0 of the workgroup has a subgroup_id of 0 in its
            // subgroup, we don't assume any other particular subgroups layout property.
            uint32_t valueFromInvocation0Count = 0;
            uint32_t valueFromOtherInvocationCount = 0;
            for (uint32_t i = 0; i < mWorkgroupSize; i++) {
                int32_t broadcastOutput = actual[i + 1];
                if (broadcastOutput == valueFromInvocation0) {
                    valueFromInvocation0Count++;
                } else if (broadcastOutput == SubgroupRegisterInitializer) {
                    valueFromOtherInvocationCount++;
                } else {
                    testing::AssertionResult result = testing::AssertionFailure()
                                                      << "Got invalid broadcastOutput[" << i
                                                      << "] : " << broadcastOutput << ", expected "
                                                      << valueFromInvocation0 << " or "
                                                      << SubgroupRegisterInitializer << ".";
                    return result;
                }
            }

            uint32_t expectedValueFromInvocation0Count =
                (static_cast<int32_t>(mWorkgroupSize) < outputSubgroupSize) ? mWorkgroupSize
                                                                            : outputSubgroupSize;
            uint32_t expectedValueFromOtherInvocationCount =
                mWorkgroupSize - expectedValueFromInvocation0Count;
            if ((valueFromInvocation0Count != expectedValueFromInvocation0Count) ||
                (valueFromOtherInvocationCount != expectedValueFromOtherInvocationCount)) {
                testing::AssertionResult result =
                    testing::AssertionFailure()
                    << "Unexpected broadcastOutput, got " << valueFromInvocation0Count
                    << " elements of value " << valueFromInvocation0 << " and "
                    << valueFromOtherInvocationCount << " elements of value "
                    << SubgroupRegisterInitializer << ", expected "
                    << expectedValueFromInvocation0Count << " elements of value "
                    << valueFromInvocation0 << " and " << expectedValueFromOtherInvocationCount
                    << " elements of value " << SubgroupRegisterInitializer << ".";
                return result;
            }

            return testing::AssertionSuccess();
        }

      private:
        uint32_t mWorkgroupSize;
    };
};

// Test that subgroupBroadcast builtin function works as expected for any workgroup size between 1
// and 256. Note that although we assume invocation 0 of the workgroup has a subgroup_id of 0 in its
// subgroup, we don't assume any other particular subgroups layout property.
TEST_P(SubgroupsBroadcastTests, SubgroupBroadcast) {
    if (GetParam().mBroadcastType == BroadcastType::F16) {
        DAWN_TEST_UNSUPPORTED_IF(!IsSubgroupsF16SupportedByBackend());
        DAWN_ASSERT(IsShaderF16EnabledInWGSL() && IsSubgroupsEnabledInWGSL() &&
                    IsSubgroupsF16EnabledInWGSL());
    } else {
        DAWN_TEST_UNSUPPORTED_IF(!IsSubgroupsEnabledInWGSL());
    }

    // TODO(351745820): Suppress the test for Qualcomm Adreno 6xx until we figure out why creating
    // compute pipeline with subgroupBroadcast shader fails on trybots using these devices.
    DAWN_SUPPRESS_TEST_IF(gpu_info::IsQualcomm_PCIAdreno6xx(GetParam().adapterProperties.vendorID,
                                                            GetParam().adapterProperties.deviceID));

    for (uint32_t workgroupSize : {1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256}) {
        TestBroadcastSubgroupSize(workgroupSize);
    }
}

// DawnTestBase::CreateDeviceImpl always enables allow_unsafe_apis toggle.
DAWN_INSTANTIATE_TEST_P(SubgroupsBroadcastTests,
                        {D3D12Backend(), D3D12Backend({}, {"use_dxc"}), MetalBackend(),
                         VulkanBackend()},
                        {false, true},  // UseChromiumExperimentalSubgroups
                        {
                            BroadcastType::I32,
                            BroadcastType::U32,
                            BroadcastType::F32,
                            BroadcastType::F16,
                        },  // BroadcastType
                        {SubgroupBroadcastValueOfInvocation0::Constant,
                         SubgroupBroadcastValueOfInvocation0::SubgroupSize}
                        // SubgroupBroadcastValueOfInvocation0
);

using UseChromiumExperimentalSubgroups = bool;
DAWN_TEST_PARAM_STRUCT(SubgroupsFullSubgroupsTestsParams,
                       UseChromiumExperimentalSubgroups);

class SubgroupsFullSubgroupsTests
    : public SubgroupsTestsBase<SubgroupsFullSubgroupsTestsParams> {
  protected:
    // Helper function that create shader module with subgroups extension required and a empty
    // compute entry point, named main, of given workgroup size
    wgpu::ShaderModule CreateShaderModuleWithSubgroupsRequired(WGPUExtent3D workgroupSize = {1, 1,
                                                                                             1}) {
        std::stringstream code;

        EnableExtensions(code) << R"(
        @compute @workgroup_size()"
                               << workgroupSize.width << ", " << workgroupSize.height << ", "
                               << workgroupSize.depthOrArrayLayers << R"()
        fn main() {}
)";
        return utils::CreateShaderModule(device, code.str().c_str());
    }

    // Helper function that create shader module with subgroups extension required and a empty
    // compute entry point, named main, of workgroup size that are override constants.
    wgpu::ShaderModule CreateShaderModuleWithOverrideWorkgroupSize() {
        std::stringstream code;
        EnableExtensions(code) << R"(
        override wgs_x: u32;
        override wgs_y: u32;
        override wgs_z: u32;

        @compute @workgroup_size(wgs_x, wgs_y, wgs_z)
        fn main() {}
)";
        return utils::CreateShaderModule(device, code.str().c_str());
    }

    struct TestCase {
        WGPUExtent3D workgroupSize;
        bool isFullSubgroups;
    };

    // Helper function that generate workgroup size cases for full subgroups test, based on device
    // reported max subgroup size.
    std::vector<TestCase> GenerateFullSubgroupsWorkgroupSizeCases() {
        wgpu::SupportedLimits limits{};
        wgpu::DawnExperimentalSubgroupLimits subgroupLimits{};
        limits.nextInChain = &subgroupLimits;
        EXPECT_EQ(device.GetLimits(&limits), wgpu::Status::Success);
        uint32_t maxSubgroupSize = subgroupLimits.maxSubgroupSize;
        EXPECT_TRUE(1 <= maxSubgroupSize && maxSubgroupSize <= 128);
        // maxSubgroupSize should be a power of 2.
        EXPECT_TRUE(IsPowerOfTwo(maxSubgroupSize));

        std::vector<TestCase> cases;

        // workgroup_size.x = maxSubgroupSize, is a multiple of maxSubgroupSize.
        cases.push_back({{maxSubgroupSize, 1, 1}, true});
        // Note that maxSubgroupSize is no larger than 128, so threads in the wrokgroups below is no
        // more than 256, fits in the maxComputeInvocationsPerWorkgroup limit which is at least 256.
        cases.push_back({{maxSubgroupSize * 2, 1, 1}, true});
        cases.push_back({{maxSubgroupSize, 2, 1}, true});
        cases.push_back({{maxSubgroupSize, 1, 2}, true});

        EXPECT_TRUE(maxSubgroupSize >= 4);
        // workgroup_size.x = maxSubgroupSize / 2, not a multiple of maxSubgroupSize.
        cases.push_back({{maxSubgroupSize / 2, 1, 1}, false});
        cases.push_back({{maxSubgroupSize / 2, 2, 1}, false});
        // workgroup_size.x = maxSubgroupSize - 1, not a multiple of maxSubgroupSize.
        cases.push_back({{maxSubgroupSize - 1, 1, 1}, false});
        // workgroup_size.x = maxSubgroupSize * 2 - 1, not a multiple of maxSubgroupSize if
        // maxSubgroupSize > 1.
        cases.push_back({{maxSubgroupSize * 2 - 1, 1, 1}, false});
        // workgroup_size.x = 1, not a multiple of maxSubgroupSize. Test that validation
        // checks the x dimension of workgroup size instead of others.
        cases.push_back({{1, maxSubgroupSize, 1}, false});

        return cases;
    }
};

// Test that creating compute pipeline with full subgroups required will validate the workgroup size
// as expected, when using compute shader with literal workgroup size.
TEST_P(SubgroupsFullSubgroupsTests,
       ComputePipelineRequiringFullSubgroupsWithLiteralWorkgroupSize) {
    // Currently DawnComputePipelineFullSubgroups only supported with ChromiumExperimentalSubgroups
    // enabled.
    DAWN_TEST_UNSUPPORTED_IF(!IsChromiumExperimentalSubgroupsRequired());

    // Keep all success compute pipeline alive, so that we can test the compute pipeline cache.
    std::vector<wgpu::ComputePipeline> computePipelines;

    for (const TestCase& c : GenerateFullSubgroupsWorkgroupSizeCases()) {
        // Reuse the shader module for both not requiring and requiring full subgroups cases, to
        // test that cached compute pipeline will not be used unexpectedly.
        auto shaderModule = CreateShaderModuleWithSubgroupsRequired(c.workgroupSize);
        for (bool requiresFullSubgroups : {false, true}) {
            wgpu::ComputePipelineDescriptor csDesc;
            csDesc.compute.module = shaderModule;

            wgpu::DawnComputePipelineFullSubgroups fullSubgroupsOption;
            fullSubgroupsOption.requiresFullSubgroups = requiresFullSubgroups;
            csDesc.nextInChain = &fullSubgroupsOption;

            // It should be a validation error if full subgroups is required but given workgroup
            // size does not fit.
            if (requiresFullSubgroups && !c.isFullSubgroups) {
                ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&csDesc));
            } else {
                // Otherwise, creating compute pipeline should succeed.
                computePipelines.push_back(device.CreateComputePipeline(&csDesc));
            }
        }
    }
}

// Test that creating compute pipeline with full subgroups required will validate the workgroup size
// as expected, when using compute shader with override constants workgroup size.
TEST_P(SubgroupsFullSubgroupsTests,
       ComputePipelineRequiringFullSubgroupsWithOverrideWorkgroupSize) {
    // Currently DawnComputePipelineFullSubgroups only supported with ChromiumExperimentalSubgroups
    // enabled.
    DAWN_TEST_UNSUPPORTED_IF(!IsChromiumExperimentalSubgroupsRequired());
    // Reuse the same shader module for all case to test the validation happened as expected.
    auto shaderModule = CreateShaderModuleWithOverrideWorkgroupSize();
    // Keep all success compute pipeline alive, so that we can test the compute pipeline cache.
    std::vector<wgpu::ComputePipeline> computePipelines;

    for (const TestCase& c : GenerateFullSubgroupsWorkgroupSizeCases()) {
        for (bool requiresFullSubgroups : {false, true}) {
            std::vector<wgpu::ConstantEntry> constants{
                {nullptr, "wgs_x", static_cast<double>(c.workgroupSize.width)},
                {nullptr, "wgs_y", static_cast<double>(c.workgroupSize.height)},
                {nullptr, "wgs_z", static_cast<double>(c.workgroupSize.depthOrArrayLayers)},
            };

            wgpu::ComputePipelineDescriptor csDesc;
            csDesc.compute.module = shaderModule;
            csDesc.compute.constants = constants.data();
            csDesc.compute.constantCount = constants.size();

            wgpu::DawnComputePipelineFullSubgroups fullSubgroupsOption;
            fullSubgroupsOption.requiresFullSubgroups = requiresFullSubgroups;
            csDesc.nextInChain = &fullSubgroupsOption;

            // It should be a validation error if full subgroups is required but given workgroup
            // size does not fit.
            if (requiresFullSubgroups && !c.isFullSubgroups) {
                ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&csDesc));
            } else {
                // Otherwise, creating compute pipeline should succeed.
                computePipelines.push_back(device.CreateComputePipeline(&csDesc));
            }
        }
    }
}

// DawnTestBase::CreateDeviceImpl always enables allow_unsafe_apis toggle.
DAWN_INSTANTIATE_TEST_P(SubgroupsFullSubgroupsTests,
                        {D3D12Backend(), D3D12Backend({}, {"use_dxc"}), MetalBackend(),
                         VulkanBackend()},
                        {false, true}  // UseChromiumExperimentalSubgroups
);

}  // anonymous namespace
}  // namespace dawn
