// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include <Windows.h>

#include <cguid.h>
#include <objbase.h>
#include <tchar.h>

#include "ui/lights/razer_chroma_platform_driver_win.h"

#include "base/synchronization/lock.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_gen_prefs.h"

#ifdef _WIN64
#define CHROMASDKDLL _T("RzChromaSDK64.dll")
#else
#define CHROMASDKDLL _T("RzChromaSDK.dll")
#endif

static const char* kRazerChromaThreadName = "Vivaldi_RazerChromaThread";
static const int kEffectFrameDelay = 66;  // milliseconds

typedef RZRESULT (*INIT)(void);
typedef RZRESULT (*UNINIT)(void);
typedef RZRESULT (*CREATEEFFECT)(RZDEVICEID DeviceId,
                                 ChromaSDK::EFFECT_TYPE Effect,
                                 PRZPARAM pParam,
                                 RZEFFECTID* pEffectId);
typedef RZRESULT (*CREATEKEYBOARDEFFECT)(
    ChromaSDK::Keyboard::EFFECT_TYPE Effect,
    PRZPARAM pParam,
    RZEFFECTID* pEffectId);
typedef RZRESULT (*CREATEHEADSETEFFECT)(ChromaSDK::Headset::EFFECT_TYPE Effect,
                                        PRZPARAM pParam,
                                        RZEFFECTID* pEffectId);
typedef RZRESULT (*CREATEMOUSEPADEFFECT)(
    ChromaSDK::Mousepad::EFFECT_TYPE Effect,
    PRZPARAM pParam,
    RZEFFECTID* pEffectId);
typedef RZRESULT (*CREATEMOUSEEFFECT)(ChromaSDK::Mouse::EFFECT_TYPE Effect,
                                      PRZPARAM pParam,
                                      RZEFFECTID* pEffectId);
typedef RZRESULT (*CREATEKEYPADEFFECT)(ChromaSDK::Keypad::EFFECT_TYPE Effect,
                                       PRZPARAM pParam,
                                       RZEFFECTID* pEffectId);
typedef RZRESULT (*CREATECHROMALINKEFFECT)(
    ChromaSDK::ChromaLink::EFFECT_TYPE Effect,
    PRZPARAM pParam,
    RZEFFECTID* pEffectId);
typedef RZRESULT (*SETEFFECT)(RZEFFECTID EffectId);
typedef RZRESULT (*DELETEEFFECT)(RZEFFECTID EffectId);
typedef RZRESULT (*REGISTEREVENTNOTIFICATION)(HWND hWnd);
typedef RZRESULT (*UNREGISTEREVENTNOTIFICATION)(void);
typedef RZRESULT (*QUERYDEVICE)(RZDEVICEID DeviceId,
                                ChromaSDK::DEVICE_INFO_TYPE& DeviceInfo);

namespace {

CREATEEFFECT pCreateEffect;
CREATEKEYBOARDEFFECT pCreateKeyboardEffect;
CREATEMOUSEEFFECT pCreateMouseEffect;
CREATEHEADSETEFFECT pCreateHeadsetEffect;
CREATEMOUSEPADEFFECT pCreateMousematEffect;
CREATEKEYPADEFFECT pCreateKeypadEffect;
CREATECHROMALINKEFFECT pCreateChromaLinkEffect;
SETEFFECT pSetEffect;
DELETEEFFECT pDeleteEffect;

}  // namespace

// static
RazerChromaPlatformDriver*
RazerChromaPlatformDriver::CreateRazerChromaPlatformDriver(Profile* profile) {
  return new RazerChromaPlatformDriverWin(profile);
}

RazerChromaPlatformDriverWin::RazerChromaPlatformDriverWin(Profile* profile)
    : RazerChromaPlatformDriver(profile), pref_service_(profile->GetPrefs()) {}

RazerChromaPlatformDriverWin::~RazerChromaPlatformDriverWin() {}

bool RazerChromaPlatformDriverWin::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (module_ == nullptr) {
    // Note the Razer system dll is by default located in the system32
    // directory. Only try to use the one there. VB-109515.
    module_ = ::LoadLibraryEx(CHROMASDKDLL, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (module_ != NULL) {
      INIT Init = (INIT)::GetProcAddress(module_, "Init");
      if (Init != NULL) {
        RZRESULT result = Init();
        if (result == RZRESULT_SUCCESS) {
          pCreateEffect =
              (CREATEEFFECT)::GetProcAddress(module_, "CreateEffect");
          pCreateKeyboardEffect = (CREATEKEYBOARDEFFECT)::GetProcAddress(
              module_, "CreateKeyboardEffect");
          pCreateMouseEffect =
              (CREATEMOUSEEFFECT)::GetProcAddress(module_, "CreateMouseEffect");
          pCreateMousematEffect = (CREATEMOUSEPADEFFECT)::GetProcAddress(
              module_, "CreateMousepadEffect");
          pCreateKeypadEffect = (CREATEKEYPADEFFECT)::GetProcAddress(
              module_, "CreateKeypadEffect");
          pCreateHeadsetEffect = (CREATEHEADSETEFFECT)::GetProcAddress(
              module_, "CreateHeadsetEffect");
          pCreateChromaLinkEffect = (CREATECHROMALINKEFFECT)::GetProcAddress(
              module_, "CreateChromaLinkEffect");
          pSetEffect = (SETEFFECT)GetProcAddress(module_, "SetEffect");
          pDeleteEffect = (DELETEEFFECT)GetProcAddress(module_, "DeleteEffect");

          thread_.reset(new base::Thread(kRazerChromaThreadName));

          base::Thread::Options options;
          if (!thread_->StartWithOptions(std::move(options))) {
            Shutdown();
            return false;
          }
          task_runner_ = thread_->task_runner();
          task_tracker_.reset(new base::CancelableTaskTracker());
          return true;
        }
      }
    }
  }
  return false;
}

void RazerChromaPlatformDriverWin::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());

  {
    base::AutoLock lock(lock_);

    while (!effects_.empty()) {
      auto iterator = effects_.begin();
      for (int i = 0; i < iterator->second.num_effects; i++) {
        DeleteEffectImpl(iterator->second.effect[i].id);
      }
      effects_.erase(iterator);
    }
  }

  if (module_) {
    UNINIT pUnInit = (UNINIT)::GetProcAddress(module_, "UnInit");
    if (pUnInit) {
      /* RZRESULT result = */ pUnInit();
    }
    ::FreeLibrary(module_);
    module_ = NULL;
  }
  task_tracker_.reset();
  task_runner_ = nullptr;
  thread_.reset();
}

bool RazerChromaPlatformDriverWin::IsAvailable() {
  if (module_) {
    // If we already have it open, return immediately.
    return true;
  }
  HMODULE library;
  library = ::LoadLibraryEx(CHROMASDKDLL, NULL, LOAD_LIBRARY_AS_DATAFILE);
  if (library != NULL) {
    ::FreeLibrary(library);
    return true;
  }
  return false;
}

bool RazerChromaPlatformDriverWin::IsReady() {
  return module_ != nullptr;
}

void RazerChromaPlatformDriverWin::GenerateDeviceListFromPrefs(
    std::vector<RazerChromaDevice>& device_list) {
  auto& devices =
      pref_service_->GetList(vivaldiprefs::kRazerChromaDevices);
  for (auto& it: devices) {
    const std::string *device = it.GetIfString();
    if (device) {
      if (*device == "keyboard") {
        device_list.push_back(RazerChromaDevice::CHROMA_DEVICE_KEYBOARD);
      } else if (*device == "chromalink") {
        device_list.push_back(RazerChromaDevice::CHROMA_DEVICE_LINK);
      } else if (*device == "mouse") {
        device_list.push_back(RazerChromaDevice::CHROMA_DEVICE_MOUSE);
      } else if (*device == "mousemat") {
        device_list.push_back(RazerChromaDevice::CHROMA_DEVICE_MOUSEMAT);
      } else if (*device == "headset") {
        device_list.push_back(RazerChromaDevice::CHROMA_DEVICE_HEADSET);
      }
    }
  }
}

void RazerChromaPlatformDriverWin::SetColors(RazerChromaColors& colors) {
  DCHECK(thread_checker_.CalledOnValidThread());

  RZEFFECTID effect_id = GUID_NULL;
  std::vector<RZEFFECTID> effect_ids;
  std::vector<RazerChromaDevice> device_list;

  GenerateDeviceListFromPrefs(device_list);

  for (auto device : device_list) {
    CreateEffectGroup(&effect_id);

    for (size_t i = 0; i < colors.size(); i++) {
      RZEFFECTID frames[] = {};
      SkColor skc = colors[i];
      COLORREF color =
          RGB(SkColorGetR(skc), SkColorGetG(skc), SkColorGetB(skc));

      if (device == RazerChromaDevice::CHROMA_DEVICE_LINK) {
        ChromaSDK::ChromaLink::STATIC_EFFECT_TYPE effect = {};
        effect.Color = color;
        pCreateChromaLinkEffect(ChromaSDK::ChromaLink::CHROMA_STATIC, &effect,
                                &frames[0]);
      } else if (device == RazerChromaDevice::CHROMA_DEVICE_KEYBOARD) {
        ChromaSDK::Keyboard::STATIC_EFFECT_TYPE effect = {};
        effect.Color = color;
        pCreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_STATIC, &effect,
                              &frames[0]);
      } else if (device == RazerChromaDevice::CHROMA_DEVICE_MOUSE) {
        ChromaSDK::Mouse::STATIC_EFFECT_TYPE effect = {};
        effect.Color = color;
        effect.LEDId = ChromaSDK::Mouse::RZLED_ALL;
        pCreateMouseEffect(ChromaSDK::Mouse::CHROMA_STATIC, &effect,
                           &frames[0]);
      } else if (device == RazerChromaDevice::CHROMA_DEVICE_MOUSEMAT) {
        ChromaSDK::Mousepad::STATIC_EFFECT_TYPE effect = {};
        effect.Color = color;
        pCreateMousematEffect(ChromaSDK::Mousepad::CHROMA_STATIC, &effect,
                              &frames[0]);
      } else if (device == RazerChromaDevice::CHROMA_DEVICE_HEADSET) {
        ChromaSDK::Headset::STATIC_EFFECT_TYPE effect = {};
        effect.Color = color;
        pCreateHeadsetEffect(ChromaSDK::Headset::CHROMA_STATIC, &effect,
                             &frames[0]);
      }
      AddToGroup(effect_id, frames[0], kEffectFrameDelay);
    }
    effect_ids.push_back(effect_id);
  }
  task_tracker_->PostTask(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&RazerChromaPlatformDriverWin::RunEffectsOnThread,
                     base::Unretained(this), std::move(effect_ids),
                     colors.size()));
}

void RazerChromaPlatformDriverWin::AddToGroup(RZEFFECTID group_effect_id,
                                              RZEFFECTID effect_id,
                                              long delay) {
  base::AutoLock lock(lock_);

  auto iterator = effects_.find(group_effect_id);
  if (iterator != effects_.end()) {
    LONG index = iterator->second.num_effects;

    iterator->second.effect[index].id = effect_id;
    iterator->second.effect[index].delay = delay;

    iterator->second.num_effects++;
  }
}

// We will run one effect per device per loop to keep them synchronized.
void RazerChromaPlatformDriverWin::RunEffectsOnThread(
    std::vector<RZEFFECTID> effect_ids,
    int num_effects) {
  base::AutoLock lock(lock_);
  int delay = 0;

  for (int effect_offset = 0; effect_offset < num_effects; effect_offset++) {
    for (auto& effect_id : effect_ids) {
      auto iter = effects_.find(effect_id);
      DCHECK(iter != effects_.end());
      if (iter != effects_.end()) {
        EFFECTDATATYPE* effect_data = &iter->second;

        pSetEffect(effect_data->effect[effect_offset].id);

        delay = effect_data->effect[effect_offset].delay;
      }
    }
    // This assume all devices have the same delay.
    Sleep(delay);
  }
  for (auto& effect_id : effect_ids) {
    DeleteEffectImpl(effect_id);
  }
}

void RazerChromaPlatformDriverWin::DeleteEffectImpl(RZEFFECTID effect_id) {
  if (pDeleteEffect == nullptr) {
    NOTREACHED();
    //return;
  }
  auto iterator = effects_.find(effect_id);
  if (iterator != effects_.end()) {
    EFFECTDATATYPE effect_data = iterator->second;
    for (int i = 0; i < effect_data.num_effects; i++) {
      pDeleteEffect(effect_data.effect[i].id);
    }
    effects_.erase(iterator);
  } else {
    pDeleteEffect(effect_id);
  }
}

void RazerChromaPlatformDriverWin::CreateEffectGroup(
    RZEFFECTID* group_effect_id) {
  RZEFFECTID effect_id = GUID_NULL;
  if (SUCCEEDED(::CoCreateGuid(&effect_id))) {
    EFFECTDATATYPE effect_data = {};

    effect_data.num_effects = 0;

    effects_.insert(std::make_pair(effect_id, effect_data));

    *group_effect_id = effect_id;
  }
}
