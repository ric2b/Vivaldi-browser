// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/history/history_private_api.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "db/vivaldi_history_types.h"
#include "extensions/schema/history_private.h"

namespace extensions {

using vivaldi::history_private::HistoryPrivateItem;
namespace DBSearch = vivaldi::history_private::DbSearch;
namespace Search = vivaldi::history_private::Search;
namespace OnVisitModified = vivaldi::history_private::OnVisitModified;

using vivaldi::history_private::TopUrlItem;
using vivaldi::history_private::TransitionType;

typedef std::vector<vivaldi::history_private::HistoryPrivateItem>
    HistoryItemList;
namespace GetTopUrlsPerDay = vivaldi::history_private::GetTopUrlsPerDay;
typedef std::vector<vivaldi::history_private::HistoryPrivateItem>
    VisitsPrivateList;

namespace VisitSearch = vivaldi::history_private::VisitSearch;
typedef std::vector<vivaldi::history_private::TopUrlItem> TopSitesPerDayList;

using bookmarks::BookmarkModel;

namespace {

// Start chromium copied code
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

std::unique_ptr<HistoryPrivateItem> GetHistoryItem(const history::URLRow& row) {
  std::unique_ptr<HistoryPrivateItem> history_item(new HistoryPrivateItem());

  history_item->id = base::Int64ToString(row.id());
  history_item->url.reset(new std::string(row.url().spec()));
  history_item->title.reset(new std::string(base::UTF16ToUTF8(row.title())));
  history_item->last_visit_time.reset(
      new double(MilliSecondsFromTime(row.last_visit())));
  history_item->typed_count.reset(new int(row.typed_count()));
  history_item->visit_count.reset(new int(row.visit_count()));

  return history_item;
}

// End chromium copied code

}  // namespace

HistoryPrivateAPI::HistoryPrivateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, OnVisitModified::kEventName);
}

HistoryPrivateAPI::~HistoryPrivateAPI() {}

void HistoryPrivateAPI::Shutdown() {
  history_event_router_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<HistoryPrivateAPI>>
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<HistoryPrivateAPI>*
HistoryPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<
    HistoryPrivateAPI>::DeclareFactoryDependencies() {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

void HistoryPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  history_event_router_.reset(new HistoryPrivateEventRouter(
      profile, HistoryServiceFactory::GetForProfile(
                   profile, ServiceAccessType::EXPLICIT_ACCESS)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

HistoryPrivateEventRouter::HistoryPrivateEventRouter(
    Profile* profile,
    history::HistoryService* history_service)
    : profile_(profile), history_service_observer_(this) {
  DCHECK(profile);
  history_service_observer_.Add(history_service);
}

HistoryPrivateEventRouter::~HistoryPrivateEventRouter() {}

void HistoryPrivateEventRouter::OnURLsModified(
    history::HistoryService* history_service,
    const history::URLRows& changed_urls) {
  OnVisitModified::Modified modified;
  std::vector<std::string>* urls = new std::vector<std::string>();
  for (const auto& row : changed_urls) {
    urls->push_back(row.url().spec());
  }
  modified.urls.reset(urls);
  std::unique_ptr<base::ListValue> args = OnVisitModified::Create(modified);

  DispatchEvent(profile_, OnVisitModified::kEventName, std::move(args));
}

// Helper to actually dispatch an event to extension listeners.
void HistoryPrivateEventRouter::DispatchEvent(
    Profile* profile,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  if (profile && EventRouter::Get(profile)) {
    EventRouter::Get(profile)->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

ExtensionFunction::ResponseAction HistoryPrivateDbSearchFunction::Run() {
  std::unique_ptr<DBSearch::Params> params(DBSearch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);

  int max_hits = 100;

  if (params->query.max_results.get())
    max_hits = *params->query.max_results;

  const char* sql_statement =
      "SELECT urls.id, urls.url, urls.title, urls.visit_count, "
      "urls.typed_count, urls.last_visit_time, urls.hidden "
      "FROM urls WHERE hidden = 0 "
      "AND urls.url LIKE ? OR urls.title LIKE ? "
      "ORDER BY urls.last_visit_time DESC LIMIT ?";
  const std::string search_text = "%" + params->query.text + "%";
  hs->QueryHistoryWStatement(
      sql_statement, search_text, max_hits,
      base::Bind(&HistoryPrivateDbSearchFunction::SearchComplete,
                 base::Unretained(this)),
      &task_tracker_);

  AddRef();               // Balanced in SearchComplete().
  return RespondLater();
}

void HistoryPrivateDbSearchFunction::SearchComplete(
    history::QueryResults* results) {
  HistoryItemList history_item_vec;
  if (results && !results->empty()) {
    for (history::QueryResults::URLResultVector::const_iterator iterator =
             results->begin();
         iterator != results->end(); ++iterator) {
      history_item_vec.push_back(
          std::move(*base::WrapUnique(GetHistoryItem(**iterator).release())));
    }
  }
  // This must be revisited since it is slow!
  Respond(ArgumentList(DBSearch::Results::Create(history_item_vec)));
  Release();
}

ExtensionFunction::ResponseAction HistoryPrivateSearchFunction::Run() {
  std::unique_ptr<Search::Params> params(Search::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 search_text = base::UTF8ToUTF16(params->query.text);

  history::QueryOptions options;
  options.SetRecentDayRange(1);
  options.max_count = 100;

  if (params->query.start_time.get())
    options.begin_time = GetTime(*params->query.start_time);
  if (params->query.end_time.get())
    options.end_time = GetTime(*params->query.end_time);
  if (params->query.max_results.get())
    options.max_count = *params->query.max_results;

  if (params->query.result_grouping) {
    if (params->query.result_grouping ==
        vivaldi::history_private::
            HISTORY_RESULT_SET_GROUPING_KEEP_ALL_DUPLICATES) {
      options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
    } else if (params->query.result_grouping ==
               vivaldi::history_private::
                   HISTORY_RESULT_SET_GROUPING_REMOVE_DUPLICATES_PER_DAY) {
      options.duplicate_policy =
          history::QueryOptions::REMOVE_DUPLICATES_PER_DAY;
    }
  }

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);

  hs->QueryHistory(search_text, options,
                   base::Bind(&HistoryPrivateSearchFunction::SearchComplete,
                              base::Unretained(this)),
                   &task_tracker_);

  AddRef();               // Balanced in SearchComplete().
  return RespondLater();
}

HistoryPrivateItem GetHistoryAndVisitItem(const history::URLResult& row,
                                          BookmarkModel* bookmark_model) {
  HistoryPrivateItem history_item;

  if (row.visit_time().is_null()) {
    // If the row has visitTime, id is not unique. Caused by grouping.
    history_item.id = row.id();
  } else {
    history_item.id = std::to_string(MilliSecondsFromTime(row.visit_time()));
    history_item.visit_time.reset(
        new double(MilliSecondsFromTime(row.visit_time())));
  }
  history_item.is_bookmarked = bookmark_model->IsBookmarked(row.url());
  history_item.visit_time.reset(
      new double(MilliSecondsFromTime(row.visit_time())));
  history_item.url.reset(new std::string(row.url().spec()));
  history_item.title.reset(new std::string(base::UTF16ToUTF8(row.title())));
  history_item.last_visit_time.reset(
      new double(MilliSecondsFromTime(row.last_visit())));
  history_item.typed_count.reset(new int(row.typed_count()));
  history_item.visit_count.reset(new int(row.visit_count()));

  return history_item;
}

void HistoryPrivateSearchFunction::SearchComplete(
    history::QueryResults* results) {
  HistoryItemList history_item_vec;
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  if (results && !results->empty()) {
    for (const history::URLResult* item : *results)
      history_item_vec.push_back(GetHistoryAndVisitItem(*item, model));
  }
  Respond(ArgumentList(DBSearch::Results::Create(history_item_vec)));
  Release();
}

ExtensionFunction::ResponseAction HistoryPrivateDeleteVisitsFunction::Run() {
  std::unique_ptr<vivaldi::history_private::DeleteVisits::Params> params(
      vivaldi::history_private::DeleteVisits::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url;
  std::string error;
  if (!ValidateUrl(params->details.url, &url, &error))
    return RespondNow(Error(error));

  base::Time start_time = GetTime(params->details.start_time);
  base::Time end_time = GetTime(params->details.end_time);

  std::set<GURL> restrict_urls;
  restrict_urls.insert(url);

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      restrict_urls, start_time, end_time,
      base::Bind(&HistoryPrivateDeleteVisitsFunction::DeleteVisitComplete,
                 base::Unretained(this)),
      &task_tracker_);

  AddRef();               // Balanced in DeleteVisitComplete().
  return RespondLater();
}

void HistoryPrivateDeleteVisitsFunction::DeleteVisitComplete() {
  Respond(NoArguments());
  Release();
}

ExtensionFunction::ResponseAction
HistoryPrivateGetTopUrlsPerDayFunction::Run() {
  std::unique_ptr<GetTopUrlsPerDay::Params> params(
      GetTopUrlsPerDay::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  double number_of_days = kMaxResultsWithinDay;
  if (params->max_top_url_results)
    number_of_days = params->max_top_url_results;

  hs->TopUrlsPerDay(
      number_of_days,
      base::Bind(&HistoryPrivateGetTopUrlsPerDayFunction::TopUrlsComplete,
                 base::Unretained(this)));
  AddRef();               // Balanced in TopUrlsComplete().
  return RespondLater();
}

std::unique_ptr<TopUrlItem> GetTopUrlPerDay(history::UrlVisitCount visit) {
  std::unique_ptr<TopUrlItem> history_item(new TopUrlItem());

  history_item->date = visit.date();
  history_item->url = visit.url().spec();
  history_item->number_of_visit.reset(new int(visit.count()));

  return history_item;
}

void HistoryPrivateGetTopUrlsPerDayFunction::TopUrlsComplete(
    const history::UrlVisitCount::TopUrlsPerDayList& results) {
  TopSitesPerDayList history_item_vec;

  for (auto& item : results) {
    history_item_vec.push_back(std::move(*(GetTopUrlPerDay(item))));
  }
  Respond(ArgumentList(GetTopUrlsPerDay::Results::Create(history_item_vec)));
  Release();
}

std::unique_ptr<HistoryPrivateItem> GetVisitsItem(
    history::Visit visit,
    BookmarkModel* bookmark_model) {
  std::unique_ptr<HistoryPrivateItem> history_item(new HistoryPrivateItem());

  base::Time visitTime = visit.visit_time;
  base::Time::Exploded exploded;
  visitTime.LocalExplode(&exploded);

  history_item->id = visit.id;
  history_item->url.reset(new std::string(visit.url.spec()));
  history_item->protocol.reset(new std::string(visit.url.scheme()));
  history_item->address.reset(new std::string(visit.url.host()));
  history_item->title.reset(new std::string(base::UTF16ToUTF8(visit.title)));
  history_item->visit_time.reset(
      new double(MilliSecondsFromTime(visit.visit_time)));
  history_item->is_bookmarked = bookmark_model->IsBookmarked(visit.url);
  history_item->date_key.reset(new std::string(base::StringPrintf(
      "%04d-%02d-%02d", exploded.year, exploded.month, exploded.day_of_month)));
  history_item->hour.reset(new int(exploded.hour));
  history_item->visit_count.reset(new int(visit.visit_count));

  TransitionType transition = vivaldi::history_private::TRANSITION_TYPE_LINK;
  switch (visit.transition & ui::PAGE_TRANSITION_CORE_MASK) {
    case ui::PAGE_TRANSITION_LINK:
      transition = vivaldi::history_private::TRANSITION_TYPE_LINK;
      break;
    case ui::PAGE_TRANSITION_TYPED:
      transition = vivaldi::history_private::TRANSITION_TYPE_TYPED;
      break;
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      transition = vivaldi::history_private::TRANSITION_TYPE_AUTO_BOOKMARK;
      break;
    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
      transition = vivaldi::history_private::TRANSITION_TYPE_AUTO_SUBFRAME;
      break;
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      transition = vivaldi::history_private::TRANSITION_TYPE_MANUAL_SUBFRAME;
      break;
    case ui::PAGE_TRANSITION_GENERATED:
      transition = vivaldi::history_private::TRANSITION_TYPE_GENERATED;
      break;
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      transition = vivaldi::history_private::TRANSITION_TYPE_AUTO_TOPLEVEL;
      break;
    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      transition = vivaldi::history_private::TRANSITION_TYPE_FORM_SUBMIT;
      break;
    case ui::PAGE_TRANSITION_RELOAD:
      transition = vivaldi::history_private::TRANSITION_TYPE_RELOAD;
      break;
    case ui::PAGE_TRANSITION_KEYWORD:
      transition = vivaldi::history_private::TRANSITION_TYPE_KEYWORD;
      break;
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      transition = vivaldi::history_private::TRANSITION_TYPE_KEYWORD_GENERATED;
      break;
    default:
      DCHECK(false);
  }

  history_item->transition_type = transition;
  return history_item;
}

ExtensionFunction::ResponseAction HistoryPrivateVisitSearchFunction::Run() {
  std::unique_ptr<VisitSearch::Params> params(
      VisitSearch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  history::QueryOptions options;

  if (params->query.start_time.get())
    options.begin_time = GetTime(*params->query.start_time);
  if (params->query.end_time.get())
    options.end_time = GetTime(*params->query.end_time);

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);

  hs->VisitSearch(options,
            base::Bind(&HistoryPrivateVisitSearchFunction::VisitsComplete,
                       base::Unretained(this)));
  AddRef();               // Balanced in VisitsComplete().
  return RespondLater();
}

// Callback for the history service to acknowledge visits search complete.
void HistoryPrivateVisitSearchFunction::VisitsComplete(
    const history::Visit::VisitsList& visit_list) {
  VisitsPrivateList history_item_vec;
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());

  for (auto& item : visit_list) {
    history_item_vec.push_back(std::move(*(GetVisitsItem(item, model))));
  }
  Respond(ArgumentList(VisitSearch::Results::Create(history_item_vec)));
  Release();
}

}  // namespace extensions
