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
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace calendar {

typedef int64_t CalendarID;
// Bit flags determing which fields should be updated in the
// UpdateCalendar API method
enum UpdateCalendarFields {
  CALENDAR_NAME = 1 << 0,
  CALENDAR_DESCRIPTION = 1 << 1,
  CALENDAR_URL = 1 << 2,
  CALENDAR_CTAG = 1 << 3,
  CALENDAR_ORDERINDEX = 1 << 4,
  CALENDAR_COLOR = 1 << 5,
  CALENDAR_HIDDEN = 1 << 6,
};

// Holds all information associated with a specific Calendar.
class CalendarRow {
 public:
  CalendarRow();
  CalendarRow(CalendarID id,
              base::string16 name,
              base::string16 description,
              GURL url,
              std::string ctag,
              int orderindex,
              std::string color,
              bool hidden,
              base::Time created,
              base::Time lastmodified);
  ~CalendarRow();

  CalendarRow(const CalendarRow& row);

  CalendarID id() const { return id_; }
  void set_id(CalendarID id) { id_ = id; }

  base::string16 name() const { return name_; }
  void set_name(base::string16 name) { name_ = name; }

  base::string16 description() const { return description_; }
  void set_description(base::string16 description) {
    description_ = description;
  }

  GURL url() const { return url_; }
  void set_url(GURL url) { url_ = url; }

  std::string ctag() const { return ctag_; }
  void set_ctag(std::string ctag) { ctag_ = ctag; }

  int orderindex() const { return orderindex_; }
  void set_orderindex(int orderindex) { orderindex_ = orderindex; }

  std::string color() const { return color_; }
  void set_color(std::string color) { color_ = color; }

  bool hidden() const { return hidden_; }
  void set_hidden(bool hidden) { hidden_ = hidden; }

  base::Time created() const { return created_; }
  void set_created(base::Time created) { created_ = created; }

  base::Time modified() const { return lastmodified_; }
  void set_modified(base::Time lastmodified) { lastmodified_ = lastmodified; }

 protected:
  void Swap(CalendarRow* other);

  CalendarID id_;
  base::string16 name_;
  base::string16 description_;
  GURL url_;
  std::string ctag_;
  int orderindex_;
  std::string color_;
  bool hidden_;
  base::Time created_;
  base::Time lastmodified_;
};

class CalendarResult : public CalendarRow {
 public:
  CalendarResult();
  CalendarResult(const CalendarResult&);
  ~CalendarResult();
  CalendarResult(const CalendarRow& calendar_row);
  void SwapResult(CalendarResult* other);
};

typedef std::vector<CalendarRow> CalendarRows;

// Represents a simplified version of a calendar.
struct Calendar {
  Calendar();
  ~Calendar();
  Calendar(const Calendar& calendar);

  CalendarID id;
  base::string16 name;
  base::string16 description;
  GURL url;
  base::string16 ctag;
  int orderindex;
  std::string color;
  bool hidden;
  base::Time created;
  base::Time lastmodified;
  int updateFields;
};

class CalendarQueryResults {
 public:
  typedef std::vector<CalendarResult> CalendarResultVector;

  CalendarQueryResults();
  ~CalendarQueryResults();

  size_t size() const { return results_.size(); }
  bool empty() const { return results_.empty(); }

  CalendarResult& back() { return results_.back(); }
  const CalendarResult& back() const { return results_.back(); }

  CalendarResult& operator[](size_t i) { return results_[i]; }
  const CalendarResult& operator[](size_t i) const { return results_[i]; }

  CalendarResultVector::const_iterator begin() const {
    return results_.begin();
  }
  CalendarResultVector::const_iterator end() const { return results_.end(); }
  CalendarResultVector::const_reverse_iterator rbegin() const {
    return results_.rbegin();
  }
  CalendarResultVector::const_reverse_iterator rend() const {
    return results_.rend();
  }

  // Swaps the current result with another. This allows ownership to be
  // efficiently transferred without copying.
  void Swap(CalendarQueryResults* other);

  // Adds the given result to the map, using swap() on the members to avoid
  // copying (there are a lot of strings and vectors). This means the parameter
  // object will be cleared after this call.
  void AppendCalendarBySwapping(CalendarResult* result);

 private:
  // The ordered list of results. The pointers inside this are owned by this
  // CalendarQueryResults object.
  std::vector<CalendarResult> results_;

  DISALLOW_COPY_AND_ASSIGN(CalendarQueryResults);
};

class CreateCalendarResult {
 public:
  CreateCalendarResult();
  bool success;
  CalendarRow createdRow;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateCalendarResult);
};

class UpdateCalendarResult {
 public:
  UpdateCalendarResult();
  bool success;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateCalendarResult);
};

class DeleteCalendarResult {
 public:
  DeleteCalendarResult();
  bool success;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteCalendarResult);
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_TYPE_H_
