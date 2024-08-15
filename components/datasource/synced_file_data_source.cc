// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/synced_file_data_source.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "sync/file_sync/file_store.h"
#include "sync/file_sync/file_store_factory.h"
#include "third_party/re2/src/re2/re2.h"

namespace {
void ForwardContent(content::URLDataSource::GotDataCallback callback,
                    std::optional<base::span<const uint8_t>> content) {
  if (!content) {
    std::move(callback).Run(scoped_refptr<base::RefCountedMemory>());
    return;
  }
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedBytes>(*content));
}
}  // namespace

void SyncedFileDataClassHandler::GetData(
    Profile* profile,
    const std::string& data_id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // data_id should be attachment checksum
  file_sync::SyncedFileStore* synced_file_store =
      SyncedFileStoreFactory::GetForBrowserContext(profile);
  synced_file_store->GetFile(
      data_id, base::BindOnce(&ForwardContent, std::move(callback)));
}

std::string SyncedFileDataClassHandler::GetMimetype(
    Profile* profile,
    const std::string& data_id) {
  file_sync::SyncedFileStore* synced_file_store =
      SyncedFileStoreFactory::GetForBrowserContext(profile);
  return synced_file_store->GetMimeType(data_id);
}