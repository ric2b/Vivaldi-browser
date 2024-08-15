// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "translate_history/th_storage.h"

#include <memory>
#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_context.h"
#include "translate_history/th_codec.h"
#include "translate_history/th_constants.h"
#include "translate_history/th_model.h"

namespace translate_history {

TH_Storage::TH_Storage(content::BrowserContext* context, TH_Model* model)
    : model_(model),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      writer_(context->GetPath().Append(kTranslateHistoryFileName),
              backend_task_runner_,
              base::Milliseconds(kSaveDelayMS)),
      weak_factory_(this) {}

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