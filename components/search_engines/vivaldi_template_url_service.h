// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_VIVALDI_TEMPLATE_URL_SERVICE_H_
#define COMPONENTS_SEARCH_ENGINES_VIVALDI_TEMPLATE_URL_SERVICE_H_

#include <string>

#include "components/search_engines/template_url_service.h"

class TemplateURL;

namespace vivaldi {

syncer::UniquePosition::Suffix VivaldiGetPositionSuffix(
    const TemplateURL* template_url);
const char* VivaldiGetDefaultProviderGuidPrefForType(
  TemplateURLService::DefaultSearchType type);

}  // namespace vivaldi

#endif  // COMPONENTS_SEARCH_ENGINES_VIVALDI_TEMPLATE_URL_SERVICE_H_
