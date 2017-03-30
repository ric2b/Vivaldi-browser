// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/common/importer/importer_data_types.h"

namespace importer {

SourceProfile::SourceProfile()
    : importer_type(TYPE_UNKNOWN),
      services_supported(0) {
}

SourceProfile::~SourceProfile() {
}

#if defined(OS_WIN)
ImporterIE7PasswordInfo::ImporterIE7PasswordInfo() {
}

ImporterIE7PasswordInfo::~ImporterIE7PasswordInfo() {
}
#endif

ImportConfig::ImportConfig()
    : imported_items(0) {
}

ImportConfig::~ImportConfig() {
  // (Attempt to) wipe parameter string
  for (std::vector<base::string16>::iterator it= arguments.begin(); it != arguments.end(); it++){
    it->clear();
  }
}

}  // namespace importer
