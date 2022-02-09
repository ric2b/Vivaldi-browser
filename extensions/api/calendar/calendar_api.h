// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_CALENDAR_CALENDAR_API_H_
#define EXTENSIONS_API_CALENDAR_CALENDAR_API_H_

#include <memory>
#include <string>

#include "base/scoped_observation.h"
#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_service.h"
#include "calendar/calendar_type.h"
#include "calendar/event_type.h"
#include "calendar/notification_type.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/calendar.h"

namespace base {
class Value;
}

namespace download {
class DownloadItem;
}

using calendar::CalendarModelObserver;
using calendar::CalendarService;

namespace extensions {

using vivaldi::calendar::CalendarEvent;

// Observes CalendarModel and then routes (some of) the notifications as
// events to the extension system.
class CalendarEventRouter : public CalendarModelObserver {
 public:
  explicit CalendarEventRouter(Profile* profile,
                               CalendarService* calendar_service);
  ~CalendarEventRouter() override;

  void OnIcsFileOpened(std::string path);
  void OnWebcalUrlOpened(GURL url);

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(Profile* profile,
                     const std::string& event_name,
                     std::vector<base::Value> event_args);

  // CalendarModelObserver
  void OnEventCreated(CalendarService* service,
                      const calendar::EventResult& event) override;

  void OnNotificationChanged(CalendarService* service,
                             const calendar::NotificationRow& row) override;

  void OnCalendarModified(CalendarService* service) override;

  void ExtensiveCalendarChangesBeginning(CalendarService* model) override;
  void ExtensiveCalendarChangesEnded(CalendarService* model) override;

  Profile* profile_;
  base::ScopedObservation<calendar::CalendarService, CalendarModelObserver>
      calendar_service_observation_{this};
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

  static void RegisterInternalHandlers();

 private:
  friend class BrowserContextKeyedAPIFactory<CalendarAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "CalendarAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<CalendarEventRouter> calendar_event_router_;
};

class CalendarAsyncFunction : public ExtensionFunction {
 public:
  CalendarAsyncFunction() = default;

 protected:
  CalendarService* GetCalendarService();
  Profile* GetProfile() const;
  ~CalendarAsyncFunction() override {}
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
};

class CalendarGetAllEventsFunction : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAllEvents", CALENDAR_GETALLEVENTS)
 public:
  CalendarGetAllEventsFunction() = default;

 private:
  ~CalendarGetAllEventsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllEventsComplete(
      std::shared_ptr<calendar::EventQueryResults> results);
};

class CalendarEventCreateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.eventCreate", CALENDAR_CREATEEVENT)
  CalendarEventCreateFunction() = default;

 private:
  ~CalendarEventCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateEventComplete(std::shared_ptr<calendar::EventResultCB> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarEventsCreateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.eventsCreate", CALENDAR_CREATEEVENTS)
  CalendarEventsCreateFunction() = default;

 private:
  ~CalendarEventsCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateEventsComplete(
      std::shared_ptr<calendar::CreateEventsResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarUpdateEventFunction : public CalendarFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.updateEvent", CALENDAR_UPDATEEVENT)
  CalendarUpdateEventFunction() = default;

 private:
  ~CalendarUpdateEventFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateEventComplete(std::shared_ptr<calendar::EventResultCB> results);
};

class CalendarDeleteEventFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteEvent", CALENDAR_DELETEEVENT)
  CalendarDeleteEventFunction() = default;

 private:
  ~CalendarDeleteEventFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteEventComplete(
      std::shared_ptr<calendar::DeleteEventResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarCreateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.create", CALENDAR_CREATE)
  CalendarCreateFunction() = default;

 private:
  ~CalendarCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateComplete(std::shared_ptr<calendar::CreateCalendarResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarGetAllFunction : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAll", CALENDAR_GETALL)
 public:
  CalendarGetAllFunction() = default;

 private:
  ~CalendarGetAllFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllComplete(std::shared_ptr<calendar::CalendarQueryResults> results);
};

class CalendarUpdateFunction : public CalendarFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.update", CALENDAR_UPDATE)
  CalendarUpdateFunction() = default;

 private:
  ~CalendarUpdateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateCalendarComplete(
      std::shared_ptr<calendar::UpdateCalendarResult> results);
};

class CalendarDeleteFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.delete", CALENDAR_DELETE)
  CalendarDeleteFunction() = default;

 private:
  ~CalendarDeleteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteCalendarComplete(
      std::shared_ptr<calendar::DeleteCalendarResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarGetAllEventTypesFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.getAllEventTypes",
                             CALENDAR_GETALL_EVENT_TYPES)
  CalendarGetAllEventTypesFunction() = default;

 private:
  ~CalendarGetAllEventTypesFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllEventTypesComplete(
      std::shared_ptr<calendar::EventTypeRows> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarEventTypeCreateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.eventTypeCreate",
                             CALENDAR_CREATEEVENTTYPE)
  CalendarEventTypeCreateFunction() = default;

 private:
  ~CalendarEventTypeCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateEventTypeComplete(
      std::shared_ptr<calendar::CreateEventTypeResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarEventTypeUpdateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.eventTypeUpdate",
                             CALENDAR_UPDATEEVENTTYPE)
  CalendarEventTypeUpdateFunction() = default;

 private:
  ~CalendarEventTypeUpdateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void UpdateEventTypeComplete(
      std::shared_ptr<calendar::UpdateEventTypeResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarDeleteEventTypeFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteEventType",
                             CALENDAR_DELETEEVENTTYPE)
  CalendarDeleteEventTypeFunction() = default;

 private:
  ~CalendarDeleteEventTypeFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteEventTypeComplete(
      std::shared_ptr<calendar::DeleteEventTypeResult> result);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarCreateEventExceptionFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.createEventException",
                             CALENDAR_CREATE_EVENT_RECURRENCE_EXCEPTION)
  CalendarCreateEventExceptionFunction() = default;

 private:
  ~CalendarCreateEventExceptionFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateEventExceptionComplete(
      std::shared_ptr<calendar::EventResultCB> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarGetAllNotificationsFunction
    : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAllNotifications",
                             CALENDAR_GETALL_NOTIFICATIONS)
 public:
  CalendarGetAllNotificationsFunction() = default;

 private:
  ~CalendarGetAllNotificationsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  void GetAllNotificationsComplete(
      std::shared_ptr<calendar::GetAllNotificationResult> results);
};

class CalendarCreateNotificationFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.createNotification",
                             CALENDAR_CREATE_NOTIFICATION)
  CalendarCreateNotificationFunction() = default;

 private:
  ~CalendarCreateNotificationFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateNotificationComplete(
      std::shared_ptr<calendar::NotificationResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarUpdateNotificationFunction : public CalendarFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.updateNotification",
                             CALENDAR_UPDATENOTIFICATION)
  CalendarUpdateNotificationFunction() = default;

 private:
  ~CalendarUpdateNotificationFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateNotificationComplete(
      std::shared_ptr<calendar::NotificationResult> results);
};

class CalendarDeleteNotificationFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteNotification",
                             CALENDAR_DELETE_NOTIFICATION)
  CalendarDeleteNotificationFunction() = default;

 private:
  ~CalendarDeleteNotificationFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteNotificationComplete(
      std::shared_ptr<calendar::DeleteNotificationResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarCreateInviteFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.createInvite", CALENDAR_CREATE_INVITE)
  CalendarCreateInviteFunction() = default;

 private:
  ~CalendarCreateInviteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateInviteComplete(std::shared_ptr<calendar::InviteResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarDeleteInviteFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteInvite", CALENDAR_DELETE_INVITE)
  CalendarDeleteInviteFunction() = default;

 private:
  ~CalendarDeleteInviteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteInviteComplete(
      std::shared_ptr<calendar::DeleteInviteResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarUpdateInviteFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.updateInvite", CALENDAR_UPDATE_INVITE)
  CalendarUpdateInviteFunction() = default;

 private:
  ~CalendarUpdateInviteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void UpdateInviteComplete(std::shared_ptr<calendar::InviteResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarCreateAccountFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.createAccount", CALENDAR_CREATE_ACCOUNT)
  CalendarCreateAccountFunction() = default;

 private:
  ~CalendarCreateAccountFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateAccountComplete(
      std::shared_ptr<calendar::CreateAccountResult> result);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarDeleteAccountFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteAccount", CALENDAR_DELETEACCOUNT)
  CalendarDeleteAccountFunction() = default;

 private:
  ~CalendarDeleteAccountFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void DeleteAccountComplete(
      std::shared_ptr<calendar::DeleteAccountResult> result);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarUpdateAccountFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.updateAccount", CALENDAR_UPDATEACCOUNT)
  CalendarUpdateAccountFunction() = default;

 private:
  ~CalendarUpdateAccountFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void UpdateAccountComplete(
      std::shared_ptr<calendar::UpdateAccountResult> result);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarGetAllAccountsFunction : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAllAccounts", CALENDAR_GETALLACCOUNTS)
 public:
  CalendarGetAllAccountsFunction() = default;

 private:
  ~CalendarGetAllAccountsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllAccountsComplete(std::shared_ptr<calendar::AccountRows> results);
};

class CalendarGetAllEventTemplatesFunction
    : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAllEventTemplates",
                             CALENDAR_GETALLEVENT_TEMPLATES)
 public:
  CalendarGetAllEventTemplatesFunction() = default;

 private:
  ~CalendarGetAllEventTemplatesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllEventTemplatesComplete(
      std::shared_ptr<calendar::EventQueryResults> results);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_CALENDAR_CALENDAR_API_H_
