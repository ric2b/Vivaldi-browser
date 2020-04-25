// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/calendar_backend.h"

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "calendar/calendar_constants.h"
#include "calendar/calendar_database.h"
#include "calendar/calendar_database_params.h"
#include "calendar/event_type.h"
#include "calendar/recurrence_exception_type.h"
#include "sql/error_delegate_util.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace calendar {

// CalendarBackend -----------------------------------------------------------
CalendarBackend::CalendarBackend(
    CalendarDelegate* delegate,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : delegate_(delegate) {}

CalendarBackend::CalendarBackend(CalendarDelegate* delegate) {}

CalendarBackend::~CalendarBackend() {}

void CalendarBackend::NotifyEventCreated(const EventResult& event) {
  if (delegate_)
    delegate_->NotifyEventCreated(event);
}

void CalendarBackend::NotifyEventModified(const EventResult& event) {
  if (delegate_)
    delegate_->NotifyEventModified(event);
}

void CalendarBackend::NotifyEventDeleted(const EventResult& event) {
  if (delegate_)
    delegate_->NotifyEventDeleted(event);
}

void CalendarBackend::NotifyCalendarCreated(const CalendarRow& row) {
  if (delegate_)
    delegate_->NotifyCalendarCreated(row);
}

void CalendarBackend::NotifyCalendarModified(const CalendarRow& row) {
  if (delegate_)
    delegate_->NotifyCalendarModified(row);
}

void CalendarBackend::NotifyCalendarDeleted(const CalendarRow& row) {
  if (delegate_)
    delegate_->NotifyCalendarDeleted(row);
}

void CalendarBackend::NotifyEventTypeCreated(const EventTypeRow& row) {
  if (delegate_)
    delegate_->NotifyEventTypeCreated(row);
}

void CalendarBackend::NotifyEventTypeModified(const EventTypeRow& row) {
  if (delegate_)
    delegate_->NotifyEventTypeModified(row);
}

void CalendarBackend::NotifyEventTypeDeleted(const EventTypeRow& row) {
  if (delegate_)
    delegate_->NotifyEventTypeDeleted(row);
}

void CalendarBackend::Init(
    bool force_fail,
    const CalendarDatabaseParams& calendar_database_params) {
  // CalendarBackend is created on the UI thread by CalendarService, then the
  // CalendarBackend::Init() method is called on the DB thread. Create the
  // base::SupportsUserData on the DB thread since it is not thread-safe.

  if (!force_fail)
    InitImpl(calendar_database_params);
  delegate_->DBLoaded();
}

void CalendarBackend::Closing() {
  // TODO(arnar): Add
  //  CancelScheduledCommit();

  // Release our reference to the delegate, this reference will be keeping the
  // calendar service alive.
  delegate_.reset();
}

void CalendarBackend::InitImpl(
    const CalendarDatabaseParams& calendar_database_params) {
  DCHECK(!db_) << "Initializing CalendarBackend twice";
  // In the rare case where the db fails to initialize a dialog may get shown
  // the blocks the caller, yet allows other messages through. For this reason
  // we only set db_ to the created database if creation is successful. That
  // way other methods won't do anything as db_ is still null.

  // Compute the file names.
  calendar_dir_ = calendar_database_params.calendar_dir;
  base::FilePath calendar_name = calendar_dir_.Append(kCalendarFilename);

  // Calendar database.
  db_.reset(new CalendarDatabase());

  sql::InitStatus status = db_->Init(calendar_name);
  switch (status) {
    case sql::INIT_OK:
      break;
    case sql::INIT_FAILURE: {
      // A null db_ will cause all calls on this object to notice this error
      // and to not continue. If the error callback scheduled killing the
      // database, the task it posted has not executed yet. Try killing the
      // database now before we close it.
      FALLTHROUGH;
    }  // Falls through.
    case sql::INIT_TOO_NEW: {
      LOG(ERROR) << "INIT_TOO_NEW";

      return;
    }
    default:
      NOTREACHED();
  }
}

void CalendarBackend::GetAllEvents(std::shared_ptr<EventQueryResults> results) {
  EventRows rows;
  RecurrenceExceptionRows recurrence_exception_rows;

  db_->GetAllCalendarEvents(&rows);
  db_->GetAllRecurrenceExceptions(&recurrence_exception_rows);

  // Now add them and the URL rows to the results.
  for (size_t i = 0; i < rows.size(); i++) {
    EventRow eventRow = rows[i];
    std::set<RecurrenceExceptionID> events_with_exceptions;
    for (auto& exception_row : recurrence_exception_rows) {
      events_with_exceptions.insert(exception_row.parent_event_id);
    }

    const bool has_exception = events_with_exceptions.find(eventRow.id()) !=
                               events_with_exceptions.end();

    if (has_exception) {
      RecurrenceExceptionRows exception_rows;
      for (auto& exception : recurrence_exception_rows) {
        if (exception.parent_event_id == eventRow.id()) {
          exception_rows.push_back(exception);
        }
      }
      eventRow.set_recurrence_exceptions(exception_rows);
    }

    EventResult result(eventRow);
    results->AppendEventBySwapping(&result);
  }
}

void CalendarBackend::CreateCalendarEvents(
    std::vector<calendar::EventRow> events,
    std::shared_ptr<CreateEventsResult> result) {
  int success_counter = 0;
  int failed_counter = 0;

  size_t count = events.size();

  std::shared_ptr<CreateEventResult> event_result =
      std::shared_ptr<CreateEventResult>(new CreateEventResult());

  for (size_t i = 0; i < count; i++) {
    EventRow ev = events[i];

    CreateCalendarEvent(ev, event_result);

    if (event_result->success) {
      success_counter++;
    } else {
      failed_counter++;
    }
  }

  result->number_success = success_counter;
  result->number_failed = failed_counter;
  EventRow ev;
  NotifyEventCreated(ev);
}

void CalendarBackend::CreateCalendarEvent(
    EventRow ev,
    std::shared_ptr<CreateEventResult> result) {
  if (!db_->DoesCalendarIdExist(ev.calendar_id())) {
    result->success = false;
    result->message = "Calendar does not exist.";
    return;
  }

  EventID id = db_->CreateCalendarEvent(ev);

  if (id) {
    ev.set_id(id);
    if (ev.event_exceptions().size() > 0) {
      for (const auto& exception : ev.event_exceptions()) {
        EventRow exception_event;
        exception_event.set_title(exception.title);
        exception_event.set_calendar_id(ev.calendar_id());
        exception_event.set_description(exception.description);
        exception_event.set_start(exception.start);
        exception_event.set_end(exception.end);
        EventID exception_id = db_->CreateCalendarEvent(exception_event);

        RecurrenceExceptionRow row;
        row.exception_event_id = exception_id;
        row.parent_event_id = id;
        row.exception_day = exception.exception_date;
        row.cancelled = false;
        db_->CreateRecurrenceException(row);
      }
    }
    result->success = true;
    EventResult res = FillEvent(id);
    result->createdEvent = res;
    NotifyEventCreated(res);
  } else {
    result->success = false;
  }
}

EventResult CalendarBackend::FillEvent(EventID id) {
  EventRow event_row;
  db_->GetRowForEvent(id, &event_row);
  EventResult res(event_row);
  return res;
}

void CalendarBackend::CreateRecurrenceException(
    RecurrenceExceptionRow row,
    std::shared_ptr<CreateRecurrenceExceptionResult> result) {
  if (!db_->DoesEventIdExist(row.parent_event_id)) {
    result->success = false;
    result->message = "Event does not exist.";
    return;
  }

  RecurrenceExceptionID id = db_->CreateRecurrenceException(row);
  EventID parent_event_id = row.parent_event_id;

  if (id) {
    row.id = id;
    RecurrenceExceptionRows exception_rows;
    db_->GetAllRecurrenceExceptionsForEvent(parent_event_id, &exception_rows);

    EventRow event_row;
    if (db_->GetRowForEvent(parent_event_id, &event_row)) {
      event_row.set_recurrence_exceptions(exception_rows);
      NotifyEventModified(event_row);
    }
    result->success = true;
    result->createdRow = row;
  } else {
    result->success = false;
  }
}

void CalendarBackend::GetAllCalendars(
    std::shared_ptr<CalendarQueryResults> results) {
  CalendarRows rows;
  db_->GetAllCalendars(&rows);
  for (size_t i = 0; i < rows.size(); i++) {
    const CalendarRow calendarRow = rows[i];
    CalendarResult result(calendarRow);
    results->AppendCalendarBySwapping(&result);
  }
}

void CalendarBackend::UpdateEvent(EventID event_id,
                                  const CalendarEvent& event,
                                  std::shared_ptr<UpdateEventResult> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  EventRow event_row;
  if (db_->GetRowForEvent(event_id, &event_row)) {
    if (event.updateFields & calendar::CALENDAR_ID) {
      event_row.set_calendar_id(event.calendar_id);
    }

    if (event.updateFields & calendar::TITLE) {
      event_row.set_title(event.title);
    }

    if (event.updateFields & calendar::DESCRIPTION) {
      event_row.set_description(event.description);
    }

    if (event.updateFields & calendar::START) {
      event_row.set_start(event.start);
    }

    if (event.updateFields & calendar::END) {
      event_row.set_end(event.end);
    }

    if (event.updateFields & calendar::ALLDAY) {
      event_row.set_all_day(event.all_day);
    }

    if (event.updateFields & calendar::ISRECURRING) {
      event_row.set_is_recurring(event.is_recurring);
    }

    if (event.updateFields & calendar::STARTRECURRING) {
      event_row.set_start_recurring(event.start_recurring);
    }

    if (event.updateFields & calendar::ENDRECURRING) {
      event_row.set_end_recurring(event.end_recurring);
    }

    if (event.updateFields & calendar::LOCATION) {
      event_row.set_location(event.location);
    }

    if (event.updateFields & calendar::URL) {
      event_row.set_url(event.url);
    }

    if (event.updateFields & calendar::ETAG) {
      event_row.set_etag(event.etag);
    }

    if (event.updateFields & calendar::HREF) {
      event_row.set_href(event.href);
    }

    if (event.updateFields & calendar::UID) {
      event_row.set_uid(event.uid);
    }

    if (event.updateFields & calendar::EVENT_TYPE_ID) {
      event_row.set_event_type_id(event.event_type_id);
    }

    if (event.updateFields & calendar::TASK) {
      event_row.set_task(event.task);
    }

    if (event.updateFields & calendar::COMPLETE) {
      event_row.set_complete(event.complete);
    }

    if (event.updateFields & calendar::TRASH) {
      event_row.set_trash(event.trash);
    }

    if (event.updateFields & calendar::SEQUENCE) {
      event_row.set_sequence(event.sequence);
    }

    if (event.updateFields & calendar::ICAL) {
      event_row.set_ical(event.ical);
    }

    if (event.updateFields & calendar::RRULE) {
      event_row.set_rrule(event.rrule);
    }

    result->success = db_->UpdateEventRow(event_row);

    if (result->success) {
      EventRow changed_row;
      if (db_->GetRowForEvent(event_id, &changed_row)) {
        NotifyEventModified(changed_row);
      }
    }
  } else {
    result->success = false;
    result->message = "Could not find event row in DB";
    NOTREACHED() << "Could not find event row in DB";
    return;
  }
}

void CalendarBackend::GetAllEventTypes(std::shared_ptr<EventTypeRows> results) {
  EventTypeRows event_type_rows;
  db_->GetAllEventTypes(&event_type_rows);

  for (size_t i = 0; i < event_type_rows.size(); i++) {
    EventTypeRow event_type_row = event_type_rows[i];
    results->push_back(event_type_row);
  }
}

void CalendarBackend::UpdateEventType(
    EventTypeID event_type_id,
    const EventType& event,
    std::shared_ptr<UpdateEventTypeResult> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  EventTypeRow event_row;
  if (db_->GetRowForEventType(event_type_id, &event_row)) {
    if (event.updateFields & calendar::NAME) {
      event_row.set_name(event.name);
    }

    if (event.updateFields & calendar::COLOR) {
      event_row.set_color(event.color);
    }

    if (event.updateFields & calendar::ICONINDEX) {
      event_row.set_iconindex(event.iconindex);
    }

    result->success = db_->UpdateEventTypeRow(event_row);

    if (result->success) {
      EventTypeRow changed_row;
      if (db_->GetRowForEventType(event_type_id, &changed_row)) {
        NotifyEventTypeModified(changed_row);
      }
    }
  } else {
    result->success = false;
    NOTREACHED() << "Could not find event type row in DB";
    return;
  }
}

void CalendarBackend::DeleteEventType(
    EventTypeID event_type_id,
    std::shared_ptr<DeleteEventTypeResult> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  EventTypeRow event_type_row;
  if (db_->GetRowForEventType(event_type_id, &event_type_row)) {
    result->success = db_->DeleteEventType(event_type_id);
    NotifyEventTypeDeleted(event_type_row);
  } else {
    result->success = false;
  }
}

void CalendarBackend::DeleteEvent(EventID event_id,
                                  std::shared_ptr<DeleteEventResult> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  EventRow event_row;
  if (db_->GetRowForEvent(event_id, &event_row)) {
    if (db_->DoesRecurrenceExceptionExistForEvent(event_id)) {
      EventIDs event_ids;

      if (db_->GetAllEventExceptionIds(event_id, &event_ids)) {
        for (auto it = event_ids.begin(); it != event_ids.end(); ++it) {
          if (!db_->DeleteEvent(*it)) {
            result->success = false;
            return;
          }
        }
      }
      if (!db_->DeleteRecurrenceExceptions(event_id)) {
        result->success = false;
        return;
      }
    }
    result->success = db_->DeleteEvent(event_id);
    NotifyEventDeleted(event_row);
  } else {
    result->success = false;
  }
}

void CalendarBackend::CreateCalendar(
    CalendarRow calendar,
    std::shared_ptr<CreateCalendarResult> result) {
  CalendarID id = db_->CreateCalendar(calendar);

  if (id) {
    calendar.set_id(id);
    result->success = true;
    result->createdRow = calendar;
    NotifyCalendarCreated(calendar);
  } else {
    result->success = false;
  }
}

void CalendarBackend::UpdateCalendar(
    CalendarID calendar_id,
    const Calendar& calendar,
    std::shared_ptr<UpdateCalendarResult> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  CalendarRow calendar_row;
  if (db_->GetRowForCalendar(calendar_id, &calendar_row)) {
    if (calendar.updateFields & calendar::CALENDAR_NAME) {
      calendar_row.set_name(calendar.name);
    }

    if (calendar.updateFields & calendar::CALENDAR_DESCRIPTION) {
      calendar_row.set_description(calendar.description);
    }

    if (calendar.updateFields & calendar::CALENDAR_URL) {
      calendar_row.set_url(calendar.url);
    }

    if (calendar.updateFields & calendar::CALENDAR_ORDERINDEX) {
      calendar_row.set_orderindex(calendar.orderindex);
    }

    if (calendar.updateFields & calendar::CALENDAR_COLOR) {
      calendar_row.set_color(calendar.color);
    }

    if (calendar.updateFields & calendar::CALENDAR_HIDDEN) {
      calendar_row.set_hidden(calendar.hidden);
    }

    if (calendar.updateFields & calendar::CALENDAR_ACTIVE) {
      calendar_row.set_active(calendar.active);
    }

    if (calendar.updateFields & calendar::CALENDAR_ICONINDEX) {
      calendar_row.set_iconindex(calendar.iconindex);
    }

    if (calendar.updateFields & calendar::CALENDAR_CTAG) {
      calendar_row.set_ctag(calendar.ctag);
    }

    if (calendar.updateFields & calendar::CALENDAR_USERNAME) {
      calendar_row.set_username(calendar.username);
    }

    if (calendar.updateFields & calendar::CALENDAR_TYPE) {
      calendar_row.set_type(calendar.type);
    }

    if (calendar.updateFields & calendar::CALENDAR_INTERVAL) {
      calendar_row.set_interval(calendar.interval);
    }

    if (calendar.updateFields & calendar::CALENDAR_LAST_CHECKED) {
      calendar_row.set_last_checked(calendar.last_checked);
    }

    result->success = db_->UpdateCalendarRow(calendar_row);

    if (result->success) {
      CalendarRow changed_row;
      if (db_->GetRowForCalendar(calendar_id, &changed_row)) {
        NotifyCalendarModified(changed_row);
      }
    }
  } else {
    result->success = false;
    NOTREACHED() << "Could not find calendar row in DB";
    return;
  }
}

void CalendarBackend::DeleteCalendar(
    CalendarID calendar_id,
    std::shared_ptr<DeleteCalendarResult> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  CalendarRow calendar_row;
  if (db_->GetRowForCalendar(calendar_id, &calendar_row)) {
    db_->DeleteEventsForCalendar(calendar_id);
    result->success = db_->DeleteCalendar(calendar_id);
    NotifyCalendarDeleted(calendar_row);
  } else {
    result->success = false;
  }
}

void CalendarBackend::CreateEventType(
    EventTypeRow event_type_row,
    std::shared_ptr<CreateEventTypeResult> result) {
  EventTypeID id = db_->CreateEventType(event_type_row);

  if (id) {
    event_type_row.set_id(id);
    result->success = true;
    NotifyEventTypeCreated(event_type_row);
  } else {
    result->success = false;
  }
}

void CalendarBackend::CloseAllDatabases() {
  if (db_) {
    // Commit the long-running transaction.
    db_->CommitTransaction();
    db_.reset();
  }
}

void CalendarBackend::CancelScheduledCommit() {
  scheduled_commit_.Cancel();
}

void CalendarBackend::Commit() {
  if (!db_)
    return;

#if defined(OS_IOS)
  // Attempts to get the application running long enough to commit the database
  // transaction if it is currently being backgrounded.
  base::ios::ScopedCriticalAction scoped_critical_action;
#endif

  // Note that a commit may not actually have been scheduled if a caller
  // explicitly calls this instead of using ScheduleCommit. Likewise, we
  // may reset the flag written by a pending commit. But this is OK! It
  // will merely cause extra commits (which is kind of the idea). We
  // could optimize more for this case (we may get two extra commits in
  // some cases) but it hasn't been important yet.
  CancelScheduledCommit();

  db_->CommitTransaction();
  DCHECK_EQ(db_->transaction_nesting(), 0)
      << "Somebody left a transaction open";
  db_->BeginTransaction();
}
}  // namespace calendar
