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
             base::string16 title,
             std::string transition)
    : id_(id),
      visit_time_(visit_time),
      url_(url),
      title_(title),
      transition_(transition) {}

Visit::~Visit() {}

Visit::Visit(const Visit& other) = default;

}  //  namespace history
