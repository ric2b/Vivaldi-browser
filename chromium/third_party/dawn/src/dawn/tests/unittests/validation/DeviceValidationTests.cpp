// Copyright 2021 The Dawn & Tint Authors
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

#include <utility>

#include "dawn/native/Device.h"
#include "dawn/native/dawn_platform.h"
#include "dawn/tests/MockCallback.h"
#include "dawn/tests/unittests/validation/ValidationTest.h"

namespace dawn {
namespace {

using testing::IsNull;
using testing::MockCppCallback;
using testing::NotNull;
using testing::WithArgs;

class RequestDeviceValidationTest : public ValidationTest {
  protected:
    void SetUp() override {
        ValidationTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());
    }

    MockCppCallback<void (*)(wgpu::RequestDeviceStatus, wgpu::Device, const char*)>
        mRequestDeviceCallback;
};

// Test that requesting a device without specifying limits is valid.
TEST_F(RequestDeviceValidationTest, NoRequiredLimits) {
    wgpu::DeviceDescriptor descriptor;

    EXPECT_CALL(mRequestDeviceCallback,
                Call(wgpu::RequestDeviceStatus::Success, NotNull(), IsNull()))
        .WillOnce(WithArgs<1>([](wgpu::Device device) {
            // Check one of the default limits.
            wgpu::SupportedLimits limits;
            device.GetLimits(&limits);
            EXPECT_EQ(limits.limits.maxBindGroups, 4u);
        }));
    adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                          mRequestDeviceCallback.Callback());
}

// Test that requesting a device with the default limits is valid.
TEST_F(RequestDeviceValidationTest, DefaultLimits) {
    wgpu::RequiredLimits limits = {};
    wgpu::DeviceDescriptor descriptor;
    descriptor.requiredLimits = &limits;

    EXPECT_CALL(mRequestDeviceCallback,
                Call(wgpu::RequestDeviceStatus::Success, NotNull(), IsNull()))
        .WillOnce(WithArgs<1>([](wgpu::Device device) {
            // Check one of the default limits.
            wgpu::SupportedLimits limits;
            device.GetLimits(&limits);
            EXPECT_EQ(limits.limits.maxTextureArrayLayers, 256u);
        }));
    adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                          mRequestDeviceCallback.Callback());
}

// Test that requesting a device where a required limit is above the maximum value.
TEST_F(RequestDeviceValidationTest, HigherIsBetter) {
    wgpu::RequiredLimits limits = {};
    wgpu::DeviceDescriptor descriptor;
    descriptor.requiredLimits = &limits;

    wgpu::SupportedLimits supportedLimits;
    EXPECT_EQ(adapter.GetLimits(&supportedLimits), wgpu::Status::Success);

    // If we can support better than the default, test below the max.
    if (supportedLimits.limits.maxBindGroups > 4u) {
        limits.limits.maxBindGroups = supportedLimits.limits.maxBindGroups - 1;
        EXPECT_CALL(mRequestDeviceCallback,
                    Call(wgpu::RequestDeviceStatus::Success, NotNull(), IsNull()))
            .WillOnce(WithArgs<1>([&](wgpu::Device device) {
                wgpu::SupportedLimits limits;
                device.GetLimits(&limits);

                // Check we got exactly the request.
                EXPECT_EQ(limits.limits.maxBindGroups, supportedLimits.limits.maxBindGroups - 1);
                // Check another default limit.
                EXPECT_EQ(limits.limits.maxTextureArrayLayers, 256u);
            }));
        adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                              mRequestDeviceCallback.Callback());
    }

    // Test the max.
    limits.limits.maxBindGroups = supportedLimits.limits.maxBindGroups;
    EXPECT_CALL(mRequestDeviceCallback,
                Call(wgpu::RequestDeviceStatus::Success, NotNull(), IsNull()))
        .WillOnce(WithArgs<1>([&](wgpu::Device device) {
            wgpu::SupportedLimits limits;
            device.GetLimits(&limits);

            // Check we got exactly the request.
            EXPECT_EQ(limits.limits.maxBindGroups, supportedLimits.limits.maxBindGroups);
            // Check another default limit.
            EXPECT_EQ(limits.limits.maxTextureArrayLayers, 256u);
        }));
    adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                          mRequestDeviceCallback.Callback());

    // Test above the max.
    limits.limits.maxBindGroups = supportedLimits.limits.maxBindGroups + 1;
    EXPECT_CALL(mRequestDeviceCallback, Call(wgpu::RequestDeviceStatus::Error, IsNull(), NotNull()))
        .Times(1);
    adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                          mRequestDeviceCallback.Callback());

    // Test worse than the default
    limits.limits.maxBindGroups = 3u;
    EXPECT_CALL(mRequestDeviceCallback,
                Call(wgpu::RequestDeviceStatus::Success, NotNull(), IsNull()))
        .WillOnce(WithArgs<1>([&](wgpu::Device device) {
            wgpu::SupportedLimits limits;
            device.GetLimits(&limits);

            // Check we got the default.
            EXPECT_EQ(limits.limits.maxBindGroups, 4u);
        }));
    adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                          mRequestDeviceCallback.Callback());
}

// Test that requesting a device where a required limit is below the minimum value.
TEST_F(RequestDeviceValidationTest, LowerIsBetter) {
    wgpu::RequiredLimits limits = {};
    wgpu::DeviceDescriptor descriptor;
    descriptor.requiredLimits = &limits;

    wgpu::SupportedLimits supportedLimits;
    EXPECT_EQ(adapter.GetLimits(&supportedLimits), wgpu::Status::Success);

    // Test below the min.
    limits.limits.minUniformBufferOffsetAlignment =
        supportedLimits.limits.minUniformBufferOffsetAlignment / 2;
    EXPECT_CALL(mRequestDeviceCallback, Call(wgpu::RequestDeviceStatus::Error, IsNull(), NotNull()))
        .Times(1);
    adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                          mRequestDeviceCallback.Callback());

    // Test the min.
    limits.limits.minUniformBufferOffsetAlignment =
        supportedLimits.limits.minUniformBufferOffsetAlignment;
    EXPECT_CALL(mRequestDeviceCallback,
                Call(wgpu::RequestDeviceStatus::Success, NotNull(), IsNull()))
        .WillOnce(WithArgs<1>([&](wgpu::Device device) {
            wgpu::SupportedLimits limits;
            device.GetLimits(&limits);

            // Check we got exactly the request.
            EXPECT_EQ(limits.limits.minUniformBufferOffsetAlignment,
                      supportedLimits.limits.minUniformBufferOffsetAlignment);
            // Check another default limit.
            EXPECT_EQ(limits.limits.maxTextureArrayLayers, 256u);
        }));
    adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                          mRequestDeviceCallback.Callback());

    // IF we can support better than the default, test above the min.
    if (supportedLimits.limits.minUniformBufferOffsetAlignment > 256u) {
        limits.limits.minUniformBufferOffsetAlignment =
            supportedLimits.limits.minUniformBufferOffsetAlignment * 2;
        EXPECT_CALL(mRequestDeviceCallback,
                    Call(wgpu::RequestDeviceStatus::Success, NotNull(), IsNull()))
            .WillOnce(WithArgs<1>([&](wgpu::Device device) {
                wgpu::SupportedLimits limits;
                device.GetLimits(&limits);

                // Check we got exactly the request.
                EXPECT_EQ(limits.limits.minUniformBufferOffsetAlignment,
                          supportedLimits.limits.minUniformBufferOffsetAlignment * 2);
                // Check another default limit.
                EXPECT_EQ(limits.limits.maxTextureArrayLayers, 256u);
            }));
        adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                              mRequestDeviceCallback.Callback());
    }

    // Test worse than the default
    limits.limits.minUniformBufferOffsetAlignment = 2u * 256u;
    EXPECT_CALL(mRequestDeviceCallback,
                Call(wgpu::RequestDeviceStatus::Success, NotNull(), IsNull()))
        .WillOnce(WithArgs<1>([&](wgpu::Device device) {
            wgpu::SupportedLimits limits;
            device.GetLimits(&limits);

            // Check we got the default.
            EXPECT_EQ(limits.limits.minUniformBufferOffsetAlignment, 256u);
        }));
    adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                          mRequestDeviceCallback.Callback());
}

// Test that it is an error to request limits with an invalid chained struct
TEST_F(RequestDeviceValidationTest, InvalidChainedStruct) {
    wgpu::PrimitiveDepthClipControl depthClipControl = {};
    wgpu::RequiredLimits limits = {};
    limits.nextInChain = &depthClipControl;

    wgpu::DeviceDescriptor descriptor;
    descriptor.requiredLimits = &limits;
    EXPECT_CALL(mRequestDeviceCallback, Call(wgpu::RequestDeviceStatus::Error, IsNull(), NotNull()))
        .Times(1);
    adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                          mRequestDeviceCallback.Callback());
}

class DeviceTickValidationTest : public ValidationTest {};

// Device destroy before API-level Tick should always result in no-op and false.
TEST_F(DeviceTickValidationTest, DestroyDeviceBeforeAPITick) {
    ExpectDeviceDestruction();
    device.Destroy();
    device.Tick();
}

class DeviceGetAHardwareBufferPropertiesValidationTest : public ValidationTest {
    void SetUp() override {
        ValidationTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());
    }
};

// Test that calling GetAHardwareBufferProperties will generate an error
// if the required feature is not present.
TEST_F(DeviceGetAHardwareBufferPropertiesValidationTest,
       GetAHardwareBufferPropertiesRequiresAHBFeature) {
    // The parameter values shouldn't matter, as the call should fail validation
    // before calling into the implementation (verified by checking the error
    // message).
    void* handle = nullptr;
    wgpu::AHardwareBufferProperties* properties = nullptr;

    ASSERT_DEVICE_ERROR(
        device.GetAHardwareBufferProperties(handle, properties),
        testing::HasSubstr(
            "without the FeatureName::SharedTextureMemoryAHardwareBuffer feature being set"));
}

}  // anonymous namespace
}  // namespace dawn
