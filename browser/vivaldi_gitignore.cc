// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

namespace {
const char gitignore_content[] =
    "# Vivaldi added this file.\n#\n"
    "# Prevent the profile from being silently added to a git repo\n#\n"
    "# Uploading the profile to an online repository, e.g. on GitHub, can "
    "leak\n"
    "# sensitive information such as passwords, cookies, and other data.\n"
    "# It is still possible to commit the files to a repo, but you have to\n"
    "# force Git to ignore this entry.\n\n"
    "*\n";
}

typedef base::OnceCallback<void(base::FilePath)> SetPrefCallback;

// Must be run on the ui thread.
void SetShowGitDirectoryWarningOnActiveProfile(base::FilePath git_root) {
#if !BUILDFLAG(IS_ANDROID)
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Profile* profile = ProfileManager::GetLastUsedProfile();
  PrefService* pref_service = profile->GetPrefs();
  pref_service->SetFilePath(vivaldiprefs::kStartupShowGitDirectoryWarning,
                            git_root);
#endif
}

void CheckForGitIgnoreOnIO(SetPrefCallback callback) {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

  base::FilePath gitignore_file =
      user_data_dir.Append(FILE_PATH_LITERAL(".gitignore"));
  if (base::PathExists(gitignore_file))
    return;

  base::WriteFile(gitignore_file, gitignore_content);

  LOG(INFO) << "Added .gitignore file to user data dir "
            << user_data_dir.AsUTF8Unsafe();

#if !BUILDFLAG(IS_ANDROID)
  // Check if this user data dir may already be inside a git repo
  base::FilePath dir = user_data_dir;
  while (dir != dir.DirName()) {
    base::FilePath git_file = dir.Append(FILE_PATH_LITERAL(".git"));
    if (base::PathExists(git_file)) {
      LOG(INFO) << "The profile folders in " << user_data_dir.AsUTF8Unsafe()
                 << " is inside a Git Repo found at " << dir.AsUTF8Unsafe()
                 << " . This may leak sensitive information.";

      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), dir));
      break;
    }

    dir = dir.DirName();
  }
#endif
}

void StartGitIgnoreCheck() {
  auto setprefs_callback =
      base::BindOnce(&SetShowGitDirectoryWarningOnActiveProfile);
  base::ThreadPool::PostTask(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&CheckForGitIgnoreOnIO, std::move(setprefs_callback)));
}

}  // namespace vivaldi