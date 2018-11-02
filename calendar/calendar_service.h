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
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"

#include "calendar/calendar_backend.h"
#include "calendar/calendar_database_params.h"
#include "calendar/calendar_model_observer.h"

class Profile;

namespace base {
class SequencedTaskRunner;
class FilePath;
class Thread;
}  // namespace base

namespace calendar {

struct CalendarDatabaseParams;
class CalendarBackend;

class CalendarService : public KeyedService {
 public:
  CalendarService();
  ~CalendarService() override;

  bool Init(bool no_db, const CalendarDatabaseParams& calendar_database_params);

  // Called from shutdown service before shutting down the browser
  void Shutdown() override;

  void AddObserver(CalendarModelObserver* observer);
  void RemoveObserver(CalendarModelObserver* observer);

  // Call to schedule a given task for running on the calendar thread with the
  // specified priority. The task will have ownership taken.
  void ScheduleTask(base::OnceClosure task);

  // Provides the result of a query. See QueryResults in calendar_types.h.
  // The common use will be to use QueryResults.Swap to suck the contents of
  // the results out of the passed in parameter and take ownership of them.
  typedef base::Callback<void(std::shared_ptr<EventQueryResults>)>
      QueryCalendarCallback;

  typedef base::Callback<void(std::shared_ptr<CalendarQueryResults>)>
      GetALLQueryCalendarCallback;

  // Provides the result of a event create. See CreateEventResult in
  // event_type.h.
  typedef base::Callback<void(std::shared_ptr<CreateEventResult>)>
      CreateEventCallback;

  // Provides the results of multiple event create. See CreateEventsResult in
  // event_type.h.
  typedef base::Callback<void(std::shared_ptr<CreateEventsResult>)>
      CreateEventsCallback;

  // Provides the result of a event update. See UpdateEventResult in
  // event_type.h.
  typedef base::Callback<void(std::shared_ptr<UpdateEventResult>)>
      UpdateEventCallback;

  // Provides the result of a delete calendar event. See DeleteEventResult in
  // event_type.h.
  typedef base::Callback<void(std::shared_ptr<DeleteEventResult>)>
      DeleteEventCallback;

  // Provides the result of a create calendar. See CreateCalendarResult in
  // calendar_type.h.
  typedef base::Callback<void(std::shared_ptr<CreateCalendarResult>)>
      CreateCalendarCallback;

  typedef base::Callback<void(std::shared_ptr<UpdateCalendarResult>)>
      UpdateCalendarCallback;

  typedef base::Callback<void(std::shared_ptr<DeleteCalendarResult>)>
      DeleteCalendarCallback;

  base::CancelableTaskTracker::TaskId GetAllEvents(
      const QueryCalendarCallback& callback,
      base::CancelableTaskTracker* tracker);
  // Returns true if this calendar service is currently in a mode where
  // extensive changes might happen, such as for import and sync. This is
  // helpful for observers that are created after the service has started, and
  // want to check state during their own initializer.
  bool IsDoingExtensiveChanges() const { return extensive_changes_ > 0; }

  base::CancelableTaskTracker::TaskId CreateCalendarEvent(
      EventRow ev,
      const CreateEventCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateCalendarEvents(
      std::vector<calendar::EventRow> events,
      const CreateEventsCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateCalendarEvent(
      EventID event_id,
      CalendarEvent event,
      const UpdateEventCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteCalendarEvent(
      EventID event_id,
      const DeleteEventCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateCalendar(
      CalendarRow ev,
      const CreateCalendarCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetAllCalendars(
      const GetALLQueryCalendarCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateCalendar(
      CalendarID calendar_id,
      Calendar calendar,
      const UpdateCalendarCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteCalendar(
      CalendarID event_id,
      const DeleteCalendarCallback& callback,
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

  void OnEventCreated(const EventRow& row);
  void OnEventDeleted(const EventRow& row);
  void OnEventChanged(const EventRow& row);

  void OnCalendarCreated(const CalendarRow& row);
  void OnCalendarDeleted(const CalendarRow& row);
  void OnCalendarChanged(const CalendarRow& row);

  void Cleanup();

  Profile* profile_;

  base::ThreadChecker thread_checker_;

  // The thread used by the calendar service to run CalendarBackend operations.
  // Intentionally not a BrowserThread because the sync integration unit tests
  // need to create multiple CalendarServices which each have their own thread.
  // Nullptr if TaskScheduler is used for CalendarBackend operations.
  std::unique_ptr<base::Thread> thread_;

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

  DISALLOW_COPY_AND_ASSIGN(CalendarService);
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_SERVICE_H_
