// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar_service_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"

#include "calendar/calendar_service.h"
#include "prefs/vivaldi_pref_names.h"

namespace calendar {

CalendarServiceFactory::CalendarServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CalendarService",
          BrowserContextDependencyManager::GetInstance()) {}

CalendarServiceFactory::~CalendarServiceFactory() {}

// static
CalendarService* CalendarServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<CalendarService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CalendarService* CalendarServiceFactory::GetForProfileIfExists(
    Profile* profile,
    ServiceAccessType sat) {
  return static_cast<CalendarService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
CalendarService* CalendarServiceFactory::GetForProfileWithoutCreating(
    Profile* profile) {
  return static_cast<CalendarService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
CalendarServiceFactory* CalendarServiceFactory::GetInstance() {
  return base::Singleton<CalendarServiceFactory>::get();
}

// static
void CalendarServiceFactory::ShutdownForProfile(Profile* profile) {
  CalendarServiceFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}

content::BrowserContext* CalendarServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* CalendarServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<calendar::CalendarService> cal_service(
      new calendar::CalendarService());

  Profile* profile = Profile::FromBrowserContext(context);

  calendar::CalendarDatabaseParams param =
      calendar::CalendarDatabaseParams(profile->GetPath());

  if (!cal_service->Init(false, param)) {
    return nullptr;
  }
  return cal_service.release();
}

bool CalendarServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace calendar
