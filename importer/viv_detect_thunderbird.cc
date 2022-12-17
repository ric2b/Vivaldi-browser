// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "importer/viv_importer.h"

#include "app/vivaldi_resources.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "importer/viv_importer_utils.h"
#include "ui/base/l10n/l10n_util.h"

using base::PathService;

namespace viv_importer {

void DetectThunderbirdProfiles(std::vector<importer::SourceProfile>* profiles) {
  importer::SourceProfile thunderbird;
  thunderbird.importer_name =
      l10n_util::GetStringUTF16(IDS_IMPORT_FROM_THUNDERBIRD);
  thunderbird.importer_type = importer::TYPE_THUNDERBIRD;
  thunderbird.mail_path = GetThunderbirdMailDirectory();
  thunderbird.source_path = GetThunderbirdMailDirectory();
  thunderbird.services_supported = importer::EMAIL | importer::CONTACTS;

  profiles->push_back(thunderbird);
}
}  // namespace viv_importer
