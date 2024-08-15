// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "db/vivaldi_history_types.h"

namespace history {

TypedUrlResult::TypedUrlResult() {}

TypedUrlResult& TypedUrlResult::operator=(const TypedUrlResult&) = default;

TypedUrlResult::TypedUrlResult(const TypedUrlResult&) = default;

DetailedUrlResult::DetailedUrlResult() {}
DetailedUrlResult::~DetailedUrlResult() {}

DetailedUrlResult& DetailedUrlResult::operator=(const DetailedUrlResult&) =
    default;

DetailedUrlResult::DetailedUrlResult(const DetailedUrlResult&) = default;

UrlVisitCount::UrlVisitCount(std::string date, GURL url, int count)
    : date_(date), url_(url), count_(count) {}

UrlVisitCount::~UrlVisitCount() {}

UrlVisitCount::UrlVisitCount(const UrlVisitCount& other) = default;

Visit::Visit(std::string id,
             base::Time visit_time,
             GURL url,
             std::u16string title,
             ui::PageTransition transition,
             int visit_count,
             int source)
    : id(id),
      visit_time(visit_time),
      url(url),
      title(title),
      transition(transition),
      visit_count(visit_count),
      source(source) {}

Visit::~Visit() {}

Visit::Visit(const Visit& other) = default;

}  //  namespace history
