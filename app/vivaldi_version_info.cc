// Copyright (c) 2015 Vivaldi Technologies.

#include "app/vivaldi_version_info.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/version_info/version_info_values.h"

namespace vivaldi {

std::string GetVivaldiVersionString() {
  return VIVALDI_VERSION_STRING;
}

const base::Version& GetVivaldiVersion() {
  static const base::NoDestructor<base::Version> version(VIVALDI_VERSION_STRING);
  DCHECK(version->IsValid());
  return *version;
}

bool IsBetaOrFinal() {
#if defined(OFFICIAL_BUILD) && \
    (BUILD_VERSION(VIVALDI_RELEASE) == VIVALDI_BUILD_PUBLIC_RELEASE)
  return true;
#else
  return false;
#endif
}

}  // namespace vivaldi
