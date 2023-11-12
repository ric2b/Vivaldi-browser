// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_TYPE_H_
#define CALENDAR_CALENDAR_TYPE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "calendar/calendar_typedefs.h"

namespace calendar {

// Bit flags determing which fields should be updated in the
// UpdateCalendar API method
enum UpdateCalendarFields {
  CALENDAR_NAME = 1 << 0,
  CALENDAR_DESCRIPTION = 1 << 1,
  CALENDAR_CTAG = 1 << 2,
  CALENDAR_ORDERINDEX = 1 << 3,
  CALENDAR_COLOR = 1 << 4,
  CALENDAR_HIDDEN = 1 << 5,
  CALENDAR_ACTIVE = 1 << 6,
  CALENDAR_ICONINDEX = 1 << 7,
  CALENDAR_LAST_CHECKED = 1 << 8,
  CALENDAR_TIMEZONE = 1 << 9,
  CALENDAR_SUPPORTED_COMPONENT_SET = 1 << 10,
};

// Bit flags determing which calendar component set are supported
enum SupportedCalendarComponentSet {
  NONE = 0,
  CALENDAR_VEVENT = 1 << 0,
  CALENDAR_VTODO = 1 << 1,
  CALENDAR_VJOURNAL = 1 << 2,
};

// Holds all information associated with a specific Calendar.
class CalendarRow {
 public:
  CalendarRow();
  CalendarRow(CalendarID id,
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
              base::Time lastmodified);
  ~CalendarRow();

  CalendarRow(const CalendarRow& row);

  CalendarID id() const { return id_; }
  void set_id(CalendarID id) { id_ = id; }

  AccountID account_id() const { return account_id_; }
  void set_account_id(AccountID account_id) { account_id_ = account_id; }

  std::u16string name() const { return name_; }
  void set_name(std::u16string name) { name_ = name; }

  std::u16string description() const { return description_; }
  void set_description(std::u16string description) {
    description_ = description;
  }

  std::string ctag() const { return ctag_; }
  void set_ctag(std::string ctag) { ctag_ = ctag; }

  int orderindex() const { return orderindex_; }
  void set_orderindex(int orderindex) { orderindex_ = orderindex; }

  std::string color() const { return color_; }
  void set_color(std::string color) { color_ = color; }

  bool hidden() const { return hidden_; }
  void set_hidden(bool hidden) { hidden_ = hidden; }

  bool active() const { return active_; }
  void set_active(bool active) { active_ = active; }

  int iconindex() const { return iconindex_; }
  void set_iconindex(int iconindex) { iconindex_ = iconindex; }

  base::Time last_checked() const { return last_checked_; }
  void set_last_checked(base::Time last_checked) {
    last_checked_ = last_checked;
  }

  std::string timezone() const { return timezone_; }
  void set_timezone(std::string timezone) { timezone_ = timezone; }

  uint16_t supported_component_set() const { return supported_component_set_; }
  void set_supported_component_set(uint16_t supported_component_set) {
    supported_component_set_ = supported_component_set;
  }

  base::Time created() const { return created_; }
  void set_created(base::Time created) { created_ = created; }

  base::Time modified() const { return lastmodified_; }
  void set_modified(base::Time lastmodified) { lastmodified_ = lastmodified; }

 protected:
  void Swap(CalendarRow* other);

  CalendarID id_;
  AccountID account_id_;
  std::u16string name_;
  std::u16string description_;
  std::string ctag_;
  int orderindex_;
  std::string color_;
  bool hidden_;
  bool active_;
  int iconindex_;
  base::Time last_checked_;
  std::string timezone_;
  uint16_t supported_component_set_;
  base::Time created_;
  base::Time lastmodified_;
};

typedef std::vector<CalendarRow> CalendarRows;

class CalendarResult : public CalendarRow {
 public:
  CalendarResult();
  CalendarResult(const CalendarResult&);
  ~CalendarResult();
  CalendarResult(const CalendarRow& calendar_row);
  void SwapResult(CalendarResult* other);
};

typedef std::vector<CalendarRow> CalendarRows;
typedef std::vector<CalendarID> CalendarIDs;

// Represents a simplified version of a calendar.
struct Calendar {
  Calendar();
  ~Calendar();
  Calendar(const Calendar& calendar);

  CalendarID id;
  AccountID account_id;
  std::u16string name;
  std::u16string description;
  std::string ctag;
  int orderindex;
  std::string color;
  bool hidden;
  bool active;
  int iconindex;
  base::Time last_checked;
  std::string timezone;
  uint16_t supported_component_set;
  base::Time created;
  base::Time lastmodified;
  int updateFields;
};

class CreateCalendarResult {
 public:
  CreateCalendarResult();
  CreateCalendarResult(const CreateCalendarResult& calendar) = default;
  CreateCalendarResult& operator=(CreateCalendarResult& calendar) = default;

  bool success;
  CalendarRow createdRow;
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_TYPE_H_
