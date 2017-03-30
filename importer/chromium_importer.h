// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "chrome/utility/importer/importer.h"
#include "chrome/common/importer/importer_data_types.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form.h"
#include "chrome/common/importer/importer_url_row.h"

#ifndef  VIVALDI_CHROMIUM_IMPORTER_H_
#define VIVALDI_CHROMIUM_IMPORTER_H_


class ImportedNoteEntry;

class ChromiumImporter : public Importer
{
 public:
   ChromiumImporter();
   ChromiumImporter(const importer::ImportConfig &import_config);
   void ImportPasswords();

   // Importer
   void StartImport(const importer::SourceProfile& source_profile,
    uint16_t items,
    ImporterBridge* bridge) override;


 protected:
   ~ChromiumImporter() override;

 private:
  base::FilePath profile_dir_;
  base::FilePath::StringType bookmarkfilename_;
  void ImportBookMarks();
  void ImportHistory();
  bool ReadAndParseSignons(const base::FilePath& sqlite_file,
    std::vector<autofill::PasswordForm>* forms);

  bool ReadAndParseHistory(const base::FilePath& sqlite_file,
    std::vector<ImporterURLRow>* forms);

  DISALLOW_COPY_AND_ASSIGN(ChromiumImporter);
};

#endif // VIVALDI_CHROMIUM_IMPORTER_H_
