// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

// This is the cross platform code for Razer Chroma, it will call the
// platform specific parts.

#ifndef COMPONENTS_LIGHTS_RAZER_CHROMA_HANDLER_H_
#define COMPONENTS_LIGHTS_RAZER_CHROMA_HANDLER_H_

#include <vector>
#include "components/prefs/pref_change_registrar.h"
#include "third_party/skia/include/core/SkColor.h"

typedef std::vector<SkColor> RazerChromaColors;
class Profile;

class RazerChromaPlatformDriver {
 public:
  explicit RazerChromaPlatformDriver(Profile* profile) {}
  virtual ~RazerChromaPlatformDriver() = default;

  // Initialize the platform layer, return false if Razer Chroma is
  // is not available or it could otherwise not initialize.
  virtual bool Initialize() = 0;

  // Called before exiting to allow the driver to clean up used
  // resources.
  virtual void Shutdown() = 0;

  // Sets the given colors for the configured devices.
  virtual void SetColors(RazerChromaColors& colors) = 0;

  // Returns whether Chroma is available on this computer, eg.
  // whether the API is installed.
  virtual bool IsAvailable() = 0;

  // Returns whether Chroma is ready to accept commands.
  virtual bool IsReady() = 0;

  // Implemented by the platform to initialize the Razer api, if
  // available.
  static RazerChromaPlatformDriver* CreateRazerChromaPlatformDriver(
      Profile* profile);
};

class RazerChromaHandler {
 public:
  explicit RazerChromaHandler(Profile* profile);
  ~RazerChromaHandler();
  RazerChromaHandler(const RazerChromaHandler&) = delete;
  RazerChromaHandler& operator=(const RazerChromaHandler&) = delete;

  void Shutdown();

  bool IsAvailable();
  bool IsReady();

  void SetColors(RazerChromaColors& colors);

 protected:
  void OnPrefChanged(const std::string& path);

  // Initialize the Razer Chroma support and the underlying platform
  // layer.  Returns false on errors.
  bool Initialize();

  bool IsEnabled();

 private:
  PrefChangeRegistrar prefs_registrar_;
  bool initialized_ = false;
  std::unique_ptr<RazerChromaPlatformDriver> platform_driver_;
  const raw_ptr<Profile> profile_ = nullptr;
};

#endif  // COMPONENTS_LIGHTS_RAZER_CHROMA_HANDLER_H_
