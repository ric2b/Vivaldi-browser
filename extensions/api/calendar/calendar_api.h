// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_CALENDAR_CALENDAR_API_H_
#define EXTENSIONS_API_CALENDAR_CALENDAR_API_H_

#include <memory>
#include <string>

#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_service.h"
#include "calendar/calendar_type.h"
#include "calendar/event_type.h"
#include "calendar/notification_type.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/calendar.h"

using calendar::CalendarModelObserver;
using calendar::CalendarService;

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

  // CalendarModelObserver
  void OnEventCreated(CalendarService* service,
                      const calendar::EventResult& event) override;
  void OnEventDeleted(CalendarService* service,
                      const calendar::EventResult& event) override;
  void OnEventChanged(CalendarService* service,
                      const calendar::EventResult& event) override;

  void OnEventTypeCreated(CalendarService* service,
                          const calendar::EventTypeRow& row) override;
  void OnEventTypeDeleted(CalendarService* service,
                          const calendar::EventTypeRow& row) override;
  void OnEventTypeChanged(CalendarService* service,
                          const calendar::EventTypeRow& row) override;

  void OnCalendarCreated(CalendarService* service,
                         const calendar::CalendarRow& row) override;
  void OnCalendarDeleted(CalendarService* service,
                         const calendar::CalendarRow& row) override;
  void OnCalendarChanged(CalendarService* service,
                         const calendar::CalendarRow& row) override;

  void OnNotificationChanged(CalendarService* service,
                             const calendar::NotificationRow& row) override;

  void OnCalendarModified(CalendarService* service) override;

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

class CalendarAsyncFunction : public ExtensionFunction {
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

class CalendarGetAllEventsFunction : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAllEvents", CALENDAR_GETALLEVENTS)
 public:
  CalendarGetAllEventsFunction() = default;

 protected:
  ~CalendarGetAllEventsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllEventsComplete(
      std::shared_ptr<calendar::EventQueryResults> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarGetAllEventsFunction);
};

class CalendarEventCreateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.eventCreate", CALENDAR_CREATEEVENT)
  CalendarEventCreateFunction() = default;

 protected:
  ~CalendarEventCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateEventComplete(std::shared_ptr<calendar::EventResultCB> results);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarEventCreateFunction);
};

class CalendarEventsCreateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.eventsCreate", CALENDAR_CREATEEVENTS)
  CalendarEventsCreateFunction() = default;

 protected:
  ~CalendarEventsCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateEventsComplete(
      std::shared_ptr<calendar::CreateEventsResult> results);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarEventsCreateFunction);
};

class CalendarUpdateEventFunction : public CalendarFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.updateEvent", CALENDAR_UPDATEEVENT)
  CalendarUpdateEventFunction() = default;

 protected:
  ~CalendarUpdateEventFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateEventComplete(
      std::shared_ptr<calendar::UpdateEventResult> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarUpdateEventFunction);
};

class CalendarDeleteEventFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteEvent", CALENDAR_DELETEEVENT)
  CalendarDeleteEventFunction() = default;

 protected:
  ~CalendarDeleteEventFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteEventComplete(
      std::shared_ptr<calendar::DeleteEventResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarDeleteEventFunction);
};

class CalendarCreateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.create", CALENDAR_CREATE)
  CalendarCreateFunction() = default;

 protected:
  ~CalendarCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateComplete(std::shared_ptr<calendar::CreateCalendarResult> results);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarCreateFunction);
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
  void GetAllComplete(std::shared_ptr<calendar::CalendarQueryResults> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarGetAllFunction);
};

class CalendarUpdateFunction : public CalendarFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.update", CALENDAR_UPDATE)
  CalendarUpdateFunction() = default;

 protected:
  ~CalendarUpdateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateCalendarComplete(
      std::shared_ptr<calendar::UpdateCalendarResult> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarUpdateFunction);
};

class CalendarDeleteFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.delete", CALENDAR_DELETE)
  CalendarDeleteFunction() = default;

 protected:
  ~CalendarDeleteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteCalendarComplete(
      std::shared_ptr<calendar::DeleteCalendarResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarDeleteFunction);
};

class CalendarGetAllEventTypesFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.getAllEventTypes",
                             CALENDAR_GETALL_EVENT_TYPES)
  CalendarGetAllEventTypesFunction() = default;

 protected:
  ~CalendarGetAllEventTypesFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllEventTypesComplete(
      std::shared_ptr<calendar::EventTypeRows> results);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarGetAllEventTypesFunction);
};

class CalendarEventTypeCreateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.eventTypeCreate",
                             CALENDAR_CREATEEVENTTYPE)
  CalendarEventTypeCreateFunction() = default;

 protected:
  ~CalendarEventTypeCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateEventTypeComplete(
      std::shared_ptr<calendar::CreateEventTypeResult> results);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarEventTypeCreateFunction);
};

class CalendarEventTypeUpdateFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.eventTypeUpdate",
                             CALENDAR_UPDATEEVENTTYPE)
  CalendarEventTypeUpdateFunction() = default;

 protected:
  ~CalendarEventTypeUpdateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void UpdateEventTypeComplete(
      std::shared_ptr<calendar::UpdateEventTypeResult> results);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarEventTypeUpdateFunction);
};

class CalendarDeleteEventTypeFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteEventType",
                             CALENDAR_DELETEEVENTTYPE)
  CalendarDeleteEventTypeFunction() = default;

 protected:
  ~CalendarDeleteEventTypeFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteEventTypeComplete(
      std::shared_ptr<calendar::DeleteEventTypeResult> result);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarDeleteEventTypeFunction);
};

class CalendarCreateEventExceptionFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.createEventException",
                             CALENDAR_CREATE_EVENT_RECURRENCE_EXCEPTION)
  CalendarCreateEventExceptionFunction() = default;

 protected:
  ~CalendarCreateEventExceptionFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateEventExceptionComplete(
      std::shared_ptr<calendar::EventResultCB> results);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarCreateEventExceptionFunction);
};

class CalendarGetAllNotificationsFunction
    : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAllNotifications",
                             CALENDAR_GETALL_NOTIFICATIONS)
 public:
  CalendarGetAllNotificationsFunction() = default;

 protected:
  ~CalendarGetAllNotificationsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  void GetAllNotificationsComplete(
      std::shared_ptr<calendar::GetAllNotificationResult> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarGetAllNotificationsFunction);
};

class CalendarCreateNotificationFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.createNotification",
                             CALENDAR_CREATE_NOTIFICATION)
  CalendarCreateNotificationFunction() = default;

 protected:
  ~CalendarCreateNotificationFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateNotificationComplete(
      std::shared_ptr<calendar::NotificationResult> results);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarCreateNotificationFunction);
};

class CalendarUpdateNotificationFunction : public CalendarFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.updateNotification",
                             CALENDAR_UPDATENOTIFICATION)
  CalendarUpdateNotificationFunction() = default;

 protected:
  ~CalendarUpdateNotificationFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateNotificationComplete(
      std::shared_ptr<calendar::NotificationResult> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarUpdateNotificationFunction);
};

class CalendarDeleteNotificationFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteNotification",
                             CALENDAR_DELETE_NOTIFICATION)
  CalendarDeleteNotificationFunction() = default;

 protected:
  ~CalendarDeleteNotificationFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteNotificationComplete(
      std::shared_ptr<calendar::DeleteNotificationResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarDeleteNotificationFunction);
};

class CalendarCreateInviteFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.createInvite", CALENDAR_CREATE_INVITE)
  CalendarCreateInviteFunction() = default;

 protected:
  ~CalendarCreateInviteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateInviteComplete(std::shared_ptr<calendar::InviteResult> results);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarCreateInviteFunction);
};

class CalendarDeleteInviteFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteInvite", CALENDAR_DELETE_INVITE)
  CalendarDeleteInviteFunction() = default;

 protected:
  ~CalendarDeleteInviteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteInviteComplete(
      std::shared_ptr<calendar::DeleteInviteResult> results);

  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarDeleteInviteFunction);
};

class CalendarUpdateInviteFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.updateInvite", CALENDAR_UPDATE_INVITE)
  CalendarUpdateInviteFunction() = default;

 protected:
  ~CalendarUpdateInviteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void UpdateInviteComplete(std::shared_ptr<calendar::InviteResult> results);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarUpdateInviteFunction);
};

class CalendarCreateAccountFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.createAccount", CALENDAR_CREATE_ACCOUNT)
  CalendarCreateAccountFunction() = default;

 protected:
  ~CalendarCreateAccountFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void CreateAccountComplete(
      std::shared_ptr<calendar::CreateAccountResult> result);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarCreateAccountFunction);
};

class CalendarDeleteAccountFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.deleteAccount", CALENDAR_DELETEACCOUNT)
  CalendarDeleteAccountFunction() = default;

 protected:
  ~CalendarDeleteAccountFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void DeleteAccountComplete(
      std::shared_ptr<calendar::DeleteAccountResult> result);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarDeleteAccountFunction);
};

class CalendarUpdateAccountFunction : public CalendarAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("calendar.updateAccount", CALENDAR_UPDATEACCOUNT)
  CalendarUpdateAccountFunction() = default;

 protected:
  ~CalendarUpdateAccountFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void UpdateAccountComplete(
      std::shared_ptr<calendar::UpdateAccountResult> result);

 private:
  // The task tracker for the CalendarService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CalendarUpdateAccountFunction);
};

class CalendarGetAllAccountsFunction : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAllAccounts", CALENDAR_GETALLACCOUNTS)
 public:
  CalendarGetAllAccountsFunction() = default;

 protected:
  ~CalendarGetAllAccountsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllAccountsComplete(std::shared_ptr<calendar::AccountRows> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarGetAllAccountsFunction);
};

class CalendarGetAllEventTemplatesFunction
    : public CalendarFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("calendar.getAllEventTemplates",
                             CALENDAR_GETALLEVENT_TEMPLATES)
 public:
  CalendarGetAllEventTemplatesFunction() = default;

 protected:
  ~CalendarGetAllEventTemplatesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the calendar function to provide results.
  void GetAllEventTemplatesComplete(
      std::shared_ptr<calendar::EventQueryResults> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(CalendarGetAllEventTemplatesFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_CALENDAR_CALENDAR_API_H_
