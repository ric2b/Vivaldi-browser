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

#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/regulatory_extension_type.h"
#include "components/search_engines/original/prepopulated_engines.h"

namespace TemplateURLPrepopulateData {

using RegulatoryExtension = TemplateURLPrepopulateDataOriginal::RegulatoryExtension;
struct PrepopulatedEngine : public TemplateURLPrepopulateDataOriginal::PrepopulatedEngine {};

extern const PrepopulatedEngine google;

extern base::span<const PrepopulatedEngine* const> kAllEngines;
}  // namespace TemplateURLPrepopulateData

#endif // COMPONENTS_SEARCH_ENGINES_PREPOPULATED_ENGINES_H_
