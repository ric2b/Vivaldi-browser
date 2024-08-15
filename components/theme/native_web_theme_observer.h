// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef NATIVE_WEB_THEME_OBSERVER_H_
#define NATIVE_WEB_THEME_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/flags_ui/flags_state.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "ui/native_theme/native_theme_observer.h"

class Profile;
namespace content {
class BrowserContext;
}
namespace ui {
class NativeTheme;
}
namespace flags_ui {
class FlagsStorage;
}  // namespace flags_ui

namespace vivaldi {

class NativeWebThemeObserver : public extensions::BrowserContextKeyedAPI,
                               public ui::NativeThemeObserver {
 public:
  enum PreferredColorScheme {
    Auto = 0,
    Light,
    Dark,
  };

  explicit NativeWebThemeObserver(content::BrowserContext* context);

  NativeWebThemeObserver(const NativeWebThemeObserver&) = delete;
  NativeWebThemeObserver& operator=(const NativeWebThemeObserver&) = delete;

  ~NativeWebThemeObserver() override;

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;
  static extensions::BrowserContextKeyedAPIFactory<NativeWebThemeObserver>*
  GetFactoryInstance();

  // ui::NativeNativeWebThemeObserver implementation.
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

 private:
  void OnPreferredColorSchemeChange(ui::NativeTheme* observed_theme);
  void OnForceDarkThemeChange();
  void OnAboutFlagsStorageRecieved(
      std::unique_ptr<flags_ui::FlagsStorage> storage,
      flags_ui::FlagAccess access);

  friend class extensions::BrowserContextKeyedAPIFactory<
      NativeWebThemeObserver>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "NativeWebThemeObserver"; }
  static const bool kServiceIsNULLWhileTesting = true;

  raw_ptr<Profile> profile_;
  PrefChangeRegistrar local_state_change_registrar;
  PrefChangeRegistrar pref_change_registrar;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage_;
};

}  // namespace vivaldi

#endif  // NATIVE_WEB_THEME_OBSERVER_H_