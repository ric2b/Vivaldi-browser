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

#include "dawn/wire/client/Instance.h"

#include <memory>
#include <string>
#include <utility>

#include "dawn/common/Log.h"
#include "dawn/common/WGSLFeatureMapping.h"
#include "dawn/wire/client/ApiObjects_autogen.h"
#include "dawn/wire/client/Client.h"
#include "dawn/wire/client/EventManager.h"
#include "dawn/wire/client/webgpu.h"
#include "partition_alloc/pointers/raw_ptr.h"
#include "tint/lang/wgsl/features/language_feature.h"
#include "tint/lang/wgsl/features/status.h"

namespace dawn::wire::client {
namespace {

class RequestAdapterEvent : public TrackedEvent {
  public:
    static constexpr EventType kType = EventType::RequestAdapter;

    RequestAdapterEvent(const WGPURequestAdapterCallbackInfo& callbackInfo, Ref<Adapter> adapter)
        : TrackedEvent(callbackInfo.mode),
          mCallback(callbackInfo.callback),
          mUserdata1(callbackInfo.userdata),
          mAdapter(std::move(adapter)) {}

    RequestAdapterEvent(const WGPURequestAdapterCallbackInfo2& callbackInfo, Ref<Adapter> adapter)
        : TrackedEvent(callbackInfo.mode),
          mCallback2(callbackInfo.callback),
          mUserdata1(callbackInfo.userdata1),
          mUserdata2(callbackInfo.userdata2),
          mAdapter(std::move(adapter)) {}

    EventType GetType() override { return kType; }

    WireResult ReadyHook(FutureID futureID,
                         WGPURequestAdapterStatus status,
                         const char* message,
                         const WGPUAdapterInfo* info,
                         const WGPUSupportedLimits* limits,
                         uint32_t featuresCount,
                         const WGPUFeatureName* features) {
        DAWN_ASSERT(mAdapter != nullptr);
        mStatus = status;
        if (message != nullptr) {
            mMessage = message;
        }
        if (status == WGPURequestAdapterStatus_Success) {
            mAdapter->SetInfo(info);
            mAdapter->SetProperties(info);
            mAdapter->SetLimits(limits);
            mAdapter->SetFeatures(features, featuresCount);
        }
        return WireResult::Success;
    }

  private:
    void CompleteImpl(FutureID futureID, EventCompletionType completionType) override {
        if (mCallback == nullptr && mCallback2 == nullptr) {
            // If there's no callback, just clean up the resources.
            mUserdata1.ExtractAsDangling();
            mUserdata2.ExtractAsDangling();
            return;
        }

        if (completionType == EventCompletionType::Shutdown) {
            mStatus = WGPURequestAdapterStatus_InstanceDropped;
            mMessage = "A valid external Instance reference no longer exists.";
        }

        if (mCallback) {
            mCallback(mStatus,
                      mStatus == WGPURequestAdapterStatus_Success ? ReturnToAPI(std::move(mAdapter))
                                                                  : nullptr,
                      mMessage ? mMessage->c_str() : nullptr, mUserdata1.ExtractAsDangling());
        } else {
            mCallback2(mStatus,
                       mStatus == WGPURequestAdapterStatus_Success
                           ? ReturnToAPI(std::move(mAdapter))
                           : nullptr,
                       mMessage ? mMessage->c_str() : nullptr, mUserdata1.ExtractAsDangling(),
                       mUserdata2.ExtractAsDangling());
        }
    }

    WGPURequestAdapterCallback mCallback = nullptr;
    WGPURequestAdapterCallback2 mCallback2 = nullptr;
    raw_ptr<void> mUserdata1;
    raw_ptr<void> mUserdata2;

    // Note that the message is optional because we want to return nullptr when it wasn't set
    // instead of a pointer to an empty string.
    WGPURequestAdapterStatus mStatus;
    std::optional<std::string> mMessage;

    // The adapter is created when we call RequestAdapter(F). It is guaranteed to be alive
    // throughout the duration of a RequestAdapterEvent because the Event essentially takes
    // ownership of it until either an error occurs at which point the Event cleans it up, or it
    // returns the adapter to the user who then takes ownership as the Event goes away.
    Ref<Adapter> mAdapter;
};

WGPUWGSLFeatureName ToWGPUFeature(tint::wgsl::LanguageFeature f) {
    switch (f) {
#define CASE(WgslName, WgpuName)                \
    case tint::wgsl::LanguageFeature::WgslName: \
        return WGPUWGSLFeatureName_##WgpuName;
        DAWN_FOREACH_WGSL_FEATURE(CASE)
#undef CASE
        case tint::wgsl::LanguageFeature::kUndefined:
            DAWN_UNREACHABLE();
    }
}

}  // anonymous namespace

// Instance

Instance::Instance(const ObjectBaseParams& params)
    : RefCountedWithExternalCount<ObjectWithEventsBase>(params, params.handle) {}

void Instance::WillDropLastExternalRef() {
    if (IsRegistered()) {
        GetEventManager().TransitionTo(EventManager::State::InstanceDropped);
    }
    Unregister();
}

ObjectType Instance::GetObjectType() const {
    return ObjectType::Instance;
}

WireResult Instance::Initialize(const WGPUInstanceDescriptor* descriptor) {
    if (descriptor == nullptr) {
        return WireResult::Success;
    }

    if (descriptor->features.timedWaitAnyEnable) {
        dawn::ErrorLog() << "Wire client instance doesn't support timedWaitAnyEnable = true";
        return WireResult::FatalError;
    }
    if (descriptor->features.timedWaitAnyMaxCount > 0) {
        dawn::ErrorLog() << "Wire client instance doesn't support non-zero timedWaitAnyMaxCount";
        return WireResult::FatalError;
    }

    const WGPUDawnWireWGSLControl* wgslControl = nullptr;
    const WGPUDawnWGSLBlocklist* wgslBlocklist = nullptr;
    for (const WGPUChainedStruct* chain = descriptor->nextInChain; chain != nullptr;
         chain = chain->next) {
        switch (chain->sType) {
            case WGPUSType_DawnWireWGSLControl:
                wgslControl = reinterpret_cast<const WGPUDawnWireWGSLControl*>(chain);
                break;
            case WGPUSType_DawnWGSLBlocklist:
                wgslBlocklist = reinterpret_cast<const WGPUDawnWGSLBlocklist*>(chain);
                break;
            default:
                dawn::ErrorLog() << "Wire client instance doesn't support InstanceDescriptor "
                                    "extension structure with sType ("
                                 << chain->sType << ")";
                return WireResult::FatalError;
        }
    }

    GatherWGSLFeatures(wgslControl, wgslBlocklist);

    return WireResult::Success;
}

void Instance::RequestAdapter(const WGPURequestAdapterOptions* options,
                              WGPURequestAdapterCallback callback,
                              void* userdata) {
    WGPURequestAdapterCallbackInfo callbackInfo = {};
    callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    callbackInfo.callback = callback;
    callbackInfo.userdata = userdata;
    RequestAdapterF(options, callbackInfo);
}

WGPUFuture Instance::RequestAdapterF(const WGPURequestAdapterOptions* options,
                                     const WGPURequestAdapterCallbackInfo& callbackInfo) {
    Client* client = GetClient();
    Ref<Adapter> adapter = client->Make<Adapter>(GetEventManagerHandle());
    auto [futureIDInternal, tracked] =
        GetEventManager().TrackEvent(std::make_unique<RequestAdapterEvent>(callbackInfo, adapter));
    if (!tracked) {
        return {futureIDInternal};
    }

    InstanceRequestAdapterCmd cmd;
    cmd.instanceId = GetWireId();
    cmd.eventManagerHandle = GetEventManagerHandle();
    cmd.future = {futureIDInternal};
    cmd.adapterObjectHandle = adapter->GetWireHandle();
    cmd.options = options;
    cmd.userdataCount = 1;

    client->SerializeCommand(cmd);
    return {futureIDInternal};
}

WGPUFuture Instance::RequestAdapter2(const WGPURequestAdapterOptions* options,
                                     const WGPURequestAdapterCallbackInfo2& callbackInfo) {
    Client* client = GetClient();
    Ref<Adapter> adapter = client->Make<Adapter>(GetEventManagerHandle());
    auto [futureIDInternal, tracked] =
        GetEventManager().TrackEvent(std::make_unique<RequestAdapterEvent>(callbackInfo, adapter));
    if (!tracked) {
        return {futureIDInternal};
    }

    InstanceRequestAdapterCmd cmd;
    cmd.instanceId = GetWireId();
    cmd.eventManagerHandle = GetEventManagerHandle();
    cmd.future = {futureIDInternal};
    cmd.adapterObjectHandle = adapter->GetWireHandle();
    cmd.options = options;
    cmd.userdataCount = 2;

    client->SerializeCommand(cmd);
    return {futureIDInternal};
}

WireResult Client::DoInstanceRequestAdapterCallback(ObjectHandle eventManager,
                                                    WGPUFuture future,
                                                    WGPURequestAdapterStatus status,
                                                    const char* message,
                                                    const WGPUAdapterInfo* info,
                                                    const WGPUSupportedLimits* limits,
                                                    uint32_t featuresCount,
                                                    const WGPUFeatureName* features) {
    return GetEventManager(eventManager)
        .SetFutureReady<RequestAdapterEvent>(future.id, status, message, info, limits,
                                             featuresCount, features);
}

void Instance::ProcessEvents() {
    GetEventManager().ProcessPollEvents();
}

WGPUWaitStatus Instance::WaitAny(size_t count, WGPUFutureWaitInfo* infos, uint64_t timeoutNS) {
    return GetEventManager().WaitAny(count, infos, timeoutNS);
}

void Instance::GatherWGSLFeatures(const WGPUDawnWireWGSLControl* wgslControl,
                                  const WGPUDawnWGSLBlocklist* wgslBlocklist) {
    WGPUDawnWireWGSLControl defaultWgslControl{};
    if (wgslControl == nullptr) {
        wgslControl = &defaultWgslControl;
    }

    for (auto wgslFeature : tint::wgsl::kAllLanguageFeatures) {
        // Skip over testing features if we don't have the toggle to expose them.
        if (!wgslControl->enableTesting) {
            switch (wgslFeature) {
                case tint::wgsl::LanguageFeature::kChromiumTestingUnimplemented:
                case tint::wgsl::LanguageFeature::kChromiumTestingUnsafeExperimental:
                case tint::wgsl::LanguageFeature::kChromiumTestingExperimental:
                case tint::wgsl::LanguageFeature::kChromiumTestingShippedWithKillswitch:
                case tint::wgsl::LanguageFeature::kChromiumTestingShipped:
                    continue;
                default:
                    break;
            }
        }

        // Expose the feature depending on its status and wgslControl.
        bool enable = false;
        switch (tint::wgsl::GetLanguageFeatureStatus(wgslFeature)) {
            case tint::wgsl::FeatureStatus::kUnknown:
            case tint::wgsl::FeatureStatus::kUnimplemented:
                enable = false;
                break;

            case tint::wgsl::FeatureStatus::kUnsafeExperimental:
                enable = wgslControl->enableUnsafe;
                break;
            case tint::wgsl::FeatureStatus::kExperimental:
                enable = wgslControl->enableExperimental;
                break;

            case tint::wgsl::FeatureStatus::kShippedWithKillswitch:
            case tint::wgsl::FeatureStatus::kShipped:
                enable = true;
                break;
        }

        if (enable && wgslFeature != tint::wgsl::LanguageFeature::kUndefined) {
            mWGSLFeatures.emplace(ToWGPUFeature(wgslFeature));
        }
    }

    // Remove blocklisted features.
    if (wgslBlocklist != nullptr) {
        for (size_t i = 0; i < wgslBlocklist->blocklistedFeatureCount; i++) {
            const char* name = wgslBlocklist->blocklistedFeatures[i];
            tint::wgsl::LanguageFeature tintFeature = tint::wgsl::ParseLanguageFeature(name);
            if (tintFeature == tint::wgsl::LanguageFeature::kUndefined) {
                // Ignore unknown features in the blocklist.
                continue;
            }
            mWGSLFeatures.erase(ToWGPUFeature(tintFeature));
        }
    }
}

bool Instance::HasWGSLLanguageFeature(WGPUWGSLFeatureName feature) const {
    return mWGSLFeatures.contains(feature);
}

size_t Instance::EnumerateWGSLLanguageFeatures(WGPUWGSLFeatureName* features) const {
    if (features != nullptr) {
        for (WGPUWGSLFeatureName f : mWGSLFeatures) {
            *features = f;
            ++features;
        }
    }
    return mWGSLFeatures.size();
}

WGPUSurface Instance::CreateSurface(const WGPUSurfaceDescriptor* desc) const {
    dawn::ErrorLog() << "Instance::CreateSurface is not supported in the wire. Use "
                        "dawn::wire::client::WireClient::InjectSurface instead.";
    return nullptr;
}

}  // namespace dawn::wire::client

// Free-standing API functions

DAWN_WIRE_EXPORT WGPUStatus wgpuDawnWireClientGetInstanceFeatures(WGPUInstanceFeatures* features) {
    if (features->nextInChain != nullptr) {
        return WGPUStatus_Error;
    }

    features->timedWaitAnyEnable = false;
    features->timedWaitAnyMaxCount = dawn::kTimedWaitAnyMaxCountDefault;
    return WGPUStatus_Success;
}

DAWN_WIRE_EXPORT WGPUInstance
wgpuDawnWireClientCreateInstance(WGPUInstanceDescriptor const* descriptor) {
    DAWN_UNREACHABLE();
    return nullptr;
}
