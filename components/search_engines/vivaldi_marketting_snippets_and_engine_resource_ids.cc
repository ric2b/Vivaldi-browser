// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include <string>

#include "ui/resources/grit/ui_resources.h"

namespace search_engines {

int GetIconResourceId(const std::u16string& engine_keyword) {
  return IDR_DEFAULT_FAVICON;
}

int GetMarketingSnippetResourceId(const std::u16string& engine_keyword) {
  return -1;
}

}  // namespace search_engines
