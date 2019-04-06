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

#include "sql/error_delegate_util.h"

#include "calendar/calendar_constants.h"
#include "calendar/calendar_database.h"
#include "calendar/calendar_database_params.h"
#include "calendar/event_type.h"

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

void CalendarBackend::NotifyEventCreated(const EventRow& row) {
  if (delegate_)
    delegate_->NotifyEventCreated(row);
}

void CalendarBackend::NotifyEventModified(const EventRow& row) {
  if (delegate_)
    delegate_->NotifyEventModified(row);
}

void CalendarBackend::NotifyEventDeleted(const EventRow& row) {
  if (delegate_)
    delegate_->NotifyEventDeleted(row);
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
  RecurrenceRows recurrenceRows;

  db_->GetAllCalendarEvents(&rows);
  db_->GetAllRecurrences(&recurrenceRows);

  // Now add them and the URL rows to the results.
  for (size_t i = 0; i < rows.size(); i++) {
    EventRow eventRow = rows[i];

    for (size_t j = 0; j < recurrenceRows.size(); j++) {
      const RecurrenceRow recurrenceRow = recurrenceRows[j];

      if (recurrenceRow.event_id() == eventRow.id()) {
        EventRecurrence eventRecurrence;
        eventRecurrence.interval = recurrenceRow.recurrence_interval();
        eventRecurrence.number_of_occurrences =
            recurrenceRow.number_of_ocurrences();
        eventRecurrence.skip_count = recurrenceRow.skip_count();
        eventRecurrence.day_of_week = recurrenceRow.day_of_week();

        eventRecurrence.week_of_month = recurrenceRow.week_of_month();
        eventRecurrence.day_of_month = recurrenceRow.day_of_month();
        eventRecurrence.month_of_year = recurrenceRow.month_of_year();

        eventRow.set_recurrence(eventRecurrence);
      }
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

  for (size_t i = 0; i < count; i++) {
    EventRow ev = events[i];

    EventID id = db_->CreateCalendarEvent(ev);

    if (id) {
      ev.set_id(id);

      RecurrenceRow recurrence_row;
      recurrence_row.set_event_id(id);

      recurrence_row.set_recurrence_interval(ev.recurrence().interval);
      recurrence_row.set_number_of_ocurrences(
          ev.recurrence().number_of_occurrences);
      recurrence_row.set_skip_count(ev.recurrence().skip_count);
      recurrence_row.set_day_of_week(ev.recurrence().day_of_week);
      recurrence_row.set_week_of_month(ev.recurrence().week_of_month);
      recurrence_row.set_day_of_month(ev.recurrence().day_of_month);
      recurrence_row.set_month_of_year(ev.recurrence().month_of_year);

      db_->CreateRecurrenceEvent(recurrence_row);

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

    RecurrenceRow recurrence_row;
    recurrence_row.set_event_id(id);

    recurrence_row.set_recurrence_interval(ev.recurrence().interval);
    recurrence_row.set_number_of_ocurrences(
        ev.recurrence().number_of_occurrences);
    recurrence_row.set_skip_count(ev.recurrence().skip_count);
    recurrence_row.set_day_of_week(ev.recurrence().day_of_week);
    recurrence_row.set_week_of_month(ev.recurrence().week_of_month);
    recurrence_row.set_day_of_month(ev.recurrence().day_of_month);
    recurrence_row.set_month_of_year(ev.recurrence().month_of_year);

    db_->CreateRecurrenceEvent(recurrence_row);

    result->success = true;
    result->createdRow = ev;
    NotifyEventCreated(ev);
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

    if (event.updateFields & calendar::RECURRENCE) {
      RecurrenceRow recurrence_row;
      db_->GetRecurrenceRow(event_id, &recurrence_row);

      recurrence_row.set_event_id(event_id);

      if (event.recurrence.updateFields & calendar::RECURRENCE_INTERVAL) {
        recurrence_row.set_recurrence_interval(event.recurrence.interval);
      }

      if (event.recurrence.updateFields & calendar::NUMBER_OF_OCCURRENCES) {
        recurrence_row.set_number_of_ocurrences(
            event.recurrence.number_of_occurrences);
      }

      if (event.recurrence.updateFields & calendar::RECURRENCE_SKIP_COUNT) {
        recurrence_row.set_skip_count(event.recurrence.skip_count);
      }

      if (event.recurrence.updateFields & calendar::RECURRENCE_DAY_OF_WEEK) {
        recurrence_row.set_day_of_week(event.recurrence.day_of_week);
      }

      if (event.recurrence.updateFields & calendar::RECURRENCE_WEEK_OF_MONTH) {
        recurrence_row.set_week_of_month(event.recurrence.week_of_month);
      }

      if (event.recurrence.updateFields & calendar::RECURRENCE_DAY_OF_MONTH) {
        recurrence_row.set_day_of_month(event.recurrence.day_of_month);
      }

      if (event.recurrence.updateFields & calendar::RECURRENCE_MONTH_OF_YEAR) {
        recurrence_row.set_month_of_year(event.recurrence.month_of_year);
      }

      db_->CreateRecurrenceEvent(recurrence_row);
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
    NOTREACHED() << "Could not find event row in DB";
    return;
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
    result->success = db_->DeleteCalendar(calendar_id);
    NotifyCalendarDeleted(calendar_row);
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
