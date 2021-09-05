// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/event_type.h"

#include <limits>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "calendar_type.h"

namespace calendar {

CalendarEvent::CalendarEvent()
    : title(base::ASCIIToUTF16("")),
      description(base::ASCIIToUTF16("")),
      all_day(false),
      trash(false),
      sequence(0),
      updateFields(0) {}

CalendarEvent::CalendarEvent(const CalendarEvent& event)
    : calendar_id(event.calendar_id),
      title(event.title),
      description(event.description),
      start(event.start),
      end(event.end),
      all_day(event.all_day),
      is_recurring(event.is_recurring),
      start_recurring(event.start_recurring),
      end_recurring(event.end_recurring),
      location(event.location),
      url(event.url),
      etag(event.etag),
      href(event.href),
      uid(event.uid),
      event_type_id(event.event_type_id),
      task(event.task),
      complete(event.complete),
      trash(event.trash),
      trash_time(event.trash_time),
      sequence(event.sequence),
      ical(event.ical),
      rrule(event.rrule),
      organizer(event.organizer),
      timezone(event.timezone),
      updateFields(event.updateFields) {}

CalendarEvent::~CalendarEvent() {}

EventRow::EventRow() {
  sequence_ = 0;
  description_ = base::ASCIIToUTF16("");
  all_day_ = false;
  trash_ = false;
  is_recurring_ = false;
  task_ = false;
  complete_ = false;
  is_template_ = false;
}

EventRow::EventRow(EventID id,
                   CalendarID calendar_id,
                   AlarmID alarm_id,
                   base::string16 title,
                   base::string16 description,
                   base::Time start,
                   base::Time end,
                   bool all_day,
                   bool is_recurring,
                   base::Time start_recurring,
                   base::Time end_recurring,
                   base::string16 location,
                   base::string16 url,
                   std::string etag,
                   std::string href,
                   std::string uid,
                   EventTypeID event_type_id,
                   bool task,
                   bool complete,
                   bool trash,
                   base::Time trash_time,
                   int sequence,
                   base::string16 ical,
                   std::string rrule,
                   std::string organizer,
                   NotificationsToCreate notifications_to_create,
                   InvitesToCreate invites_to_create,
                   std::string timezone)
    : id_(id),
      calendar_id_(calendar_id),
      alarm_id_(alarm_id),
      title_(title),
      description_(description),
      start_(start),
      end_(end),
      all_day_(all_day),
      is_recurring_(is_recurring),
      start_recurring_(start_recurring),
      end_recurring_(end_recurring),
      location_(location),
      url_(url),
      etag_(etag),
      href_(href),
      uid_(uid),
      event_type_id_(event_type_id),
      task_(task),
      complete_(complete),
      trash_(trash),
      trash_time_(trash_time),
      sequence_(sequence),
      ical_(ical),
      rrule_(rrule),
      organizer_(organizer),
      notifications_to_create_(notifications_to_create),
      invites_to_create_(invites_to_create),
      timezone_(timezone) {}

EventRow::~EventRow() {}

void EventRow::Swap(EventRow* other) {
  std::swap(id_, other->id_);
  std::swap(calendar_id_, other->calendar_id_);
  std::swap(title_, other->title_);
  std::swap(description_, other->description_);
  std::swap(start_, other->start_);
  std::swap(end_, other->end_);
  std::swap(all_day_, other->all_day_);
  std::swap(is_recurring_, other->is_recurring_);
  std::swap(start_recurring_, other->start_recurring_);
  std::swap(end_recurring_, other->end_recurring_);
  std::swap(location_, other->location_);
  std::swap(url_, other->url_);
  std::swap(recurrence_exceptions_, other->recurrence_exceptions_);
  std::swap(etag_, other->etag_);
  std::swap(href_, other->href_);
  std::swap(uid_, other->uid_);
  std::swap(event_type_id_, other->event_type_id_);
  std::swap(task_, other->task_);
  std::swap(complete_, other->complete_);
  std::swap(trash_, other->trash_);
  std::swap(trash_time_, other->trash_time_);
  std::swap(sequence_, other->sequence_);
  std::swap(ical_, other->ical_);
  std::swap(event_exceptions_, other->event_exceptions_);
  std::swap(rrule_, other->rrule_);
  std::swap(notifications_, other->notifications_);
  std::swap(invites_, other->invites_);
  std::swap(organizer_, other->organizer_);
  std::swap(notifications_to_create_, other->notifications_to_create_);
  std::swap(timezone_, other->timezone_);
  std::swap(is_template_, other->is_template_);
}

EventRow::EventRow(const EventRow& other) = default;

EventRow& EventRow::operator=(const EventRow& other) = default;

EventRow::EventRow(const EventRow&& other) noexcept
    : id_(other.id_),
      calendar_id_(other.calendar_id_),
      title_(other.title_),
      description_(other.description_),
      start_(other.start_),
      end_(other.end_),
      all_day_(other.all_day_),
      is_recurring_(other.is_recurring_),
      start_recurring_(other.start_recurring_),
      end_recurring_(other.end_recurring_),
      location_(other.location_),
      url_(other.url_),
      recurrence_exceptions_(other.recurrence_exceptions_),
      etag_(other.etag_),
      href_(other.href_),
      uid_(other.uid_),
      event_type_id_(other.event_type_id_),
      task_(other.task_),
      complete_(other.complete_),
      trash_(other.trash_),
      trash_time_(other.trash_time_),
      sequence_(other.sequence_),
      ical_(other.ical_),
      event_exceptions_(other.event_exceptions_),
      rrule_(other.rrule_),
      notifications_(other.notifications_),
      invites_(other.invites_),
      organizer_(other.organizer_),
      notifications_to_create_(other.notifications_to_create_),
      invites_to_create_(other.invites_to_create_),
      timezone_(other.timezone_),
      is_template_(other.is_template_) {}

EventResult::EventResult() {}

EventResult::EventResult(const EventRow& event_row) : EventRow(event_row) {}

void EventResult::SwapResult(EventResult* other) {
  EventRow::Swap(other);
}

EventQueryResults::~EventQueryResults() {}

EventQueryResults::EventQueryResults() {}

void EventQueryResults::AppendEventBySwapping(EventResult* result) {
  std::unique_ptr<EventResult> new_result = base::WrapUnique(new EventResult);
  new_result->SwapResult(result);

  results_.push_back(std::move(new_result));
}

EventResult::~EventResult() {}

UpdateEventResult::UpdateEventResult() {}

DeleteEventResult::DeleteEventResult() {}

UpdateEventTypeResult::UpdateEventTypeResult() {}

EventType::EventType()
    : name(base::ASCIIToUTF16("")), color(""), iconindex(0), updateFields(0) {}

EventType::EventType(const EventType& event_type)
    : event_type_id(event_type.event_type_id),
      name(event_type.name),
      color(event_type.color),
      iconindex(event_type.iconindex),
      updateFields(event_type.updateFields) {}

EventType::~EventType() {}

}  // namespace calendar
