// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar_types.h"

#include <limits>
#include "base/stl_util.h"

namespace calendar {

CalendarEvent::CalendarEvent()
    : title(base::ASCIIToUTF16("")),
      description(base::ASCIIToUTF16("")),
      updateFields(0) {}

CalendarEvent::CalendarEvent(const CalendarEvent& event)
    : calendar_id(event.calendar_id),
      title(event.title),
      description(event.description),
      start(event.start),
      end(event.end),
      updateFields(event.updateFields) {}

EventRow::EventRow() {}

EventRow::EventRow(base::string16 id,
                   base::string16 calendar_id,
                   base::string16 title,
                   base::string16 description,
                   base::Time start,
                   base::Time end)
    : id_(id),
      calendar_id_(calendar_id),
      title_(title),
      description_(description),
      start_(start),
      end_(end) {}

EventRow::~EventRow() {}

void EventRow::Swap(EventRow* other) {
  std::swap(title_, other->title_);
  std::swap(description_, other->description_);
}

EventRow::EventRow(const EventRow& other)
    : id_(other.id_),
      calendar_id_(other.calendar_id_),
      title_(other.title_),
      description_(other.description_),
      start_(other.start_),
      end_(other.end_) {}

EventResult::EventResult() {}

void EventResult::SwapResult(EventResult* other) {
  EventRow::Swap(other);
  std::swap(id_, other->id_);
  std::swap(calendar_id_, other->calendar_id_);
  std::swap(title_, other->title_);
  std::swap(description_, other->description_);
  std::swap(start_, other->start_);
  std::swap(end_, other->end_);
}

EventQueryResults::~EventQueryResults() {}

EventQueryResults::EventQueryResults() {}

void EventQueryResults::AppendEventBySwapping(EventResult* result) {
  EventResult* new_result = new EventResult;
  new_result->SwapResult(result);

  results_.push_back(new_result);
}

EventResult::~EventResult() {}

UpdateEventResult::UpdateEventResult() {}

}  // namespace calendar
