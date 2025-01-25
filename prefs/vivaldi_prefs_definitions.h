// Copyright (c) 2015 Vivaldi Technologies

#ifndef PREFS_VIVALDI_PREFS_DEFINITIONS_H_
#define PREFS_VIVALDI_PREFS_DEFINITIONS_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/no_destructor.h"
#include "base/values.h"
#include "components/sync_preferences/syncable_prefs_database.h"

class PrefRegistrySimple;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace vivaldi {

class VivaldiPrefsDefinitions : public sync_preferences::SyncablePrefsDatabase {
 public:
  enum class PrefKind {
    kNone,
    kEnum,
    kString,
    kFilePath,
    kBoolean,
    kInteger,
    kDouble,
    kList,
    kDictionary,
  };

  struct EnumPrefValues {
    EnumPrefValues();
    ~EnumPrefValues();
    EnumPrefValues(EnumPrefValues&&);
    EnumPrefValues& operator=(EnumPrefValues&&);

    std::optional<int> FindValue(std::string_view name) const;
    const std::string* FindName(int value) const;

    std::vector<std::pair<std::string, int>> name_value_pairs;
  };

  struct SyncedPrefProperties {
    SyncedPrefProperties();
    ~SyncedPrefProperties();
    SyncedPrefProperties(SyncedPrefProperties&&);
    SyncedPrefProperties& operator=(SyncedPrefProperties&&);

    int id = -1;
    sync_preferences::MergeBehavior merge_behavior =
        sync_preferences::MergeBehavior::kNone;
  };

  struct PrefDefinition {
    PrefDefinition();
    ~PrefDefinition();
    PrefDefinition(PrefDefinition&&);
    PrefDefinition& operator=(PrefDefinition&&);

    PrefKind pref_kind = PrefKind::kNone;
    base::Value default_value;
    std::optional<SyncedPrefProperties> sync_properties;
    std::optional<EnumPrefValues> enum_values;
  };

  // Preference properties. These are used when registering prefs, in JS
  // bindings and as the synced prefs database
  struct PrefProperties {
    PrefProperties();
    ~PrefProperties();
    PrefProperties(PrefProperties&&);
    PrefProperties& operator=(PrefProperties&&);

    bool local_pref = false;
    std::optional<PrefDefinition> definition;
  };

  using PrefPropertiesMap =
      std::unordered_map<std::string, const PrefProperties>;

  static VivaldiPrefsDefinitions* GetInstance();

  const PrefPropertiesMap& get_pref_properties() const {
    return pref_properties_;
  }

  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  void MigrateObsoleteProfilePrefs(PrefService* profile_prefs);

  // Implementing sync_preferences::SyncablePrefsDatabase
  std::optional<sync_preferences::SyncablePrefMetadata> GetSyncablePrefMetadata(
      const std::string_view pref_name) const override;

 private:
  friend class base::NoDestructor<VivaldiPrefsDefinitions>;

  VivaldiPrefsDefinitions();
  ~VivaldiPrefsDefinitions() override;

  void AddPropertiesFromDefinition(base::Value::Dict& definition,
                                   base::Value::Dict& syncable_paths,
                                   std::string current_path);

  void AddChromiumProperties(base::Value::Dict& prefs,
                             std::string_view current_path,
                             bool local_pref);

  base::Value GetComputedDefault(const std::string& path);
  base::Value GetPlatformComputedDefault(const std::string& path);

  PrefPropertiesMap pref_properties_;
};
}  // namespace vivaldi

#endif  // PREFS_VIVALDI_PREFS_DEFINITIONS_H_
