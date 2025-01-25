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
      new CalendarBackendDelegate(
          weak_ptr_factory_.GetWeakPtr(),
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

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateCalendarEvent, calendar_backend_,
                     ev, true),
      base::BindOnce(std::move(callback)));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateCalendarEvents(
    std::vector<calendar::EventRow> events,
    CreateEventsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateCalendarEvents, calendar_backend_,
                     events),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateCalendarEvent(
    EventID event_id,
    EventRow event,
    EventResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateEvent, calendar_backend_, event_id,
                     event),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteCalendarEvent(
    EventID event_id,
    base::OnceCallback<void(bool)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteEvent, calendar_backend_,
                     event_id),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateRecurrenceException(
    RecurrenceExceptionID recurrence_id,
    RecurrenceExceptionRow recurrence,
    EventResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateRecurrenceException,
                     calendar_backend_, recurrence_id, recurrence),
      base::BindOnce(std::move(callback)));
}

base::CancelableTaskTracker::TaskId
CalendarService::DeleteEventRecurrenceException(
    RecurrenceExceptionID exception_id,
    EventResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteEventRecurrenceException,
                     calendar_backend_, exception_id),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllEvents(
    base::OnceCallback<void(EventRows)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllEvents, calendar_backend_),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateCalendar(
    CalendarRow ev,
    CreateCalendarCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateCalendar, calendar_backend_, ev),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllCalendars(
    GetALLQueryCalendarCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllCalendars, calendar_backend_),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateCalendar(
    CalendarID calendar_id,
    Calendar calendar,
    base::OnceCallback<void(StatusCB)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateCalendar, calendar_backend_,
                     calendar_id, calendar),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteCalendar(
    CalendarID calendar_id,
    base::OnceCallback<void(bool)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteCalendar, calendar_backend_,
                     calendar_id),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateEventType(
    EventTypeID event_type_id,
    EventType ev,
    base::OnceCallback<void(bool)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateEventType, calendar_backend_,
                     event_type_id, ev),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllEventTypes(
    GetALLEventTypesCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllEventTypes, calendar_backend_),
      (std::move(callback)));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateEventType(
    EventTypeRow ev,
    base::OnceCallback<void(bool)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateEventType, calendar_backend_, ev),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteEventType(
    EventTypeID event_type_id,
    DeleteEventTypeCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteEventType, calendar_backend_,
                     event_type_id),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateRecurrenceException(
    RecurrenceExceptionRow ev,
    EventResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateRecurrenceException,
                     calendar_backend_, ev),
      base::BindOnce(std::move(callback)));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllNotifications(
    GetAllNotificationsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllNotifications, calendar_backend_),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateNotification(
    NotificationRow row,
    NotificationCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateNotification, calendar_backend_,
                     row),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateNotification(
    NotificationID notification_id,
    UpdateNotificationRow notification,
    NotificationCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateNotification, calendar_backend_,
                     notification),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteNotification(
    NotificationID notification_id,
    base::OnceCallback<void(bool)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteNotification, calendar_backend_,
                     notification_id),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateInvite(
    InviteRow invite,
    InviteCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateInvite, calendar_backend_, invite),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteInvite(
    InviteID invite_id,
    base::OnceCallback<void(bool)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteInvite, calendar_backend_,
                     invite_id),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateInvite(
    UpdateInviteRow invite,
    InviteCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateInvite, calendar_backend_, invite),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateAccount(
    AccountRow account,
    CreateAccountCallback callback,
    base::CancelableTaskTracker* tracker) {
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateAccount, calendar_backend_,
                     account),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteAccount(
    AccountID id,
    DeleteAccountCallback callback,
    base::CancelableTaskTracker* tracker) {
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteAccount, calendar_backend_, id),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateAccount(
    AccountRow invite,
    UpdateAccountCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateAccount, calendar_backend_,
                     invite),
      base::BindOnce(std::move(callback)));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllAccounts(
    GetALLAccounsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllAccounts, calendar_backend_),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::CreateEventTemplate(
    EventTemplateRow event_template,
    EventTemplateResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::CreateEventTemplate, calendar_backend_,
                     event_template),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::GetAllEventTemplates(
    base::OnceCallback<void(calendar::EventTemplateRows)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetAllEventTemplates, calendar_backend_),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::UpdateEventTemplate(
    EventTemplateID event_template_id,
    EventTemplateRow event_template,
    EventTemplateResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::UpdateEventTemplate, calendar_backend_,
                     event_template_id, event_template),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::DeleteEventTemplate(
    EventTemplateID event_template_id,
    base::OnceCallback<void(bool)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::DeleteEventTemplate, calendar_backend_,
                     event_template_id),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId CalendarService::GetParentExceptionEventId(
    calendar::EventID exception_event_id,
    base::OnceCallback<void(calendar::EventID)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Calendar service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CalendarBackend::GetParentExceptionEventId,
                     calendar_backend_, exception_event_id),
      std::move(callback));
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
