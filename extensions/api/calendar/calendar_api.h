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

class Profile;

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
  void OnMailtoOpened(GURL mailto);

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(Profile* profile,
                     const std::string& event_name,
                     base::Value::List event_args);

  // CalendarModelObserver
  void OnEventCreated(CalendarService* service,
                      const calendar::EventResult& event) override;

  void OnNotificationChanged(CalendarService* service,
                             const calendar::NotificationRow& row) override;

  void OnCalendarModified(CalendarService* service) override;

  void ExtensiveCalendarChangesBeginning(CalendarService* model) override;
  void ExtensiveCalendarChangesEnded(CalendarService* model) override;

  const raw_ptr<Profile> profile_;
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

  const raw_ptr<content::BrowserContext> browser_context_;

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
  void GetAllEventsComplete(std::vector<calendar::EventRow> results);
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
  void CreateEventComplete(calendar::EventResultCB results);

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
  void CreateEventsComplete(calendar::CreateEventsResult results);

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

  void UpdateEventComplete(calendar::EventResultCB results);
};

class CalendarDeleteEventFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteEvent", CALENDAR_DELETEEVENT)
  CalendarDeleteEventFunction() = default;

 private:
  ~CalendarDeleteEventFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteEventComplete(bool results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarUpdateRecurrenceExceptionFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.updateRecurrenceException",
                             CALENDAR_UPDATE_RECURRENCE_EXCEPTION)
  CalendarUpdateRecurrenceExceptionFunction() = default;

 private:
  ~CalendarUpdateRecurrenceExceptionFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateRecurrenceExceptionComplete(calendar::EventResultCB results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarDeleteEventExceptionFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteEventException",
                             CALENDAR_DELETEEVENT_EXCEPTION)
  CalendarDeleteEventExceptionFunction() = default;

 private:
  ~CalendarDeleteEventExceptionFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteEventExceptionComplete(calendar::EventResultCB results);

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
  void CreateComplete(calendar::CreateCalendarResult results);

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
  void GetAllComplete(calendar::CalendarRows results);
};

class CalendarUpdateFunction : public CalendarFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.update", CALENDAR_UPDATE)
  CalendarUpdateFunction() = default;

 private:
  ~CalendarUpdateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateCalendarComplete(calendar::StatusCB results);
};

class CalendarDeleteFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.delete", CALENDAR_DELETE)
  CalendarDeleteFunction() = default;

 private:
  ~CalendarDeleteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteCalendarComplete(bool results);

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
  void GetAllEventTypesComplete(calendar::EventTypeRows results);

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
  void CreateEventTypeComplete(bool results);

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
  void UpdateEventTypeComplete(bool results);

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

  void DeleteEventTypeComplete(bool result);

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
  void CreateEventExceptionComplete(calendar::EventResultCB results);

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

  void GetAllNotificationsComplete(calendar::GetAllNotificationResult results);
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
  void CreateNotificationComplete(calendar::NotificationResult results);

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

  void UpdateNotificationComplete(calendar::NotificationResult results);
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

  void DeleteNotificationComplete(bool results);

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
  void CreateInviteComplete(calendar::InviteResult results);

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

  void DeleteInviteComplete(bool results);

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
  void UpdateInviteComplete(calendar::InviteResult results);

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
  void CreateAccountComplete(calendar::CreateAccountResult result);

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
  void DeleteAccountComplete(calendar::DeleteAccountResult result);

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
  void UpdateAccountComplete(calendar::UpdateAccountResult result);

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
  void GetAllAccountsComplete(calendar::AccountRows results);
};

class CalendarCreateEventTemplateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.createEventTemplate",
                             CALENDAR_CREATE_EVENT_TEMPLATE)
  CalendarCreateEventTemplateFunction() = default;

 private:
  ~CalendarCreateEventTemplateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateEventTemplateComplete(calendar::EventTemplateResultCB result);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarGetAllEventTemplatesFunction
    : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAllEventTemplates",
                             CALENDAR_GETALL_EVENT_TEMPLATES)
 public:
  CalendarGetAllEventTemplatesFunction() = default;

 private:
  ~CalendarGetAllEventTemplatesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllEventTemplatesComplete(calendar::EventTemplateRows results);
};

class CalendarUpdateEventTemplateFunction
    : public CalendarFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.updateEventTemplate",
                             CALENDAR_UPDATE_EVENT_TEMPLATE)
  CalendarUpdateEventTemplateFunction() = default;

 private:
  ~CalendarUpdateEventTemplateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateEventTemplateComplete(calendar::EventTemplateResultCB results);
};

class CalendarDeleteEventTemplateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteEventTemplate",
                             CALENDAR_DELETE_EVENT_TEMPLATE)
  CalendarDeleteEventTemplateFunction() = default;

 private:
  ~CalendarDeleteEventTemplateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteEventTemplateComplete(bool result);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class CalendarGetParentExceptionIdFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.getParentExceptionId",
                             CALENDAR_GET_PARENT_EXCEPTION_ID)
  CalendarGetParentExceptionIdFunction() = default;

 private:
  ~CalendarGetParentExceptionIdFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void GetParentExceptionIdComplete(calendar::EventID parent_event_id);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_CALENDAR_CALENDAR_API_H_
