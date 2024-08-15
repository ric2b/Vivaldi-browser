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

#include "prefs/vivaldi_pref_names.h"
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

  auto* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  local_state_change_registrar.Init(local_state);
  local_state_change_registrar.Add(
      vivaldiprefs::kVivaldiPreferredColorScheme,
      base::BindRepeating(&NativeWebThemeObserver::OnNativeThemeUpdated,
                          base::Unretained(this),
                          ui::NativeTheme::GetInstanceForWeb()));

  auto* pref_state = profile_->GetPrefs();
  DCHECK(pref_state);
  pref_change_registrar.Init(pref_state);
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
  OnPreferredColorSchemeChange(observed_theme);
  OnForceDarkThemeChange();
}

void NativeWebThemeObserver::OnPreferredColorSchemeChange(
    ui::NativeTheme* observed_theme) {
  const auto* local_state = g_browser_process->local_state();
  const auto colorScheme =
      local_state->GetInteger(vivaldiprefs::kVivaldiPreferredColorScheme);

  ui::NativeTheme::PreferredColorScheme newScheme;
  switch (colorScheme) {
    case PreferredColorScheme::Auto:
      newScheme = observed_theme->ShouldUseDarkColors()
                      ? ui::NativeTheme::PreferredColorScheme::kDark
                      : ui::NativeTheme::PreferredColorScheme::kLight;
      break;
    case PreferredColorScheme::Light:
      newScheme = ui::NativeTheme::PreferredColorScheme::kLight;
      break;
    case PreferredColorScheme::Dark:
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
  const auto* local_state = g_browser_process->local_state();
  if (flags_storage_ && prefs && local_state) {
    const auto preferredColorScheme =
        local_state->GetInteger(vivaldiprefs::kVivaldiPreferredColorScheme);
    auto shouldForceDarkTheme =
        prefs->GetBoolean(vivaldiprefs::kAppearanceForceDarkModeTheme);

    // If preferred color sheme is Auto deduct value from preferred color theme
    if (preferredColorScheme == PreferredColorScheme::Auto) {
      auto* nativeTheme = ui::NativeTheme::GetInstanceForWeb();
      DCHECK(nativeTheme);
      shouldForceDarkTheme &= nativeTheme->ShouldUseDarkColors();
    } else {
      shouldForceDarkTheme &=
          preferredColorScheme == PreferredColorScheme::Dark;
    }

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