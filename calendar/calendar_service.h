// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_SERVICE_H_
#define CALENDAR_CALENDAR_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

#include "calendar/calendar_backend.h"
#include "calendar/calendar_database_params.h"
#include "calendar/calendar_model_observer.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace calendar {

struct CalendarDatabaseParams;
class CalendarBackend;

class CalendarService : public KeyedService {
 public:
  CalendarService();
  ~CalendarService() override;
  CalendarService(const CalendarService&) = delete;
  CalendarService& operator=(const CalendarService&) = delete;

  bool Init(bool no_db, const CalendarDatabaseParams& calendar_database_params);

  // Called from shutdown service before shutting down the browser
  void Shutdown() override;

  void AddObserver(CalendarModelObserver* observer);
  void RemoveObserver(CalendarModelObserver* observer);

  // Call to schedule a given task for running on the calendar thread with the
  // specified priority. The task will have ownership taken.
  void ScheduleTask(base::OnceClosure task);

  typedef base::OnceCallback<void(CalendarRows)> GetALLQueryCalendarCallback;

  // Provides the result of a event create. See EventResultCB in
  // event_type.h.
  typedef base::OnceCallback<void(EventResultCB)> EventResultCallback;

  typedef base::OnceCallback<void(EventTemplateResultCB)>
      EventTemplateResultCallback;

  // Provides the results of multiple event create. See CreateEventsResult in
  // event_type.h.
  typedef base::OnceCallback<void(CreateEventsResult)> CreateEventsCallback;

  // Provides the result of a create calendar. See CreateCalendarResult in
  // calendar_type.h.
  typedef base::OnceCallback<void(CreateCalendarResult)> CreateCalendarCallback;

  typedef base::OnceCallback<void(EventTypeRows)> GetALLEventTypesCallback;

  typedef base::OnceCallback<void(bool)> DeleteEventTypeCallback;

  typedef base::OnceCallback<void(GetAllNotificationResult)>
      GetAllNotificationsCallback;

  typedef base::OnceCallback<void(NotificationResult)> NotificationCallback;

  typedef base::OnceCallback<void(InviteResult)> InviteCallback;

  typedef base::OnceCallback<void(CreateAccountResult)> CreateAccountCallback;

  typedef base::OnceCallback<void(DeleteAccountResult)> DeleteAccountCallback;

  typedef base::OnceCallback<void(UpdateAccountResult)> UpdateAccountCallback;

  typedef base::OnceCallback<void(AccountRows)> GetALLAccounsCallback;

  base::CancelableTaskTracker::TaskId GetAllEvents(
      base::OnceCallback<void(EventRows)>,
      base::CancelableTaskTracker* tracker);

  // Returns true if this calendar service is currently in a mode where
  // extensive changes might happen, such as for import and sync. This is
  // helpful for observers that are created after the service has started, and
  // want to check state during their own initializer.
  bool IsDoingExtensiveChanges() const { return extensive_changes_ > 0; }

  base::CancelableTaskTracker::TaskId CreateCalendarEvent(
      EventRow ev,
      EventResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateCalendarEvents(
      std::vector<calendar::EventRow> events,
      CreateEventsCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateCalendarEvent(
      EventID event_id,
      EventRow event,
      EventResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteCalendarEvent(
      EventID event_id,
      base::OnceCallback<void(bool)> callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteEventRecurrenceException(
      RecurrenceExceptionID event_id,
      EventResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateRecurrenceException(
      RecurrenceExceptionID recurrence_id,
      RecurrenceExceptionRow recurrence,
      EventResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateCalendar(
      CalendarRow ev,
      CreateCalendarCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetAllCalendars(
      GetALLQueryCalendarCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateCalendar(
      CalendarID calendar_id,
      Calendar calendar,
      base::OnceCallback<void(StatusCB)> callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteCalendar(
      CalendarID event_id,
      base::OnceCallback<void(bool)>,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetAllEventTypes(
      GetALLEventTypesCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateEventType(
      EventTypeRow ev,
      base::OnceCallback<void(bool)> callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateEventType(
      EventTypeID event_type_id,
      EventType ev,
      base::OnceCallback<void(bool)> callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteEventType(
      EventTypeID event_type_id,
      DeleteEventTypeCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateRecurrenceException(
      RecurrenceExceptionRow ev,
      EventResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetAllNotifications(
      GetAllNotificationsCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateNotification(
      NotificationRow notification,
      NotificationCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateNotification(
      NotificationID notification_id,
      UpdateNotificationRow notification,
      NotificationCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteNotification(
      NotificationID notification_id,
      base::OnceCallback<void(bool)> callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateInvite(
      InviteRow invite,
      InviteCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteInvite(
      InviteID invite_id,
      base::OnceCallback<void(bool)> callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateInvite(
      UpdateInviteRow invite,
      InviteCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateAccount(
      AccountRow ev,
      CreateAccountCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteAccount(
      AccountID id,
      DeleteAccountCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateAccount(
      AccountRow invite,
      UpdateAccountCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetAllAccounts(
      GetALLAccounsCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateEventTemplate(
      EventTemplateRow event_template,
      EventTemplateResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetAllEventTemplates(
      base::OnceCallback<void(EventTemplateRows)> callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateEventTemplate(
      EventTemplateID event_template_id,
      EventTemplateRow event_template,
      EventTemplateResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteEventTemplate(
      EventTemplateID event_template_id,
      base::OnceCallback<void(bool)> callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetParentExceptionEventId(
      calendar::EventID exception_eventId,
      base::OnceCallback<void(calendar::EventID)> callback,
      base::CancelableTaskTracker* tracker);

 private:
  class CalendarBackendDelegate;
  friend class base::RefCountedThreadSafe<CalendarService>;
  friend class CalendarBackendDelegate;
  friend class CalendarBackend;

  friend std::unique_ptr<CalendarService> CreateCalendarService(
      const base::FilePath& calendar_dir,
      bool create_db);

  void OnDBLoaded();

  // Notify all CalendarServiceObservers registered that the
  // CalendarService has finished loading.
  void NotifyCalendarServiceLoaded();
  void NotifyCalendarServiceBeingDeleted();

  void OnEventCreated(const EventResult& event);

  void OnNotificationChanged(const NotificationRow& row);
  void OnCalendarModified();

  void Cleanup();

  SEQUENCE_CHECKER(sequence_checker_);

  // The TaskRunner to which CalendarBackend tasks are posted. Nullptr once
  // Cleanup() is called.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // This pointer will be null once Cleanup() has been called, meaning no
  // more calls should be made to the calendar thread.
  scoped_refptr<CalendarBackend> calendar_backend_;

  // Has the backend finished loading? The backend is loaded once Init has
  // completed.
  bool backend_loaded_;

  // The observers.
  base::ObserverList<CalendarModelObserver> observers_;

  // See description of IsDoingExtensiveChanges above.
  int extensive_changes_;

  // All vended weak pointers are invalidated in Cleanup().
  base::WeakPtrFactory<CalendarService> weak_ptr_factory_;
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_SERVICE_H_
