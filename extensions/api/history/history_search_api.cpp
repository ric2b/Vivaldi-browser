// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/history/history_search_api.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_service_factory.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"

#include "extensions/schema/history_search.h"


namespace extensions {

using vivaldi::history_search::HistoryItem;

namespace DBSearch = vivaldi::history_search::DbSearch;

typedef std::vector<vivaldi::history_search::HistoryItem> HistoryItemList;

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

//End chromium copied code

} // namespace


bool HistorySearchDbSearchFunction::RunAsyncImpl() {
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
                   base::Bind(&HistorySearchDbSearchFunction::SearchComplete,
                              base::Unretained(this)),
                   &task_tracker_);

  return true;
}

void HistorySearchDbSearchFunction::SearchComplete(
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

} // namespace extensions
