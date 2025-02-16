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

#include <memory>
#include <utility>

#include "dawn/common/StringViewUtils.h"
#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"
#include "dawn/tests/StringViewMatchers.h"
#include "dawn/tests/unittests/wire/WireFutureTest.h"
#include "dawn/tests/unittests/wire/WireTest.h"
#include "dawn/utils/TerribleCommandBuffer.h"
#include "dawn/wire/WireClient.h"
#include "dawn/wire/WireServer.h"

namespace dawn::wire {
namespace {

using testing::_;
using testing::EmptySizedString;
using testing::InvokeWithoutArgs;
using testing::NonEmptySizedString;
using testing::NotNull;
using testing::Return;
using testing::SizedString;

using WireCreateComputePipelineAsyncTestBase =
    WireFutureTest<WGPUCreateComputePipelineAsyncCallback,
                   WGPUCreateComputePipelineAsyncCallbackInfo,
                   wgpuDeviceCreateComputePipelineAsync,
                   wgpuDeviceCreateComputePipelineAsyncF>;
using WireCreateRenderPipelineAsyncTestBase =
    WireFutureTest<WGPUCreateRenderPipelineAsyncCallback,
                   WGPUCreateRenderPipelineAsyncCallbackInfo,
                   wgpuDeviceCreateRenderPipelineAsync,
                   wgpuDeviceCreateRenderPipelineAsyncF>;

class WireCreateComputePipelineAsyncTest : public WireCreateComputePipelineAsyncTestBase {
  protected:
    // Overridden version of wgpuDeviceCreateComputePipelineAsync that defers to the API call based
    // on the test callback mode.
    void DeviceCreateComputePipelineAsync(WGPUDevice d,
                                          WGPUComputePipelineDescriptor const* desc,
                                          void* userdata = nullptr) {
        CallImpl(userdata, d, desc);
    }

    // Sets up default descriptors to use in the tests.
    void SetUp() override {
        WireCreateComputePipelineAsyncTestBase::SetUp();

        apiPipeline = api.GetNewComputePipeline();

        WGPUShaderModuleDescriptor shaderDesc = {};
        mShader = wgpuDeviceCreateShaderModule(cDevice, &shaderDesc);
        mApiShader = api.GetNewShaderModule();
        EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(mApiShader));
        FlushClient();

        mDescriptor.compute.module = mShader;
    }

    WGPUShaderModule mShader;
    WGPUShaderModule mApiShader;
    WGPUComputePipelineDescriptor mDescriptor = {};

    // A successfully created pipeline.
    WGPUComputePipeline apiPipeline;
};
class WireCreateRenderPipelineAsyncTest : public WireCreateRenderPipelineAsyncTestBase {
  protected:
    // Overriden version of wgpuDeviceCreateRenderPipelineAsync that defers to the API call based on
    // the test callback mode.
    void DeviceCreateRenderPipelineAsync(WGPUDevice d,
                                         WGPURenderPipelineDescriptor const* desc,
                                         void* userdata = nullptr) {
        CallImpl(userdata, d, desc);
    }

    // Sets up default descriptors to use in the tests.
    void SetUp() override {
        WireCreateRenderPipelineAsyncTestBase::SetUp();

        apiPipeline = api.GetNewRenderPipeline();

        WGPUShaderModuleDescriptor shaderDesc = {};
        mShader = wgpuDeviceCreateShaderModule(cDevice, &shaderDesc);
        mApiShader = api.GetNewShaderModule();
        EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(mApiShader));
        FlushClient();

        mDescriptor.vertex.module = mShader;
        mFragment.module = mShader;
        mDescriptor.fragment = &mFragment;
    }

    WGPUShaderModule mShader;
    WGPUShaderModule mApiShader;
    WGPUFragmentState mFragment = {};
    WGPURenderPipelineDescriptor mDescriptor = {};

    // A successfully created pipeline.
    WGPURenderPipeline apiPipeline;
};
DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(WireCreateComputePipelineAsyncTest);
DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(WireCreateRenderPipelineAsyncTest);

// Test when creating a compute pipeline with CreateComputePipelineAsync() successfully.
TEST_P(WireCreateComputePipelineAsyncTest, CreateSuccess) {
    DeviceCreateComputePipelineAsync(cDevice, &mDescriptor, this);

    EXPECT_CALL(api, OnDeviceCreateComputePipelineAsync2(apiDevice, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallDeviceCreateComputePipelineAsync2Callback(apiDevice,
                                                              WGPUCreatePipelineAsyncStatus_Success,
                                                              apiPipeline, kEmptyOutputStringView);
        }));

    FlushClient();
    FlushFutures();
    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb,
                    Call(WGPUCreatePipelineAsyncStatus_Success, NotNull(), SizedString(""), this))
            .Times(1);

        FlushCallbacks();
    });
}

// Test when creating a compute pipeline with CreateComputePipelineAsync() results in an error.
TEST_P(WireCreateComputePipelineAsyncTest, CreateError) {
    DeviceCreateComputePipelineAsync(cDevice, &mDescriptor, this);

    EXPECT_CALL(api, OnDeviceCreateComputePipelineAsync2(apiDevice, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallDeviceCreateComputePipelineAsync2Callback(
                apiDevice, WGPUCreatePipelineAsyncStatus_ValidationError, nullptr,
                ToOutputStringView("Some error message"));
        }));

    FlushClient();
    FlushFutures();
    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call(WGPUCreatePipelineAsyncStatus_ValidationError, _,
                                 SizedString("Some error message"), this))
            .Times(1);

        FlushCallbacks();
    });
}

// Test when creating a render pipeline with CreateRenderPipelineAsync() successfully.
TEST_P(WireCreateRenderPipelineAsyncTest, CreateSuccess) {
    DeviceCreateRenderPipelineAsync(cDevice, &mDescriptor, this);

    EXPECT_CALL(api, OnDeviceCreateRenderPipelineAsync2(apiDevice, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallDeviceCreateRenderPipelineAsync2Callback(apiDevice,
                                                             WGPUCreatePipelineAsyncStatus_Success,
                                                             apiPipeline, kEmptyOutputStringView);
        }));

    FlushClient();
    FlushFutures();
    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call(WGPUCreatePipelineAsyncStatus_Success, NotNull(),
                                 EmptySizedString(), this))
            .Times(1);

        FlushCallbacks();
    });
}

// Test when creating a render pipeline with CreateRenderPipelineAsync() results in an error.
TEST_P(WireCreateRenderPipelineAsyncTest, CreateError) {
    DeviceCreateRenderPipelineAsync(cDevice, &mDescriptor, this);

    EXPECT_CALL(api, OnDeviceCreateRenderPipelineAsync2(apiDevice, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallDeviceCreateRenderPipelineAsync2Callback(
                apiDevice, WGPUCreatePipelineAsyncStatus_ValidationError, nullptr,
                ToOutputStringView("Some error message"));
        }));

    FlushClient();
    FlushFutures();
    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call(WGPUCreatePipelineAsyncStatus_ValidationError, _,
                                 SizedString("Some error message"), this))
            .Times(1);

        FlushCallbacks();
    });
}

// Test that registering a callback then wire disconnect calls the callback with
// Success.
TEST_P(WireCreateRenderPipelineAsyncTest, CreateThenDisconnect) {
    DeviceCreateRenderPipelineAsync(cDevice, &mDescriptor, this);

    EXPECT_CALL(api, OnDeviceCreateRenderPipelineAsync2(apiDevice, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallDeviceCreateRenderPipelineAsync2Callback(apiDevice,
                                                             WGPUCreatePipelineAsyncStatus_Success,
                                                             apiPipeline, kEmptyOutputStringView);
        }));

    FlushClient();
    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call(WGPUCreatePipelineAsyncStatus_InstanceDropped, nullptr,
                                 NonEmptySizedString(), this))
            .Times(1);

        GetWireClient()->Disconnect();
    });
}

// Test that registering a callback then wire disconnect calls the callback with
// Success.
TEST_P(WireCreateComputePipelineAsyncTest, CreateThenDisconnect) {
    DeviceCreateComputePipelineAsync(cDevice, &mDescriptor, this);

    EXPECT_CALL(api, OnDeviceCreateComputePipelineAsync2(apiDevice, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallDeviceCreateComputePipelineAsync2Callback(apiDevice,
                                                              WGPUCreatePipelineAsyncStatus_Success,
                                                              apiPipeline, kEmptyOutputStringView);
        }));

    FlushClient();
    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call(WGPUCreatePipelineAsyncStatus_InstanceDropped, nullptr,
                                 NonEmptySizedString(), this))
            .Times(1);

        GetWireClient()->Disconnect();
    });
}

// Test that registering a callback after wire disconnect calls the callback with
// Success.
TEST_P(WireCreateRenderPipelineAsyncTest, CreateAfterDisconnect) {
    GetWireClient()->Disconnect();

    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call(WGPUCreatePipelineAsyncStatus_InstanceDropped, nullptr,
                                 NonEmptySizedString(), this))
            .Times(1);

        DeviceCreateRenderPipelineAsync(cDevice, &mDescriptor, this);
    });
}

// Test that registering a callback after wire disconnect calls the callback with
// Success.
TEST_P(WireCreateComputePipelineAsyncTest, CreateAfterDisconnect) {
    GetWireClient()->Disconnect();

    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call(WGPUCreatePipelineAsyncStatus_InstanceDropped, nullptr,
                                 NonEmptySizedString(), this))
            .Times(1);

        DeviceCreateComputePipelineAsync(cDevice, &mDescriptor, this);
    });
}

// TODO(dawn:2298) Add tests for callbacks when the Instance is released.

// Test that if the server is deleted before the callback, it forces the
// callback to complete.
TEST(WireCreatePipelineAsyncTestNullBackend, ServerDeletedBeforeCallback) {
    // This test sets up its own wire facilities, because unlike the other
    // tests which use mocks, this test needs the null backend and the
    // threadpool which automatically pushes async pipeline compilation
    // to completion. With mocks, we need to explicitly trigger callbacks,
    // but this test depends on triggering the async compilation from
    // *within* the wire server destructor.
    auto c2sBuf = std::make_unique<dawn::utils::TerribleCommandBuffer>();
    auto s2cBuf = std::make_unique<dawn::utils::TerribleCommandBuffer>();

    dawn::wire::WireServerDescriptor serverDesc = {};
    serverDesc.procs = &dawn::native::GetProcs();
    serverDesc.serializer = s2cBuf.get();

    auto wireServer = std::make_unique<dawn::wire::WireServer>(serverDesc);
    c2sBuf->SetHandler(wireServer.get());

    dawn::wire::WireClientDescriptor clientDesc = {};
    clientDesc.serializer = c2sBuf.get();

    auto wireClient = std::make_unique<dawn::wire::WireClient>(clientDesc);
    s2cBuf->SetHandler(wireClient.get());

    dawnProcSetProcs(&dawn::wire::client::GetProcs());

    auto reserved = wireClient->ReserveInstance();
    wgpu::Instance instance = wgpu::Instance::Acquire(reserved.instance);
    wireServer->InjectInstance(dawn::native::GetProcs().createInstance(nullptr), reserved.handle);

    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.backendType = wgpu::BackendType::Null;

    wgpu::Adapter adapter;
    instance.RequestAdapter(&adapterOptions, wgpu::CallbackMode::AllowSpontaneous,
                            [&adapter](wgpu::RequestAdapterStatus, wgpu::Adapter result,
                                       wgpu::StringView) { adapter = std::move(result); });
    ASSERT_TRUE(c2sBuf->Flush());
    ASSERT_TRUE(s2cBuf->Flush());

    wgpu::DeviceDescriptor deviceDesc = {};
    wgpu::Device device;
    adapter.RequestDevice(&deviceDesc, wgpu::CallbackMode::AllowSpontaneous,
                          [&device](wgpu::RequestDeviceStatus, wgpu::Device result,
                                    wgpu::StringView) { device = std::move(result); });
    ASSERT_TRUE(c2sBuf->Flush());
    ASSERT_TRUE(s2cBuf->Flush());

    wgpu::ShaderSourceWGSL wgslDesc = {};
    wgslDesc.code.data = "@compute @workgroup_size(64) fn main() {}";

    wgpu::ShaderModuleDescriptor smDesc = {};
    smDesc.nextInChain = &wgslDesc;

    wgpu::ShaderModule sm = device.CreateShaderModule(&smDesc);

    wgpu::ComputePipelineDescriptor computeDesc = {};
    computeDesc.compute.module = sm;

    wgpu::ComputePipeline pipeline;
    device.CreateComputePipelineAsync(
        &computeDesc, wgpu::CallbackMode::AllowSpontaneous,
        [&pipeline](wgpu::CreatePipelineAsyncStatus, wgpu::ComputePipeline result,
                    wgpu::StringView) { pipeline = std::move(result); });
    ASSERT_TRUE(c2sBuf->Flush());

    // Delete the server. It should force async work to complete.
    c2sBuf->SetHandler(nullptr);
    wireServer.reset();

    ASSERT_TRUE(s2cBuf->Flush());
    ASSERT_NE(pipeline, nullptr);

    pipeline = nullptr;
    sm = nullptr;
    device = nullptr;
    adapter = nullptr;
    instance = nullptr;

    s2cBuf->SetHandler(nullptr);
}

}  // anonymous namespace
}  // namespace dawn::wire
