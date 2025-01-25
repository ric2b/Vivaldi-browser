// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_CHROMIUM_SESSION_IMPORTER_H
#define IMPORTER_CHROMIUM_SESSION_IMPORTER_H

#include "base/memory/weak_ptr.h"
#include "base/memory/raw_ptr.h"
#include "base/files/file_path.h"
#include "chrome/common/importer/importer_type.h"
#include "components/sessions/vivaldi_session_service_commands.h"

class Profile;
class ExternalProcessImporterHost;

namespace session_importer {

class ChromiumSessionImporter {
 public:
  ChromiumSessionImporter(Profile* profile,
                          base::WeakPtr<ExternalProcessImporterHost> host);

  static sessions::IdToSessionTab GetOpenTabs(
      const base::FilePath& profile_dir,
      importer::ImporterType importer_type);

 private:
  const raw_ptr<Profile> profile_;
  const base::WeakPtr<ExternalProcessImporterHost> host_;
};

}  // namespace session_importer

#endif /* IMPORTER_CHROMIUM_SESSION_IMPORTER_H */
