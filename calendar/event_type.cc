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

EventRow::EventRow() {
  sequence = 0;
  description = u"";
  all_day = false;
  trash = false;
  is_recurring = false;
  task = false;
  complete = false;
  is_template = false;
  priority = 0;
  percentage_complete = 0;
  sync_pending = false;
  delete_pending = false;
  updateFields = 0;
}

EventRow::~EventRow() {}

void EventRow::Swap(EventRow* other) {
  std::swap(id, other->id);
  std::swap(calendar_id, other->calendar_id);
  std::swap(title, other->title);
  std::swap(description, other->description);
  std::swap(start, other->start);
  std::swap(end, other->end);
  std::swap(all_day, other->all_day);
  std::swap(is_recurring, other->is_recurring);
  std::swap(location, other->location);
  std::swap(url, other->url);
  std::swap(recurrence_exceptions, other->recurrence_exceptions);
  std::swap(etag, other->etag);
  std::swap(href, other->href);
  std::swap(uid, other->uid);
  std::swap(event_type_id, other->event_type_id);
  std::swap(task, other->task);
  std::swap(complete, other->complete);
  std::swap(trash, other->trash);
  std::swap(trash_time, other->trash_time);
  std::swap(sequence, other->sequence);
  std::swap(ical, other->ical);
  std::swap(event_exceptions, other->event_exceptions);
  std::swap(rrule, other->rrule);
  std::swap(notifications, other->notifications);
  std::swap(invites, other->invites);
  std::swap(organizer, other->organizer);
  std::swap(notifications_to_create, other->notifications_to_create);
  std::swap(timezone, other->timezone);
  std::swap(is_template, other->is_template);
  std::swap(priority, other->priority);
  std::swap(status, other->status);
  std::swap(percentage_complete, other->percentage_complete);
  std::swap(categories, other->categories);
  std::swap(component_class, other->component_class);
  std::swap(attachment, other->attachment);
  std::swap(completed, other->completed);
  std::swap(sync_pending, other->sync_pending);
  std::swap(delete_pending, other->delete_pending);
  std::swap(end_recurring, other->end_recurring);
  std::swap(updateFields, other->updateFields);
}

EventRow::EventRow(const EventRow& other) = default;

EventRow& EventRow::operator=(const EventRow& other) = default;

EventRow::EventRow(const EventRow&& other) noexcept
    : id(other.id),
      calendar_id(other.calendar_id),
      title(other.title),
      description(other.description),
      start(other.start),
      end(other.end),
      all_day(other.all_day),
      is_recurring(other.is_recurring),
      location(other.location),
      url(other.url),
      recurrence_exceptions(other.recurrence_exceptions),
      etag(other.etag),
      href(other.href),
      uid(other.uid),
      event_type_id(other.event_type_id),
      task(other.task),
      complete(other.complete),
      trash(other.trash),
      trash_time(other.trash_time),
      sequence(other.sequence),
      ical(other.ical),
      event_exceptions(other.event_exceptions),
      rrule(other.rrule),
      notifications(other.notifications),
      invites(other.invites),
      organizer(other.organizer),
      notifications_to_create(other.notifications_to_create),
      invites_to_create(other.invites_to_create),
      timezone(other.timezone),
      is_template(other.is_template),
      priority(other.priority),
      percentage_complete(other.percentage_complete),
      sync_pending(other.sync_pending),
      delete_pending(other.delete_pending),
      end_recurring(other.end_recurring),
      updateFields(other.updateFields) {}

EventResult::EventResult() {}
EventResult::~EventResult() {}
EventResult::EventResult(const EventRow& event_row) : EventRow(event_row) {}

EventType::EventType() : name(u""), color(""), iconindex(0), updateFields(0) {}

EventType::EventType(const EventType& event_type)
    : event_type_id(event_type.event_type_id),
      name(event_type.name),
      color(event_type.color),
      iconindex(event_type.iconindex),
      updateFields(event_type.updateFields) {}

EventType::~EventType() {}

}  // namespace calendar
