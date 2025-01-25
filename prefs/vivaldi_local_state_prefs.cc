#include "prefs/vivaldi_local_state_prefs.h"

#include "chrome/installer/util/google_update_settings.h"
#include "components/browser/vivaldi_brand_select.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/version_info/version_info.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

namespace {
constexpr int kDefaultLanguageListSize = 26;
constexpr const char* kDefaultLanguageList[kDefaultLanguageListSize] = {
    "ar", "cs", "de", "en", "es", "fa", "fi",      "fr",     "hi",
    "hu", "id", "is", "it", "ja", "ko", "nl",      "no",     "pl",
    "pt", "ru", "sv", "tr", "uk", "ur", "zh-Hans", "zh-Hant"};
}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiAutoUpdateStandalone,
                                true);
  registry->RegisterStringPref(vivaldiprefs::kVivaldiUniqueUserId, "");
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextDailyPing,
                             base::Time());
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextWeeklyPing,
                             base::Time());
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextMonthlyPing,
                             base::Time());
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextTrimestrialPing,
                             base::Time());
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextSemestrialPing,
                             base::Time());
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextYearlyPing,
                             base::Time());
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiStatsExtraPing, 0);
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsExtraPingTime,
                             base::Time());
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiStatsPingsSinceLastMonth,
                                0);
  registry->RegisterListPref(vivaldiprefs::kVivaldiProfileImagePath);
  registry->RegisterTimePref(
      vivaldiprefs::kVivaldiTranslateLanguageListLastUpdate, base::Time());
  registry->RegisterStringPref(vivaldiprefs::kVivaldiAccountServerUrlIdentity,
                               "https://login.vivaldi.net/oauth2/token");
  if (version_info::IsOfficialBuild()) {
    registry->RegisterStringPref(vivaldiprefs::kVivaldiSyncServerUrl,
                                 "https://bifrost.vivaldi.com/vivid-sync");
  } else {
    registry->RegisterStringPref(vivaldiprefs::kVivaldiSyncServerUrl,
                                 "https://bifrost.vivaldi.com:4433/vivid-sync");
  }
  registry->RegisterStringPref(vivaldiprefs::kVivaldiSyncNotificationsServerUrl,
                               "stomps://stream.vivaldi.com:61613/");

  base::Value::List args;
  for (int i = 0; i < kDefaultLanguageListSize; i++) {
    args.Append(base::Value(kDefaultLanguageList[i]));
  }
  registry->RegisterListPref(vivaldiprefs::kVivaldiTranslateLanguageList,
                             std::move(args));

  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiClientHintsBrand,
                                int(BrandSelection::kChromeBrand));
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiClientHintsBrandAppendVivaldi, false);
  registry->RegisterStringPref(
      vivaldiprefs::kVivaldiClientHintsBrandCustomBrand, "");
  registry->RegisterStringPref(
      vivaldiprefs::kVivaldiClientHintsBrandCustomBrandVersion, "");
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiCrashReportingConsentGranted,
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      GoogleUpdateSettings::GetCollectStatsConsent());
#else
      false);
#endif
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiPreferredColorScheme, 0);
}
}  // namespace vivaldi