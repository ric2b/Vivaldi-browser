// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

namespace vivaldi {

#define VIVALDI_IMPORT_OPEN_TABS                                             \
  if ((items & importer::TABS) && !cancelled()) {                            \
    bridge_->NotifyItemStarted(importer::TABS);                              \
    vivaldi::ImportFirefoxTabs(this, bridge_,                                \
                               GetCopiedSourcePath("sessionstore.jsonlz4")); \
    bridge_->NotifyItemEnded(importer::TABS);                                \
  }

void ImportFirefoxTabs(FirefoxImporter* instance,
                       scoped_refptr<ImporterBridge>& bridge,
                       const base::FilePath &sessionstore_path);

} // namespace vivaldi
