// Copyright (c) 2013-2016 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_IMPORTED_SPEEDDIAL_ENTRY_H_
#define IMPORTER_IMPORTED_SPEEDDIAL_ENTRY_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "url/gurl.h"

struct ImportedSpeedDialEntry {
  ImportedSpeedDialEntry();
  ~ImportedSpeedDialEntry();

  GURL url;
  base::string16 title;
};

#endif  // IMPORTER_IMPORTED_SPEEDDIAL_ENTRY_H_
