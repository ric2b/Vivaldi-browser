// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_IMPORTER_TYPE_H_
#define CHROME_COMMON_IMPORTER_IMPORTER_TYPE_H_

#include "build/build_config.h"

namespace importer {

// An enumeration of the type of importers that we support to import
// settings and data from (browsers, google toolbar and a bookmarks html file).
// NOTE: Numbers added so that data can be reliably cast to ints and passed
// across IPC.
enum ImporterType {
  TYPE_UNKNOWN         = -1,
#if defined(OS_WIN)
  TYPE_IE              = 0,
#endif
  // Value 1 was the (now deleted) Firefox 2 profile importer.
  TYPE_FIREFOX         = 2,
#if defined(OS_MACOSX)
  TYPE_SAFARI          = 3,
#endif
  // Value 4 was the (now deleted) Google Toolbar importer.
  TYPE_BOOKMARKS_FILE  = 5,  // Identifies a 'bookmarks.html' file.

  // Identifies an Opera bookmark file
  TYPE_OPERA           = 6,
  TYPE_OPERA_BOOKMARK_FILE           = 7,
  TYPE_CHROME          = 8,
  TYPE_VIVALDI          = 9,
  TYPE_YANDEX = 10,
  TYPE_OPERA_OPIUM_BETA = 11,   // Chromium-based Opera Beta channel
  TYPE_OPERA_OPIUM_DEV = 12,    // Chromium-based Opera Developer channel

  // Must be last due to profile_import_process_param_traits_macros.h
  TYPE_OPERA_OPIUM = 13,        // Chromium-based Opera
};

}  // namespace importer


#endif  // CHROME_COMMON_IMPORTER_IMPORTER_TYPE_H_
