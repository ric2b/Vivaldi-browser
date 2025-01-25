// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "menus/menu_storage.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#include "components/datasource/resource_reader.h"
#include "menus/menu_codec.h"
#include "menus/menu_model.h"
#include "menus/menu_node.h"
#include "menus/menu_upgrade.h"

namespace menus {

const base::FilePath::CharType kMenuFolder[] = FILE_PATH_LITERAL("menus");
const base::FilePath::CharType kMainMenuFileName[] =
    FILE_PATH_LITERAL("mainmenu.json");
const base::FilePath::CharType kContextMenuFileName[] =
    FILE_PATH_LITERAL("contextmenu.json");
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

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
        sprintf(&buf[s-buf], "\"guid\": \"%s\",", base::Uuid::GenerateRandomV4().c_str());
        puts(buf);
      }
    }
    fclose(fp);
  }
} Menu_Model* model,
*/

const base::FilePath::CharType* GetFileName(Menu_Model* model) {
  if (model->mode() == Menu_Model::kMainMenu) {
    return kMainMenuFileName;
  } else {
    return kContextMenuFileName;
  }
}

// Make a backup. Note, this file only exists during startup, It is deleted
// elsewhere once startup completes.
void MakeBackup(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

bool GetVersion(std::string* version, const base::FilePath& file) {
  bool exists = base::PathExists(file);
  if (exists) {
    JSONFileValueDeserializer serializer(file);
    std::unique_ptr<base::Value> root(serializer.Deserialize(NULL, NULL));
    if (root.get()) {
      MenuCodec codec;
      return codec.GetVersion(version, *root.get());
    }
  }
  return false;
}

bool HasVersionStepped(const std::string& bundled_version,
                       const std::string& profile_version) {
  if (bundled_version != profile_version) {
    std::vector<std::string> bundled_array = base::SplitString(
        bundled_version, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    std::vector<std::string> profile_array = base::SplitString(
        profile_version, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (bundled_array.size() != 4) {
      LOG(ERROR) << "Menu Storage: Failed to upgrade, illegal bundled version: "
                 << bundled_version;
      return false;  // Should never happen. We have full control of the string.
    }
    if (profile_array.size() != 4) {
      return true;  // If profile is broken we should upgrade to correct it.
    }
    bool increased[4];  // major, minor, build, patch
    for (int i = 0; i < 4; i++) {
      int bundled, profile;
      if (!base::StringToInt(bundled_array[i], &bundled)) {
        LOG(ERROR)
            << "Menu Storage: Failed to upgrade, illegal bundled version: "
            << bundled_version;
        return false;  // Should never happen. We have full control of the
                       // string.
      }
      if (!base::StringToInt(profile_array[i], &profile)) {
        return true;  // If profile is broken we should upgrade to correct it.
      }
      increased[i] = bundled > profile;
    }
    // We only upgrade upwards to prevent potential looping if sync is enabled
    // between two diffrent builds.
    return increased[0] || (!increased[0] && increased[1]) || increased[2] ||
           (!increased[2] && increased[3]);
  }
  return false;
}

void OnLoad(const base::FilePath& profile_file,
            const base::FilePath::StringPieceType& filename,
            const base::WeakPtr<menus::MenuStorage> storage,
            std::unique_ptr<MenuLoadDetails> details) {
  bool exists;
  const base::FilePath* file;

  // Set up the bundled path here as GetResourceDirectory() calls code
  // that should not be used in the UI thread.
  base::FilePath bundled_file = ResourceReader::GetResourceDirectory()
                                    .Append(kMenuFolder)
                                    .Append(filename);

  if (details->force_bundle()) {
    // Revert to default while running.
    file = &bundled_file;
    exists = base::PathExists(*file);
  } else {
    // Check for upgrade. We use the full build number as a version test key.
    // The build number is saved in the profile file, but not in the bundled
    // so we take the bundled value from the load details segment.
    if (base::PathExists(profile_file)) {
      std::string profile_version;
      if (!GetVersion(&profile_version, profile_file)) {
        LOG(ERROR) << "Menu Storage: Can not check for upgrade, version missing";
      } else {
        std::string bundled_version = details->control()->version;
        if (HasVersionStepped(bundled_version, profile_version)) {
          MenuUpgrade upgrade;
          std::unique_ptr<base::Value> root(
              upgrade.Run(profile_file, bundled_file, bundled_version));
          if (root.get()) {
            MenuCodec codec;
            if (!codec.Decode(details->mainmenu_node(), details->control(),
                              *root.get(), false, "")) {
              LOG(ERROR) << "Menu Storage: Failed to decode JSON content after "
                            "upgrade. Upgrade ignored.";
            } else {
              details->SetUpgradeRoot(std::move(root));
            }
          }
        }
      }
    }

    if (!details->has_upgraded()) {
      // Load on startup. Read from profile file if exists, otherwise bundled.
      file = &profile_file;
      exists = base::PathExists(*file);
      if (!exists) {
        file = &bundled_file;
        exists = base::PathExists(*file);
      }
    }
  }

  if (!details->has_upgraded()) {
    if (!exists) {
      if (details->force_bundle()) {
        LOG(ERROR) << "Menu Storage: File does not exist: " << bundled_file;
      } else {
        LOG(ERROR) << "Menu Storage: No files exists:\n"
                   << profile_file << "\n"
                   << bundled_file;
      }
    } else {
      // Loop so that we can attempt the bundled file if the local fails.
      while (exists) {
        bool is_error = false;

        JSONFileValueDeserializer serializer(*file);
        std::unique_ptr<base::Value> root(serializer.Deserialize(NULL, NULL));
        if (!root.get()) {
          LOG(ERROR) << "Menu Storage: Failed to parse JSON. Check format";
          std::string content;
          base::ReadFileToString(*file, &content);
          LOG(ERROR) << "Menu Storage: " << *file;
          LOG(ERROR) << "Menu Storage: Content: " << content;
          is_error = true;
        } else {
          MenuCodec codec;
          // Use the version set up in 'details' when reading from the bundle.
          std::string version =
              file == &bundled_file ? details->control()->version : "";
          if (!codec.Decode(details->mainmenu_node(), details->control(),
                            *root.get(), file == &bundled_file, version)) {
            LOG(ERROR) << "Menu Storage: Failed to decode JSON content from: "
                       << *file;
            is_error = true;
          }
        }

        if (!is_error || file == &bundled_file) {
          break;
        }
        // Fallback to bundled file.
        file = &bundled_file;
        exists = base::PathExists(*file);
        if (!exists) {
          LOG(ERROR) << "Menu Storage: Bundled file does not exist "
                     << bundled_file;
        } else {
          LOG(ERROR) << "Menu Storage: Attempting fallback " << bundled_file;
        }
      }
    }
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindRepeating(&MenuStorage::OnLoadFinished, storage,
                                     base::Passed(&details)));
}

MenuLoadDetails::MenuLoadDetails(Menu_Node* mainmenu_node,
                                 Menu_Control* control,
                                 int64_t id,
                                 bool force_bundle,
                                 Mode mode)
    : mainmenu_node_(mainmenu_node),
      control_(control),
      id_(id),
      force_bundle_(force_bundle),
      mode_(mode) {}

MenuLoadDetails::MenuLoadDetails(Menu_Node* mainmenu_node,
                                 Menu_Control* control,
                                 const std::string& menu,
                                 bool force_bundle,
                                 Mode mode)
    : mainmenu_node_(mainmenu_node),
      control_(control),
      id_(-1),
      force_bundle_(force_bundle),
      mode_(mode),
      menu_(menu) {}

MenuLoadDetails::~MenuLoadDetails() {}

void MenuLoadDetails::SetUpgradeRoot(
    std::unique_ptr<base::Value> upgrade_root) {
  upgrade_root_ = std::move(upgrade_root);
}

MenuStorage::MenuStorage(content::BrowserContext* context,
                         Menu_Model* model,
                         base::SequencedTaskRunner* sequenced_task_runner)
    : model_(model),
      writer_(context->GetPath().Append(GetFileName(model)),
              sequenced_task_runner,
              base::Milliseconds(kSaveDelayMS)),
      weak_factory_(this) {
  sequenced_task_runner_ = sequenced_task_runner;
  sequenced_task_runner_->PostTask(FROM_HERE,
                                   base::BindOnce(&MakeBackup, writer_.path()));
}

MenuStorage::~MenuStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void MenuStorage::Load(std::unique_ptr<MenuLoadDetails> details) {
  DCHECK(details);
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindRepeating(&OnLoad, writer_.path(), GetFileName(model_),
                          weak_factory_.GetWeakPtr(), base::Passed(&details)));
}

void MenuStorage::ScheduleSave() {
  switch (backup_state_) {
    case BACKUP_NONE:
      backup_state_ = BACKUP_DISPATCHED;
      sequenced_task_runner_->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&MakeBackup, writer_.path()),
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

  model_ = nullptr;
}

std::optional<std::string> MenuStorage::SerializeData() {
  if (!model_) {
    // We can get into this state if there is a pending save on exit. It will
    // only happen if a forced save fails (eg std::nullopt is returned below)
    // A forced save is initiated from ~Menu_Model() which calls
    // MenuStorage::OnModelWillBeDeleted(). The forced save will clear the
    // pending save request in the file writer only if it succeeds. If not we
    // can end up here with model_ set to nullptr.
    return std::nullopt;
  }

  MenuCodec codec;
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  base::Value value = codec.Encode(model_);
  if (!serializer.Serialize(value))
    return std::nullopt;

  return output;
}

void MenuStorage::OnLoadFinished(std::unique_ptr<MenuLoadDetails> details) {
  if (details->has_upgraded()) {
    SaveValue(details->get_upgrade_root());
  }
  if (model_) {
    model_->LoadFinished(std::move(details));
  }
}

bool MenuStorage::SaveNow() {
  if (!model_ || !model_->loaded()) {
    // We should only get here if we have a valid model and it's finished
    // loading.
    NOTREACHED();
    //return false;
  }

  std::optional<std::string> data = SerializeData();
  if (!data)
    return false;
  writer_.WriteNow(data.value());
  return true;
}

bool MenuStorage::SaveValue(const std::unique_ptr<base::Value>& value) {
  std::unique_ptr<std::string> data(new std::string);
  JSONStringValueSerializer serializer(data.get());
  serializer.set_pretty_print(true);
  std::optional<std::string> datavalue = SerializeData();
  if (!datavalue)
    return false;
  writer_.WriteNow(datavalue.value());
  return true;
}

}  // namespace menus
