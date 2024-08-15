// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef TRANSLATE_HISTORY_TH_STORAGE_H_
#define TRANSLATE_HISTORY_TH_STORAGE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/files/important_file_writer.h"
#include "base/memory/ref_counted.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserContext;
}

namespace translate_history {
class TH_Model;
}

namespace translate_history {

class TH_Storage : public base::ImportantFileWriter::DataSerializer {
 public:
  TH_Storage(content::BrowserContext* context, TH_Model* model);
  ~TH_Storage() override;
  TH_Storage(const TH_Storage&) = delete;
  TH_Storage& operator=(const TH_Storage&) = delete;

  // Schedules saving data to disk.
  void ScheduleSave();

  void SaveNow();

  void OnModelWillBeDeleted();

  // ImportantFileWriter::DataSerializer implementation.
  std::optional<std::string> SerializeData() override;

 private:
  friend class base::RefCountedThreadSafe<TH_Storage>;

  raw_ptr<TH_Model> model_;

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Path to the file where we can read and write data (in profile).
  base::ImportantFileWriter writer_;

  base::WeakPtrFactory<TH_Storage> weak_factory_;
};

}  // namespace translate_history

#endif  // TRANSLATE_HISTORY_TH_STORAGE_H_