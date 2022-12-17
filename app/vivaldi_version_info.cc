// Copyright (c) 2015 Vivaldi Technologies.

#include "app/vivaldi_version_info.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/version_info/version_info_values.h"

namespace vivaldi {

std::string GetVivaldiVersionString() {
  return VIVALDI_VERSION;
}

const base::Version& GetVivaldiVersion() {
  static const base::NoDestructor<base::Version> version(VIVALDI_VERSION);
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
std::string GetVivaldiMailVersionString() {
  return VIVALDI_MAIL_VERSION;
}
#endif

}  // namespace vivaldi
