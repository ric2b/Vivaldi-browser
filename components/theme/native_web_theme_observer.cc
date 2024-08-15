// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "components/theme/native_web_theme_observer.h"

#include "base/lazy_instance.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/flags_ui/flags_storage.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "ui/native_theme/native_theme.h"

#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

NativeWebThemeObserver::NativeWebThemeObserver(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  ::about_flags::GetStorage(
      profile_,
      base::BindOnce(&NativeWebThemeObserver::OnAboutFlagsStorageRecieved,
                     base::Unretained(this)));

  auto* nativeTheme = ui::NativeTheme::GetInstanceForWeb();
  DCHECK(nativeTheme);
  native_theme_observation_.Observe(nativeTheme);
  auto* local_state = profile_->GetPrefs();
  DCHECK(local_state);
  pref_change_registrar.Init(local_state);
  pref_change_registrar.Add(
      prefs::kBrowserColorScheme,
      base::BindRepeating(&NativeWebThemeObserver::OnNativeThemeUpdated,
                          base::Unretained(this),
                          ui::NativeTheme::GetInstanceForWeb()));
  pref_change_registrar.Add(
      vivaldiprefs::kAppearanceForceDarkModeTheme,
      base::BindRepeating(&NativeWebThemeObserver::OnForceDarkThemeChange,
                          base::Unretained(this)));

  OnNativeThemeUpdated(nativeTheme);
}

NativeWebThemeObserver::~NativeWebThemeObserver() = default;

void NativeWebThemeObserver::Shutdown() {}

void NativeWebThemeObserver::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  OnPreferedColorSchemeChange(observed_theme);
  OnForceDarkThemeChange();
}

void NativeWebThemeObserver::OnPreferedColorSchemeChange(
    ui::NativeTheme* observed_theme) {
  const auto colorScheme =
      profile_->GetPrefs()->GetInteger(prefs::kBrowserColorScheme);

  ui::NativeTheme::PreferredColorScheme newScheme;
  switch (colorScheme) {
    case 0:
      newScheme = observed_theme->ShouldUseDarkColors()
                      ? ui::NativeTheme::PreferredColorScheme::kDark
                      : ui::NativeTheme::PreferredColorScheme::kLight;
      break;
    case 1:
      newScheme = ui::NativeTheme::PreferredColorScheme::kLight;
      break;
    case 2:
      newScheme = ui::NativeTheme::PreferredColorScheme::kDark;
      break;
    default:
      return;
  }

  if (newScheme != observed_theme->GetPreferredColorScheme()) {
    observed_theme->set_preferred_color_scheme(newScheme);
    observed_theme->NotifyOnNativeThemeUpdated();
  }
}

void NativeWebThemeObserver::OnForceDarkThemeChange() {
  auto* prefs = profile_->GetPrefs();
  if (flags_storage_ && prefs) {
    const auto shouldForceDarkTheme =
        prefs->GetBoolean(vivaldiprefs::kAppearanceForceDarkModeTheme) &&
        prefs->GetInteger(prefs::kBrowserColorScheme) == 2;

    ::about_flags::SetFeatureEntryEnabled(
        flags_storage_.get(),
        // "Enabled with selective inversion of non-image elements" or
        // "Default"
        shouldForceDarkTheme ? "enable-force-dark@6" : "enable-force-dark@0",
        true);

    prefs->SetBoolean(prefs::kWebKitForceDarkModeEnabled, shouldForceDarkTheme);
  }
}

void NativeWebThemeObserver::OnAboutFlagsStorageRecieved(
    std::unique_ptr<flags_ui::FlagsStorage> storage,
    flags_ui::FlagAccess) {
  flags_storage_ = std::move(storage);
}

static base::LazyInstance<extensions::BrowserContextKeyedAPIFactory<
    NativeWebThemeObserver>>::DestructorAtExit g_crashreport_observer_factory =
    LAZY_INSTANCE_INITIALIZER;

extensions::BrowserContextKeyedAPIFactory<NativeWebThemeObserver>*
NativeWebThemeObserver::GetFactoryInstance() {
  return g_crashreport_observer_factory.Pointer();
}

}  // namespace vivaldi