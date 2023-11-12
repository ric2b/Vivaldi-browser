// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "db/vivaldi_history_types.h"

namespace history {

UrlVisitCount::UrlVisitCount(std::string date, GURL url, int count)
    : date_(date), url_(url), count_(count) {}

UrlVisitCount::~UrlVisitCount() {}

UrlVisitCount::UrlVisitCount(const UrlVisitCount& other) = default;

Visit::Visit(std::string id,
             base::Time visit_time,
             GURL url,
             std::u16string title,
             ui::PageTransition transition,
             int visit_count)
    : id(id),
      visit_time(visit_time),
      url(url),
      title(title),
      transition(transition),
      visit_count(visit_count) {}

Visit::~Visit() {}

Visit::Visit(const Visit& other) = default;

DetailedHistory::DetailedHistory() = default;

DetailedHistory::DetailedHistory(const DetailedHistory&) = default;

DetailedHistory::~DetailedHistory() = default;

}  //  namespace history
