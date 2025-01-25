// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/x11_desktop_resizer.h"

#include <gio/gio.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/types/cxx23_to_underlying.h"
#include "remoting/base/logging.h"
#include "remoting/host/desktop_geometry.h"
#include "remoting/host/linux/x11_util.h"
#include "remoting/host/x11_crtc_resizer.h"
#include "remoting/host/x11_display_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/randr.h"

// On Linux, we use the xrandr extension to change the desktop resolution.
//
// Xrandr has a number of restrictions that make exact resize more complex:
//
//   1. It's not possible to change the resolution of an existing mode. Instead,
//      the mode must be deleted and recreated.
//   2. It's not possible to delete a mode that's in use.
//   3. Errors are communicated via Xlib's spectacularly unhelpful mechanism
//      of terminating the process unless you install an error handler.
//   4. The root window size must always enclose any enabled Outputs (that is,
//      any output which is attached to a CRTC).
//   5. An Output cannot be given properties (xy-offsets, mode) which would
//      extend its rectangle beyond the root window size.
//
// Since we want the current mode name to be consistent (for each Output), the
// approach is as follows:
//
//   1. Fetch information about all the active (enabled) CRTCs.
//   2. Disable the RANDR Output being resized.
//   3. Delete the CRD mode, if it exists.
//   4. Create the CRD mode at the new resolution, and add it to the Output's
//      list of modes.
//   5. Adjust the properties (in memory) of any CRTCs to be modified:
//      * Width/height (mode) of the CRTC being resized.
//      * xy-offsets to avoid overlapping CRTCs.
//   6. Disable any CRTCs that might prevent changing the root window size.
//   7. Compute the bounding rectangle of all CRTCs (after adjustment), and set
//      it as the new root window size.
//   8. Apply all adjusted CRTC properties to the CRTCs. This will set the
//      Output being resized to the new CRD mode (which re-enables it), and it
//      will re-enable any other CRTCs that were disabled.

namespace {

constexpr auto kInvalidMode = static_cast<x11::RandR::Mode>(0);
constexpr auto kDisabledCrtc = static_cast<x11::RandR::Crtc>(0);
constexpr base::TimeDelta kGnomeWaitTime = base::Seconds(1);

int PixelsToMillimeters(int pixels, int dpi) {
  DCHECK(dpi != 0);

  const double kMillimetersPerInch = 25.4;

  // (pixels / dpi) is the length in inches. Multiplying by
  // kMillimetersPerInch converts to mm. Multiplication is done first to
  // avoid integer division.
  return static_cast<int>(kMillimetersPerInch * pixels / dpi);
}

// Returns a physical size in mm that will work well with GNOME's
// automatic scale-selection algorithm.
gfx::Size CalculateSizeInMmForGnome(
    const remoting::DesktopResolution& resolution) {
  int width_mm = PixelsToMillimeters(resolution.dimensions().width(),
                                     resolution.dpi().x());
  int height_mm = PixelsToMillimeters(resolution.dimensions().height(),
                                      resolution.dpi().y());

  // GNOME will, by default, choose an automatic scaling-factor based on the
  // monitor's physical size (mm) and resolution (pixels). Some versions of
  // GNOME have a problem when the computed DPI is close to 192. GNOME
  // calculates the DPI using:
  // dpi = size_pixels / (size_mm / 25.4)
  // This is the reverse of PixelsToMillimeters() which should result in
  // the same values as resolution.dpi() except for any floating-point
  // truncation errors. GNOME will choose 2x scaling only if both the width and
  // height DPIs are strictly greater than 192. The problem is that a user might
  // connect from a 192dpi device and then GNOME's choice of scaling is randomly
  // subject to rounding errors. If the calculation worked out at exactly
  // 192dpi, the inequality test would fail and GNOME would choose 1x scaling.
  // To address this, width_mm/height_mm are decreased slightly (increasing the
  // calculated DPI) to favor 2x over 1x scaling for 192dpi devices.
  width_mm--;
  height_mm--;

  // GNOME treats some pairs of width/height values as untrustworthy and will
  // always choose 1x scaling for them. These values come from
  // meta_monitor_has_aspect_as_size() in
  // https://gitlab.gnome.org/GNOME/mutter/-/blob/main/src/backends/meta-monitor-manager.c
  constexpr std::pair<int, int> kBadSizes[] = {
      {16, 9}, {16, 10}, {160, 90}, {160, 100}, {1600, 900}, {1600, 1000}};
  if (base::Contains(kBadSizes, std::pair(width_mm, height_mm))) {
    width_mm--;
  }
  return {width_mm, height_mm};
}

// TODO(jamiewalch): Use the correct DPI for the mode: http://crbug.com/172405.
const int kDefaultDPI = 96;

x11::RandR::Output GetOutputFromContext(void* context) {
  return reinterpret_cast<x11::RandR::MonitorInfo*>(context)->outputs[0];
}

std::string GetModeNameForOutput(x11::RandR::Output output) {
  // The name of the mode representing the current client view resolution. This
  // must be unique per Output, so that Outputs can be resized independently.
  return "CRD_" + base::NumberToString(base::to_underlying(output));
}

uint32_t GetDotClockForModeInfo() {
  static int proc_num = base::SysInfo::NumberOfProcessors();
  // Keep the proc_num logic in sync with linux_me2me_host.py
  if (proc_num > 16) {
    return 120 * 1e6;
  }
  return 60 * 1e6;
}

// Gets current layout with context information from a list of monitors.
std::vector<remoting::DesktopLayoutWithContext> GetLayoutWithContext(
    std::vector<x11::RandR::MonitorInfo>& monitors) {
  std::vector<remoting::DesktopLayoutWithContext> current_displays;
  for (auto& monitor : monitors) {
    // This implementation only supports resizing synthesized Monitors which
    // automatically track their Outputs.
    // TODO(crbug.com/40225767): Maybe support resizing manually-created
    // monitors?
    if (monitor.automatic) {
      current_displays.push_back(
          {.layout = remoting::ToVideoTrackLayout(monitor),
           .context = &monitor});
    }
  }
  return current_displays;
}

}  // namespace

namespace remoting {

ScreenResources::ScreenResources() = default;

ScreenResources::~ScreenResources() = default;

bool ScreenResources::Refresh(x11::RandR* randr, x11::Window window) {
  resources_ = nullptr;
  if (auto response = randr->GetScreenResourcesCurrent({window}).Sync()) {
    resources_ = std::move(response.reply);
  }
  return resources_ != nullptr;
}

x11::RandR::Mode ScreenResources::GetIdForMode(const std::string& name) {
  CHECK(resources_);
  const char* names = reinterpret_cast<const char*>(resources_->names.data());
  for (const auto& mode_info : resources_->modes) {
    std::string mode_name(names, mode_info.name_len);
    names += mode_info.name_len;
    if (name == mode_name) {
      return static_cast<x11::RandR::Mode>(mode_info.id);
    }
  }
  return kInvalidMode;
}

x11::RandR::GetScreenResourcesCurrentReply* ScreenResources::get() {
  return resources_.get();
}

X11DesktopResizer::X11DesktopResizer()
    : connection_(x11::Connection::Get()),
      randr_(&connection_->randr()),
      screen_(&connection_->default_screen()),
      root_(screen_->root),
      is_virtual_session_(IsVirtualSession(connection_)) {
  has_randr_ = randr_->present();
  if (!has_randr_) {
    return;
  }
  randr_->SelectInput({root_, x11::RandR::NotifyMask::ScreenChange});

  gnome_display_config_.Init();
  registry_ = TakeGObject(g_settings_new("org.gnome.desktop.interface"));
}

X11DesktopResizer::~X11DesktopResizer() = default;

DesktopResolution X11DesktopResizer::GetCurrentResolution(
    DesktopScreenId screen_id) {
  // Process pending events so that the connection setup data is updated
  // with the correct display metrics.
  if (has_randr_) {
    connection_->DispatchAll();
  }

  // RANDR does not allow fetching information on a particular monitor. So
  // fetch all of them and try to find the requested monitor.
  auto reply = randr_->GetMonitors({root_}).Sync();
  if (reply) {
    for (const auto& monitor : reply->monitors) {
      if (static_cast<DesktopScreenId>(monitor.name) != screen_id) {
        continue;
      }
      return DesktopResolution(gfx::Size(monitor.width, monitor.height),
                               GetMonitorDpi(monitor));
    }
  }

  LOG(ERROR) << "Cannot find current resolution for screen ID " << screen_id
             << ". Resolution of the default screen will be returned.";

  DesktopResolution result(
      gfx::Size(connection_->default_screen().width_in_pixels,
                connection_->default_screen().height_in_pixels),
      gfx::Vector2d(kDefaultDPI, kDefaultDPI));
  return result;
}

std::list<DesktopResolution> X11DesktopResizer::GetSupportedResolutions(
    const DesktopResolution& preferred,
    DesktopScreenId screen_id) {
  std::list<DesktopResolution> result;
  if (!has_randr_ || !is_virtual_session_) {
    return result;
  }

  // Clamp the specified size to something valid for the X server.
  if (auto response = randr_->GetScreenSizeRange({root_}).Sync()) {
    int width =
        std::clamp(static_cast<uint16_t>(preferred.dimensions().width()),
                   response->min_width, response->max_width);
    int height =
        std::clamp(static_cast<uint16_t>(preferred.dimensions().height()),
                   response->min_height, response->max_height);
    // Additionally impose a minimum size of 640x480, since anything smaller
    // doesn't seem very useful.
    result.emplace_back(gfx::Size(std::max(640, width), std::max(480, height)),
                        preferred.dpi());
  }
  return result;
}

void X11DesktopResizer::SetResolution(const DesktopResolution& resolution,
                                      DesktopScreenId screen_id) {
  if (!has_randr_ || !is_virtual_session_) {
    return;
  }

  // Grab the X server while we're changing the display resolution. This ensures
  // that the display configuration doesn't change under our feet.
  ScopedXGrabServer grabber(connection_);

  // RANDR does not allow fetching information on a particular monitor. So
  // fetch all of them and try to find the requested monitor.
  std::vector<x11::RandR::MonitorInfo> monitors;
  if (!TryGetCurrentMonitors(monitors)) {
    return;
  }

  for (const auto& monitor : monitors) {
    if (static_cast<DesktopScreenId>(monitor.name) != screen_id) {
      continue;
    }

    if (monitor.outputs.size() != 1) {
      // This implementation only supports resizing a Monitor attached to a
      // single output. The case where size() > 1 should never occur with
      // Xorg+video-dummy.
      // TODO(crbug.com/40225767): Maybe support resizing a Monitor not
      // attached to any Output?
      LOG(ERROR) << "Monitor " << screen_id
                 << " has unexpected #outputs: " << monitor.outputs.size();
      return;
    }

    if (!monitor.automatic) {
      // This implementation only supports resizing synthesized Monitors which
      // automatically track their Outputs.
      // TODO(crbug.com/40225767): Maybe support resizing manually-created
      // Monitors?
      LOG(ERROR) << "Not resizing Monitor " << screen_id
                 << " that was created manually.";
      return;
    }

    SetResolutionForOutput(monitor.outputs[0], resolution);
    return;
  }
  LOG(ERROR) << "Monitor " << screen_id << " not found.";
}

void X11DesktopResizer::RestoreResolution(const DesktopResolution& original,
                                          DesktopScreenId screen_id) {
  SetResolution(original, screen_id);
}

bool X11DesktopResizer::TryGetCurrentMonitors(
    std::vector<x11::RandR::MonitorInfo>& list) {
  if (!has_randr_ || !is_virtual_session_) {
    return false;
  }

  if (!resources_.Refresh(randr_, root_)) {
    return false;
  }

  auto reply = randr_->GetMonitors({root_}).Sync();
  if (!reply) {
    return false;
  }
  std::copy(reply->monitors.begin(), reply->monitors.end(),
            std::back_inserter(list));
  return true;
}

DesktopLayoutSet X11DesktopResizer::GetLayout() {
  DesktopLayoutSet result;
  std::vector<x11::RandR::MonitorInfo> monitors;
  if (!TryGetCurrentMonitors(monitors)) {
    return DesktopLayoutSet();
  }
  for (const auto& layout : GetLayoutWithContext(monitors)) {
    result.layouts.emplace_back(layout.layout);
  }
  return result;
}

void X11DesktopResizer::SetVideoLayout(const DesktopLayoutSet& layout) {
  if (!has_randr_ || !is_virtual_session_) {
    return;
  }
  // Grab the X server while we're changing the display resolution. This ensures
  // that the display configuration doesn't change under our feet.
  ScopedXGrabServer grabber(connection_);

  std::vector<x11::RandR::MonitorInfo> monitors;
  if (!TryGetCurrentMonitors(monitors)) {
    return;
  }
  std::vector<DesktopLayoutWithContext> current_displays =
      GetLayoutWithContext(monitors);

  // TODO(yuweih): Verify that the layout is valid, e.g. no overlaps or gaps
  // between displays.
  DisplayLayoutDiff diff = CalculateDisplayLayoutDiff(current_displays, layout);

  X11CrtcResizer resizer(resources_.get(), connection_);
  resizer.FetchActiveCrtcs();

  const std::vector<DesktopLayout>& new_layouts = diff.new_displays.layouts;
  // Add displays
  if (!new_layouts.empty()) {
    auto outputs = GetDisabledOutputs();
    size_t i = 0u;
    for (; i < outputs.size() && i < new_layouts.size(); i++) {
      auto& output_pair = outputs[i];
      auto output = output_pair.first;
      auto& output_info = output_pair.second;
      // For the video-dummy driver, the size of |crtcs| is exactly 1 and is
      // different for each Output. In general, this is not true for other
      // video-drivers, and the lists can overlap.
      // TODO(yuweih): Consider making CRTC allocation smarter so it works with
      // non-video-dummy drivers.
      if (output_info.crtcs.empty()) {
        LOG(ERROR) << "No available CRTC found associated with "
                   << reinterpret_cast<char*>(output_info.name.data());
        continue;
      }
      auto crtc = output_info.crtcs.front();
      auto track_layout = new_layouts[i];
      // Note that this has a weird behavior in GNOME, such that, if |output| is
      // "disconnected", creating the mode somehow resizes all existing displays
      // to 1024x768. Once the output is successfully enabled, it will remain
      // "connected" and will no longer have the problem. The problem doesn't
      // occur on XFCE or Cinnamon.
      // TODO(yuweih): See if this is fixable, or at least implement some
      // workaround, such as re-applying the layout.
      auto mode =
          UpdateMode(output, track_layout.width(), track_layout.height());
      if (mode == kInvalidMode) {
        LOG(ERROR) << "Failed to create new mode.";
        continue;
      }
      resizer.AddActiveCrtc(
          crtc, mode, {output},
          gfx::Rect(track_layout.position_x(), track_layout.position_y(),
                    track_layout.width(), track_layout.height()));
      HOST_LOG << "Added display with crtc: " << base::to_underlying(crtc)
               << ", output: " << base::to_underlying(output);
    }
    if (i < diff.new_displays.layouts.size()) {
      LOG(WARNING) << "Failed to create "
                   << (diff.new_displays.layouts.size() - i)
                   << " display(s) due to insufficient resources.";
    }
  }

  // Update displays
  for (const auto& updated_display : diff.updated_displays) {
    auto track_layout = updated_display.layout;
    auto output = GetOutputFromContext(updated_display.context);
    auto crtc = resizer.GetCrtcForOutput(output);
    if (crtc == kDisabledCrtc) {
      // This is not expected to happen. Disabled Outputs are not expected to
      // have any Monitor, but |output| was found in the RRGetMonitors response,
      // so it should have a CRTC attached.
      LOG(ERROR) << "No CRTC found for output: " << base::to_underlying(output);
      continue;
    }
    resizer.DisableCrtc(crtc);
    auto mode = UpdateMode(output, track_layout.width(), track_layout.height());
    if (mode == kInvalidMode) {
      LOG(ERROR) << "Failed to create new mode.";
      continue;
    }
    resizer.UpdateActiveCrtc(
        crtc, mode,
        gfx::Rect(track_layout.position_x(), track_layout.position_y(),
                  track_layout.width(), track_layout.height()));
    HOST_LOG << "Updated display with screen ID: "
             << updated_display.layout.screen_id().value_or(-1);
  }

  // Remove displays
  for (const auto& removed_display : diff.removed_displays) {
    auto output = GetOutputFromContext(removed_display.context);
    auto crtc = resizer.GetCrtcForOutput(output);
    if (crtc == kDisabledCrtc) {
      LOG(ERROR) << "No CRTC found for output: " << base::to_underlying(output);
      continue;
    }
    resizer.DisableCrtc(crtc);
    resizer.RemoveActiveCrtc(crtc);
    DeleteMode(output, GetModeNameForOutput(output));
    HOST_LOG << "Removed display with screen ID: "
             << removed_display.layout.screen_id().value_or(-1);
  }

  resizer.NormalizeCrtcs();
  UpdateRootWindow(resizer);
}

void X11DesktopResizer::SetResolutionForOutput(
    x11::RandR::Output output,
    const DesktopResolution& resolution) {
  // Actually do the resize operation, preserving the current mode name. Note
  // that we have to detach the output from the mode in order to delete the
  // mode and re-create it with the new resolution. The output may also need to
  // be detached from all modes in order to reduce the root window size.
  HOST_LOG << "Resizing RANDR Output " << base::to_underlying(output) << " to "
           << resolution.dimensions().width() << "x"
           << resolution.dimensions().height();

  X11CrtcResizer resizer(resources_.get(), connection_);

  resizer.FetchActiveCrtcs();
  auto crtc = resizer.GetCrtcForOutput(output);

  if (crtc == kDisabledCrtc) {
    // This is not expected to happen. Disabled Outputs are not expected to
    // have any Monitor, but |output| was found in the RRGetMonitors response,
    // so it should have a CRTC attached.
    LOG(ERROR) << "No CRTC found for output: " << base::to_underlying(output);
    return;
  }

  // Disable the output now, so that the old mode can be deleted and the new
  // mode created and added to the output's available modes. The previous size
  // and offsets will be stored in |resizer|.
  resizer.DisableCrtc(crtc);

  auto mode = UpdateMode(output, resolution.dimensions().width(),
                         resolution.dimensions().height());
  if (mode == kInvalidMode) {
    // The CRTC is disabled, but there's no easy way to recover it here
    // (the mode it was attached to has gone).
    LOG(ERROR) << "Failed to create new mode.";
    return;
  }

  // Update |active_crtcs_| with new sizes and offsets.
  resizer.UpdateActiveCrtcs(crtc, mode, resolution.dimensions());
  UpdateRootWindow(resizer);

  gfx::Size size_mm = CalculateSizeInMmForGnome(resolution);
  int width_mm = size_mm.width();
  int height_mm = size_mm.height();
  HOST_LOG << "Setting physical size in mm: " << width_mm << "x" << height_mm;
  SetOutputPhysicalSizeInMM(connection_, output, width_mm, height_mm);

  // Check to see if GNOME is using automatic-scaling. If the value is non-zero,
  // the user prefers a particular scaling, so don't adjust the
  // text-scaling-factor here.
  if (g_settings_get_uint(registry_.get(), "scaling-factor") == 0U) {
    // Start the timer to update the text-scaling-factor. Any previously
    // started timer will be cancelled.
    requested_dpi_ = resolution.dpi().x();
    gnome_delay_timer_.Start(FROM_HERE, kGnomeWaitTime, this,
                             &X11DesktopResizer::RequestGnomeDisplayConfig);
  }
}

x11::RandR::Mode X11DesktopResizer::UpdateMode(x11::RandR::Output output,
                                               int width,
                                               int height) {
  std::string mode_name = GetModeNameForOutput(output);
  DeleteMode(output, mode_name);

  // Set some clock values so that the computed refresh-rate is a realistic
  // number:
  // 60Hz = dot_clock / (htotal * vtotal).
  // This allows GNOME's Display Settings tool to apply new settings for
  // resolution/scaling - see crbug.com/1374488.
  x11::RandR::ModeInfo mode;
  mode.width = width;
  mode.height = height;
  mode.dot_clock = GetDotClockForModeInfo();
  mode.htotal = 1000;
  mode.vtotal = 1000;
  mode.name_len = mode_name.size();
  if (auto reply =
          randr_->CreateMode({root_, mode, mode_name.c_str()}).Sync()) {
    randr_->AddOutputMode({
        output,
        reply->mode,
    });
    return reply->mode;
  }
  return kInvalidMode;
}

void X11DesktopResizer::DeleteMode(x11::RandR::Output output,
                                   const std::string& name) {
  x11::RandR::Mode mode_id = resources_.GetIdForMode(name);
  if (mode_id != kInvalidMode) {
    randr_->DeleteOutputMode({output, mode_id});
    randr_->DestroyMode({mode_id});
    resources_.Refresh(randr_, root_);
  }
}

void X11DesktopResizer::UpdateRootWindow(X11CrtcResizer& resizer) {
  // Disable any CRTCs that have been changed, so that the root window can be
  // safely resized to the bounding-box of the new CRTCs.
  // This is non-optimal: the only CRTCs that need disabling are those whose
  // original rectangles don't fit into the new root window - they are the ones
  // that would prevent resizing the root window. But figuring these out would
  // involve keeping track of all the original rectangles as well as the new
  // ones. So, to keep the implementation simple (and working for any arbitrary
  // layout algorithm), all changed CRTCs are disabled here.
  resizer.DisableChangedCrtcs();

  // Get the dimensions to resize the root window to.
  auto dimensions = resizer.GetBoundingBox();

  // TODO(lambroslambrou): Use the DPI from client size information.
  uint32_t width_mm = PixelsToMillimeters(dimensions.width(), kDefaultDPI);
  uint32_t height_mm = PixelsToMillimeters(dimensions.height(), kDefaultDPI);
  randr_->SetScreenSize({root_, static_cast<uint16_t>(dimensions.width()),
                         static_cast<uint16_t>(dimensions.height()), width_mm,
                         height_mm});

  resizer.MoveApplicationWindows();

  // Apply the new CRTCs, which will re-enable any that were disabled.
  resizer.ApplyActiveCrtcs();
}

X11DesktopResizer::OutputInfoList X11DesktopResizer::GetDisabledOutputs() {
  OutputInfoList disabled_outputs;
  for (x11::RandR::Output output : resources_.get()->outputs) {
    auto reply = randr_
                     ->GetOutputInfo({.output = output,
                                      .config_timestamp =
                                          resources_.get()->config_timestamp})
                     .Sync();
    if (!reply) {
      continue;
    }
    if (reply->crtc == kDisabledCrtc) {
      disabled_outputs.emplace_back(output, std::move(*reply.reply));
    }
  }
  return disabled_outputs;
}

void X11DesktopResizer::RequestGnomeDisplayConfig() {
  // Unretained() is safe because `this` owns gnome_display_config_ which
  // cancels callbacks on destruction.
  gnome_display_config_.GetMonitorsConfig(
      base::BindOnce(&X11DesktopResizer::OnGnomeDisplayConfigReceived,
                     base::Unretained(this)));
}

void X11DesktopResizer::OnGnomeDisplayConfigReceived(
    GnomeDisplayConfig config) {
  // Look for an enabled monitor. Disabled monitors have no Mode set - a
  // monitor can become disabled by being added then removed (using the website
  // Display options). The Xorg xf86-video-dummy driver has a quirk that, once a
  // monitor becomes "connected", it stays forever in the connected state, even
  // if it is later disabled. All connected monitors (enabled or disabled) are
  // included in the GNOME config.

  // For X11, the calculation of the text-scaling-factor does not depend on
  // which enabled monitor is chosen here, because GNOME's X11 backend forces
  // all monitors to have the same scale. However, it makes sense to select
  // an enabled monitor, since a disabled monitor might not have a reliable
  // "scale" property returned by GNOME.
  auto monitor_iter =
      base::ranges::find_if(config.monitors, [](const auto& entry) {
        return entry.second.GetCurrentMode() != nullptr;
      });
  if (monitor_iter == std::ranges::end(config.monitors)) {
    LOG(ERROR) << "No enabled monitor found in GNOME config.";
    return;
  }
  const auto& monitor = monitor_iter->second;

  if (monitor.scale == 0) {
    // This should never happen - avoid division by 0.
    return;
  }

  // The GNOME scaling, multiplied by the GNOME text-scaling-factor, will be the
  // rendered scaling of text. This should be the client's requested DPI divided
  // by kDefaultDPI.
  double text_scaling_factor =
      static_cast<double>(requested_dpi_) / kDefaultDPI / monitor.scale;
  HOST_LOG << "Target DPI = " << requested_dpi_
           << ", GNOME scale = " << monitor.scale
           << ", calculated text-scaling = " << text_scaling_factor;

  if (!g_settings_set_double(registry_.get(), "text-scaling-factor",
                             text_scaling_factor)) {
    // Just log a warning - failure is expected if the value falls outside the
    // interval [0.5, 3.0].
    LOG(WARNING) << "Failed to set text-scaling-factor.";
  }
}

}  // namespace remoting
