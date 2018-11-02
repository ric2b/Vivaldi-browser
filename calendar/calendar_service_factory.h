// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_SERVICE_FACTORY_H_
#define CALENDAR_CALENDAR_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "calendar/calendar_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"

class Profile;

namespace calendar {

// Singleton that owns all CalendarService and associates them with
// Profiles.
class CalendarServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CalendarService* GetForProfile(Profile* profile);

  static CalendarService* GetForProfileIfExists(Profile* profile,
                                                ServiceAccessType sat);

  static CalendarService* GetForProfileWithoutCreating(Profile* profile);

  static CalendarServiceFactory* GetInstance();

  // In the testing profile, we often clear the calendar before making a new
  // one. This takes care of that work. It should only be used in tests.
  // Note: This does not do any cleanup; it only destroys the service. The
  // calling test is expected to do the cleanup before calling this function.
  static void ShutdownForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<CalendarServiceFactory>;

  CalendarServiceFactory();
  ~CalendarServiceFactory() override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsNULLWhileTesting() const override;
};

}  //  namespace calendar

#endif  // CALENDAR_CALENDAR_SERVICE_FACTORY_H_
