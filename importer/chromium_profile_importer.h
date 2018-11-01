// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_CHROMIUM_PROFILE_IMPORTER_H_
#define IMPORTER_CHROMIUM_PROFILE_IMPORTER_H_

#include <vector>

#include "base/values.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/importer_type.h"
#include "chrome/utility/importer/importer.h"
#include "components/autofill/core/common/password_form.h"

using autofill::PasswordForm;

struct ChromiumProfile {
  ChromiumProfile();
  importer::ImporterType importer_type;
  int import_name_resource_idx;
};

class ImportedNoteEntry;

class ChromiumProfileImporter {
 public:
  ChromiumProfileImporter();
  ~ChromiumProfileImporter();
  void DetectChromiumProfiles(std::vector<importer::SourceProfile>* profiles);

 private:
  ChromiumProfile GetChromeProfile(importer::ImporterType importerType);
  void ReadProfiles(std::vector<importer::ChromeProfileInfo>* cp,
                    base::FilePath profileDirectory);
  std::vector<ChromiumProfile> chromeProfiles;

  DISALLOW_COPY_AND_ASSIGN(ChromiumProfileImporter);
};

#endif  // IMPORTER_CHROMIUM_PROFILE_IMPORTER_H_
