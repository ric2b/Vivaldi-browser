// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef CRASHREPORT_OBSERVER_H_
#define CRASHREPORT_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace content {
class BrowserContext;
}

namespace vivaldi {

class CrashReportObserver : public extensions::BrowserContextKeyedAPI {
 public:
  explicit CrashReportObserver(content::BrowserContext* context);

  CrashReportObserver(const CrashReportObserver&) = delete;
  CrashReportObserver& operator=(const CrashReportObserver&) = delete;

  ~CrashReportObserver() override;

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;
  static extensions::BrowserContextKeyedAPIFactory<CrashReportObserver>*
  GetFactoryInstance();

 private:
  void OnPrefChange();

  friend class extensions::BrowserContextKeyedAPIFactory<CrashReportObserver>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "CrashReportObserver"; }
  static const bool kServiceIsNULLWhileTesting = true;

  PrefChangeRegistrar pref_change_registrar;
};

}  // namespace vivaldi

#endif  // CRASHREPORT_OBSERVER_H_
