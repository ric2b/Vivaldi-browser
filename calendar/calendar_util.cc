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

  if (event.description.has_value()) {
    row.description = base::UTF8ToUTF16(event.description.value());
  }

  if (event.start.has_value()) {
    row.start = GetTime(event.start.value());
  }

  if (event.end.has_value()) {
    row.end = GetTime(event.end.value());
  }

  row.all_day = event.all_day.value_or(false);
  row.is_recurring = event.is_recurring.value_or(false);

  if (event.location.has_value()) {
    row.location = base::UTF8ToUTF16(event.location.value());
  }

  if (event.url.has_value()) {
    row.url = base::UTF8ToUTF16(event.url.value());
  }

  if (event.etag.has_value()) {
    row.etag = event.etag.value();
  }

  if (event.href.has_value()) {
    row.href = event.href.value();
  }

  if (event.uid.has_value()) {
    row.uid = event.uid.value();
  }

  calendar::CalendarID calendar_id;
  if (GetStdStringAsInt64(event.calendar_id, &calendar_id)) {
    row.calendar_id = calendar_id;
  }

  if (event.task.has_value()) {
    row.task = event.task.value();
  }

  if (event.complete.has_value()) {
    row.complete = event.complete.value();
  }

  if (event.sequence.has_value()) {
    row.sequence = event.sequence.value();
  }

  if (event.ical.has_value()) {
    row.ical = base::UTF8ToUTF16(event.ical.value());
  }

  if (event.rrule.has_value()) {
    row.rrule = event.rrule.value();
  }

  if (event.organizer.has_value()) {
    row.organizer = event.organizer.value();
  }

  if (event.event_type_id.has_value()) {
    calendar::EventTypeID event_type_id;
    if (GetStdStringAsInt64(event.event_type_id.value(), &event_type_id)) {
      row.event_type_id = event_type_id;
    }
  }

  if (event.recurrence_exceptions.has_value()) {
    std::vector<RecurrenceExceptionRow> event_exceptions;
    for (const auto& exception : event.recurrence_exceptions.value()) {
      event_exceptions.push_back(CreateEventException(exception));
    }
    row.event_exceptions = event_exceptions;
  }

  if (event.notifications.has_value()) {
    std::vector<NotificationToCreate> event_noficications;
    for (const auto& notification : event.notifications.value()) {
      event_noficications.push_back(CreateNotificationRow(notification));
    }
    row.notifications_to_create = event_noficications;
  }

  if (event.invites.has_value()) {
    std::vector<InviteToCreate> event_invites;
    for (const auto& invite : event.invites.value()) {
      event_invites.push_back(CreateInviteRow(invite));
    }
    row.invites_to_create = event_invites;
  }

  if (event.timezone.has_value()) {
    row.timezone = event.timezone.value();
  }

  row.is_template = event.is_template;

  if (event.priority.has_value()) {
    row.priority = event.priority.value();
  }

  if (event.status.has_value()) {
    row.status = event.status.value();
  }

  if (event.percentage_complete.has_value()) {
    row.percentage_complete = event.percentage_complete.value();
  }

  if (event.categories.has_value()) {
    row.categories = base::UTF8ToUTF16(event.categories.value());
  }

  if (event.component_class.has_value()) {
    row.component_class = base::UTF8ToUTF16(event.component_class.value());
  }

  if (event.attachment.has_value()) {
    row.attachment = base::UTF8ToUTF16(event.attachment.value());
  }

  if (event.completed.has_value()) {
    row.completed = GetTime(event.completed.value());
  }

  if (event.sync_pending.has_value()) {
    row.sync_pending = event.sync_pending.value();
  }

  if (event.delete_pending.has_value()) {
    row.delete_pending = event.delete_pending.value();
  }

  if (event.end_recurring.has_value()) {
    row.end_recurring = GetTime(event.end_recurring.value());
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

RecurrenceExceptionRow CreateEventException(
    const RecurrenceException& exception) {
  RecurrenceExceptionRow exception_event;

  exception_event.exception_day = GetTime(exception.date);

  if (exception.cancelled.has_value()) {
    exception_event.cancelled = exception.cancelled.value();
  }

  if (exception.parent_event_id.has_value()) {
    calendar::EventID parent_event_id;
    if (GetStdStringAsInt64(exception.parent_event_id.value(),
                            &parent_event_id)) {
      exception_event.parent_event_id = parent_event_id;
    }
  }

  if (exception.exception_event_id.has_value()) {
    calendar::EventID exception_event_id;
    if (GetStdStringAsInt64(exception.exception_event_id.value(),
                            &exception_event_id)) {
      exception_event.exception_event_id = exception_event_id;
    }
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
