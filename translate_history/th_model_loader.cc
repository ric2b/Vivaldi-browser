// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "translate_history/th_model_loader.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "translate_history/th_codec.h"
#include "translate_history/th_constants.h"

namespace translate_history {

namespace {

void Load(const base::FilePath& profile_path, TH_LoadDetails* details) {
  base::FilePath path = profile_path.Append(kTranslateHistoryFileName);
  bool exists = base::PathExists(path);
  if (exists) {
    JSONFileValueDeserializer serializer(path);
    std::unique_ptr<base::Value> root(serializer.Deserialize(NULL, NULL));
    if (!root.get()) {
      LOG(ERROR) << "Translate history: Failed to parse JSON. Check format";
      std::string content;
      base::ReadFileToString(path, &content);
      LOG(ERROR) << "Translate Storage: Content: " << content;
    } else {
      TH_Codec codec;
      if (!codec.Decode(*details->list(), *root.get())) {
        LOG(ERROR) << "Translate history: Failed to decode JSON content from: "
                   << path;
      }
    }
  }
}

}  // namespace

TH_LoadDetails::TH_LoadDetails(NodeList* list) : list_(list) {}

TH_LoadDetails::~TH_LoadDetails() = default;

// static
scoped_refptr<TH_ModelLoader> TH_ModelLoader::Create(
    const base::FilePath& profile_path,
    std::unique_ptr<TH_LoadDetails> details,
    LoadCallback callback) {
  // Note: base::MakeRefCounted is not available here, as ModelLoader's
  // constructor is private.
  auto model_loader = base::WrapRefCounted(new TH_ModelLoader());
  model_loader->backend_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  model_loader->backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TH_ModelLoader::DoLoadOnBackgroundThread, model_loader,
                     profile_path, std::move(details)),
      std::move(callback));
  return model_loader;
}

void TH_ModelLoader::BlockTillLoaded() {
  loaded_signal_.Wait();
}

TH_ModelLoader::TH_ModelLoader()
    : loaded_signal_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}

TH_ModelLoader::~TH_ModelLoader() = default;

std::unique_ptr<TH_LoadDetails> TH_ModelLoader::DoLoadOnBackgroundThread(
    const base::FilePath& profile_path,
    std::unique_ptr<TH_LoadDetails> details) {
  Load(profile_path, details.get());
  loaded_signal_.Signal();
  return details;
}
}  // namespace translate_history
