// Copyright (c) 2017-2020 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_ui_web_contents_delegate.h"

#include "app/vivaldi_constants.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/color_chooser.h"
#include "chrome/common/pref_names.h"
#include "components/printing/browser/print_composite_client.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_messages.h"
#include "renderer/vivaldi_render_messages.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_native_app_window_views.h"

namespace {

void OnUIProcessCrash(base::TerminationStatus status) {
  static bool after_ui_crash = false;
  if (after_ui_crash)
    return;
  after_ui_crash = true;
  double uptime_seconds = (base::TimeTicks::Now() -
                           VivaldiBrowserWindow::GetFirstWindowCreationTime())
                              .InSecondsF();
  LOG(ERROR) << "UI Process abnormally terminates with status " << status
             << " after running for " << uptime_seconds << " seconds!";

  // Restart or exit while preserving the tab and window session as it was
  // before the crash. For that pretend that we got the end-of-ssession signal
  // that makes Chromium to close all windows without running any unload
  // handlers or recording session updates.
  browser_shutdown::OnShutdownStarting(
      browser_shutdown::ShutdownType::kEndSession);

  chrome::CloseAllBrowsers();

  bool want_restart = false;
#ifndef DEBUG
  // TODO(igor@vivaldi.com): Consider restarting on
  // TERMINATION_STATUS_PROCESS_WAS_KILLED in addition to crashes in case
  // the user accidentally kills the UI process in the task manager.
  using base::TerminationStatus;
  if (status == TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED) {
    want_restart = true;
  }
#endif  // _DEBUG
  if (want_restart) {
    // Prevent restart loop if UI crashes shortly after the startup.
    constexpr double MIN_UPTIME_TO_RESTART_SECONDS = 60.0;
    if (uptime_seconds >= MIN_UPTIME_TO_RESTART_SECONDS) {
      LOG(ERROR) << "Restarting Vivaldi";
      chrome::AttemptRestart();
      return;
    }
  }
  LOG(ERROR) << "Quiting Vivaldi";
  chrome::AttemptExit();
}

}  // namespace

VivaldiUIWebContentsDelegate::VivaldiUIWebContentsDelegate(
  VivaldiBrowserWindow* window)
  : window_(window) {}

VivaldiUIWebContentsDelegate::~VivaldiUIWebContentsDelegate() = default;

void VivaldiUIWebContentsDelegate::Initialize() {
  Observe(window_->web_contents());
  window_->web_contents()->SetDelegate(this);
}

bool VivaldiUIWebContentsDelegate::HandleKeyboardEvent(
  content::WebContents* source,
  const content::NativeWebKeyboardEvent& event) {
  return window_->HandleKeyboardEvent(event);
}

void VivaldiUIWebContentsDelegate::ContentsMouseEvent(
  content::WebContents* source,
  bool motion,
  bool exited) {
  window_->HandleMouseChange(motion);
}

bool VivaldiUIWebContentsDelegate::PreHandleGestureEvent(
  content::WebContents* source,
  const blink::WebGestureEvent& event) {
  // When called this means the user has attempted a gesture on the UI. We do
  // not allow that.
#if defined(OS_MAC)
  if (event.GetType() == blink::WebInputEvent::Type::kGestureDoubleTap)
    return true;
#endif
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

content::ColorChooser* VivaldiUIWebContentsDelegate::OpenColorChooser(
  content::WebContents* web_contents,
  SkColor initial_color,
  const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void VivaldiUIWebContentsDelegate::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

void VivaldiUIWebContentsDelegate::NavigationStateChanged(
  content::WebContents* source,
  content::InvalidateTypes changed_flags) {
  if (changed_flags &
    (content::INVALIDATE_TYPE_TAB | content::INVALIDATE_TYPE_TITLE)) {
    window_->UpdateTitleBar();
  }
}

void VivaldiUIWebContentsDelegate::RequestMediaAccessPermission(
  content::WebContents* web_contents,
  const content::MediaStreamRequest& request,
  content::MediaResponseCallback callback) {
  DCHECK_EQ(window_->web_contents(), web_contents);
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), window_->extension());
}

bool VivaldiUIWebContentsDelegate::CheckMediaAccessPermission(
  content::RenderFrameHost* render_frame_host,
  const GURL& security_origin,
  blink::mojom::MediaStreamType type) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(window_->GetProfile())
          ->enabled_extensions()
          .GetByID(security_origin.host());

  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type,
                                   extension);
}

// If we should ever need to play PIP videos in our UI, this code enables
// it. The implementation for webpages is in WebViewGuest.
content::PictureInPictureResult
VivaldiUIWebContentsDelegate::EnterPictureInPicture(
  content::WebContents* web_contents,
  const viz::SurfaceId& surface_id,
  const gfx::Size& natural_size) {
  return PictureInPictureWindowManager::GetInstance()->EnterPictureInPicture(
      web_contents, surface_id, natural_size);
}

void VivaldiUIWebContentsDelegate::ExitPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

void VivaldiUIWebContentsDelegate::PrintCrossProcessSubframe(
  content::WebContents* web_contents,
  const gfx::Rect& rect,
  int document_cookie,
  content::RenderFrameHost* subframe_host) const {

  // |web_contents| is the app-contents which we do not want to print.
  web_contents = content::WebContentsImpl::FromRenderFrameHostID(
      subframe_host->GetProcess()->GetID(), subframe_host->GetRoutingID());

  auto* client = printing::PrintCompositeClient::FromWebContents(web_contents);
  if (client) {
    client->PrintCrossProcessSubframe(rect, document_cookie, subframe_host);
  }
}

void VivaldiUIWebContentsDelegate::ActivateContents(
    content::WebContents* contents) {
  window_->Activate();
}

void VivaldiUIWebContentsDelegate::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // Follow ChromeExtensionWebContentsObserver::InitializeRenderFrame() and
  // notify the renderer about the window id so
  // chrome.extension.getViews({windowId}) works in our UI.
  render_frame_host->Send(new ExtensionMsg_UpdateBrowserWindowId(
      render_frame_host->GetRoutingID(), window_->id()));

  if (window_->requested_alpha_enabled()) {
    VivaldiNativeAppWindowViews* window_views = window_->views();
    if (window_views && window_views->CanHaveAlphaEnabled()) {
      content::RenderWidgetHostView* host_view =
          render_frame_host->GetView();
      DCHECK(host_view);
      host_view->SetBackgroundColor(SK_ColorTRANSPARENT);
    }
  }

  // An incognito profile is not initialized with the UI zoom value. Set it up
  // here by reading prefs from the regular profile. At this point we do not
  // know the partition key (see ChromeZoomLevelPrefs::InitHostZoomMap) so we
  // just test all keys until we match the kVivaldiAppId host.
  if (window_->GetProfile()->IsOffTheRecord()) {
    PrefService* pref_service =
      window_->GetProfile()->GetOriginalProfile()->GetPrefs();
    const base::DictionaryValue* partition_dict =
      pref_service->GetDictionary(prefs::kPartitionPerHostZoomLevels);
    bool match = false;
    for (base::DictionaryValue::Iterator partition(*partition_dict);
         !partition.IsAtEnd() && !match; partition.Advance()) {
      const base::DictionaryValue* host_dict = nullptr;
      partition_dict->GetDictionary(partition.key(), &host_dict);
      if (host_dict) {
        // Test each host until we match kVivaldiAppId
        for (base::DictionaryValue::Iterator host(*host_dict); !host.IsAtEnd();
             host.Advance()) {
          if (host.key() == ::vivaldi::kVivaldiAppId) {
            match = true;
            // Each host is another dictionary with settings
            const base::DictionaryValue* settings_dict = nullptr;
            if (host.value().GetAsDictionary(&settings_dict)) {
              for (base::DictionaryValue::Iterator setting(*settings_dict);
                   !setting.IsAtEnd(); setting.Advance()) {
                if (setting.key() == "zoom_level") {
                  double zoom_level = 0;
                  if (setting.value().GetAsDouble(&zoom_level)) {
                    content::HostZoomMap* zoom_map =
                        content::HostZoomMap::GetForWebContents(
                            window_->web_contents());
                    DCHECK(zoom_map);
                    zoom_map->SetZoomLevelForHost(::vivaldi::kVivaldiAppId,
                      zoom_level);
                  }
                  break;
                }
              }
            }
            break;
          }
        }
      }
    }
  }
}

void VivaldiUIWebContentsDelegate::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  window_->views()->OnViewWasResized();
}

void VivaldiUIWebContentsDelegate::RenderProcessGone(
  base::TerminationStatus status) {
  if (status !=
    base::TerminationStatus::TERMINATION_STATUS_NORMAL_TERMINATION &&
    status != base::TerminationStatus::TERMINATION_STATUS_STILL_RUNNING) {
    OnUIProcessCrash(status);
  }
}

bool VivaldiUIWebContentsDelegate::OnMessageReceived(
  const IPC::Message& message,
  content::RenderFrameHost* sender) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(VivaldiUIWebContentsDelegate, message,
                                   sender)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_UpdateDraggableRegions,
                        UpdateDraggableRegions)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void VivaldiUIWebContentsDelegate::DidFinishNavigation(
  content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  // ExtensionFrameHelper::ReadyToCommitNavigation() will suspend the parser
  // to avoid a race condition reported in
  // https://bugs.chromium.org/p/chromium/issues/detail?id=822650.
  // We need to resume the parser here as we do not use the app window
  // bindings.
  content::RenderFrameHostImpl* host =
    static_cast<content::RenderFrameHostImpl*>(
      navigation_handle->GetRenderFrameHost());
  DCHECK(host);
  if (host->GetParent() == nullptr) {
    host->Send(new VivaldiFrameHostMsg_ResumeParser(host->routing_id()));
  }
  // will run the callback set in WindowPrivateCreateFunction and then remove
  // it.
  window_->OnDidFinishNavigation(/*success=*/true);
}

void VivaldiUIWebContentsDelegate::UpdateDraggableRegions(
  content::RenderFrameHost* sender,
  const std::vector<extensions::DraggableRegion>& regions) {
  // Only process events for the main frame.
  if (!sender->GetParent()) {
    window_->views()->UpdateDraggableRegions(regions);
  }
}

void VivaldiUIWebContentsDelegate::DidFinishLoad(
  content::RenderFrameHost* render_frame_host,
  const GURL& validated_url) {
  window_->UpdateTitleBar();
  window_->ForceShow();
}

void VivaldiUIWebContentsDelegate::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only fire for mainframe.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  window_->ContentsDidStartNavigation();
}
