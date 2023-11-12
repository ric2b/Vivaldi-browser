// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRANSLATE_HISTORY_TH_MODEL_LOADER_H_
#define TRANSLATE_HISTORY_TH_MODEL_LOADER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "translate_history/th_node.h"  // For NodeList

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace translate_history {

class TH_LoadDetails {
 public:
  explicit TH_LoadDetails(NodeList* list);
  ~TH_LoadDetails();
  TH_LoadDetails(const TH_LoadDetails&) = delete;
  TH_LoadDetails& operator=(const TH_LoadDetails&) = delete;

  NodeList* list() { return list_.get(); }

  std::unique_ptr<NodeList> release_list() { return std::move(list_); }

 private:
  std::unique_ptr<NodeList> list_;
};

// Created by the model to implement loading of its data.
// May be used on multiple threads. May outlive the model
class TH_ModelLoader : public base::RefCountedThreadSafe<TH_ModelLoader> {
 public:
  using LoadCallback =
      base::OnceCallback<void(std::unique_ptr<TH_LoadDetails>)>;

  static scoped_refptr<TH_ModelLoader> Create(
      const base::FilePath& profile_path,
      std::unique_ptr<TH_LoadDetails> details,
      LoadCallback callback);

  // Blocks until loaded. This is intended for usage on a thread other than
  // the main thread.
  void BlockTillLoaded();

 private:
  friend base::RefCountedThreadSafe<TH_ModelLoader>;
  TH_ModelLoader();
  ~TH_ModelLoader();
  TH_ModelLoader(const TH_ModelLoader&) = delete;
  TH_ModelLoader& operator=(const TH_ModelLoader&) = delete;

  // Performs the load on a background thread.
  std::unique_ptr<TH_LoadDetails> DoLoadOnBackgroundThread(
      const base::FilePath& profile_path,
      std::unique_ptr<TH_LoadDetails> details);

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Signaled once loading completes.
  base::WaitableEvent loaded_signal_;
};

}  // namespace translate_history

#endif  // TRANSLATE_HISTORY_TH_MODEL_LOADER_H_
