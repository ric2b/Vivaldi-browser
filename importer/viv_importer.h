// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "chrome/utility/importer/importer.h"

#ifndef  VIVALDI_IMPORTER_VIV_IMPORTER_H_
#define VIVALDI_IMPORTER_VIV_IMPORTER_H_

struct ImportedBookmarkEntry;

namespace base {
class DictionaryValue;
}

namespace autofill {
struct PasswordForm;
}

namespace viv_importer {
void DetectOperaProfiles(std::vector<importer::SourceProfile*>* profiles);
}

class ImportedNoteEntry;

class OperaImporter : public Importer
{
 public:
  OperaImporter(const importer::ImportConfig &import_config);

  void StartImport(const importer::SourceProfile& source_profile,
                           uint16 items,
                           ImporterBridge* bridge) override;

 private:
  ~OperaImporter() override;

  void ImportBookMarks();

  //void ReadFaviconIndexFile(base::string16 domain, );

  void ImportNotes();

  void ImportWand();
  bool ImportWand_ReadEntryHTML(std::string::iterator &buffer,
                                std::string::iterator &buffer_end,
                                std::vector<autofill::PasswordForm> &passwords,
                                bool ignore_entry=false);
  bool ImportWand_ReadEntryAuth(std::string::iterator &buffer,
                                std::string::iterator &buffer_end,
                                std::vector<autofill::PasswordForm> &passwords,
                                bool ignore_entry=false);
  bool GetMasterPasswordInfo();

 private:

  base::FilePath profile_dir_;
  base::FilePath::StringType bookmarkfilename_;
  base::FilePath::StringType notesfilename_;
  base::FilePath::StringType wandfilename_;
  base::FilePath::StringType masterpassword_filename_;

  uint32 wand_version_;
  bool master_password_required;

  base::string16 master_password;

  std::string master_password_block;

  /*
  struct favicon_item {
    uint index;
    base::string16 favicon_filename;
  };

  unordered_map<base::string16, pair<base::string16, >>;
  */

  DISALLOW_COPY_AND_ASSIGN(OperaImporter);
};


#endif // VIVALDI_IMPORTER_VIV_IMPORTER_H_
