// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_service_impl.h"

content::BrowserContext* ThumbnailServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return static_cast<Profile*>(context)->GetOriginalProfile();
}
