// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "ui/lights/razer_chroma_handler.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

#include "extensions/schema/vivaldi_utilities.h"
#include "extensions/tools/vivaldi_tools.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if !BUILDFLAG(IS_WIN)
// static
RazerChromaPlatformDriver*
RazerChromaPlatformDriver::CreateRazerChromaPlatformDriver(Profile* profile) {
  // Only Windows has a Chroma SDK.
  return nullptr;
}
#endif  // !IS_WIN

RazerChromaHandler::RazerChromaHandler(Profile* profile) : profile_(profile) {
  platform_driver_.reset(
      RazerChromaPlatformDriver::CreateRazerChromaPlatformDriver(profile));

  prefs_registrar_.Init(profile->GetPrefs());
  prefs_registrar_.Add(vivaldiprefs::kRazerChromaEnabled,
                       base::BindRepeating(&RazerChromaHandler::OnPrefChanged,
                                           base::Unretained(this)));

  // Set initial value.
  OnPrefChanged(vivaldiprefs::kRazerChromaEnabled);
}

RazerChromaHandler::~RazerChromaHandler() {}

bool RazerChromaHandler::Initialize() {
  if (initialized_ || !IsEnabled()) {
    NOTREACHED();
    //return false;
  }
  if (platform_driver_) {
    initialized_ = platform_driver_->Initialize();
  }
  return initialized_;
}

void RazerChromaHandler::Shutdown() {
  if (platform_driver_) {
    platform_driver_->Shutdown();
  }
  initialized_ = false;
}

bool RazerChromaHandler::IsAvailable() {
  return platform_driver_ && platform_driver_->IsAvailable();
}

bool RazerChromaHandler::IsReady() {
  return platform_driver_ && platform_driver_->IsReady();
}

bool RazerChromaHandler::IsEnabled() {
  return profile_->GetPrefs()->GetBoolean(vivaldiprefs::kRazerChromaEnabled);
}

void RazerChromaHandler::SetColors(RazerChromaColors& colors) {
  if (!initialized_ || !IsEnabled()) {
    // Silently ignore to avoid complexity in the theme code.
    return;
  }
  DCHECK(platform_driver_);

  platform_driver_->SetColors(colors);
}

void RazerChromaHandler::OnPrefChanged(const std::string& path) {
  DCHECK(path == vivaldiprefs::kRazerChromaEnabled);

  if (IsEnabled()) {
    initialized_ = Initialize();
  } else {
    Shutdown();
  }
  if (initialized_) {
    ::vivaldi::BroadcastEvent(
        extensions::vivaldi::utilities::OnRazerChromaReady::kEventName,
        extensions::vivaldi::utilities::OnRazerChromaReady::Create(), profile_);
  }
}
