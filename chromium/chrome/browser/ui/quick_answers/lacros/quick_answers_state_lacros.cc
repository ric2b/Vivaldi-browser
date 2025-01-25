// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/lacros/quick_answers_state_lacros.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using quick_answers::prefs::ConsentStatus;

void SetPref(crosapi::mojom::PrefPath path, base::Value value) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(WARNING) << "crosapi: Prefs API not available";
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
      path, std::move(value), base::DoNothing());
}

}  // namespace

QuickAnswersStateLacros::QuickAnswersStateLacros() {
  // The observers are fired immediate with the current pref value on
  // initialization.
  settings_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersEnabled,
      base::BindRepeating(&QuickAnswersStateLacros::OnSettingsEnabledChanged,
                          base::Unretained(this)));
  consent_status_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersConsentStatus,
      base::BindRepeating(&QuickAnswersStateLacros::OnConsentStatusChanged,
                          base::Unretained(this)));
  definition_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersDefinitionEnabled,
      base::BindRepeating(&QuickAnswersStateLacros::OnDefinitionEnabledChanged,
                          base::Unretained(this)));
  translation_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersTranslationEnabled,
      base::BindRepeating(&QuickAnswersStateLacros::OnTranslationEnabledChanged,
                          base::Unretained(this)));
  unit_conversion_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersUnitConversionEnabled,
      base::BindRepeating(
          &QuickAnswersStateLacros::OnUnitConversionEnabledChanged,
          base::Unretained(this)));
  application_locale_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kApplicationLocale,
      base::BindRepeating(&QuickAnswersStateLacros::OnApplicationLocaleChanged,
                          base::Unretained(this)));
  preferred_languages_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kPreferredLanguages,
      base::BindRepeating(&QuickAnswersStateLacros::OnPreferredLanguagesChanged,
                          base::Unretained(this)));
  spoken_feedback_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
      base::BindRepeating(
          &QuickAnswersStateLacros::OnSpokenFeedbackEnabledChanged,
          base::Unretained(this)));
  impression_count_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersNoticeImpressionCount,
      base::BindRepeating(&QuickAnswersStateLacros::OnImpressionCountChanged,
                          base::Unretained(this)));
  impression_duration_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersNoticeImpressionDuration,
      base::BindRepeating(&QuickAnswersStateLacros::OnImpressionDurationChanged,
                          base::Unretained(this)));

  prefs_initialized_ = true;
  for (auto& observer : observers_) {
    observer.OnPrefsInitialized();
  }

  MaybeNotifyEligibilityChanged();
}

QuickAnswersStateLacros::~QuickAnswersStateLacros() = default;

void QuickAnswersStateLacros::AsyncWriteConsentUiImpressionCount(
    int32_t count) {
  SetPref(crosapi::mojom::PrefPath::kQuickAnswersNoticeImpressionCount,
          base::Value(count));
}

void QuickAnswersStateLacros::AsyncWriteConsentStatus(
    ConsentStatus consent_status) {
  SetPref(crosapi::mojom::PrefPath::kQuickAnswersConsentStatus,
          base::Value(consent_status));
}
void QuickAnswersStateLacros::AsyncWriteEnabled(bool enabled) {
  SetPref(crosapi::mojom::PrefPath::kQuickAnswersEnabled, base::Value(enabled));
}

void QuickAnswersStateLacros::OnSettingsEnabledChanged(base::Value value) {
  DCHECK(value.is_bool());
  bool settings_enabled = value.GetBool();

  // `QuickAnswersStateAsh` co-exists with `QuickAnswersStateLacros`. As
  // `QuickAnswersStateAsh` should also get notified for those pref changes,
  // `QuickAnswersStateLacros` doesn't need to modify prefs. For now, leave
  // KioskSession logic as its logic works in a fail-safe way. Toggled from the
  // settings logic is removed.
  //
  // TODO(b/340628526): Remove this as we update consent status logic.
  if (chromeos::IsKioskSession()) {
    settings_enabled = false;
    SetPref(crosapi::mojom::PrefPath::kQuickAnswersEnabled, base::Value(false));
    SetPref(crosapi::mojom::PrefPath::kQuickAnswersConsentStatus,
            base::Value(quick_answers::prefs::ConsentStatus::kRejected));
  }

  quick_answers_enabled_ = settings_enabled;
  MaybeNotifyIsEnabledChanged();
}

void QuickAnswersStateLacros::OnConsentStatusChanged(base::Value value) {
  DCHECK(value.is_int());
  SetQuickAnswersFeatureConsentStatus(
      static_cast<quick_answers::prefs::ConsentStatus>(value.GetInt()));
}

void QuickAnswersStateLacros::OnDefinitionEnabledChanged(base::Value value) {
  DCHECK(value.is_bool());
  SetIntentEligibilityAsQuickAnswers(quick_answers::Intent::kDefinition,
                                     value.GetBool());
}

void QuickAnswersStateLacros::OnTranslationEnabledChanged(base::Value value) {
  DCHECK(value.is_bool());
  SetIntentEligibilityAsQuickAnswers(quick_answers::Intent::kTranslation,
                                     value.GetBool());
}

void QuickAnswersStateLacros::OnUnitConversionEnabledChanged(
    base::Value value) {
  DCHECK(value.is_bool());
  SetIntentEligibilityAsQuickAnswers(quick_answers::Intent::kUnitConversion,
                                     value.GetBool());
}

void QuickAnswersStateLacros::OnApplicationLocaleChanged(base::Value value) {
  DCHECK(value.is_string());
  auto locale = value.GetString();

  if (locale.empty()) {
    return;
  }

  // We should not directly use the pref locale, resolve the generic locale name
  // to one of the locally defined ones first.
  std::string resolved_locale;
  bool resolve_success =
      l10n_util::CheckAndResolveLocale(locale, &resolved_locale,
                                       /*perform_io=*/false);
  DCHECK(resolve_success);

  if (resolved_application_locale_ == resolved_locale) {
    return;
  }
  resolved_application_locale_ = resolved_locale;

  for (auto& observer : observers_) {
    observer.OnApplicationLocaleReady(resolved_locale);
  }

  MaybeNotifyEligibilityChanged();
}

void QuickAnswersStateLacros::OnPreferredLanguagesChanged(base::Value value) {
  DCHECK(value.is_string());
  preferred_languages_ = value.GetString();

  for (auto& observer : observers_) {
    observer.OnPreferredLanguagesChanged(preferred_languages_);
  }
}

void QuickAnswersStateLacros::OnImpressionCountChanged(base::Value value) {
  DCHECK(value.is_int());
  consent_ui_impression_count_ = value.GetInt();
}

void QuickAnswersStateLacros::OnImpressionDurationChanged(base::Value value) {
  DCHECK(value.is_int());
  impression_duration_ = value.GetInt();
}

void QuickAnswersStateLacros::OnSpokenFeedbackEnabledChanged(
    base::Value value) {
  DCHECK(value.is_bool());
  spoken_feedback_enabled_ = value.GetBool();
}
