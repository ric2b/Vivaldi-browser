// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/calendar_type.h"

#include <limits>
#include "base/stl_util.h"

namespace calendar {

CreateCalendarResult::CreateCalendarResult() {}

CalendarRow::CalendarRow() {}
CalendarRow::~CalendarRow() {}

CalendarRow::CalendarRow(CalendarID id,
                         base::string16 name,
                         base::string16 description,
                         GURL url,
                         std::string ctag,
                         int orderindex,
                         std::string color,
                         bool hidden,
                         base::Time created,
                         base::Time lastmodified)
    : name_(name),
      description_(description),
      url_(url),
      ctag_(ctag),
      orderindex_(orderindex),
      color_(color),
      hidden_(hidden),
      created_(created),
      lastmodified_(lastmodified) {}

void CalendarRow::Swap(CalendarRow* other) {
  std::swap(id_, other->id_);
  std::swap(name_, other->name_);
  std::swap(description_, other->description_);
  std::swap(url_, other->url_);
  std::swap(orderindex_, other->orderindex_);
  std::swap(color_, other->color_);
  std::swap(hidden_, other->hidden_);
}

CalendarRow::CalendarRow(const CalendarRow& other)
    : id_(other.id_),
      name_(other.name_),
      description_(other.description_),
      url_(other.url_),
      ctag_(other.ctag_),
      orderindex_(other.orderindex_),
      color_(other.color_),
      hidden_(other.hidden_),
      created_(other.created_),
      lastmodified_(other.lastmodified_) {}

Calendar::Calendar()
    : name(base::ASCIIToUTF16("")),
      description(base::ASCIIToUTF16("")),
      orderindex(0),
      updateFields(0) {}

Calendar::Calendar(const Calendar& calendar)
    : id(calendar.id),
      name(calendar.name),
      description(calendar.description),
      url(calendar.url),
      ctag(calendar.ctag),
      orderindex(calendar.orderindex),
      color(calendar.color),
      hidden(calendar.hidden),
      updateFields(calendar.updateFields) {}

Calendar::~Calendar() {}

CalendarResult::CalendarResult() {}
CalendarResult::CalendarResult(const CalendarResult&) = default;
CalendarResult::~CalendarResult() {}

CalendarResult::CalendarResult(const CalendarRow& calendar_row)
    : CalendarRow(calendar_row) {}

void CalendarResult::SwapResult(CalendarResult* other) {
  CalendarRow::Swap(other);
}

CalendarQueryResults::CalendarQueryResults() {}
CalendarQueryResults::~CalendarQueryResults() {}

void CalendarQueryResults::AppendCalendarBySwapping(CalendarResult* result) {
  results_.push_back(std::move(*result));
}

UpdateCalendarResult::UpdateCalendarResult() {}

DeleteCalendarResult::DeleteCalendarResult() {}
}  // namespace calendar
