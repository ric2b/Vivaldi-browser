// Copyright (c) 2015 Vivaldi Technologies.

#include "app/vivaldi_version_info.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/version.h"
#include "build/build_config.h"

namespace vivaldi {

const base::Version& GetVivaldiVersion() {
  static const base::NoDestructor<base::Version> version(VIVALDI_VERSION_STRING);
  DCHECK(version->IsValid());
  return *version;
}

}  // namespace vivaldi
