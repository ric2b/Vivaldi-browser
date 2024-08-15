// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "components/crashreport/crashreport_observer.h"

#include "base/lazy_instance.h"
#include "chrome/browser/browser_process.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_pref_names.h"
#include "content/public/browser/browser_context.h"

#include "components/crashreport/crashreport_accessor.h"
#include "prefs/vivaldi_pref_names.h"

#if defined(IS_WIN)
#include "chrome/install_static/install_util.h"
#include "components/crash/core/app/crash_export_thunks.h"
#endif

namespace vivaldi {

CrashReportObserver::CrashReportObserver(content::BrowserContext*) {
  auto* local_state = g_browser_process->local_state();
  if (local_state) {
    pref_change_registrar.Init(local_state);
    pref_change_registrar.Add(
        vivaldiprefs::kVivaldiCrashReportingConsentGranted,
        base::BindRepeating(&CrashReportObserver::OnPrefChange,
                            base::Unretained(this)));
  }
  OnPrefChange();
}

CrashReportObserver::~CrashReportObserver() = default;

void CrashReportObserver::Shutdown() {}

void CrashReportObserver::OnPrefChange() {
  GoogleUpdateSettings::CollectStatsConsentTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult([](bool consent) {
#if defined(IS_WIN)
                       // ChromeMetricsServicesManagerClient::UpdateRunningServices
                       // can override those settings, but won't react to pref
                       // change.
                       install_static::SetCollectStatsInSample(consent);
                       SetUploadConsent_ExportThunk(consent);
#endif
                       GoogleUpdateSettings::SetCollectStatsConsent(consent);
                     }),
                     vivaldi::IsVivaldiCrashReportingEnabled()));
}

static base::LazyInstance<extensions::BrowserContextKeyedAPIFactory<
    CrashReportObserver>>::DestructorAtExit g_crashreport_observer_factory =
    LAZY_INSTANCE_INITIALIZER;

extensions::BrowserContextKeyedAPIFactory<CrashReportObserver>*
CrashReportObserver::GetFactoryInstance() {
  return g_crashreport_observer_factory.Pointer();
}

}  // namespace vivaldi
