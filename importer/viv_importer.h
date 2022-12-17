// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_VIV_IMPORTER_H_
#define IMPORTER_VIV_IMPORTER_H_

#include <string>
#include <vector>

#include "chrome/common/importer/importer_data_types.h"
#include "chrome/utility/importer/importer.h"

namespace password_manager {
struct PasswordForm;
}

namespace viv_importer {
void DetectOperaProfiles(std::vector<importer::SourceProfile>* profiles);
void DetectThunderbirdProfiles(std::vector<importer::SourceProfile>* profiles);
}

class OperaImporter : public Importer {
 public:
  OperaImporter();

  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

 private:
  ~OperaImporter() override;
  OperaImporter(const OperaImporter&) = delete;
  OperaImporter& operator=(const OperaImporter&) = delete;

  // Returns false on error in which case error is filled with an error message.
  bool ImportBookMarks(std::string* error);

  // void ReadFaviconIndexFile(std::u16string domain, );

  bool ImportNotes(std::string* error);
  bool ImportSpeedDial(std::string* error);

  bool ImportWand(std::string* error);
  bool ImportWand_ReadEntryHTML(
      std::string::iterator* buffer,
      const std::string::iterator& buffer_end,
      std::vector<importer::ImportedPasswordForm>* passwords,
      bool ignore_entry = false);
  bool ImportWand_ReadEntryAuth(
      std::string::iterator* buffer,
      const std::string::iterator& buffer_end,
      std::vector<importer::ImportedPasswordForm>* passwords,
      bool ignore_entry = false);
  bool GetMasterPasswordInfo();

 private:
  base::FilePath profile_dir_;
  base::FilePath::StringType bookmarkfilename_;
  base::FilePath::StringType notesfilename_;
  base::FilePath::StringType wandfilename_;
  base::FilePath::StringType masterpassword_filename_;

  uint32_t wand_version_;
  bool master_password_required_;

  std::u16string master_password_;

  std::string master_password_block_;

  /*
  struct favicon_item {
    uint index;
    std::u16string favicon_filename;
  };

  unordered_map<std::u16string, pair<std::u16string, >>;
  */
};

#endif  // IMPORTER_VIV_IMPORTER_H_
