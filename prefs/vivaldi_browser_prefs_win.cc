// Copyright 2015 Vivaldi Technologies

#include "prefs/vivaldi_browser_prefs.h"

#include <Shobjidl.h>
#include "base/win/scoped_comptr.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "prefs/vivaldi_pref_names.h"

namespace vivaldi {

void RegisterPlatformPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      vivaldiprefs::kSmoothScrollingEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(
      vivaldiprefs::kHideMouseCursorInFullscreen, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  base::win::ScopedComPtr<IDesktopWallpaper> desktop_w;
  HRESULT hr = CoCreateInstance(__uuidof(DesktopWallpaper), nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&desktop_w));

  // If the interface can be instanciated we support desktop wallpapers.
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiHasDesktopWallpaperProtocol, SUCCEEDED(hr));
}

}  // namespace vivaldi
