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

#include "base/functional/callback.h"
#include "base/i18n/string_compare.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "calendar/calendar_backend.h"
#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_type.h"
#include "calendar/event_type.h"
#include "calendar/notification_type.h"

using base::Time;

using calendar::CalendarBackend;
namespace calendar {
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
        FROM_HERE,
        base::BindOnce(&CalendarService::OnDBLoaded, calendar_service_));
  }

  void NotifyEventCreated(const EventResult& event) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CalendarService::OnEventCreated,
                                  calendar_service_, event));
  }

  void NotifyNotificationChanged(const NotificationRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CalendarService::OnNotificationChanged,
                                  calendar_service_, row));
  }

  void NotifyCalendarChanged() override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CalendarService::OnCalendarModified,
                                  calendar_service_));
  }

 private:
  const base::WeakPtr<CalendarService> calendar_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
};

CalendarService::CalendarService()
    : backend_loaded_(false), weak_ptr_factory_(this) {}

CalendarService::~CalendarService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
  Cleanup();
}

void CalendarService::Shutdown() {
  Cleanup();
}

bool CalendarService::Init(
    bool no_db,
    const CalendarDatabaseParams& calendar_database_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!backend_task_runner_);

  backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  // Create the calendar backend.
  scoped_refptr<CalendarBackend> backend(new CalendarBackend(
    new CalendarBackendDelegate(weak_ptr_factory_.GetWeakPtr(),
      base::SingleThreadTaskRunner::GetCurrentDefault()),
    backend_task_runner_));
  calendar_backend_.swap(backend);

  ScheduleTask(base::BindOnce(&CalendarBackend::Init, calendar_backend_, no_db,
                              calendar_database_params));

  return true;
}

base::CancelableTaskTracker::TaskId CalendarService::CreateCalendarEvent(
    EventRow ev,
    EventResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<EventResultCB> create_results =
      std::shared_ptr<EventResultCB>(new EventResultCB());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateCalendarEvent, calendar_backend_,
                     ev, true, create_results),
      base::BindOnce(std::move(callback), create_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateCalendarEvents(
    std::vector<calendar::EventRow> events,
    CreateEventsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<CreateEventsResult> create_results =
      std::shared_ptr<CreateEventsResult>(new CreateEventsResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateCalendarEvents, calendar_backend_,
                     events, create_results),
      base::BindOnce(std::move(callback), create_results));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateCalendarEvent(
    EventID event_id,
    EventRow event,
    EventResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<EventResultCB> update_results =
      std::shared_ptr<EventResultCB>(new EventResultCB());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateEvent, calendar_backend_, event_id,
                     event, update_results),
      base::BindOnce(std::move(callback), update_results));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteCalendarEvent(
    EventID event_id,
    DeleteEventCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<DeleteEventResult> delete_results =
      std::shared_ptr<DeleteEventResult>(new DeleteEventResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteEvent, calendar_backend_, event_id,
                     delete_results),
      base::BindOnce(std::move(callback), delete_results));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateRecurrenceException(
    RecurrenceExceptionID recurrence_id,
    RecurrenceExceptionRow recurrence,
    EventResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<EventResultCB> update_results =
      std::shared_ptr<EventResultCB>(new EventResultCB());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateRecurrenceException,
                     calendar_backend_, recurrence_id, recurrence,
                     update_results),
      base::BindOnce(std::move(callback), update_results));
}

base::CancelableTaskTracker::TaskId
CalendarService::DeleteEventRecurrenceException(
    RecurrenceExceptionID exception_id,
    EventResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<EventResultCB> delete_results =
      std::shared_ptr<EventResultCB>(new EventResultCB());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteEventRecurrenceException,
                     calendar_backend_, exception_id, delete_results),
      base::BindOnce(std::move(callback), delete_results));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllEvents(
    QueryCalendarCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<EventQueryResults> query_results =
      std::shared_ptr<EventQueryResults>(new EventQueryResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllEvents, calendar_backend_,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateCalendar(
    CalendarRow ev,
    CreateCalendarCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<CreateCalendarResult> create_results =
      std::shared_ptr<CreateCalendarResult>(new CreateCalendarResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateCalendar, calendar_backend_, ev,
                     create_results),
      base::BindOnce(std::move(callback), create_results));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllCalendars(
    GetALLQueryCalendarCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<CalendarQueryResults> query_results =
      std::shared_ptr<CalendarQueryResults>(new CalendarQueryResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllCalendars, calendar_backend_,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateCalendar(
    CalendarID calendar_id,
    Calendar calendar,
    UpdateCalendarCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<UpdateCalendarResult> update_results =
      std::shared_ptr<UpdateCalendarResult>(new UpdateCalendarResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateCalendar, calendar_backend_,
                     calendar_id, calendar, update_results),
      base::BindOnce(std::move(callback), update_results));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteCalendar(
    CalendarID calendar_id,
    DeleteCalendarCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<DeleteCalendarResult> delete_results =
      std::shared_ptr<DeleteCalendarResult>(new DeleteCalendarResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteCalendar, calendar_backend_,
                     calendar_id, delete_results),
      base::BindOnce(std::move(callback), delete_results));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateEventType(
    EventTypeID event_type_id,
    EventType ev,
    UpdateEventTypeCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<UpdateEventTypeResult> update_event_type_results =
      std::shared_ptr<UpdateEventTypeResult>(new UpdateEventTypeResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateEventType, calendar_backend_,
                     event_type_id, ev, update_event_type_results),
      base::BindOnce(std::move(callback), update_event_type_results));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllEventTypes(
    GetALLEventTypesCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<EventTypeRows> query_results =
      std::shared_ptr<EventTypeRows>(new EventTypeRows());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllEventTypes, calendar_backend_,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateEventType(
    EventTypeRow ev,
    CreateEventTypeCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<CreateEventTypeResult> create_event_type_results =
      std::shared_ptr<CreateEventTypeResult>(new CreateEventTypeResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateEventType, calendar_backend_, ev,
                     create_event_type_results),
      base::BindOnce(std::move(callback), create_event_type_results));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteEventType(
    EventTypeID event_type_id,
    DeleteEventTypeCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<DeleteEventTypeResult> delete_results =
      std::shared_ptr<DeleteEventTypeResult>(new DeleteEventTypeResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteEventType, calendar_backend_,
                     event_type_id, delete_results),
      base::BindOnce(std::move(callback), delete_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateRecurrenceException(
    RecurrenceExceptionRow ev,
    EventResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<EventResultCB> create_recurrence_exception_results =
      std::shared_ptr<EventResultCB>(new EventResultCB());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateRecurrenceException,
                     calendar_backend_, ev,
                     create_recurrence_exception_results),
      base::BindOnce(std::move(callback), create_recurrence_exception_results));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllNotifications(
    GetAllNotificationsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<GetAllNotificationResult> query_results =
      std::shared_ptr<GetAllNotificationResult>(new GetAllNotificationResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllNotifications, calendar_backend_,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateNotification(
    NotificationRow row,
    NotificationCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<NotificationResult> create_notification_result =
      std::shared_ptr<NotificationResult>(new NotificationResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateNotification, calendar_backend_,
                     row, create_notification_result),
      base::BindOnce(std::move(callback), create_notification_result));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateNotification(
    NotificationID notification_id,
    UpdateNotificationRow notification,
    NotificationCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<NotificationResult> update_notification_result =
      std::shared_ptr<NotificationResult>(new NotificationResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateNotification, calendar_backend_,
                     notification, update_notification_result),
      base::BindOnce(std::move(callback), update_notification_result));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteNotification(
    NotificationID notification_id,
    DeleteNotificationCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<DeleteNotificationResult> delete_results =
      std::shared_ptr<DeleteNotificationResult>(new DeleteNotificationResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteNotification, calendar_backend_,
                     notification_id, delete_results),
      base::BindOnce(std::move(callback), delete_results));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateInvite(
    InviteRow invite,
    InviteCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<InviteResult> create_invite_result =
      std::shared_ptr<InviteResult>(new InviteResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateInvite, calendar_backend_, invite,
                     create_invite_result),
      base::BindOnce(std::move(callback), create_invite_result));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteInvite(
    InviteID invite_id,
    DeleteInviteCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<DeleteInviteResult> delete_results =
      std::shared_ptr<DeleteInviteResult>(new DeleteInviteResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteInvite, calendar_backend_,
                     invite_id, delete_results),
      base::BindOnce(std::move(callback), delete_results));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateInvite(
    UpdateInviteRow invite,
    InviteCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<InviteResult> update_invite_result =
      std::shared_ptr<InviteResult>(new InviteResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateInvite, calendar_backend_, invite,
                     update_invite_result),
      base::BindOnce(std::move(callback), update_invite_result));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateAccount(
    AccountRow account,
    CreateAccountCallback callback,
    base::CancelableTaskTracker* tracker) {
  std::shared_ptr<CreateAccountResult> create_account_result =
      std::shared_ptr<CreateAccountResult>(new CreateAccountResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateAccount, calendar_backend_,
                     account, create_account_result),
      base::BindOnce(std::move(callback), create_account_result));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteAccount(
    AccountID id,
    DeleteAccountCallback callback,
    base::CancelableTaskTracker* tracker) {
  std::shared_ptr<DeleteAccountResult> delete_account_result =
      std::shared_ptr<DeleteAccountResult>(new DeleteAccountResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteAccount, calendar_backend_, id,
                     delete_account_result),
      base::BindOnce(std::move(callback), delete_account_result));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateAccount(
    AccountRow invite,
    UpdateAccountCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<UpdateAccountResult> update_account_result =
      std::shared_ptr<UpdateAccountResult>(new UpdateAccountResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateAccount, calendar_backend_, invite,
                     update_account_result),
      base::BindOnce(std::move(callback), update_account_result));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllAccounts(
    GetALLAccounsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<calendar::AccountRows> accounts =
      std::shared_ptr<calendar::AccountRows>(new calendar::AccountRows());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllAccounts, calendar_backend_,
                     accounts),
      base::BindOnce(std::move(callback), accounts));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllEventTemplates(
    QueryCalendarCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<EventQueryResults> query_results =
      std::shared_ptr<EventQueryResults>(new EventQueryResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllEventTemplates, calendar_backend_,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

void CalendarService::ScheduleTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_task_runner_);

  backend_task_runner_->PostTask(FROM_HERE, std::move(task));
}

void CalendarService::AddObserver(CalendarModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void CalendarService::RemoveObserver(CalendarModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void CalendarService::OnDBLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_loaded_ = true;
  NotifyCalendarServiceLoaded();
}

void CalendarService::NotifyCalendarServiceLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (CalendarModelObserver& observer : observers_)
    observer.OnCalendarServiceLoaded(this);
}

void CalendarService::Cleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_task_runner_) {
    // We've already cleaned up.
    return;
  }

  NotifyCalendarServiceBeingDeleted();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Unload the backend.
  if (calendar_backend_) {
    // Get rid of the in-memory backend.

    ScheduleTask(base::BindOnce(&CalendarBackend::Closing,
                                std::move(calendar_backend_)));
  }

  backend_task_runner_ = nullptr;
}

void CalendarService::NotifyCalendarServiceBeingDeleted() {}

void CalendarService::OnEventCreated(const EventResult& event) {
  for (CalendarModelObserver& observer : observers_) {
    observer.OnEventCreated(this, event);
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
