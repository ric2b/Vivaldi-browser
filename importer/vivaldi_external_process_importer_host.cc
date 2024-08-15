// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "chrome/browser/importer/external_process_importer_host.h"

#include "app/vivaldi_resources.h"
#include "chrome/browser/importer/importer_lock_dialog.h"
#include "chrome/common/importer/importer_type.h"
#include "ui/base/l10n/l10n_util.h"

void ExternalProcessImporterHost::NotifyImportItemFailed(
    importer::ImportItem item,
    const std::string& error) {
  if (observer_)
    observer_->ImportItemFailed(item, error);
}

void ExternalProcessImporterHost::ShowChromeWarningDialog() {
  int importerLockString_id=0;

  if (source_profile_.importer_type == importer::TYPE_CHROME ||
      source_profile_.importer_type == importer::TYPE_CHROMIUM) {
    importerLockString_id = IDS_CHROME_IMPORTER_LOCK_TEXT;
  } else if (source_profile_.importer_type == importer::TYPE_OPERA_OPIUM ||
             source_profile_.importer_type == importer::TYPE_OPERA_OPIUM_BETA ||
             source_profile_.importer_type == importer::TYPE_OPERA_OPIUM_DEV) {
    importerLockString_id = IDS_OPIUM_IMPORTER_LOCK_TEXT;
  } else if (source_profile_.importer_type == importer::TYPE_YANDEX) {
    importerLockString_id = IDS_YANDEX_IMPORTER_LOCK_TEXT;
  } else if (source_profile_.importer_type == importer::TYPE_BRAVE) {
    importerLockString_id = IDS_BRAVE_IMPORTER_LOCK_TEXT;
  } else if (source_profile_.importer_type == importer::TYPE_EDGE_CHROMIUM) {
    importerLockString_id = IDS_EDGE_CHROMIUM_IMPORTER_LOCK_TEXT;
  } else if (source_profile_.importer_type == importer::TYPE_ARC) {
    importerLockString_id = IDS_ARC_IMPORTER_LOCK_TEXT;
  } else if (source_profile_.importer_type == importer::TYPE_OPERA_GX) {
    importerLockString_id = IDS_OPERA_GX_IMPORTER_LOCK_TEXT;
  }

  DCHECK(!headless_);
  importer::ShowImportLockDialog(
      parent_window_,
      base::BindOnce(
          &ExternalProcessImporterHost::OnChromiumImportLockDialogEnd,
          weak_ptr_factory_.GetWeakPtr()),
      IDS_IMPORTER_LOCK_TITLE,
      importerLockString_id);
}

void ExternalProcessImporterHost::OnChromiumImportLockDialogEnd(
    bool is_continue) {
  if (is_continue) {
    // User chose to continue, then we check the lock again to make
    // sure that Chromium has been closed. Try to import the settings
    // if successful. Otherwise, show a warning dialog.
    chromium_lock_->Lock();
    if (chromium_lock_->HasAcquired()) {
      is_source_readable_ = true;
      LaunchImportIfReady();
    } else {
      ShowChromeWarningDialog();
    }
  } else {
    NotifyImportEnded();
  }
}

bool ExternalProcessImporterHost::CheckForChromeLock(
    const importer::SourceProfile& source_profile) {
  if (source_profile.importer_type != importer::TYPE_CHROME &&
      source_profile.importer_type != importer::TYPE_YANDEX &&
      source_profile.importer_type != importer::TYPE_OPERA_OPIUM &&
      source_profile.importer_type != importer::TYPE_OPERA_OPIUM_BETA &&
      source_profile.importer_type != importer::TYPE_OPERA_OPIUM_DEV &&
      source_profile.importer_type != importer::TYPE_BRAVE &&
      source_profile.importer_type != importer::TYPE_EDGE_CHROMIUM &&
      source_profile.importer_type != importer::TYPE_VIVALDI)
    return true;

  DCHECK(!chromium_lock_.get());
  chromium_lock_.reset(new ChromiumProfileLock(source_profile.source_path));
  if (chromium_lock_->HasAcquired())
    return true;

  // If fail to acquire the lock, we set the source unreadable and
  // show a warning dialog, unless running without UI (in which case the import
  // must be aborted).
  is_source_readable_ = false;
  if (headless_)
    return false;

  ShowChromeWarningDialog();
  return true;
}
