// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/importer_creator.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/utility/importer/bookmarks_file_importer.h"
#include "chrome/utility/importer/firefox_importer.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/common/importer/edge_importer_utils_win.h"
#include "chrome/utility/importer/edge_importer_win.h"
#include "chrome/utility/importer/ie_importer_win.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "chrome/utility/importer/safari_importer.h"
#endif

#include "importer/chromium_importer.h"
#include "importer/viv_importer.h"

namespace importer {

scoped_refptr<Importer> CreateImporterByType(
    user_data_importer::ImporterType type) {
  switch (type) {
#if BUILDFLAG(IS_WIN)
    case user_data_importer::TYPE_IE:
      return new IEImporter();
    case user_data_importer::TYPE_EDGE:
      // If legacy mode we pass back an IE importer.
      if (importer::IsEdgeFavoritesLegacyMode())
        return new IEImporter();
      return new EdgeImporter();
#endif
    case user_data_importer::TYPE_BOOKMARKS_FILE:
      return new BookmarksFileImporter();
#if !BUILDFLAG(IS_CHROMEOS)
    case user_data_importer::TYPE_FIREFOX:
      return new FirefoxImporter();
#endif
#if BUILDFLAG(IS_MAC)
    case user_data_importer::TYPE_SAFARI:
      return new SafariImporter(base::apple::GetUserLibraryPath());
#endif
    case user_data_importer::TYPE_OPERA:
      return new OperaImporter();
    case user_data_importer::TYPE_CHROME:
    case user_data_importer::TYPE_CHROMIUM:
    case user_data_importer::TYPE_YANDEX:
    case user_data_importer::TYPE_OPERA_OPIUM:
    case user_data_importer::TYPE_OPERA_OPIUM_BETA:
    case user_data_importer::TYPE_OPERA_OPIUM_DEV:
    case user_data_importer::TYPE_VIVALDI:
    case user_data_importer::TYPE_BRAVE:
    case user_data_importer::TYPE_EDGE_CHROMIUM:
    case user_data_importer::TYPE_ARC:
    case user_data_importer::TYPE_OPERA_GX:
      return new ChromiumImporter();
    default:
      NOTREACHED();
  }
}

}  // namespace importer
