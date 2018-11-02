// Copyright (c) 2013-2016 Vivaldi Technologies AS. All rights reserved

#include "importer/import_config.h"

#include <vector>

#include "base/strings/string16.h"

namespace importer {

ImportConfig::ImportConfig() : imported_items(0) {}

ImportConfig::~ImportConfig() {
  // (Attempt to) wipe parameter string
  for (std::vector<base::string16>::iterator it = arguments.begin();
       it != arguments.end(); it++) {
    it->clear();
  }
}

}  // namespace importer
