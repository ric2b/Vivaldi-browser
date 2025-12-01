// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_CHROMIUM_IMPORTER_H_
#define IMPORTER_CHROMIUM_IMPORTER_H_

#include <vector>

#include "base/values.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "components/user_data_importer/common/importer_type.h"
#include "components/user_data_importer/common/importer_url_row.h"
#include "chrome/utility/importer/importer.h"
#include "components/password_manager/core/browser/password_form.h"

class ImportedNoteEntry;

class ChromiumImporter : public Importer {
 public:
  ChromiumImporter();
  void ImportPasswords(user_data_importer::ImporterType importer_type);

  // Importer
  void StartImport(const user_data_importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

 protected:
  ~ChromiumImporter() override;
  ChromiumImporter(const ChromiumImporter&) = delete;
  ChromiumImporter& operator=(const ChromiumImporter&) = delete;

 private:
  base::FilePath profile_dir_;
  base::FilePath::StringType bookmarkfilename_;
  void ImportBookMarks();
  void ImportHistory();
  void ImportExtensions();
  void ImportTabs(user_data_importer::ImporterType importer_type);

  bool ReadAndParseSignons(const base::FilePath& sqlite_file,
      std::vector<user_data_importer::ImportedPasswordForm>* forms,
      user_data_importer::ImporterType importer_type);

  bool ReadAndParseHistory(const base::FilePath& sqlite_file,
      std::vector<user_data_importer::ImporterURLRow>* forms);
};

#endif  // IMPORTER_CHROMIUM_IMPORTER_H_
