// Copyright (c) 2015-2019 Vivaldi Technologies, All Rights Reserved.

#include "prefs/vivaldi_prefs_definitions.h"

#include <Shobjidl.h>
#include <wrl/client.h>

#include "components/prefs/pref_service.h"

#include "prefs/native_settings_helper_win.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

base::Value VivaldiPrefsDefinitions::GetPlatformComputedDefault(
    const std::string& path) {
  if (path == vivaldiprefs::kSystemHasDesktopWallpaperProtocol) {
    Microsoft::WRL::ComPtr<IDesktopWallpaper> desktop_w;
    HRESULT hr = CoCreateInstance(__uuidof(DesktopWallpaper), nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&desktop_w));
    return base::Value(SUCCEEDED(hr));
  } else if (path == vivaldiprefs::kSystemAccentColor) {
    return base::Value(GetSystemAccentColor());
  } else if (path == vivaldiprefs::kSystemHighlightColor) {
    return base::Value(GetSystemHighlightColor());
  }
  return base::Value();
}

}  // namespace vivaldi
