// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/history/history_private_api.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_service_factory.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"

#include "extensions/schema/history_private.h"

namespace extensions {

using vivaldi::history_private::HistoryItem;

namespace DBSearch = vivaldi::history_private::DbSearch;

namespace Search = vivaldi::history_private::Search;

typedef std::vector<vivaldi::history_private::HistoryItem> HistoryItemList;

namespace {

// Start chromium copied code
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

std::unique_ptr<HistoryItem> GetHistoryItem(const history::URLRow& row) {
  std::unique_ptr<HistoryItem> history_item(new HistoryItem());

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


bool HistoryPrivateDbSearchFunction::RunAsyncImpl() {
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
      "AND urls.url LIKE ? OR urls.title LIKE ? ORDER BY urls.id LIMIT ?";
  const std::string search_text = "%" + params->query.text + "%";
  hs->QueryHistoryWStatement(sql_statement, search_text, max_hits,
                   base::Bind(&HistoryPrivateDbSearchFunction::SearchComplete,
                              base::Unretained(this)),
                   &task_tracker_);

  return true;
}

void HistoryPrivateDbSearchFunction::SearchComplete(
        history::QueryResults* results) {
  HistoryItemList history_item_vec;
  if (results && !results->empty()) {
    for (history::QueryResults::URLResultVector::const_iterator iterator =
            results->begin();
         iterator != results->end();
        ++iterator) {
      history_item_vec.push_back(std::move(*base::WrapUnique(
          GetHistoryItem(**iterator).release())));
    }
  }
  // This must be revisited since it is slow!
  results_ = DBSearch::Results::Create(history_item_vec);
  SendAsyncResponse();
}


bool HistoryPrivateSearchFunction::RunAsyncImpl() {
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
    if (params->query.result_grouping == vivaldi::history_private::
      HISTORY_RESULT_SET_GROUPING_KEEP_ALL_DUPLICATES) {
      options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
    } else if (params->query.result_grouping == vivaldi::history_private::
      HISTORY_RESULT_SET_GROUPING_REMOVE_DUPLICATES_PER_DAY) {
      options.duplicate_policy =
        history::QueryOptions::REMOVE_DUPLICATES_PER_DAY;
    }
  }

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
    GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);

  hs->QueryHistory(search_text,
    options,
    base::Bind(&HistoryPrivateSearchFunction::SearchComplete,
      base::Unretained(this)),
    &task_tracker_);

  return true;
}


HistoryItem GetHistoryAndVisitItem(const history::URLResult& row) {
  HistoryItem history_item;

  if (row.visit_time().is_null()) {
    // If the row has visitTime, id is not unique. Caused by grouping.
    history_item.id = row.id();
  } else {
    history_item.id = std::to_string(MilliSecondsFromTime(row.visit_time()));
    history_item.visit_time.reset(
      new double(MilliSecondsFromTime(row.visit_time())));
  }
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
  if (results && !results->empty()) {
    for (const history::URLResult* item : *results)
      history_item_vec.push_back(GetHistoryAndVisitItem(*item));
  }
  results_ = DBSearch::Results::Create(history_item_vec);
  SendAsyncResponse();
}



}  // namespace extensions
