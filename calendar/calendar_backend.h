// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_BACKEND_H_
#define CALENDAR_CALENDAR_BACKEND_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "calendar/calendar_backend_notifier.h"
#include "calendar/calendar_database.h"
#include "calendar/event_template_type.h"
#include "calendar/event_type.h"
#include "calendar/notification_type.h"
#include "calendar/recurrence_exception_type.h"
#include "sql/init_status.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace calendar {

class CalendarDatabase;
struct CalendarDatabaseParams;
class CalendarBackendHelper;
class EventDatabase;

// Internal calendar implementation which does most of the work of the calendar
// system. This runs on a custom created db thread (to not block the browser
// when we do expensive operations) and is NOT threadsafe, so it must only be
// called from message handlers on the background thread.
//
// Most functions here are just the implementations of the corresponding
// functions in the calendar service. These functions are not documented
// here, see the calendar service for behavior.
class CalendarBackend
    : public base::RefCountedThreadSafe<calendar::CalendarBackend>,
      public CalendarBackendNotifier {
 public:
  // Interface implemented by the owner of the CalendarBackend object. Normally,
  // the calendar service implements this to send stuff back to the main thread.
  // The unit tests can provide a different implementation if they don't have
  // a calendar service object.
  class CalendarDelegate {
   public:
    virtual ~CalendarDelegate() {}

    virtual void NotifyEventCreated(const EventResult& event) = 0;
    virtual void NotifyNotificationChanged(const NotificationRow& row) = 0;
    virtual void NotifyCalendarChanged() = 0;

    // Invoked when the backend has finished loading the db.
    virtual void DBLoaded() = 0;
  };

  explicit CalendarBackend(CalendarDelegate* delegate);

  // This constructor is fast and does no I/O, so can be called at any time.
  CalendarBackend(CalendarDelegate* delegate,
                  scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Must be called after creation but before any objects are created. If this
  // fails, all other functions will fail as well. (Since this runs on another
  // thread, we don't bother returning failure.)
  //
  // |force_fail| can be set during unittests to unconditionally fail to init.
  void Init(bool force_fail,
            const CalendarDatabaseParams& calendar_database_params);

  // Notification that the calendar system is shutting down. This will break
  // the refs owned by the delegate and any pending transaction so it will
  // actually be deleted.
  void Closing();

  void CancelScheduledCommit();

  void Commit();

  // Creates an Event
  EventResultCB CreateCalendarEvent(EventRow ev, bool notify);

  // Creates multiple events
  CreateEventsResult CreateCalendarEvents(
      std::vector<calendar::EventRow> events);

  std::vector<calendar::EventRow> GetAllEvents();

  // Updates an event
  EventResultCB UpdateEvent(EventID event_id, const EventRow& event);
  bool DeleteEvent(EventID event_id);

  EventResultCB CreateRecurrenceException(RecurrenceExceptionRow row);
  EventResultCB UpdateRecurrenceException(
      RecurrenceExceptionID recurrence_id,
      const RecurrenceExceptionRow& recurrence);
  EventResultCB DeleteEventRecurrenceException(
      RecurrenceExceptionID exception_id);

  // Creates an Calendar
  CreateCalendarResult CreateCalendar(CalendarRow ev);
  CalendarRows GetAllCalendars();
  StatusCB UpdateCalendar(CalendarID calendar_id, const Calendar& calendar);
  bool DeleteCalendar(CalendarID calendar_id);

  EventTypeRows GetAllEventTypes();

  bool CreateEventType(EventTypeRow event_type_row);
  bool UpdateEventType(EventTypeID event_id, const EventType& event_type);
  bool DeleteEventType(EventTypeID event_type_id);

  GetAllNotificationResult GetAllNotifications();

  NotificationResult CreateNotification(calendar::NotificationRow row);

  NotificationResult UpdateNotification(calendar::UpdateNotificationRow row);

  bool DeleteNotification(NotificationID notification_id);

  InviteResult CreateInvite(calendar::InviteRow row);

  bool DeleteInvite(InviteID invite_id);

  InviteResult UpdateInvite(calendar::UpdateInviteRow row);

  CreateAccountResult CreateAccount(AccountRow account_row);

  DeleteAccountResult DeleteAccount(calendar::AccountID account_id);

  UpdateAccountResult UpdateAccount(calendar::AccountRow account_row);

  AccountRows GetAllAccounts();

  EventResult FillEvent(EventID id);

  EventTemplateResultCB CreateEventTemplate(EventTemplateRow event_template);
  EventTemplateRows GetAllEventTemplates();
  EventTemplateResultCB UpdateEventTemplate(
      EventTemplateID template_id,
      const EventTemplateRow& event_template);

  bool DeleteEventTemplate(EventTemplateID event_template_id);
  EventID GetParentExceptionEventId(EventID exception_event_id);

  void NotifyEventCreated(const EventResult& event) override;
  void NotifyNotificationChanged(const NotificationRow& row) override;
  void NotifyCalendarChanged() override;

 protected:
  ~CalendarBackend() override;
  CalendarBackend(const CalendarBackend&) = delete;
  CalendarBackend& operator=(const CalendarBackend&) = delete;

 private:
  friend class base::RefCountedThreadSafe<CalendarBackend>;

  // Does the work of Init.
  void InitImpl(const CalendarDatabaseParams& calendar_database_params);
  void FillEventsWithExceptions(EventRows rows, EventRows* results);

  // Closes all databases managed by CalendarBackend. Commits any pending
  // transactions.
  void CloseAllDatabases();

  // Querying ------------------------------------------------------------------

  // Directory where database files will be stored, empty until Init is called.
  base::FilePath calendar_dir_;

  // Delegate. See the class definition above for more information. This will
  // be null before Init is called and after Cleanup, but is guaranteed
  // non-null in between.
  std::unique_ptr<CalendarDelegate> delegate_;

  // A commit has been scheduled to occur sometime in the future. We can check
  // !IsCancelled() to see if there is a commit scheduled in the future (note
  // that CancelableClosure starts cancelled with the default constructor), and
  // we can use Cancel() to cancel the scheduled commit. There can be only one
  // scheduled commit at a time (see ScheduleCommit).
  base::CancelableOnceClosure scheduled_commit_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The calendar database. Either may be null if the database could
  // not be opened, all users must first check for null and return immediately
  // if it is.
  std::unique_ptr<calendar::CalendarDatabase> db_;
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_BACKEND_H_
