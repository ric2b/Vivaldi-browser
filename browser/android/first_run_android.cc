// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "components/web_resource/web_resource_pref_names.h"

namespace first_run {

bool IsChromeFirstRun() {
  return !g_browser_process->local_state()->GetBoolean(prefs::kEulaAccepted);
}

}  // namespace first_run
