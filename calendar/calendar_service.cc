// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar_service.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/string_compare.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/variations/variations_associated_data.h"

#include "calendar/calendar_backend.h"
#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_type.h"
#include "calendar/event_type.h"

using base::Time;

using calendar::CalendarBackend;
namespace calendar {
namespace {

static const char* kCalendarThreadName = "Vivaldi_CalendarThread";
}
// Sends messages from the db backend to us on the main thread. This must be a
// separate class from the calendar service so that it can hold a reference to
// the history service (otherwise we would have to manually AddRef and
// Release when the Backend has a reference to us).
class CalendarService::CalendarBackendDelegate
    : public CalendarBackend::CalendarDelegate {
 public:
  CalendarBackendDelegate(
      const base::WeakPtr<CalendarService>& calendar_service,
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner)
      : calendar_service_(calendar_service),
        service_task_runner_(service_task_runner) {}

  void DBLoaded() override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&CalendarService::OnDBLoaded, calendar_service_));
  }

  void NotifyEventCreated(const EventRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CalendarService::OnEventCreated, calendar_service_, row));
  }

  void NotifyEventModified(const EventRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CalendarService::OnEventChanged, calendar_service_, row));
  }

  void NotifyEventDeleted(const EventRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CalendarService::OnEventDeleted, calendar_service_, row));
  }

  void NotifyCalendarCreated(const CalendarRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&CalendarService::OnCalendarCreated,
                              calendar_service_, row));
  }

  void NotifyCalendarModified(const CalendarRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&CalendarService::OnCalendarChanged,
                              calendar_service_, row));
  }

  void NotifyCalendarDeleted(const CalendarRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&CalendarService::OnCalendarDeleted,
                              calendar_service_, row));
  }

 private:
  const base::WeakPtr<CalendarService> calendar_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
};

CalendarService::CalendarService()
    : thread_(variations::GetVariationParamValue("BrowserScheduler",
                                                 "RedirectCalendarService") ==
                      "true"
                  ? nullptr
                  : new base::Thread(kCalendarThreadName)),
      backend_loaded_(false),
      weak_ptr_factory_(this) {}

CalendarService::~CalendarService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
}

void CalendarService::Shutdown() {}

bool CalendarService::Init(
    bool no_db,
    const CalendarDatabaseParams& calendar_database_params) {
  TRACE_EVENT0("browser,startup", "CalendarService::Init")
  SCOPED_UMA_HISTOGRAM_TIMER("Calendar.CalendarServiceInitTime");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!backend_task_runner_);

  if (thread_) {
    base::Thread::Options options;
    options.timer_slack = base::TIMER_SLACK_MAXIMUM;
    if (!thread_->StartWithOptions(options)) {
      Cleanup();
      return false;
    }
    backend_task_runner_ = thread_->task_runner();
  } else {
    backend_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        base::TaskTraits(base::TaskPriority::USER_BLOCKING,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                         base::MayBlock(), base::WithBaseSyncPrimitives()));
  }

  // Create the calendar backend.
  scoped_refptr<CalendarBackend> backend(new CalendarBackend(
      new CalendarBackendDelegate(weak_ptr_factory_.GetWeakPtr(),
                                  base::ThreadTaskRunnerHandle::Get()),
      backend_task_runner_));
  calendar_backend_.swap(backend);

  ScheduleTask(base::Bind(&CalendarBackend::Init, calendar_backend_, no_db,
                          calendar_database_params));

  return true;
}

base::CancelableTaskTracker::TaskId CalendarService::CreateCalendarEvent(
    EventRow ev,
    const CreateEventCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<CreateEventResult> create_results =
      std::shared_ptr<CreateEventResult>(new CreateEventResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::CreateCalendarEvent, calendar_backend_, ev,
                 create_results),
      base::Bind(callback, create_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateCalendarEvents(
    std::vector<calendar::EventRow> events,
    const CreateEventsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<CreateEventsResult> create_results =
      std::shared_ptr<CreateEventsResult>(new CreateEventsResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::CreateCalendarEvents, calendar_backend_,
                 events, create_results),
      base::Bind(callback, create_results));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateCalendarEvent(
    EventID event_id,
    CalendarEvent event,
    const UpdateEventCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<UpdateEventResult> update_results =
      std::shared_ptr<UpdateEventResult>(new UpdateEventResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::UpdateEvent, calendar_backend_, event_id,
                 event, update_results),
      base::Bind(callback, update_results));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteCalendarEvent(
    EventID event_id,
    const DeleteEventCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<DeleteEventResult> delete_results =
      std::shared_ptr<DeleteEventResult>(new DeleteEventResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::DeleteEvent, calendar_backend_, event_id,
                 delete_results),
      base::Bind(callback, delete_results));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllEvents(
    const QueryCalendarCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<EventQueryResults> query_results =
      std::shared_ptr<EventQueryResults>(new EventQueryResults());

  return tracker->PostTaskAndReply(backend_task_runner_.get(), FROM_HERE,
                                   base::Bind(&CalendarBackend::GetAllEvents,
                                              calendar_backend_, query_results),
                                   base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateCalendar(
    CalendarRow ev,
    const CreateCalendarCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<CreateCalendarResult> create_results =
      std::shared_ptr<CreateCalendarResult>(new CreateCalendarResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::CreateCalendar, calendar_backend_, ev,
                 create_results),
      base::Bind(callback, create_results));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllCalendars(
    const GetALLQueryCalendarCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<CalendarQueryResults> query_results =
      std::shared_ptr<CalendarQueryResults>(new CalendarQueryResults());

  return tracker->PostTaskAndReply(backend_task_runner_.get(), FROM_HERE,
                                   base::Bind(&CalendarBackend::GetAllCalendars,
                                              calendar_backend_, query_results),
                                   base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateCalendar(
    CalendarID calendar_id,
    Calendar calendar,
    const UpdateCalendarCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<UpdateCalendarResult> update_results =
      std::shared_ptr<UpdateCalendarResult>(new UpdateCalendarResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::UpdateCalendar, calendar_backend_,
                 calendar_id, calendar, update_results),
      base::Bind(callback, update_results));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteCalendar(
    CalendarID calendar_id,
    const DeleteCalendarCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<DeleteCalendarResult> delete_results =
      std::shared_ptr<DeleteCalendarResult>(new DeleteCalendarResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::DeleteCalendar, calendar_backend_,
                 calendar_id, delete_results),
      base::Bind(callback, delete_results));
}

void CalendarService::ScheduleTask(base::OnceClosure task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(backend_task_runner_);

  backend_task_runner_->PostTask(FROM_HERE, std::move(task));
}

void CalendarService::AddObserver(CalendarModelObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void CalendarService::RemoveObserver(CalendarModelObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void CalendarService::OnDBLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  backend_loaded_ = true;
  NotifyCalendarServiceLoaded();
}

void CalendarService::NotifyCalendarServiceLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (CalendarModelObserver& observer : observers_)
    observer.OnCalendarServiceLoaded(this);
}

void CalendarService::Cleanup() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!backend_task_runner_) {
    // We've already cleaned up.
    return;
  }

  NotifyCalendarServiceBeingDeleted();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Unload the backend.
  if (calendar_backend_.get()) {
    // The backend's destructor must run on the calendar thread since it is not
    // threadsafe. So this thread must not be the last thread holding a
    // reference to the backend, or a crash could happen.
    //
    // We have a reference to the calendar backend. There is also an extra
    // reference held by our delegate installed in the backend, which
    // CalendarBackend::Closing will release. This means if we scheduled a call
    // to CalendarBackend::Closing and *then* released our backend reference,
    // there will be a race between us and the backend's Closing function to see
    // who is the last holder of a reference. If the backend thread's Closing
    // manages to run before we release our backend refptr, the last reference
    // will be held by this thread and the destructor will be called from here.
    //
    // Therefore, we create a closure to run the Closing operation first. This
    // holds a reference to the backend. Then we release our reference, then we
    // schedule the task to run. After the task runs, it will delete its
    // reference from the calendar thread, ensuring everything works properly.
    //
    calendar_backend_->AddRef();
    base::Closure closing_task =
        base::Bind(&CalendarBackend::Closing, calendar_backend_);
    ScheduleTask(closing_task);
    closing_task.Reset();
    CalendarBackend* raw_ptr = calendar_backend_.get();
    calendar_backend_ = nullptr;
    backend_task_runner_->ReleaseSoon(FROM_HERE, raw_ptr);
  }

  // Clear |backend_task_runner_| to make sure it's not used after Cleanup().
  backend_task_runner_ = nullptr;

  // Join the background thread, if any.
  thread_.reset();
}

void CalendarService::NotifyCalendarServiceBeingDeleted() {
  // TODO(arnar): Add
  /* DCHECK(thread_checker_.CalledOnValidThread());
  for (CalendarServiceObserver& observer : observers_)
    observer.CalendarServiceBeingDeleted(this);*/
}

void CalendarService::OnEventCreated(const EventRow& row) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnEventCreated(this, row);
  }
}

void CalendarService::OnEventDeleted(const EventRow& row) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnEventDeleted(this, row);
  }
}

void CalendarService::OnEventChanged(const EventRow& row) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnEventChanged(this, row);
  }
}

void CalendarService::OnCalendarCreated(const CalendarRow& row) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnCalendarCreated(this, row);
  }
}

void CalendarService::OnCalendarDeleted(const CalendarRow& row) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnCalendarDeleted(this, row);
  }
}

void CalendarService::OnCalendarChanged(const CalendarRow& row) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnCalendarChanged(this, row);
  }
}

}  // namespace calendar
