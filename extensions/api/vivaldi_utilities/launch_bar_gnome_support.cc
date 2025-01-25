// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "launch_bar_gnome_support.h"
#include <algorithm>

#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"

#include "chrome/common/channel_info.h"

#if defined(USE_GIO)
#include <gio/gio.h>
#endif  // defined(USE_GIO)

namespace dock {
namespace {

#if defined(USE_GIO)
const char kOrgGnomeShell[] = "org.gnome.shell";
const char kFavoriteApps[] = "favorite-apps";
#endif  // defined(USE_GIO)

// Helper class handling gnome settings for app pins.
class GnomeSettings {
 public:
  GnomeSettings();
  ~GnomeSettings();

  bool ReadPinnedApps(std::vector<std::string>* favorite_apps);
  bool WritePinnedApps(const std::vector<std::string>& favorite_apps);

#if defined(USE_GIO)
 private:
  raw_ptr<GSettings> settings_;
#endif  // defined(USE_GIO)
};

GnomeSettings::GnomeSettings() {
#if defined(USE_GIO)
  settings_ = g_settings_new(kOrgGnomeShell);

  if (!settings_) {
    LOG(ERROR)
        << "Could not initialize gsettings instance. Pinning won't be possible";
  }
#endif  //  defined(USE_GIO)
}

GnomeSettings ::~GnomeSettings() {
#if defined(USE_GIO)
  g_object_unref(settings_.ExtractAsDangling());
  settings_ = nullptr;
#endif
}

bool GnomeSettings::ReadPinnedApps(std::vector<std::string>* favorite_apps) {
#if defined(USE_GIO)
  if (!settings_)
    return false;

  gchar** apps = g_settings_get_strv(settings_, kFavoriteApps);
  for (gchar** app = apps; *app; app++) {
    favorite_apps->emplace_back(*app);
  }
  g_strfreev(apps);

  return true;
#else   // defined(USE_GIO)
  LOG(ERROR) << "GIO not enabled - gnome settings access is disabled.";
  return false;
#endif  // defined(USE_GIO)
}

bool GnomeSettings::WritePinnedApps(
    const std::vector<std::string>& favorite_apps) {
#if defined(USE_GIO)
  if (!settings_)
    return false;

  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));

  for (const auto& app : favorite_apps) {
    g_variant_builder_add(&builder, "s", app.c_str());
  }

  GVariant* variant = g_variant_builder_end(&builder);

  // Variant is consumed by the call. No need to unref.
  return g_settings_set_value(settings_, kFavoriteApps, variant);
#else   // defined(USE_GIO)
  LOG(ERROR) << "GIO not enabled - gnome settings access is disabled.";
  return false;
#endif  // defined(USE_GIO)
}

bool FindVivaldi(std::vector<std::string>& pinned) {
  return std::find_if(pinned.begin(), pinned.end(), [](const std::string& pin) {
           return pin.starts_with("vivaldi");
         }) != pinned.end();
}

std::array<std::string, 10> kBrowserPrefixes = {
    "epiphany", "org.gnome.Epiphany",    "safari",  "edge",  "chrome",
    "chromium", "org.chromium.Chromium", "firefox", "brave", "konqueror"};

bool AddVivaldiToPins(std::vector<std::string>* pins) {
  // Is vivaldi already in pins? If so just skip adding.
  if (FindVivaldi(*pins))
    return false;

  // Find any browser already in there or the end of the pins.
  auto pos =
      std::find_if(pins->begin(), pins->end(), [](const std::string& pin) {
        for (const std::string& prefix : kBrowserPrefixes) {
          if (pin.starts_with(prefix)) {
            return true;
          }
        }
        return false;
      });

  // We want to insert after, not before the found position.
  if (pos != pins->end())
    ++pos;

  auto env = base::Environment::Create();
  pins->insert(pos, chrome::GetDesktopName(env.get()));
  return true;
}

}  // namespace

bool GnomeLaunchBar::IsGnomeRunning() {
  auto env = base::Environment::Create();

  std::string var;
  if (env->GetVar("XDG_CURRENT_DESKTOP", &var)) {
    return var == "GNOME";
  }

  return false;
}

std::optional<bool> GnomeLaunchBar::IsVivaldiPinned() {
  GnomeSettings gnome;

  std::vector<std::string> pins;

  if (gnome.ReadPinnedApps(&pins)) {
    return FindVivaldi(pins);
  }

  return std::nullopt;
}

bool GnomeLaunchBar::PinVivaldi() {
  GnomeSettings gnome;
  std::vector<std::string> pins;

  if (gnome.ReadPinnedApps(&pins)) {
    if (AddVivaldiToPins(&pins)) {
      return gnome.WritePinnedApps(pins);
    }
  }

  return false;
}

}  // namespace dock
