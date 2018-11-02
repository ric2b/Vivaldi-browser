// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_CALENDAR_CALENDAR_API_H_
#define EXTENSIONS_API_CALENDAR_CALENDAR_API_H_

#include <memory>
#include <string>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/calendar.h"

#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_service.h"

using calendar::CalendarService;
using calendar::CalendarModelObserver;

namespace extensions {

using vivaldi::calendar::CalendarEvent;

// Observes CalendarModel and then routes (some of) the notifications as
// events to the extension system.
class CalendarEventRouter : public CalendarModelObserver {
 public:
  explicit CalendarEventRouter(Profile* profile);
  ~CalendarEventRouter() override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

  content::BrowserContext* browser_context_;
  CalendarService* model_;

  void ExtensiveCalendarChangesBeginning(CalendarService* model) override;
  void ExtensiveCalendarChangesEnded(CalendarService* model) override;

  DISALLOW_COPY_AND_ASSIGN(CalendarEventRouter);
};

class CalendarAPI : public BrowserContextKeyedAPI,
                    public EventRouter::Observer {
 public:
  explicit CalendarAPI(content::BrowserContext* context);
  ~CalendarAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<CalendarAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<CalendarAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "CalendarAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<CalendarEventRouter> calendar_event_router_;
  DISALLOW_COPY_AND_ASSIGN(CalendarAPI);
};

class CalendarAsyncFunction : public UIThreadExtensionFunction {
 public:
  CalendarAsyncFunction() = default;

 protected:
  CalendarService* GetCalendarService();
  Profile* GetProfile() const;
  ~CalendarAsyncFunction() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarAsyncFunction);
};

// Base class for calendar funciton APIs which require async interaction with
// chrome services and the extension thread.
class CalendarFunctionWithCallback : public CalendarAsyncFunction {
 public:
  CalendarFunctionWithCallback() = default;

 protected:
  ~CalendarFunctionWithCallback() override = default;

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarFunctionWithCallback);
};

class CalendarGetAllFunction : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAll", CALENDAR_GETALL)
 public:
  CalendarGetAllFunction() = default;

 protected:
  ~CalendarGetAllFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllEventsComplete(
      std::shared_ptr<calendar::EventQueryResults> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarGetAllFunction);
};

class CalendarCreateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.create", CALENDAR_CREATE)
  CalendarCreateFunction() = default;

 protected:
  ~CalendarCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarCreateFunction);
};

class CalendarDeleteFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.delete", CALENDAR_DELETE)
  CalendarDeleteFunction() = default;

 protected:
  ~CalendarDeleteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarDeleteFunction);
};

class CalendarUpdateFunction : public CalendarFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.update", CALENDAR_UPDATE)
  CalendarUpdateFunction() = default;

 protected:
  ~CalendarUpdateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateEventComplete(
      std::shared_ptr<calendar::UpdateEventResult> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarUpdateFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_CALENDAR_CALENDAR_API_H_
