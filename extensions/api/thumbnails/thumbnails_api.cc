//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/thumbnails/thumbnails_api.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

bool ThumbnailsIsThumbnailAvailableFunction::RunAsync() {
  std::unique_ptr<vivaldi::thumbnails::IsThumbnailAvailable::Params> params(
      vivaldi::thumbnails::IsThumbnailAvailable::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<thumbnails::ThumbnailService> service(
      ThumbnailServiceFactory::GetForProfile(profile));
  GURL url;
  if (!params->bookmark_id.empty()) {
    std::string url_str = "http://bookmark_thumbnail/" + params->bookmark_id;
    GURL local(url_str);
    url.Swap(&local);
  } else if (!params->url.empty()) {
    GURL local(params->url);
    url.Swap(&local);
  }
  bool has_thumbnail = service->HasPageThumbnail(url);
  vivaldi::thumbnails::ThumbnailQueryResult result;
  result.has_thumbnail = has_thumbnail;
  result.thumbnail_url = url.spec();

  results_ = vivaldi::thumbnails::IsThumbnailAvailable::Results::Create(result);

  SendResponse(true);

  return true;
}

ThumbnailsIsThumbnailAvailableFunction::
    ThumbnailsIsThumbnailAvailableFunction() {
}

ThumbnailsIsThumbnailAvailableFunction::
    ~ThumbnailsIsThumbnailAvailableFunction() {
}

}  // namespace extensions
