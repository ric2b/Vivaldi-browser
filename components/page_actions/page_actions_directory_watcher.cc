// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/page_actions/page_actions_directory_watcher.h"

#include "base/files/file_enumerator.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"

#ifdef OS_ANDROID
#include "base/android/apk_assets.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#endif

namespace page_actions {
namespace {
const base::FilePath::CharType kCSSExtension[] = FILE_PATH_LITERAL(".css");
const base::FilePath::CharType kJSExtension[] = FILE_PATH_LITERAL(".js");

constexpr base::TimeDelta kUpdateCooldownTime =
    base::TimeDelta::FromMilliseconds(500);

std::unique_ptr<base::Value> ReadApkAssets(base::FilePath apk_assets) {
#ifdef OS_ANDROID
  base::MemoryMappedFile::Region region;
  base::MemoryMappedFile mapped_file;
  int json_fd = base::android::OpenApkAsset(apk_assets.AsUTF8Unsafe(), &region);
  if (json_fd < 0) {
    LOG(ERROR) << "Page actions resources not found in APK assets.";
    return nullptr;
  }

  if (!mapped_file.Initialize(base::File(json_fd), region)) {
    LOG(ERROR) << "failed to initialize memory mapping for " << apk_assets;
    return nullptr;
  }

  base::StringPiece json_text(reinterpret_cast<char*>(mapped_file.data()),
                              mapped_file.length());
  JSONStringValueDeserializer deserializer(json_text);
  return deserializer.Deserialize(nullptr, nullptr);
#else
  return nullptr;
#endif
}

void GetApkAssets(base::FilePath& apk_assets,
                  DirectoryWatcher::UpdatedFileContents& update_contents) {
  std::unique_ptr<base::Value> asset_contents = ReadApkAssets(apk_assets);
  if (!asset_contents || !asset_contents->is_dict()) {
    return;
  }

  base::FilePath apk_dir = apk_assets.DirName();

  for (auto content : asset_contents->DictItems()) {
    if (!content.second.is_string())
      continue;
    update_contents[apk_dir].emplace(std::make_pair(
        apk_dir.Append(base::FilePath::FromUTF8Unsafe(content.first)),
        std::move(content.second.GetString())));
  }
}

}  // namespace

DirectoryWatcher::DirectoryWatcher(ChangesCallback callback,
                                   base::FilePath apk_assets)
    : callback_(std::move(callback)),
      callback_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      apk_assets_(apk_assets),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DirectoryWatcher::~DirectoryWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DirectoryWatcher::Deleter::operator()(const DirectoryWatcher* ptr) {
  ptr->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DirectoryWatcher::Destroy, base::Unretained(ptr)));
}

void DirectoryWatcher::AddPaths(const std::vector<base::FilePath>& paths) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DirectoryWatcher::DoAddPaths,
                                        base::Unretained(this), paths));
}

void DirectoryWatcher::RemovePath(const base::FilePath& path) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DirectoryWatcher::DoRemovePath,
                                        base::Unretained(this), path));
}

void DirectoryWatcher::DoAddPaths(const std::vector<base::FilePath>& paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We need to be able to watch recursively to watch the files contained in a
  // folder. If we can't then we just do the initial reading of the folder and
  // skip the watching functionality.
  if (!base::FilePathWatcher::RecursiveWatchAvailable()) {
    std::copy(paths.begin(), paths.end(),
              std::inserter(pending_paths_, pending_paths_.end()));
    ReportChanges();
    return;
  }

  for (const auto& path : paths) {
    DCHECK(path_watchers_.count(path) == 0);
    pending_paths_.insert(path);
    auto watcher = std::make_unique<base::FilePathWatcher>();
    // Unretained OK because we own the watcher.
    if (!watcher->Watch(path, base::FilePathWatcher::Type::kRecursive,
                        base::BindRepeating(&DirectoryWatcher::OnPathChanged,
                                            base::Unretained(this))))
      continue;
    path_watchers_.insert({path, std::move(watcher)});
  }
  ReportChanges();
}

void DirectoryWatcher::DoRemovePath(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  path_watchers_.erase(path);
  file_path_times_.erase(path);
}

void DirectoryWatcher::OnPathChanged(const base::FilePath& path, bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_paths_.insert(path);
  ReportChanges();
}

void DirectoryWatcher::ReportChanges() {
  if (in_update_cooldown_)
    return;

  in_update_cooldown_ = true;

  UpdatedFileContents updated_contents;
  std::vector<base::FilePath> invalid_paths;

  for (const auto& path : pending_paths_) {
    if (!base::DirectoryExists(path)) {
      invalid_paths.push_back(path);
      continue;
    }

    FilePathTimesMap& old_times = file_path_times_[path];
    FilePathTimesMap current_times = GetModificationTimes(path);
    for (const auto& path_time : current_times) {
      const base::FilePath& file_path = path_time.first;
      auto old_timestamp = old_times.find(file_path);
      if (old_timestamp == old_times.end() ||
          old_timestamp->second != path_time.second)
        base::ReadFileToString(file_path, &updated_contents[path][file_path]);
    }
    for (const auto& path_time : old_times) {
      const base::FilePath& file_path = path_time.first;
      if (current_times.find(file_path) == current_times.end())
        updated_contents[path][file_path] = "";
    }
    old_times.swap(current_times);
  }
  pending_paths_.clear();

  if (!apk_assets_.empty()) {
    GetApkAssets(apk_assets_, updated_contents);
    // We only need to report the bundled in scripts once
    apk_assets_.clear();
  }

  if (!updated_contents.empty())
    callback_task_runner_->PostTask(
        FROM_HERE, base::Bind(callback_, std::move(updated_contents),
                              std::move(invalid_paths)));

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DirectoryWatcher::end_update_cooldown,
                     base::Unretained(this)),
      kUpdateCooldownTime);
}

DirectoryWatcher::FilePathTimesMap DirectoryWatcher::GetModificationTimes(
    const base::FilePath& path) {
  FilePathTimesMap times_map;
  base::FileEnumerator enumerator(path, false, base::FileEnumerator::FILES);
  base::FilePath file_path = enumerator.Next();
  while (!file_path.empty()) {
    if (!file_path.MatchesExtension(kCSSExtension) &&
        !file_path.MatchesExtension(kJSExtension))
      continue;
    base::FileEnumerator::FileInfo file_info = enumerator.GetInfo();
    DCHECK(file_path.DirName() == path);
    times_map[file_path] = file_info.GetLastModifiedTime();
    file_path = enumerator.Next();
  }
  return times_map;
}

}  // namespace page_actions
