// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_session_importer.h"
#include <memory>

#include "base/test/task_environment.h"
#include "browser/sessions/vivaldi_session_utils.h"

#include "chrome/browser/sessions/session_service.h"
#include "components/sessions/core/session_command.h"
#include "sessions/index_storage.h"

#include "chromium/components/sessions/core/command_storage_backend.h"

#include "base/task/thread_pool.h"

namespace session_importer {

sessions::IdToSessionTab ChromiumSessionImporter::GetOpenTabs(
    const base::FilePath& profile_dir,
    importer::ImporterType importer_type) {
  sessions::SessionContent content;

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  scoped_refptr<sessions::CommandStorageBackend> backend =
      new sessions::CommandStorageBackend(
          task_runner, profile_dir,
          sessions::CommandStorageManager::kSessionRestore);

  auto unfiltered_cmds = backend->ReadLastSessionCommands().commands;
  auto commands = sessions::VivaldiFilterImportedTabsSessionCommands(
      unfiltered_cmds, importer_type == importer::TYPE_VIVALDI);

  sessions::TokenToSessionTabGroup tab_groups;
  sessions::VivaldiCreateTabsAndWindows(commands, &content.tabs, &tab_groups,
                                        &content.windows,
                                        &content.active_window_id);

  return std::move(content.tabs);
}

}  // namespace session_importer
