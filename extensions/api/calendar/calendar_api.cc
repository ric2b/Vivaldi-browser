// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/calendar/calendar_api.h"

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/calendar.h"

#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_service.h"
#include "calendar/calendar_service_factory.h"

using calendar::CalendarServiceFactory;
using calendar::CalendarService;

namespace extensions {

typedef std::vector<vivaldi::calendar::CalendarEvent> EventList;

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

CalendarEventRouter::CalendarEventRouter(Profile* profile)
    : browser_context_(profile),
      model_(CalendarServiceFactory::GetForProfile(profile)) {
  model_->AddObserver(this);
}

CalendarEventRouter::~CalendarEventRouter() {
  model_->RemoveObserver(this);
}

void CalendarEventRouter::ExtensiveCalendarChangesBeginning(
    CalendarService* model) {}

void CalendarEventRouter::ExtensiveCalendarChangesEnded(
    CalendarService* model) {}

// Helper to actually dispatch an event to extension listeners.
void CalendarEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

void BroadcastCalendarEvent(const std::string& eventname,
                            std::unique_ptr<base::ListValue> args,
                            content::BrowserContext* context) {
  std::unique_ptr<extensions::Event> event(
      new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                            eventname, std::move(args), context));
  EventRouter* event_router = EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}

CalendarAPI::CalendarAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 vivaldi::calendar::OnCreated::kEventName);
}

CalendarAPI::~CalendarAPI() {}

void CalendarAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<CalendarAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<CalendarAPI>* CalendarAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void CalendarAPI::OnListenerAdded(const EventListenerInfo& details) {
  calendar_event_router_.reset(
      new CalendarEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

std::unique_ptr<CalendarEvent> CreateVivaldiEvent(
    const calendar::EventResult& event) {
  std::unique_ptr<CalendarEvent> cal_event(new CalendarEvent());

  cal_event->id = base::UTF16ToUTF8(event.id());
  cal_event->calendar_id.reset(
      new std::string(base::UTF16ToUTF8(event.calendar_id())));
  cal_event->title.reset(new std::string(base::UTF16ToUTF8(event.title())));
  cal_event->description.reset(
      new std::string(base::UTF16ToUTF8(event.description())));
  cal_event->start.reset(new double(MilliSecondsFromTime(event.start())));
  cal_event->end.reset(new double(MilliSecondsFromTime(event.end())));

  return cal_event;
}

CalendarService* CalendarAsyncFunction::GetCalendarService() {
  return CalendarServiceFactory::GetForProfile(GetProfile());
}

CalendarGetAllFunction::~CalendarGetAllFunction() {}

ExtensionFunction::ResponseAction CalendarGetAllFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllEvents(
      base::Bind(&CalendarGetAllFunction::GetAllEventsComplete, this),
      &task_tracker_);

  return RespondLater();  // GetAllEventsComplete() will be called
                          // asynchronously.
}

void CalendarGetAllFunction::GetAllEventsComplete(
    std::shared_ptr<calendar::EventQueryResults> results) {
  EventList eventList;

  if (results && !results->empty()) {
    for (calendar::EventQueryResults::EventResultVector::const_iterator
             iterator = results->begin();
         iterator != results->end(); ++iterator) {
      eventList.push_back(std::move(
          *base::WrapUnique((CreateVivaldiEvent(**iterator).release()))));
    }
  }

  Respond(ArgumentList(vivaldi::calendar::GetAll::Results::Create(eventList)));
}

Profile* CalendarAsyncFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

base::Time GetTime(double ms_from_epoch) {
  double seconds_from_epoch = ms_from_epoch / 1000.0;
  return (seconds_from_epoch == 0)
             ? base::Time::UnixEpoch()
             : base::Time::FromDoubleT(seconds_from_epoch);
}

ExtensionFunction::ResponseAction CalendarCreateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::Create::Params> params(
      vivaldi::calendar::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::EventRow createEvent;

  base::string16 title;
  title = base::UTF8ToUTF16(params->event.title);

  createEvent.set_title(title);

  base::string16 description;
  if (params->event.description.get()) {
    description = base::UTF8ToUTF16(*params->event.description);
    createEvent.set_description(description);
  }

  double start;
  if (params->event.start.get()) {
    start = *params->event.start.get();
    createEvent.set_start(GetTime(start));
  }

  double end;
  if (params->event.end.get()) {
    end = *params->event.end.get();
    createEvent.set_end(GetTime(end));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->CreateCalendarEvent(createEvent);
  CalendarEvent ev = CalendarEvent();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction CalendarDeleteFunction::Run() {
  return RespondNow(NoArguments());
}

bool GetEventIdAsInt64(const base::string16& id_string, int64_t* id) {
  if (base::StringToInt64(id_string, id))
    return true;

  return false;
}

ExtensionFunction::ResponseAction CalendarUpdateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::Update::Params> params(
      vivaldi::calendar::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::CalendarEvent updatedEvent;

  base::string16 id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventID eventId;
  GetEventIdAsInt64(id, &eventId);

  if (params->changes.calendar_id.get()) {
    updatedEvent.calendar_id = base::UTF8ToUTF16(*params->changes.calendar_id);
    updatedEvent.updateFields |= calendar::CALENDAR_ID;
  }

  if (params->changes.description.get()) {
    updatedEvent.description = base::UTF8ToUTF16(*params->changes.description);
    updatedEvent.updateFields |= calendar::DESCRIPTION;
  }

  if (params->changes.title.get()) {
    updatedEvent.title = base::UTF8ToUTF16(*params->changes.title);
    updatedEvent.updateFields |= calendar::TITLE;
  }

  if (params->changes.start.get()) {
    double start = *params->changes.start;
    updatedEvent.start = GetTime(start);
    updatedEvent.updateFields |= calendar::START;
  }

  if (params->changes.end.get()) {
    double end = *params->changes.end;
    updatedEvent.end = GetTime(end);
    updatedEvent.updateFields |= calendar::END;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->UpdateCalendarEvent(
      eventId, updatedEvent,
      base::Bind(&CalendarUpdateFunction::UpdateEventComplete, this),
      &task_tracker_);
  return RespondLater();  // UpdateEventComplete() will be called
                          // asynchronously.
}

void CalendarUpdateFunction::UpdateEventComplete(
    std::shared_ptr<calendar::UpdateEventResult> results) {
  if (!results->success) {
    Respond(Error("Error updating event"));
  } else {
    Respond(NoArguments());
  }
}

//  CalendarUpdateFunction

}  //  namespace extensions
