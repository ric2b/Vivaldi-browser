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
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#include "sql/error_delegate_util.h"

#include "calendar/calendar_constants.h"
#include "calendar/calendar_database_params.h"
#include "calendar/calendar_types.h"
#include "calendar_database.h"

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

void CalendarBackend::NotifyEventCreated() {}
void CalendarBackend::NotifyEventModified() {}
void CalendarBackend::NotifyEventDeleted() {}

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
    }  // Falls through.
    case sql::INIT_TOO_NEW: {
      LOG(ERROR) << "INIT_TOO_NEW";

      return;
    }
    default:
      NOTREACHED();
  }
}

// Observers -------------------------------------------------------------------
void CalendarBackend::AddObserver(CalendarBackendObserver* observer) {
  observers_.AddObserver(observer);
}

void CalendarBackend::RemoveObserver(CalendarBackendObserver* observer) {
  observers_.RemoveObserver(observer);
}

void CalendarBackend::CreateCalendarEvent(EventRow ev) {
  db_->CreateCalendarEvent(ev);
}

void CalendarBackend::GetAllEvents(std::shared_ptr<EventQueryResults> results) {
  EventRows rows;
  db_->GetAllCalendarEvents(&rows);
  // Now add them and the URL rows to the results.
  calendar::EventResult ev;
  for (size_t i = 0; i < rows.size(); i++) {
    const EventRow eventRow = rows[i];
    ev.set_id(eventRow.id());
    ev.set_calendar_id(eventRow.calendar_id());
    ev.set_title(eventRow.title());
    ev.set_description(eventRow.description());
    ev.set_start(eventRow.start());
    ev.set_end(eventRow.end());

    results->AppendEventBySwapping(&ev);
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
    result->success = db_->UpdateEventRow(event_row);
  } else {
    result->success = false;
    NOTREACHED() << "Could not find event row in DB";
    return;
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
