// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.

#include "content/browser/storage_partition_impl.h"

#include "content/browser/blob_storage/blob_registry_wrapper.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"

namespace content {

void StoragePartitionImpl::UpdateBlobRegistryWithParentAsFallback(
    StoragePartitionImpl* fallback_for_blob_urls) {
  scoped_refptr<ChromeBlobStorageContext> blob_context =
      ChromeBlobStorageContext::GetFor(browser_context_);

  BlobRegistryWrapper* fallback_blob_registry =
      fallback_for_blob_urls ? fallback_for_blob_urls->GetBlobRegistry()
                             : nullptr;
  blob_registry_ = BlobRegistryWrapper::Create(
      blob_context, filesystem_context_, fallback_blob_registry);
}

}  // namespace content
