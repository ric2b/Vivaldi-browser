// This header was originally generated from the
// prepopulated_engines_schema.json file. Due to VB-102933, it became necessary
// to move it under the Vivaldi umbrella.
//
// Specifically, the strings could no longer remain as 'const char* const' because
// it would be impossible to initialize them in any way other than by assigning
// hard-coded strings at startup.
//
#ifndef COMPONENTS_SEARCH_ENGINES_PREPOPULATED_ENGINES_H_
#define COMPONENTS_SEARCH_ENGINES_PREPOPULATED_ENGINES_H_

#include "base/containers/span.h"

#include "third_party/search_engines_data/search_engines.h"
// Unused include but it limits the chromium side include changes
#include "third_party/search_engines_data/regional_settings.h"

namespace TemplateURLPrepopulateData {

extern base::span<const PrepopulatedEngine* const> kAllEngines;
}  // namespace TemplateURLPrepopulateData

#endif // COMPONENTS_SEARCH_ENGINES_PREPOPULATED_ENGINES_H_
