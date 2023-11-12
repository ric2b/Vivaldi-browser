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

CalendarRow::CalendarRow()
    : orderindex_(0),
      hidden_(0),
      active_(0),
      iconindex_(0),
      supported_component_set_(CALENDAR_VEVENT) {}

CalendarRow::~CalendarRow() {}

CalendarRow::CalendarRow(CalendarID id,
                         AccountID account_id,
                         std::u16string name,
                         std::u16string description,
                         std::string ctag,
                         int orderindex,
                         std::string color,
                         bool hidden,
                         bool active,
                         int iconindex,
                         base::Time last_checked,
                         std::string timezone,
                         uint16_t supported_component_set,
                         base::Time created,
                         base::Time lastmodified)
    : account_id_(account_id),
      name_(name),
      description_(description),
      ctag_(ctag),
      orderindex_(orderindex),
      color_(color),
      hidden_(hidden),
      active_(active),
      iconindex_(iconindex),
      last_checked_(last_checked),
      timezone_(timezone),
      supported_component_set_(supported_component_set),
      created_(created),
      lastmodified_(lastmodified) {}

void CalendarRow::Swap(CalendarRow* other) {
  std::swap(id_, other->id_);
  std::swap(account_id_, other->account_id_);
  std::swap(name_, other->name_);
  std::swap(description_, other->description_);
  std::swap(orderindex_, other->orderindex_);
  std::swap(color_, other->color_);
  std::swap(hidden_, other->hidden_);
  std::swap(active_, other->active_);
  std::swap(iconindex_, other->iconindex_);
  std::swap(last_checked_, other->last_checked_);
  std::swap(timezone_, other->timezone_);
  std::swap(supported_component_set_, other->supported_component_set_);
}

CalendarRow::CalendarRow(const CalendarRow& other)
    : id_(other.id_),
      account_id_(other.account_id_),
      name_(other.name_),
      description_(other.description_),
      ctag_(other.ctag_),
      orderindex_(other.orderindex_),
      color_(other.color_),
      hidden_(other.hidden_),
      active_(other.active_),
      iconindex_(other.iconindex_),
      last_checked_(other.last_checked_),
      timezone_(other.timezone_),
      supported_component_set_(other.supported_component_set_),
      created_(other.created_),
      lastmodified_(other.lastmodified_) {}

Calendar::Calendar()
    : name(u""),
      description(u""),
      supported_component_set(0),
      updateFields(0) {}

Calendar::Calendar(const Calendar& calendar)
    : id(calendar.id),
      account_id(calendar.account_id),
      name(calendar.name),
      description(calendar.description),
      ctag(calendar.ctag),
      orderindex(calendar.orderindex),
      color(calendar.color),
      hidden(calendar.hidden),
      active(calendar.active),
      iconindex(calendar.iconindex),
      last_checked(calendar.last_checked),
      timezone(calendar.timezone),
      supported_component_set(calendar.supported_component_set),
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

}  // namespace calendar
