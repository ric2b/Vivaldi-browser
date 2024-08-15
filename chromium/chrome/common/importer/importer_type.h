// Copyright 2013 The Chromium Authors
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
  TYPE_UNKNOWN = -1,
#if BUILDFLAG(IS_WIN)
  TYPE_IE = 0,
#endif
  // Value 1 was the (now deleted) Firefox 2 profile importer.
  TYPE_FIREFOX = 2,
#if BUILDFLAG(IS_MAC)
  TYPE_SAFARI = 3,
#endif
  // Value 4 was the (now deleted) Google Toolbar importer.
  TYPE_BOOKMARKS_FILE = 5,  // Identifies a 'bookmarks.html' file.
#if BUILDFLAG(IS_WIN)
  TYPE_EDGE = 6,
#endif

  // Identifies an old Opera
  TYPE_OPERA = 7,
  // DEPRECATED
  // TYPE_OPERA_BOOKMARK_FILE = 8,
  TYPE_CHROME = 9,
  TYPE_CHROMIUM = 10,
  TYPE_VIVALDI = 11,
  TYPE_YANDEX = 12,
  TYPE_OPERA_OPIUM_BETA = 13,  // Chromium-based Opera Beta channel
  TYPE_OPERA_OPIUM_DEV = 14,
  TYPE_BRAVE = 15,
  TYPE_EDGE_CHROMIUM = 16,
  TYPE_THUNDERBIRD = 17,

  // NOTE, any addition here must be reflected in SUPPORTED_IMPORT_BROWSERS in
  // WelcomePageImport.jsx
  TYPE_OPERA_OPIUM = 18,  // Chromium-based Opera
  TYPE_ARC = 19,
  TYPE_OPERA_GX = 20,

  // NOTE: Any addition here must be reflected in
  // the IPC enum traits in profile_import_process_param_traits_macros.h
};

}  // namespace importer


#endif  // CHROME_COMMON_IMPORTER_IMPORTER_TYPE_H_
