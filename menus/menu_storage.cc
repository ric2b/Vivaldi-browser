// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "menus/menu_storage.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "menus/menu_codec.h"
#include "menus/menu_model.h"
#include "menus/menu_node.h"
#include "prefs/vivaldi_browser_prefs_util.h"

namespace menus {

const base::FilePath::CharType kVivaldiResourcesFolder[] =
    FILE_PATH_LITERAL("vivaldi");
const base::FilePath::CharType kMenuFolder[] =
    FILE_PATH_LITERAL("menus");
const base::FilePath::CharType kFileName[] =
    FILE_PATH_LITERAL("mainmenu.json");
const base::FilePath::CharType kBackupExtension[] =
    FILE_PATH_LITERAL("bak");

// How often we save.
const int kSaveDelayMS = 2500;

// Generator for setting up guids. Useful for development. Use in OnLoad for the
// bundles file if necessary.
/*
void MakeGuids(const base::FilePath& path) {
  FILE *fp = fopen(path.AsUTF8Unsafe().c_str(), "r");
  if (fp) {
    char buf[1000];
    while (fgets(buf, 999, fp)) {
      printf("%s", buf);
      char* s = strstr(buf, "\"action\":");
      if (s) {
        sprintf(&buf[s-buf], "\"guid\": \"%s\",", base::GenerateGUID().c_str());
        puts(buf);
      }
    }
    fclose(fp);
  }
}
*/

// Make a backup. Note, this file only exists during startup, It is deleted
// elsewhere once startup completes.
void MakeBackup(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

std::string GetVersion(const base::FilePath& file) {
  std::string version;
  bool exists = base::PathExists(file);
  if (exists) {
    JSONFileValueDeserializer serializer(file);
    std::unique_ptr<base::Value> root(serializer.Deserialize(NULL, NULL));
    if (root.get()) {
      MenuCodec codec;
      codec.GetVersion(&version, *root.get());
    }
  }
  return version;
}

void OnLoad(const base::FilePath& profile_file,
            const base::FilePath& bundled_file,
            const base::WeakPtr<menus::MenuStorage> storage,
            std::unique_ptr<MenuLoadDetails> details) {
  bool exists;
  const base::FilePath* file;

  if (details->force_bundle()) {
    // Revert to default while running.
    file = &bundled_file;
    exists = base::PathExists(*file);
  } else  {
    // Check for upgrade. There is a version number in the bundled file which
    // is saved in the profile based. If the version is stepped we will attempt
    // an upgrade of the profile based file.
    std::string bundled_version = GetVersion(bundled_file);
    std::string profile_version = GetVersion(profile_file);
    if (!bundled_version.empty() && bundled_version != profile_version) {
      // TODO: Handle regular upgrade.
    }

    // Load on startup. Read from profile file if exists, otherwise bundled.
    file = &profile_file;
    exists = base::PathExists(*file);
    if (!exists) {
      file = &bundled_file;
      exists = base::PathExists(*file);
    }
  }

  if (!exists) {
    if (details->force_bundle()/*is_reset()*/) {
      LOG(ERROR) << "Menu Storage: File does not exist: " << bundled_file;
    } else {
      LOG(ERROR) << "Menu Storage: No files exists:\n"
          << profile_file
          << "\n"
          << bundled_file;
    }
  } else {
    JSONFileValueDeserializer serializer(*file);
    std::unique_ptr<base::Value> root(serializer.Deserialize(NULL, NULL));
    if (!root.get()) {
      LOG(ERROR) << "Menu Storage: Failed to parse JSON. Check format";
      std::string content;
      base::ReadFileToString(*file, &content);
      LOG(ERROR) << "Menu Storage: Content: " << content;
    } else {
      MenuCodec codec;
      if (!codec.Decode(details->root_node(), details->control(),
                        *root.get(), file == &bundled_file)) {
        LOG(ERROR) << "Menu Storage: Failed to decode JSON content from: "
                   << *file;
      }
    }
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                        base::Bind(&MenuStorage::OnLoadFinished, storage,
                                   base::Passed(&details)));
}

MenuLoadDetails::MenuLoadDetails(Menu_Node* root, Menu_Control* control,
                                 int64_t id, bool force_bundle)
  : root_(root),
    control_(control),
    id_(id),
    force_bundle_(force_bundle) {}

MenuLoadDetails::MenuLoadDetails(Menu_Node* root, Menu_Control* control,
                                 const std::string& menu, bool force_bundle)
  : root_(root),
    control_(control),
    id_(-1),
    force_bundle_(force_bundle),
    menu_(menu) {}

MenuLoadDetails::~MenuLoadDetails() {}


MenuStorage::MenuStorage(content::BrowserContext* context,
                         Menu_Model* model,
                         base::SequencedTaskRunner* sequenced_task_runner)
  : model_(model),
    writer_(context->GetPath().Append(kFileName), sequenced_task_runner,
            base::TimeDelta::FromMilliseconds(kSaveDelayMS)),
    weak_factory_(this) {
  // Set up bundled file. We will use a developer version of this file if
  // appropriate (non official build and using load-and-launch-app) to simplify
  // testing and general workflow. Note that if there is a proper file present
  // in the profile it will overrule this bundled file regardless of type.
  if (!vivaldi::GetDeveloperFilePath(kFileName, &bundled_file_)) {
    base::PathService::Get(chrome::DIR_RESOURCES, &bundled_file_);
    bundled_file_ = bundled_file_.Append(kVivaldiResourcesFolder)
                                 .Append(kMenuFolder)
                                 .Append(kFileName);
  }

  sequenced_task_runner_ = sequenced_task_runner;
  sequenced_task_runner_->PostTask(FROM_HERE,
                                   base::Bind(&MakeBackup, writer_.path()));
}

MenuStorage::~MenuStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void MenuStorage::Load(std::unique_ptr<MenuLoadDetails> details) {
  DCHECK(details);
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&OnLoad, writer_.path(), bundled_file_,
                 weak_factory_.GetWeakPtr(), base::Passed(&details)));
}

void MenuStorage::ScheduleSave() {
  switch (backup_state_) {
    case BACKUP_NONE:
      backup_state_ = BACKUP_DISPATCHED;
      sequenced_task_runner_->PostTaskAndReply(
          FROM_HERE, base::Bind(&MakeBackup, writer_.path()),
          base::BindOnce(&MenuStorage::OnBackupFinished,
                         weak_factory_.GetWeakPtr()));
      return;
    case BACKUP_DISPATCHED:
      return;
    case BACKUP_ATTEMPTED:
      writer_.ScheduleWrite(this);
      return;
  }
  NOTREACHED();
}

void MenuStorage::OnBackupFinished() {
  backup_state_ = BACKUP_ATTEMPTED;
  ScheduleSave();
}

void MenuStorage::OnModelWillBeDeleted() {
  // We need to save now as otherwise by the time SaveNow is invoked
  // the model is gone.
  if (writer_.HasPendingWrite())
    SaveNow();

  model_ = NULL;
}

bool MenuStorage::SerializeData(std::string* output) {
  MenuCodec codec;
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  base::Value value = codec.Encode(model_);
  return serializer.Serialize(value);
}

void MenuStorage::OnLoadFinished(std::unique_ptr<MenuLoadDetails> details) {
  if (!model_)
    return;

  model_->LoadFinished(std::move(details));
}

bool MenuStorage::SaveNow() {
  if (!model_ || !model_->loaded()) {
    // We should only get here if we have a valid model and it's finished
    // loading.
    NOTREACHED();
    return false;
  }

  std::unique_ptr<std::string> data(new std::string);
  if (!SerializeData(data.get()))
    return false;
  writer_.WriteNow(std::move(data));
  return true;
}

}  // namespace menus
