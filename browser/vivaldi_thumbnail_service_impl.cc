// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_service_impl.h"

namespace thumbnails {

bool ThumbnailService::HasPageThumbnail(const GURL& url) {
  return false;
}

bool ThumbnailServiceImpl::HasPageThumbnail(const GURL& url) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);

  if (!local_ptr)
    return false;

  if (!local_ptr->IsKnownURL(url))
    return false;

  return local_ptr->HasPageThumbnail(url);
}

}  // namespace thumbnails

content::BrowserContext* ThumbnailServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return static_cast<Profile*>(context)->GetOriginalProfile();
}
