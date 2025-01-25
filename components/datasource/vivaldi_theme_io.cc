// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/vivaldi_theme_io.h"

#include <type_traits>

#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/filename_util.h"
#include "third_party/zlib/google/zip.h"

#include "components/datasource/resource_reader.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "components/datasource/vivaldi_image_store.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi_theme_io {

const char kIdKey[] = "id";

const char kVivaldiIdPrefix[] = "Vivaldi";
const char kVendorIdPrefix[] = "Vendor";

namespace {

// JSON max nesting is theme.buttons.name.small.value
constexpr size_t kMaxSettingsDepth = 4;

constexpr size_t kMaxSettingsSize = 10 * 1024;

constexpr const char kSettingsFileName[] = "settings.json";

// File name for temporary file to read/write memory blobs to.
constexpr const char kTempBlobFileName[] = ".data.zip";

constexpr const char kBackgroundImageKey[] = "backgroundImage";
constexpr const char kButtonsKey[] = "buttons";
constexpr const char kSmallKey[] = "small";
constexpr const char kLargeKey[] = "large";
constexpr const char kVersionKey[] = "version";

// Prefixes of theme ids for system themes. Such id is never exposed to users or
// the theme server. In particular, on export it is replaced with a random UUID.
constexpr const char* kSystemThemeIdPrefixes[] = {
    kVendorIdPrefix,
    kVivaldiIdPrefix,
};

// Get a sequence for theme IO operations.
scoped_refptr<base::SequencedTaskRunner> GetOneShotFileTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE});
}

// Helper to destroy the temp_dir on blocking sequence, not UI thread.
void RemoveTempDirLater(const scoped_refptr<base::SequencedTaskRunner>& runner,
                        base::ScopedTempDir temp_dir) {
  // callback that just calls the destructor.
  auto callback = base::BindOnce([](base::ScopedTempDir temp_dir) -> void {},
                                 std::move(temp_dir));
  runner->PostTask(FROM_HERE, std::move(callback));
}

// Return the index of the theme with the given id in |value| or SIZE_MAX if
// not found.
size_t FindThemeIndex(const base::Value::List& list,
                      std::string_view theme_id) {
  // Be defensive and do not assume any structure of the value.
  for (size_t i = 0; i < list.size(); ++i) {
    const base::Value& elem = list[i];
    if (!elem.is_dict())
      continue;
    if (const std::string* id = elem.GetDict().FindString(kIdKey)) {
      if (*id == theme_id) {
        return i;
      }
    }
  }
  return SIZE_MAX;
}

// Find the theme object in the given preference list.
const base::Value* FindThemeValue(const PrefService* prefs,
                                  const std::string& theme_list_pref_path,
                                  std::string_view theme_id) {
  auto& themes = prefs->GetList(theme_list_pref_path);
  size_t index = FindThemeIndex(themes, theme_id);
  if (index == SIZE_MAX)
    return nullptr;
  return &themes[index];
}

class Exporter : public base::RefCountedThreadSafe<Exporter> {
 public:
  Exporter() = default;

  static void Start(content::BrowserContext* browser_context,
                    base::Value theme_object,
                    base::FilePath theme_archive,
                    ExportResult callback) {
    scoped_refptr<Exporter> exporter = base::MakeRefCounted<Exporter>();

    exporter->theme_object_ = std::move(theme_object);
    exporter->theme_archive_ = std::move(theme_archive);
    exporter->result_callback_ = std::move(callback);
    exporter->data_source_api_ =
        VivaldiImageStore::FromBrowserContext(browser_context);
    if (!exporter->data_source_api_) {
      // Let the destructor call the callback.
      exporter->error_ = "No data API";
      return;
    }
    exporter->work_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&Exporter::StartOnWorkSequence, exporter.get()));
  }

 private:
  friend base::RefCountedThreadSafe<Exporter>;

  // This will be called when all work is finished.
  ~Exporter() {
    LOG_IF(ERROR, !error_.empty()) << error_;
    RemoveTempDirLater(work_sequence_, std::move(temp_dir_));
    ui_thread_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback_),
                                  std::move(data_blob_), error_.empty()));
  }

  void StartOnWorkSequence() {
    DCHECK(work_sequence_->RunsTasksInCurrentSequence());

    // Zip API in Chromium do not work with memory, so copy all to a temporary
    // directory.
    if (!temp_dir_.CreateUniqueTempDir()) {
      error_ = "Failed to create a temporary directory";
      return;
    }

    std::string* image_url =
        theme_object_.GetDict().FindString(kBackgroundImageKey);
    bool has_background_image = image_url && !image_url->empty();
    if (has_background_image) {
      ExportImage(theme_object_, kBackgroundImageKey, image_url, "background");
    }

    base::Value* buttons = theme_object_.GetDict().Find(kButtonsKey);
    if (buttons) {
      for (auto iter : buttons->GetDict()) {
        const std::string& button = iter.first;
        image_url = buttons->GetDict().FindString(button);
        if (image_url) {
          ExportImage(*buttons, button, image_url, button);
        } else {
          base::Value* dict = buttons->GetDict().Find(button);
          if (!dict) {
            continue;
          }
          image_url = dict->GetDict().FindString(kSmallKey);
          if (image_url) {
            const std::string base_name =
                base::StringPrintf("%s_small", button.c_str());
            ExportImage(*dict, kSmallKey, image_url, base_name);
          }
          image_url = dict->GetDict().FindString(kLargeKey);
          if (image_url) {
            const std::string base_name =
                base::StringPrintf("%s_large", button.c_str());
            ExportImage(*dict, kLargeKey, image_url, base_name);
          }
        }
      }
    }
    if (num_images_being_processed_async == 0) {
      SaveJSONAndCreateZip();
    }
  }

  void ExportImage(base::Value& object,
                   std::string image_key,
                   std::string* image_url,
                   std::string base_name) {
    if (image_url) {
      VivaldiImageStore::UrlKind url_kind;
      std::string url_id;
      if (VivaldiImageStore::ParseDataUrl(*image_url, url_kind, url_id)) {
        num_images_being_processed_async++;
        data_source_api_->GetDataForId(
            url_kind, url_id,
            base::BindOnce(&Exporter::SaveImage, this, &object, image_key,
                           url_id, base_name));
        return;
      }
      std::string resource;
      if (vivaldi_data_url_utils::IsResourceURL(*image_url, &resource)) {
        ResourceReader reader(resource);
        if (!reader.IsValid()) {
          error_ = reader.GetError();
          return;
        }
        WriteImage(object, image_key, resource, base_name, reader.data(),
                   reader.size());
      }
    }
  }

  void SaveImage(base::Value* object,
                 std::string image_key,
                 std::string name,
                 std::string base_name,
                 scoped_refptr<base::RefCountedMemory> data) {
    if (!data) {
      error_ = "Failed to read the background image";
      return;
    }
    WriteImage(*object, image_key, name, base_name, data->data(), data->size());
    num_images_being_processed_async--;
    if (num_images_being_processed_async == 0) {
      SaveJSONAndCreateZip();
    }
  }

  void WriteImage(base::Value& object,
                  std::string image_key,
                  std::string_view name,
                  std::string base_name,
                  const uint8_t* data,
                  size_t size) {
    if (size == 0)
      return;
    std::string file_name = base_name;
    size_t last_dot = name.rfind('.');
    if (last_dot != std::string::npos) {
      file_name.append(name.data() + last_dot, name.size() - last_dot);
    }
    base::FilePath path =
        temp_dir_.GetPath().Append(base::FilePath::FromUTF8Unsafe(file_name));
    if (!base::WriteFile(path, base::span(data, size))) {
      error_ = std::string("Failed to write ") + file_name;
      return;
    }
    archive_files_.push_back(path.BaseName());

    object.GetDict().Set(image_key, std::move(file_name));
  }

  void SaveJSONAndCreateZip() {
    if (!error_.empty())
      return;

    std::string json;
    if (!base::JSONWriter::WriteWithOptions(
            theme_object_,
            base::JSONWriter::OPTIONS_PRETTY_PRINT |
                base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
            &json, /*max_depth=*/kMaxSettingsDepth)) {
      error_ = "Invalid theme object";
      return;
    }

    base::FilePath settings_path =
        temp_dir_.GetPath().AppendASCII(kSettingsFileName);
    if (!base::WriteFile(settings_path, json)) {
      error_ = std::string("Failed to write ") + kSettingsFileName;
      return;
    }
    archive_files_.push_back(settings_path.BaseName());

    bool zip_to_blob = theme_archive_.empty();
    if (zip_to_blob) {
      theme_archive_ = temp_dir_.GetPath().AppendASCII(kTempBlobFileName);
    }

    zip::ZipParams zip_params;
    zip_params.src_dir = temp_dir_.GetPath();
    zip_params.src_files = archive_files_;
    zip_params.dest_file = theme_archive_;
    if (!zip::Zip(zip_params)) {
      error_ = "Failed to zip a temporary folder into " +
               theme_archive_.BaseName().AsUTF8Unsafe();
      return;
    }

    if (zip_to_blob) {
      auto archive_size64 = base::GetFileSize(theme_archive_);
      if (!archive_size64.has_value() || archive_size64.value() <= 0 ||
          archive_size64.value() > kMaxArchiveSize) {
        error_ = "Invalid archive size for " +
                 theme_archive_.BaseName().AsUTF8Unsafe();
        return;
      }
      int archive_size = static_cast<int>(archive_size64.value());
      std::vector<uint8_t> data;
      data.resize(static_cast<size_t>(archive_size));
      int nread = base::ReadFile(
          theme_archive_, reinterpret_cast<char*>(data.data()), archive_size);
      if (nread != archive_size) {
        error_ = "Failed to read the archive " +
                 theme_archive_.BaseName().AsUTF8Unsafe();
        return;
      }
      data_blob_ = std::move(data);
    }
  }

  const scoped_refptr<base::SequencedTaskRunner> ui_thread_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();

  const scoped_refptr<base::SequencedTaskRunner> work_sequence_ =
      GetOneShotFileTaskRunner();

  base::Value theme_object_;
  base::FilePath theme_archive_;
  ExportResult result_callback_;
  scoped_refptr<VivaldiImageStore> data_source_api_;
  base::ScopedTempDir temp_dir_;
  std::string error_;
  std::vector<base::FilePath> archive_files_;
  std::vector<uint8_t> data_blob_;
  int num_images_being_processed_async = 0;
};

// Based on chromium/extensions/browser/zipfile_installer.cc
class Importer : public base::RefCountedThreadSafe<Importer> {
 public:
  Importer(base::WeakPtr<Profile> profile,
           ImportResult callback,
           base::FilePath theme_archive_path,
           std::vector<uint8_t> theme_archive_data)
      : profile_(std::move(profile)),
        callback_(std::move(callback)),
        theme_archive_path_(std::move(theme_archive_path)),
        theme_archive_data_(std::move(theme_archive_data)) {}

  void Start() {
    if (profile_) {
      data_source_api_ = VivaldiImageStore::FromBrowserContext(profile_.get());
    }
    if (!data_source_api_) {
      // Let the destructor call the callback.
      AddError(ImportError::kIO, "No data API");
      return;
    }

    work_sequence_->PostTask(
        FROM_HERE, base::BindOnce(&Importer::StartOnWorkSequence, this));
  }

 private:
  friend base::RefCountedThreadSafe<Importer>;

  // This will be called when all work is finished.
  ~Importer() {
    RemoveTempDirLater(work_sequence_, std::move(temp_dir_));
    ui_thread_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(theme_id_),
                                  std::move(error_)));
  }

  void AddError(ImportError::Kind kind, std::string details) {
    LOG(ERROR) << details;
    if (!error_) {
      error_ = std::make_unique<ImportError>(kind, std::move(details));
    }
  }

  void StartOnWorkSequence() {
    DCHECK(work_sequence_->RunsTasksInCurrentSequence());
    if (!temp_dir_.CreateUniqueTempDir()) {
      AddError(ImportError::kIO, "Failed to create a temporary directory");
      return;
    }

    if (theme_archive_path_.empty()) {
      theme_archive_path_ = temp_dir_.GetPath().AppendASCII(kTempBlobFileName);
      if (!base::WriteFile(theme_archive_path_, theme_archive_data_)) {
        AddError(ImportError::kIO,
                 "Failed to write archive data to a temporary file");
        return;
      }
    }

    // For forward compatibility we do not try to filter out unwanted files.
    // We copy the necessary pieces from the decompressed directory and then
    // delete the whole directory so presence of unknown files does not lead
    // to their permanent storage.
    unzip::Unzip(unzip::LaunchUnzipper(), theme_archive_path_,
                 temp_dir_.GetPath(), unzip::mojom::UnzipOptions::New(),
                 unzip::AllContents(), base::DoNothing(),
                 base::BindOnce(&Importer::ProcessUnzipped, this));
  }

  void ProcessUnzipped(bool success) {
    DCHECK(work_sequence_->RunsTasksInCurrentSequence());
    if (!success) {
      AddError(ImportError::kBadArchive, "Failed to unzip the archive");
    }
    ProcessSettings();
    ImportImages();
  }

  void ProcessSettings() {
    DCHECK(work_sequence_->RunsTasksInCurrentSequence());
    if (error_)
      return;
    base::FilePath unzipped_settings_file =
        temp_dir_.GetPath().AppendASCII(kSettingsFileName);
    if (!base::PathExists(unzipped_settings_file)) {
      AddError(
          ImportError::kBadArchive,
          base::StringPrintf("No %s file in the archive", kSettingsFileName));
      return;
    }
    std::string settings_text;
    if (!base::ReadFileToStringWithMaxSize(unzipped_settings_file,
                                           &settings_text, kMaxSettingsSize)) {
      AddError(ImportError::kIO,
               base::StringPrintf("Failed to read %s or the file is too big",
                                  kSettingsFileName));
      return;
    }
    std::optional<base::Value> settings =
        base::JSONReader::Read(settings_text, base::JSON_ALLOW_TRAILING_COMMAS);
    if (!settings) {
      AddError(ImportError::kBadSettings,
               base::StringPrintf("%s is not a valid JSON", kSettingsFileName));
      return;
    }
    std::string verify_error;
    VerifyAndNormalizeJson({}, *settings, verify_error);
    if (!verify_error.empty()) {
      AddError(ImportError::kBadSettings, std::move(verify_error));
      return;
    }

    theme_object_ = std::move(*settings);
  }

  void ImportImages() {
    DCHECK(work_sequence_->RunsTasksInCurrentSequence());
    if (error_)
      return;

    std::string* image_url =
        theme_object_.GetDict().FindString(kBackgroundImageKey);
    bool has_background_image = image_url && !image_url->empty();
    if (has_background_image) {
      ImportImage(theme_object_, kBackgroundImageKey, image_url);
    }

    base::Value* buttons = theme_object_.GetDict().Find(kButtonsKey);
    if (buttons) {
      for (auto iter : buttons->GetDict()) {
        const std::string& button = iter.first;
        image_url = buttons->GetDict().FindString(button);
        if (image_url) {
          ImportImage(*buttons, button, image_url);
        } else {
          base::Value* dict = buttons->GetDict().Find(button);
          if (!dict) {
            continue;
          }
          image_url = dict->GetDict().FindString(kSmallKey);
          if (image_url) {
            ImportImage(*dict, kSmallKey, image_url);
          }
          image_url = dict->GetDict().FindString(kLargeKey);
          if (image_url) {
            ImportImage(*dict, kLargeKey, image_url);
          }
        }
      }
    } else if (!has_background_image) {
      StoreTheme(*image_url);
    }
  }

  void ImportImage(base::Value& object,
                   std::string image_key,
                   std::string* image) {
    base::FilePath relative_path = base::FilePath::FromUTF8Unsafe(*image);
    if (!net::IsSafePortableRelativePath(relative_path)) {
      AddError(
          ImportError::kBadSettings,
          base::StringPrintf("The value of %s propert '%s' is not a valid file",
                             image_key.c_str(), image->c_str()));
      return;
    }

    // TODO(igor@vivaldi.com): Consider checking the extension matches the
    // data format not to rely on chromium image loader be tolerant to image
    // format mismatch and to report junk data earlier.
    std::optional<VivaldiImageStore::ImageFormat> image_format =
        VivaldiImageStore::FindFormatForPath(relative_path);
    if (!image_format) {
      AddError(ImportError::kBadSettings,
               base::StringPrintf("Unsupported image format in '%s' - %s",
                                  image_key.c_str(),
                                  relative_path.AsUTF8Unsafe().c_str()));
      return;
    }
    std::string image_data;
    if (!base::ReadFileToStringWithMaxSize(
            temp_dir_.GetPath().Append(relative_path), &image_data,
            kMaxImageSize)) {
      AddError(ImportError::kIO,
               base::StringPrintf("Failed to read %s or the file is too big",
                                  image->c_str()));
      return;
    }
    processed_images_counter++;
    scoped_refptr<base::RefCountedMemory> image_buf(
        new base::RefCountedString(image_data));
    data_source_api_->StoreImageData(
        *image_format, std::move(image_buf),
        base::BindOnce(&Importer::OnImageStored, this, &object, image_key));
    return;
  }

  void OnImageStored(base::Value* object,
                     std::string image_key,
                     std::string image_url) {
    DCHECK(work_sequence_->RunsTasksInCurrentSequence());
    DCHECK(!error_);
    if (image_url.empty()) {
      AddError(ImportError::kIO,
               base::StringPrintf("Failed to store image %s locally",
                                  image_key.c_str()));
      return;
    }
    object->GetDict().Set(image_key, image_url);
    processed_images_counter--;
    if (processed_images_counter == 0) {
      StoreTheme(image_url);
    }
  }

  void StoreTheme(std::string image_url) {
    DCHECK(work_sequence_->RunsTasksInCurrentSequence());
    ui_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Importer::StoreThemeOnUIThread, this, image_url));
  }

  void StoreThemeOnUIThread(std::string image_url) {
    DCHECK(ui_thread_runner_->RunsTasksInCurrentSequence());
    if (error_)
      return;

    if (!profile_)
      return;

    theme_id_ = *theme_object_.GetDict().FindString(kIdKey);

    // Extra scope to ensure that ListPrefUpdate destructor runs before we
    // forget the url.
    {
      // When importing a theme with an already existing id on the preview list
      // the new import replaces the old value.
      ScopedListPrefUpdate update(profile_->GetPrefs(),
                                  vivaldiprefs::kThemesPreview);
      auto& list_value = update.Get();
      size_t index = FindThemeIndex(list_value, theme_id_);
      if (index != SIZE_MAX) {
        list_value[index] = std::move(theme_object_);
      } else {
        list_value.Append(std::move(theme_object_));
      }
    }
    data_source_api_->ForgetNewbornUrl(std::move(image_url));
  }

  base::WeakPtr<Profile> profile_;

  const scoped_refptr<base::SequencedTaskRunner> ui_thread_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();

  const scoped_refptr<base::SequencedTaskRunner> work_sequence_ =
      GetOneShotFileTaskRunner();
  ImportResult callback_;

  base::ScopedTempDir temp_dir_;
  base::FilePath theme_archive_path_;
  std::vector<uint8_t> theme_archive_data_;
  base::Value theme_object_;
  std::string theme_id_;
  scoped_refptr<VivaldiImageStore> data_source_api_;
  std::unique_ptr<ImportError> error_;

  int processed_images_counter = 0;
};

}  // namespace

void VerifyAndNormalizeJson(VerifyAndNormalizeFlags flags,
                            base::Value& object,
                            std::string& error) {
  // Manually check that object contains only known keys of the proper type and
  // values. The FooInfo structures below hold restructions on the type. They
  // are put into a lazily-built name->info map. Then the Checker class below
  // does all the checks using this map.

  struct BoolInfo {};
  struct NumberInfo {
    NumberInfo(double min_value, std::optional<double> max_value)
        : min_value(min_value), max_value(max_value) {}
    double min_value;
    std::optional<double> max_value;
  };
  struct StringInfo {
    bool can_be_empty = false;
  };
  struct EnumInfo {
    EnumInfo(std::vector<std::string_view> enum_cases)
        : enum_cases(std::move(enum_cases)) {}
    std::vector<std::string_view> enum_cases;
  };

  using InfoUnion = std::variant<BoolInfo, NumberInfo, StringInfo, EnumInfo>;

  enum Presence {
    kOptional,
    kRequired,
  };

  struct Info {
    Info(Presence presence, InfoUnion infoUnion)
        : presence(presence), infoUnion(std::move(infoUnion)) {}
    Presence presence;
    InfoUnion infoUnion;
  };

  using InfoMap = base::flat_map<std::string_view, Info>;

  auto build_map = []() -> InfoMap {
    std::vector<std::pair<std::string_view, Info>> list;

    auto add_bool = [&](const char* key) -> void {
      list.emplace_back(key, Info(kOptional, InfoUnion(BoolInfo())));
    };
    auto add_number = [&](const char* key, double min_value,
                          std::optional<double> max_value) -> void {
      // The number is required if 0 cannot be used as the default value.
      Presence presence = min_value <= 0.0 && (!max_value || *max_value >= 0.0)
                              ? kOptional
                              : kRequired;
      list.emplace_back(
          key, Info(presence, InfoUnion(NumberInfo(min_value, max_value))));
    };
    auto add_string = [&](const char* key, bool can_be_empty) -> void {
      StringInfo info;
      info.can_be_empty = can_be_empty;
      Presence presence = can_be_empty ? kOptional : kRequired;
      list.emplace_back(key, Info(presence, InfoUnion(std::move(info))));
    };
    auto add_color = [&](const char* key, bool can_be_empty) -> void {
      add_string(key, can_be_empty);
    };
    auto add_enum = [&](const char* key,
                        std::vector<std::string_view> enum_cases) -> void {
      DCHECK(!enum_cases.empty());
      list.emplace_back(
          key, Info(kOptional, InfoUnion(EnumInfo(std::move(enum_cases)))));
    };

    constexpr bool kNotEmpty = false;
    constexpr bool kCanBeEmpty = true;

    add_string(kIdKey, kNotEmpty);

    // This is populated by the server as necessary.
    add_string("url", kCanBeEmpty);

    add_number("engineVersion", 1.0, 1.0);
    add_number(kVersionKey, 0, std::nullopt);
    add_string("name", kNotEmpty);

    add_bool("accentFromPage");
    add_bool("accentOnWindow");
    add_number("accentSaturationLimit", 0.0, 1.0);
    add_bool("dimBlurred");
    add_bool("preferSystemAccent");
    add_number("blur", 0.0, 10.0);
    add_bool("transparencyTabs");
    add_bool("transparencyTabBar");
    add_bool("simpleScrollbar");
    add_color("colorAccentBg", kNotEmpty);
    add_color("colorBg", kNotEmpty);
    add_color("colorFg", kNotEmpty);
    add_color("colorHighlightBg", kNotEmpty);
    add_color("colorWindowBg", kCanBeEmpty);
    add_string(kBackgroundImageKey, kCanBeEmpty);
    add_enum("backgroundPosition", {"", "stretch", "center", "repeat"});
    add_number("radius", -1.0, 14.0);
    add_number("contrast", -10, 20.0);
    add_number("alpha", 0.0, 1.0);

    return InfoMap(std::move(list));
  };

  static base::NoDestructor<const InfoMap> info_map(build_map());

  class Checker {
   public:
    Checker(base::Value& object,
            std::string& error,
            VerifyAndNormalizeFlags flags)
        : object_(object), error_(error), flags_(flags) {}

    void DoCheck() {
      if (!object_->is_dict()) {
        AddError("The theme is not a JSON object.");
        return;
      }

      // We ignore unknown properties for forward compatibility - the users can
      // have an old vivaldi executable for quite some time after we
      // introduce new features on the server.
      for (auto name_info : *info_map) {
        key_ = name_info.first;
        presence_ = name_info.second.presence;
        std::visit(*this, name_info.second.infoUnion);
      }
    }

    // function call operators to use with the std::visit() call above
    // to check individual keys. They are public to avoiding friending a
    // template method.

    void operator()(const BoolInfo&) {
      base::Value* value = FindValue();
      if (!value) {
        object_->GetDict().Set(key_, false);
        return;
      }
      if (!value->is_bool()) {
        AddError(base::StringPrintf("The property %s is not a boolean",
                                    KeyText().c_str()));
        return;
      }
    }

    void operator()(const NumberInfo& info) {
      // Check that we can always use 0.0 as a default.
      DCHECK(presence_ == kRequired ||
             (info.min_value <= 0.0 &&
              (!info.max_value || 0.0 <= *info.max_value)));
      base::Value* value = FindValue();
      if (!value) {
        object_->GetDict().Set(key_, 0);
        return;
      }
      if (!value->is_double() && !value->is_int()) {
        AddError(base::StringPrintf("The property %s is not a number",
                                    KeyText().c_str()));
        return;
      }
      double d = value->GetDouble();
      if (!info.max_value) {
        if (d < info.min_value) {
          AddError(base::StringPrintf(
              "The property %s value %f is below the allowed minimum %f",
              KeyText().c_str(), d, info.min_value));
        }
      } else {
        if (d < info.min_value || d > *info.max_value) {
          AddError(base::StringPrintf(
              "The property %s value %f is outside the allowed range [%f %f]",
              KeyText().c_str(), d, info.min_value, *info.max_value));
        }
      }
    }

    void operator()(const StringInfo& info) {
      base::Value* value = FindValue();
      if (!value) {
        if (info.can_be_empty) {
          object_->GetDict().Set(key_, std::string());
        }
        return;
      }
      if (!value->is_string()) {
        AddError(base::StringPrintf("The property %s is not a string",
                                    KeyText().c_str()));
        return;
      }
      if (!info.can_be_empty && value->GetString().empty()) {
        AddError(base::StringPrintf("The property %s cannot be an empty string",
                                    KeyText().c_str()));
        return;
      }

      // The special case for the id, see comments for kSystemThemeIdPrefixes.
      if (key_ == kIdKey) {
        bool named_id = false;
        if (flags_.allow_named_id) {
          for (const char* prefix : kSystemThemeIdPrefixes) {
            if (base::StartsWith(value->GetString(), prefix)) {
              named_id = true;
              break;
            }
          }
        }
        if (named_id) {
          if (flags_.for_export) {
            value->GetString() =
                base::Uuid::GenerateRandomV4().AsLowercaseString();
          }
        } else if (!base::Uuid::ParseLowercase(value->GetString()).is_valid()) {
          AddError(base::StringPrintf(
              "The property %s is a not a valid Uuid - %s", KeyText().c_str(),
              value->GetString().c_str()));
        }
      }
    }

    void operator()(const EnumInfo& info) {
      DCHECK(!info.enum_cases.empty());
      base::Value* value = FindValue();
      if (!value) {
        // Set to the first enum case by default.
        object_->GetDict().Set(key_, info.enum_cases[0]);
        return;
      }
      if (!value->is_string()) {
        AddError(base::StringPrintf("The property %s is not a string",
                                    KeyText().c_str()));
        return;
      }
      const std::string& s = value->GetString();
      if (std::find(info.enum_cases.begin(), info.enum_cases.end(), s) ==
          info.enum_cases.cend()) {
        std::string cases_text = base::JoinString(info.enum_cases, "' '");
        AddError(base::StringPrintf(
            "The property %s value '%s' is not from the "
            "allowed list of values ('%s')",
            KeyText().c_str(), s.c_str(), cases_text.c_str()));
      }
    }

   private:
    void AddError(std::string error_message) {
      if (!error_->empty())
        return;
      *error_ = std::move(error_message);
    }

    std::string KeyText() const {
      return std::string(key_.data(), key_.size());
    }

    base::Value* FindValue() {
      base::Value* value = object_->GetDict().Find(key_);
      if (value) {
        return value;
      }
      if (presence_ == kRequired) {
        AddError(base::StringPrintf("Missing %s property", KeyText().c_str()));
      }
      return nullptr;
    }

    const raw_ref<base::Value> object_;
    const raw_ref<std::string> error_;
    const VerifyAndNormalizeFlags flags_;
    std::string_view key_;
    Presence presence_ = kOptional;
  };

  Checker checker(object, error, flags);
  checker.DoCheck();
}

void Export(content::BrowserContext* browser_context,
            base::Value theme_object,
            base::FilePath theme_archive,
            ExportResult callback) {
  Exporter::Start(browser_context, std::move(theme_object),
                  std::move(theme_archive), std::move(callback));
}

void Import(base::WeakPtr<Profile> profile,
            base::FilePath theme_archive_path,
            std::vector<uint8_t> theme_archive_data,
            ImportResult callback) {
  DCHECK(theme_archive_path.empty() != theme_archive_data.empty());
  scoped_refptr<Importer> importer = base::MakeRefCounted<Importer>(
      std::move(profile), std::move(callback), std::move(theme_archive_path),
      std::move(theme_archive_data));
  importer->Start();
}

void EnumerateUserThemeUrls(
    PrefService* prefs,
    base::RepeatingCallback<void(std::string_view url)> callback) {
  auto enumerate_theme_list = [&](const std::string& theme_list_pref_path) {
    auto& themes = prefs->GetList(theme_list_pref_path);
    for (const base::Value& value : themes) {
      if (!value.is_dict()) {
        continue;
      }
      const std::string* image_url =
          value.GetDict().FindString(kBackgroundImageKey);
      if (image_url) {
        callback.Run(*image_url);
      }

      if (const base::Value* buttons = value.GetDict().Find(kButtonsKey)) {
        for (auto iter : buttons->GetDict()) {
          const std::string& button = iter.first;
          image_url = buttons->GetDict().FindString(button);
          if (image_url) {
            callback.Run(*image_url);
          } else {
            const base::Value* dict = buttons->GetDict().Find(button);
            if (!dict) {
              continue;
            }
            image_url = dict->GetDict().FindString(kSmallKey);
            if (image_url) {
              callback.Run(*image_url);
            }
            image_url = dict->GetDict().FindString(kLargeKey);
            if (image_url) {
              callback.Run(*image_url);
            }
          }
        }
      }
    }
  };
  enumerate_theme_list(vivaldiprefs::kThemesUser);
  enumerate_theme_list(vivaldiprefs::kThemesPreview);
  enumerate_theme_list(vivaldiprefs::kThemesSystem);
}

bool StoreImageUrl(PrefService* prefs,
                   std::string_view theme_id,
                   std::string url) {
  auto store_image = [&](const std::string& theme_list_pref_path) -> bool {
    size_t index =
        FindThemeIndex(prefs->GetList(theme_list_pref_path), theme_id);
    if (index == SIZE_MAX)
      return false;
    ScopedListPrefUpdate update(prefs, theme_list_pref_path);
    update.Get()[index].GetDict().Set(kBackgroundImageKey, url);
    return true;
  };

  if (!store_image(vivaldiprefs::kThemesUser) &&
      !store_image(vivaldiprefs::kThemesSystem)) {
    LOG(ERROR) << "Failed to locate theme with id " << theme_id;
    return false;
  }
  return true;
}

double FindVersionByThemeId(PrefService* prefs, const std::string& theme_id) {
  const base::Value* theme_object =
      FindThemeValue(prefs, vivaldiprefs::kThemesUser, theme_id);
  if (!theme_object) {
    theme_object =
        FindThemeValue(prefs, vivaldiprefs::kThemesPreview, theme_id);
  }
  if (!theme_object)
    return 0.0;

  // If the key does not exist, assume 0. With properly formatted themes this
  // should not happen, but be defensive.
  return theme_object->GetDict().FindDouble(kVersionKey).value_or(0.0);
}

}  // namespace vivaldi_theme_io
