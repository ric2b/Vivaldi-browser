// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "content/public/browser/media_session.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

constexpr base::TimeDelta kInactiveTimerDelay =
    base::TimeDelta::FromMinutes(60);

// Here we check to see if the WebContents is focused. Note that since Session
// is a WebContentsObserver, we could in theory listen for
// |OnWebContentsFocused()| and |OnWebContentsLostFocus()|. However, this won't
// actually work since focusing the MediaDialogView causes the WebContents to
// "lose focus", so we'd never be focused.
bool IsWebContentsFocused(content::WebContents* web_contents) {
  DCHECK(web_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return false;

  // If the given WebContents is not in the focused window, then it's not
  // focused. Note that we know a Browser is focused because otherwise the user
  // could not interact with the MediaDialogView.
  if (BrowserList::GetInstance()->GetLastActive() != browser)
    return false;

  return browser->tab_strip_model()->GetActiveWebContents() == web_contents;
}

}  // anonymous namespace

MediaToolbarButtonController::Session::Session(
    MediaToolbarButtonController* owner,
    const std::string& id,
    std::unique_ptr<media_message_center::MediaNotificationItem> item,
    content::WebContents* web_contents,
    const Browser* browser,
    mojo::Remote<media_session::mojom::MediaController> controller)
    : content::WebContentsObserver(web_contents),
      owner_(owner),
      id_(id),
      item_(std::move(item)),
      browser_(browser) {
  DCHECK(owner_);
  DCHECK(item_);

  SetController(std::move(controller));
}

MediaToolbarButtonController::Session::~Session() {
  // ~WebContentsObserver is ran after all the member's destructors. When
  // `item_` is destroyed, it triggers a list of destruction which ends up
  // re-entering and attempts to call a WebContentsObserver callback which
  // fails. In order to avoid this from happening, the destructor stops
  // observing before the implicit destructors are run.
  Observe(nullptr);
}

void MediaToolbarButtonController::Session::WebContentsDestroyed() {
  // If the WebContents is destroyed, then we should just remove the item
  // instead of freezing it.
  owner_->RemoveItem(id_);
}

void MediaToolbarButtonController::Session::OnWebContentsFocused(
    content::RenderWidgetHost*) {
  OnSessionInteractedWith();
}

void MediaToolbarButtonController::Session::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  bool playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;

  // If we've started playing, we don't want the inactive timer to be running.
  if (playing) {
    inactive_timer_.Stop();
    return;
  }

  // If the timer is already running, we don't need to do anything.
  if (inactive_timer_.IsRunning())
    return;

  StartInactiveTimer();
}

void MediaToolbarButtonController::Session::MediaSessionPositionChanged(
    const base::Optional<media_session::MediaPosition>& position) {
  OnSessionInteractedWith();
}

void MediaToolbarButtonController::Session::SetController(
    mojo::Remote<media_session::mojom::MediaController> controller) {
  if (controller.is_bound()) {
    observer_receiver_.reset();
    controller->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
  }
}

void MediaToolbarButtonController::Session::OnSessionInteractedWith() {
  // If we're not currently tracking inactive time, then no action is needed.
  if (!inactive_timer_.IsRunning())
    return;

  // Otherwise, reset the timer.
  inactive_timer_.Stop();
  StartInactiveTimer();
}

void MediaToolbarButtonController::Session::StartInactiveTimer() {
  DCHECK(!inactive_timer_.IsRunning());
  // Using |base::Unretained()| here is okay since |this| owns
  // |inactive_timer_|.
  inactive_timer_.Start(
      FROM_HERE, kInactiveTimerDelay,
      base::BindOnce(
          &MediaToolbarButtonController::Session::OnInactiveTimerFired,
          base::Unretained(this)));
}

void MediaToolbarButtonController::Session::OnInactiveTimerFired() {
  // If the session has been paused and inactive for long enough, then
  // dismiss it. To prevent issues, only the MediaToolbarButtonController for
  // same window as the session will do the dismissing and record metrics.
  if (IsSameWindow())
    item_->Dismiss();
}

bool MediaToolbarButtonController::Session::IsSameWindow() {
  if (!web_contents() || !browser_)
    return false;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  return browser_ == browser;
}

MediaToolbarButtonController::MediaToolbarButtonController(
    const base::UnguessableToken& source_id,
    service_manager::Connector* connector,
    MediaToolbarButtonControllerDelegate* delegate,
    const Browser* browser)
    : connector_(connector), delegate_(delegate), browser_(browser) {
  DCHECK(delegate_);

  // |connector| can be null in tests.
  if (!connector_)
    return;

  // Connect to the controller manager so we can create media controllers for
  // media sessions.
  connector_->Connect(media_session::mojom::kServiceName,
                      controller_manager_remote_.BindNewPipeAndPassReceiver());

  // Connect to receive audio focus events.
  connector_->Connect(media_session::mojom::kServiceName,
                      audio_focus_remote_.BindNewPipeAndPassReceiver());
  audio_focus_remote_->AddSourceObserver(
      source_id, audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

  audio_focus_remote_->GetSourceFocusRequests(
      source_id,
      base::BindOnce(
          &MediaToolbarButtonController::OnReceivedAudioFocusRequests,
          weak_ptr_factory_.GetWeakPtr()));
}

MediaToolbarButtonController::~MediaToolbarButtonController() {
  for (auto container_pair : observed_containers_)
    container_pair.second->RemoveObserver(this);
}

void MediaToolbarButtonController::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  // If we have an existing unfrozen item then this is a duplicate call and
  // we should ignore it.
  auto it = sessions_.find(id);
  if (it != sessions_.end() && !it->second.item()->frozen())
    return;

  mojo::Remote<media_session::mojom::MediaController> item_controller;
  mojo::Remote<media_session::mojom::MediaController> session_controller;

  // |controller_manager_remote_| may be null in tests where connector is
  // unavailable.
  if (controller_manager_remote_) {
    controller_manager_remote_->CreateMediaControllerForSession(
        item_controller.BindNewPipeAndPassReceiver(), *session->request_id);
    controller_manager_remote_->CreateMediaControllerForSession(
        session_controller.BindNewPipeAndPassReceiver(), *session->request_id);
  }

  if (it != sessions_.end()) {
    // If the notification was previously frozen then we should reset the
    // controller because the mojo pipe would have been reset.
    it->second.SetController(std::move(session_controller));
    it->second.item()->SetController(std::move(item_controller),
                                     std::move(session->session_info));
    active_controllable_session_ids_.insert(id);
    frozen_session_ids_.erase(id);
    UpdateToolbarButtonState();
  } else {
    sessions_.emplace(
        std::piecewise_construct, std::forward_as_tuple(id),
        std::forward_as_tuple(
            this, id,
            std::make_unique<media_message_center::MediaNotificationItem>(
                this, id, session->source_name.value_or(std::string()),
                std::move(item_controller), std::move(session->session_info)),
            content::MediaSession::GetWebContentsFromRequestId(
                *session->request_id),
            browser_, std::move(session_controller)));
  }
}

void MediaToolbarButtonController::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  it->second.item()->Freeze();
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.insert(id);
  UpdateToolbarButtonState();
}

void MediaToolbarButtonController::ShowNotification(const std::string& id) {
  active_controllable_session_ids_.insert(id);
  UpdateToolbarButtonState();

  if (!dialog_delegate_)
    return;

  base::WeakPtr<media_message_center::MediaNotificationItem> item;

  auto it = sessions_.find(id);
  if (it != sessions_.end())
    item = it->second.item()->GetWeakPtr();

  MediaNotificationContainerImpl* container =
      dialog_delegate_->ShowMediaSession(id, item);

  // Observe the container for dismissal.
  if (container) {
    container->AddObserver(this);
    observed_containers_[id] = container;
  }
}

void MediaToolbarButtonController::HideNotification(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);
  UpdateToolbarButtonState();

  if (!dialog_delegate_)
    return;

  dialog_delegate_->HideMediaSession(id);
}

scoped_refptr<base::SequencedTaskRunner>
MediaToolbarButtonController::GetTaskRunner() const {
  return nullptr;
}

void MediaToolbarButtonController::RemoveItem(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);
  sessions_.erase(id);

  UpdateToolbarButtonState();
}

void MediaToolbarButtonController::LogMediaSessionActionButtonPressed(
    const std::string& id) {
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  content::WebContents* web_contents = it->second.web_contents();
  if (!web_contents)
    return;

  base::UmaHistogramBoolean("Media.GlobalMediaControls.UserActionFocus",
                            IsWebContentsFocused(web_contents));
}

void MediaToolbarButtonController::OnContainerClicked(const std::string& id) {
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  content::WebContents* web_contents = it->second.web_contents();
  if (!web_contents)
    return;

  content::WebContentsDelegate* delegate = web_contents->GetDelegate();
  if (!delegate)
    return;

  delegate->ActivateContents(web_contents);
}

void MediaToolbarButtonController::OnContainerDismissed(const std::string& id) {
  auto it = sessions_.find(id);
  if (it != sessions_.end())
    it->second.item()->Dismiss();
}

void MediaToolbarButtonController::OnContainerDestroyed(const std::string& id) {
  auto iter = observed_containers_.find(id);
  DCHECK(iter != observed_containers_.end());

  iter->second->RemoveObserver(this);
  observed_containers_.erase(iter);
}

void MediaToolbarButtonController::SetDialogDelegate(
    MediaDialogDelegate* delegate) {
  DCHECK(!delegate || !dialog_delegate_);
  dialog_delegate_ = delegate;

  UpdateToolbarButtonState();

  if (!dialog_delegate_)
    return;

  for (const std::string& id : active_controllable_session_ids_) {
    base::WeakPtr<media_message_center::MediaNotificationItem> item;

    auto it = sessions_.find(id);
    if (it != sessions_.end())
      item = it->second.item()->GetWeakPtr();

    MediaNotificationContainerImpl* container =
        dialog_delegate_->ShowMediaSession(id, item);

    // Observe the container for dismissal.
    if (container) {
      container->AddObserver(this);
      observed_containers_[id] = container;
    }
  }

  media_message_center::RecordConcurrentNotificationCount(
      active_controllable_session_ids_.size());
}

void MediaToolbarButtonController::OnReceivedAudioFocusRequests(
    std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions) {
  for (auto& session : sessions)
    OnFocusGained(std::move(session));
}

void MediaToolbarButtonController::UpdateToolbarButtonState() {
  if (!active_controllable_session_ids_.empty()) {
    if (delegate_display_state_ != DisplayState::kShown) {
      delegate_->Enable();
      delegate_->Show();
    }
    delegate_display_state_ = DisplayState::kShown;
    return;
  }

  if (frozen_session_ids_.empty()) {
    if (delegate_display_state_ != DisplayState::kHidden)
      delegate_->Hide();
    delegate_display_state_ = DisplayState::kHidden;
    return;
  }

  if (!dialog_delegate_) {
    if (delegate_display_state_ != DisplayState::kDisabled)
      delegate_->Disable();
    delegate_display_state_ = DisplayState::kDisabled;
  }
}
