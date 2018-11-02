// Copyright (c) 2013-2016 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORT_CONFIG_H_
#define IMPORT_CONFIG_H_

#include <vector>
#include "base/strings/string16.h"

namespace importer {

struct ImportConfig {
  ImportConfig();
  ~ImportConfig();

  // Bitmask of items to be imported (see importer::ImportItem enum).
  int imported_items;
  std::vector<base::string16> arguments;
};

}  // namespace importer

#endif  // IMPORT_CONFIG_H_
