// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "components/crashreport/crashreport_accessor.h"

#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "prefs/vivaldi_pref_names.h"

namespace vivaldi {

bool IsVivaldiCrashReportingEnabled() {
  auto* local_state = g_browser_process->local_state();

  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!local_state)
    return false;

  return g_browser_process->local_state()->GetBoolean(
      vivaldiprefs::kVivaldiCrashReportingConsentGranted);
}

}  // namespace vivaldi
