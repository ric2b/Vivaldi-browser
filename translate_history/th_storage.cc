// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "translate_history/th_storage.h"

#include <memory>
#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/task/thread_pool.h"
#include "translate_history/th_codec.h"
#include "translate_history/th_constants.h"
#include "translate_history/th_model.h"

#if !BUILDFLAG(IS_IOS)
#include "content/public/browser/browser_context.h"
#else
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "ios/chrome/browser/shared/model/paths/paths.h"
#endif

namespace translate_history {

#if !BUILDFLAG(IS_IOS)
TH_Storage::TH_Storage(content::BrowserContext* context, TH_Model* model)
    : model_(model),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      writer_(context->GetPath().Append(kTranslateHistoryFileName),
              backend_task_runner_,
              base::Milliseconds(kSaveDelayMS)),
      weak_factory_(this) {}
#else
TH_Storage::TH_Storage(TH_Model* model)
    : model_(model),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      writer_(
          [] {
            base::FilePath path;
            if (base::PathService::Get(ios::DIR_USER_DATA, &path)) {
              path = path.Append(kTranslateHistoryFileName);
            }
            return path;
          }(),
          backend_task_runner_,
          base::Milliseconds(kSaveDelayMS)),
      weak_factory_(this) {

  // Create the user-data directory if needed (NOT the file).
  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](const base::FilePath& dir) {
            base::CreateDirectory(dir);
        },
        user_data_dir));
  }
}
#endif

TH_Storage::~TH_Storage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void TH_Storage::ScheduleSave() {
  writer_.ScheduleWrite(this);
}

void TH_Storage::SaveNow() {
  writer_.ScheduleWrite(this);
  if (writer_.HasPendingWrite()) {
    writer_.DoScheduledWrite();
  }
}

void TH_Storage::OnModelWillBeDeleted() {
  // We need to save now as otherwise by the time SaveNow is invoked
  // the model is gone.
  if (writer_.HasPendingWrite()) {
    writer_.DoScheduledWrite();
    DCHECK(!writer_.HasPendingWrite());
  }

  model_ = nullptr;
}

std::optional<std::string> TH_Storage::SerializeData() {
  TH_Codec codec;
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  base::Value value = codec.Encode(*model_->list());
  if (!serializer.Serialize(value))
    return std::nullopt;

  return output;
}

}  // namespace translate_history
