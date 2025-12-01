// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_main_delegate.h"

#include "build/build_config.h"
#include "components/version_info/version_info.h"

#include "app/vivaldi_apptools.h"
#include "extraparts/vivaldi_content_browser_client.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/vivaldi_browser_component_wrapper.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "app/vivaldi_constants.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/vivaldi_switches.h"
#include "gin/v8_initializer.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if defined(OEM_MERCEDES_BUILD)
#include "base/base_switches.h"
#include "gpu/config/gpu_switches.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gl/gl_switches.h"
#endif

VivaldiMainDelegate::VivaldiMainDelegate()
#if !BUILDFLAG(IS_ANDROID)
    : VivaldiMainDelegate({.exe_entry_point_ticks = base::TimeTicks::Now()})
#endif
{
}

#if !BUILDFLAG(IS_ANDROID)
VivaldiMainDelegate::VivaldiMainDelegate(
    const StartupTimestamps& timestamps)
    : ChromeMainDelegate(timestamps) {}
#endif

VivaldiMainDelegate::~VivaldiMainDelegate() {}

content::ContentBrowserClient*
VivaldiMainDelegate::CreateContentBrowserClient() {
  if (!vivaldi::IsVivaldiRunning() && !vivaldi::ForcedVivaldiRunning()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Create a browser-component side function wrapper that is accessed in the
    // extension module via VivaldiBrowserComponentWrapper.
    VivaldiBrowserComponentWrapper::CreateImpl();
#endif

    return ChromeMainDelegate::CreateContentBrowserClient();
  }

  if (chrome_content_browser_client_ == nullptr) {
    chrome_content_browser_client_ =
        std::make_unique<VivaldiContentBrowserClient>();
  }
  return chrome_content_browser_client_.get();
}

std::optional<int> VivaldiMainDelegate::BasicStartupComplete() {
  constexpr const char chromium_version_switch[] = "chromium-version";
  base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(chromium_version_switch)) {
    printf("%s\n", version_info::GetVersionNumber().data());
    return 0;
  }

#if BUILDFLAG(IS_ANDROID)
  // Vivaldi ref. AUTO-264
#if defined(OEM_MERCEDES_BUILD)
  command_line.AppendSwitchASCII(switches::kEnableFeatures, "SkiaGraphite");
  command_line.AppendSwitch(switches::kEnableSkiaGraphite);
  command_line.AppendSwitch(blink::switches::kEnableZeroCopy);
  command_line.AppendSwitch(switches::kIgnoreGpuBlocklist);
  //!! The kUseAngle switch may be used later. Keep it commented out for now.
  //!!    command_line.AppendSwitchASCII(switches::kUseANGLE,
  //!!                                   gl::kANGLEImplementationOpenGLESName);
#endif // OEM_MERCEDES_BUILD
  return ChromeMainDelegateAndroid::BasicStartupComplete();
#else
#ifdef VIVALDI_V8_CONTEXT_SNAPSHOT
  if (command_line.HasSwitch(switches::kVivaldiSnapshotProcess)) {
    base::FilePath path;
    base::PathService::Get(base::DIR_ASSETS, &path);

#if BUILDFLAG(IS_MAC)
    if (base::mac::GetCPUType() == base::mac::CPUType::kIntel) {
      // We use a different filename for x64 builds so that the arm64 and
      // x64 snapshots can live side-by-side in a universal macOS app.
      path = path.Append(
          FILE_PATH_LITERAL("vivaldi_v8_context_snapshot.x86_64.bin"));
    } else {
      path = path.Append(FILE_PATH_LITERAL("vivaldi_v8_context_snapshot.bin"));
    }
#else
    path = path.Append(FILE_PATH_LITERAL("vivaldi_v8_context_snapshot.bin"));
#endif // BUILDFLAG(IS_MAC)

    base::File file(path, base::File::FLAG_READ | base::File::FLAG_OPEN);
    gin::V8Initializer::LoadV8SnapshotFromFile(
        std::move(file), nullptr,
        gin::V8SnapshotFileType::kWithAdditionalContext);
  }
#endif // VIVALDI_V8_CONTEXT_SNAPSHOT
  return ChromeMainDelegate::BasicStartupComplete();
#endif // !BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_WIN)
bool VivaldiTestMainDelegate::ShouldHandleConsoleControlEvents() {
  return false;
}
#endif
