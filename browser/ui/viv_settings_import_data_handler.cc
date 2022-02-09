// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/ui/webui/settings/import_data_handler.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace settings {

void ImportDataHandler::ImportItemFailed(importer::ImportItem item,
                                         const std::string& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // NOTE(pettern@vivaldi.com): We don't use this code, so this
  // means nothing for us.
  import_did_succeed_ = true;
}

}  // namespace settings
