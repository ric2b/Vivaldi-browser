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
      recurrence(event.recurrence),
      updateFields(event.updateFields) {}

CalendarEvent::~CalendarEvent() {}

EventRow::EventRow() {}

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
                   EventRecurrence recurrence)
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
      recurrence_(recurrence) {}

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
  std::swap(recurrence_, other->recurrence_);
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
      recurrence_(other.recurrence_) {}

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

CreateEventResult::CreateEventResult() {}

UpdateEventResult::UpdateEventResult() {}

DeleteEventResult::DeleteEventResult() {}

}  // namespace calendar
