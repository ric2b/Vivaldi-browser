// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_LIGHTS_RAZER_CHROMA_PLATFORM_DRIVER_WIN_H_
#define COMPONENTS_LIGHTS_RAZER_CHROMA_PLATFORM_DRIVER_WIN_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "thirdparty/ChromaSDK/inc/RzChromaSDKDefines.h"
#include "thirdparty/ChromaSDK/inc/RzChromaSDKTypes.h"
#include "thirdparty/ChromaSDK/inc/RzErrors.h"
#include "ui/lights/razer_chroma_handler.h"

#define MAX_EFFECTS 100

struct EFFECTDATATYPE {
  LONG num_effects;
  struct _EFFECT {
    RZEFFECTID id;
    LONG delay;
  } effect[MAX_EFFECTS];
};

struct GUIDCompare {
  bool operator()(const GUID& left, const GUID& right) const {
    return memcmp(&left, &right, sizeof(right)) < 0;
  }
};

class RazerChromaPlatformDriverWin : public RazerChromaPlatformDriver {
 public:
  explicit RazerChromaPlatformDriverWin(Profile* profile);
  ~RazerChromaPlatformDriverWin() override;

  // Initialize the platform layer, return false if Razer Chroma is
  // is not available or it could otherwise not initialize.
  bool Initialize() override;

  bool IsAvailable() override;
  bool IsReady() override;

  void SetColors(RazerChromaColors& colors) override;

  void Shutdown() override;

 private:
  enum RazerChromaDevice {
    CHROMA_DEVICE_KEYBOARD,
    CHROMA_DEVICE_MOUSE,
    CHROMA_DEVICE_MOUSEMAT,
    CHROMA_DEVICE_LINK,
    CHROMA_DEVICE_HEADSET,
  };
  void RunEffectsOnThread(std::vector<RZEFFECTID> effect_ids, int num_effects);
  void DeleteEffectImpl(RZEFFECTID effect_id);
  void CreateEffectGroup(RZEFFECTID* group_effect_Id);
  void AddToGroup(RZEFFECTID group_effect_id, RZEFFECTID effect_id, long delay);

  void GenerateDeviceListFromPrefs(std::vector<RazerChromaDevice>& device_list);

 private:
  HMODULE module_ = nullptr;

  // Thread to run the effects on.
  std::unique_ptr<base::Thread> thread_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::ThreadChecker thread_checker_;
  std::unique_ptr<base::CancelableTaskTracker> task_tracker_;

  std::map<RZEFFECTID, EFFECTDATATYPE, GUIDCompare> effects_;
  base::Lock lock_;
  PrefService* pref_service_;
};

#endif  // COMPONENTS_LIGHTS_RAZER_CHROMA_PLATFORM_DRIVER_WIN_H_
