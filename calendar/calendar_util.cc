// Copyright (c) 2020 Vivaldi. All rights reserved.

#include "calendar_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "calendar/calendar_type.h"
#include "calendar/event_type.h"
#include "extensions/tools/vivaldi_tools.h"

namespace calendar {

using vivaldi::GetTime;

calendar::EventRow GetEventRow(
    const extensions::vivaldi::calendar::CreateDetails& event) {
  calendar::EventRow row;
  row.title = base::UTF8ToUTF16(event.title);

  if (event.description.get()) {
    row.description = base::UTF8ToUTF16(*event.description);
  }

  if (event.start.get()) {
    row.start = GetTime(*event.start.get());
  }

  if (event.end.get()) {
    row.end = GetTime(*event.end.get());
  }

  if (event.all_day.get()) {
    row.all_day = *event.all_day.get();
  }

  if (event.is_recurring.get()) {
    row.is_recurring = *event.is_recurring.get();
  }

  if (event.start_recurring.get()) {
    row.start_recurring = GetTime(*event.start_recurring.get());
  }

  if (event.end_recurring.get()) {
    row.end_recurring = GetTime(*event.end_recurring.get());
  }

  if (event.location.get()) {
    row.location = base::UTF8ToUTF16(*event.location);
  }

  if (event.url.get()) {
    row.url = base::UTF8ToUTF16(*event.url);
  }

  if (event.etag.get()) {
    row.etag = *event.etag;
  }

  if (event.href.get()) {
    row.href = *event.href;
  }

  if (event.uid.get()) {
    row.uid = *event.uid;
  }

  calendar::CalendarID calendar_id;
  if (GetStdStringAsInt64(event.calendar_id, &calendar_id)) {
    row.calendar_id = calendar_id;
  }

  if (event.task.get()) {
    row.task = *event.task;
  }

  if (event.complete.get()) {
    row.complete = *event.complete;
  }

  if (event.sequence.get()) {
    row.sequence = *event.sequence;
  }

  if (event.ical.get()) {
    row.ical = base::UTF8ToUTF16(*event.ical);
  }

  if (event.rrule.get()) {
    row.rrule = *event.rrule;
  }

  if (event.organizer.get()) {
    row.organizer = *event.organizer;
  }

  if (event.event_type_id.get()) {
    calendar::EventTypeID event_type_id;
    if (GetStdStringAsInt64(*event.event_type_id, &event_type_id)) {
      row.event_type_id = event_type_id;
    }
  }

  if (event.event_exceptions.get()) {
    std::vector<EventExceptionType> event_exceptions;
    for (const auto& exception : *event.event_exceptions) {
      event_exceptions.push_back(CreateEventException(exception));
    }
    row.event_exceptions = event_exceptions;
  }

  if (event.notifications.get()) {
    std::vector<NotificationToCreate> event_noficications;
    for (const auto& notification : *event.notifications) {
      event_noficications.push_back(CreateNotificationRow(notification));
    }
    row.notifications_to_create = event_noficications;
  }

  if (event.invites.get()) {
    std::vector<InviteToCreate> event_invites;
    for (const auto& invite : *event.invites) {
      event_invites.push_back(CreateInviteRow(invite));
    }
    row.invites_to_create = event_invites;
  }

  if (event.timezone.get()) {
    row.timezone = *event.timezone;
  }

  row.is_template = event.is_template;

  if (event.due.get()) {
    row.due = GetTime(*event.due.get());
  }

  if (event.priority.get()) {
    row.priority = *event.priority;
  }

  if (event.status.get()) {
    row.status = *event.status;
  }

  if (event.percentage_complete.get()) {
    row.percentage_complete = *event.percentage_complete;
  }

  if (event.categories.get()) {
    row.categories = base::UTF8ToUTF16(*event.categories);
  }

  if (event.component_class.get()) {
    row.component_class = base::UTF8ToUTF16(*event.component_class);
  }

  if (event.attachment.get()) {
    row.attachment = base::UTF8ToUTF16(*event.attachment);
  }

  if (event.completed.get()) {
    row.completed = GetTime(*event.completed.get());
  }

  return row;
}

bool GetIdAsInt64(const std::u16string& id_string, int64_t* id) {
  if (base::StringToInt64(id_string, id))
    return true;

  return false;
}

bool GetStdStringAsInt64(const std::string& id_string, int64_t* id) {
  if (base::StringToInt64(id_string, id))
    return true;

  return false;
}

EventExceptionType CreateEventException(
    const extensions::vivaldi::calendar::CreateDetails::EventExceptionsType&
        exception) {
  EventExceptionType exception_event;

  if (exception.exception.exception_date.get()) {
    exception_event.exception_date =
        GetTime(*exception.exception.exception_date);
  }

  if (exception.exception.cancelled.get()) {
    exception_event.cancelled = *exception.exception.cancelled;
  }

  if (exception.exception_event.get()) {
    exception_event.event.reset(
        new EventRow(calendar::GetEventRow(*exception.exception_event.get())));
  }

  return exception_event;
}

NotificationToCreate CreateNotificationRow(
    const extensions::vivaldi::calendar::CreateNotificationRow& notification) {
  NotificationToCreate notification_create;

  notification_create.name = base::UTF8ToUTF16(notification.name);
  notification_create.when = GetTime(notification.when);
  return notification_create;
}

InviteToCreate CreateInviteRow(
    const extensions::vivaldi::calendar::CreateInviteRow& invite) {
  InviteToCreate invite_create;
  invite_create.name = base::UTF8ToUTF16(invite.name);
  invite_create.address = base::UTF8ToUTF16(invite.address);
  invite_create.partstat = invite.partstat;

  return invite_create;
}

}  // namespace calendar
