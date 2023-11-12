// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "content/public/browser/url_data_source.h"

namespace content {

bool URLDataSource::AllowCaching(const GURL& url) {
  return AllowCaching();
}

}
