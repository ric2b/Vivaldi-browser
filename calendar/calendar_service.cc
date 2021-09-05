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
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "calendar/calendar_backend.h"
#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_type.h"
#include "calendar/event_type.h"
#include "calendar/notification_type.h"
#include "components/variations/variations_associated_data.h"

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

  void NotifyEventCreated(const EventResult& event) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CalendarService::OnEventCreated, calendar_service_, event));
  }

  void NotifyEventModified(const EventResult& event) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CalendarService::OnEventChanged, calendar_service_, event));
  }

  void NotifyEventDeleted(const EventResult& event) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CalendarService::OnEventDeleted, calendar_service_, event));
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

  void NotifyEventTypeCreated(const EventTypeRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&CalendarService::OnEventTypeCreated,
                              calendar_service_, row));
  }

  void NotifyEventTypeModified(const EventTypeRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&CalendarService::OnEventTypeChanged,
                              calendar_service_, row));
  }

  void NotifyEventTypeDeleted(const EventTypeRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&CalendarService::OnEventTypeDeleted,
                              calendar_service_, row));
  }

  void NotifyNotificationChanged(const NotificationRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&CalendarService::OnNotificationChanged,
                              calendar_service_, row));
  }

  void NotifyCalendarChanged() override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CalendarService::OnCalendarModified, calendar_service_));
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
    backend_task_runner_ = base::CreateSequencedTaskRunner(
        base::TaskTraits(base::ThreadPool(), base::TaskPriority::USER_BLOCKING,
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

  std::shared_ptr<EventResultCB> create_results =
      std::shared_ptr<EventResultCB>(new EventResultCB());

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

base::CancelableTaskTracker::TaskId CalendarService::UpdateEventType(
    EventTypeID event_type_id,
    EventType ev,
    const UpdateEventTypeCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<UpdateEventTypeResult> update_event_type_results =
      std::shared_ptr<UpdateEventTypeResult>(new UpdateEventTypeResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::UpdateEventType, calendar_backend_,
                 event_type_id, ev, update_event_type_results),
      base::Bind(callback, update_event_type_results));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllEventTypes(
    const GetALLEventTypesCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<EventTypeRows> query_results =
      std::shared_ptr<EventTypeRows>(new EventTypeRows());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::GetAllEventTypes, calendar_backend_,
                 query_results),
      base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateEventType(
    EventTypeRow ev,
    const CreateEventTypeCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<CreateEventTypeResult> create_event_type_results =
      std::shared_ptr<CreateEventTypeResult>(new CreateEventTypeResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::CreateEventType, calendar_backend_, ev,
                 create_event_type_results),
      base::Bind(callback, create_event_type_results));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteEventType(
    EventTypeID event_type_id,
    const DeleteEventTypeCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<DeleteEventTypeResult> delete_results =
      std::shared_ptr<DeleteEventTypeResult>(new DeleteEventTypeResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::DeleteEventType, calendar_backend_,
                 event_type_id, delete_results),
      base::Bind(callback, delete_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateRecurrenceException(
    RecurrenceExceptionRow ev,
    const CreateRecurrenceExceptionCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<EventResultCB> create_recurrence_exception_results =
      std::shared_ptr<EventResultCB>(new EventResultCB());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::CreateRecurrenceException, calendar_backend_,
                 ev, create_recurrence_exception_results),
      base::Bind(callback, create_recurrence_exception_results));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllNotifications(
    const GetAllNotificationsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<GetAllNotificationResult> query_results =
      std::shared_ptr<GetAllNotificationResult>(new GetAllNotificationResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::GetAllNotifications, calendar_backend_,
                 query_results),
      base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateNotification(
    NotificationRow row,
    const CreateNotificationCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<CreateNotificationResult> create_notification_result =
      std::shared_ptr<CreateNotificationResult>(new CreateNotificationResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::CreateNotification, calendar_backend_, row,
                 create_notification_result),
      base::Bind(callback, create_notification_result));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteNotification(
    NotificationID notification_id,
    const DeleteNotificationCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<DeleteNotificationResult> delete_results =
      std::shared_ptr<DeleteNotificationResult>(new DeleteNotificationResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::DeleteNotification, calendar_backend_,
                 notification_id, delete_results),
      base::Bind(callback, delete_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateInvite(
    InviteRow invite,
    const InviteCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<InviteResult> create_invite_result =
      std::shared_ptr<InviteResult>(new InviteResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::CreateInvite, calendar_backend_, invite,
                 create_invite_result),
      base::Bind(callback, create_invite_result));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteInvite(
    InviteID invite_id,
    const DeleteInviteCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<DeleteInviteResult> delete_results =
      std::shared_ptr<DeleteInviteResult>(new DeleteInviteResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::DeleteInvite, calendar_backend_, invite_id,
                 delete_results),
      base::Bind(callback, delete_results));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateInvite(
    UpdateInviteRow invite,
    const InviteCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<InviteResult> update_invite_result =
      std::shared_ptr<InviteResult>(new InviteResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::UpdateInvite, calendar_backend_, invite,
                 update_invite_result),
      base::Bind(callback, update_invite_result));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateAccount(
    AccountRow account,
    const CreateAccountCallback& callback,
    base::CancelableTaskTracker* tracker) {
  std::shared_ptr<CreateAccountResult> create_account_result =
      std::shared_ptr<CreateAccountResult>(new CreateAccountResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::CreateAccount, calendar_backend_, account,
                 create_account_result),
      base::Bind(callback, create_account_result));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteAccount(
    AccountID id,
    const DeleteAccountCallback& callback,
    base::CancelableTaskTracker* tracker) {
  std::shared_ptr<DeleteAccountResult> delete_account_result =
      std::shared_ptr<DeleteAccountResult>(new DeleteAccountResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::DeleteAccount, calendar_backend_, id,
                 delete_account_result),
      base::Bind(callback, delete_account_result));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateAccount(
    AccountRow invite,
    const UpdateAccountCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<UpdateAccountResult> update_account_result =
      std::shared_ptr<UpdateAccountResult>(new UpdateAccountResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::UpdateAccount, calendar_backend_, invite,
                 update_account_result),
      base::Bind(callback, update_account_result));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllAccounts(
    const GetALLAccounsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<calendar::AccountRows> accounts =
      std::shared_ptr<calendar::AccountRows>(new calendar::AccountRows());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::GetAllAccounts, calendar_backend_, accounts),
      base::Bind(callback, accounts));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllEventTemplates(
    const QueryCalendarCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<EventQueryResults> query_results =
      std::shared_ptr<EventQueryResults>(new EventQueryResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&CalendarBackend::GetAllEventTemplates, calendar_backend_,
                 query_results),
      base::Bind(callback, query_results));
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
    backend_task_runner_->ReleaseSoon(FROM_HERE, std::move(calendar_backend_));
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

void CalendarService::OnEventCreated(const EventResult& event) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnEventCreated(this, event);
  }
}

void CalendarService::OnEventDeleted(const EventResult& event) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnEventDeleted(this, event);
  }
}

void CalendarService::OnEventChanged(const EventResult& event) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnEventChanged(this, event);
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

void CalendarService::OnEventTypeCreated(const EventTypeRow& row) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnEventTypeCreated(this, row);
  }
}

void CalendarService::OnEventTypeDeleted(const EventTypeRow& row) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnEventTypeDeleted(this, row);
  }
}

void CalendarService::OnEventTypeChanged(const EventTypeRow& row) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnEventTypeChanged(this, row);
  }
}
void CalendarService::OnNotificationChanged(const NotificationRow& row) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnNotificationChanged(this, row);
  }
}

void CalendarService::OnCalendarModified() {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnCalendarModified(this);
  }
}

}  // namespace calendar
