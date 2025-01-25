// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_model.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace tabs {

namespace {

// This class exists to allow consumers to look up a TabInterface from an
// instance of WebContents. This is necessary while transitioning features to
// use TabInterface and TabModel instead of WebContents.
class TabLookupFromWebContents
    : public content::WebContentsUserData<TabLookupFromWebContents> {
 public:
  ~TabLookupFromWebContents() override = default;

  TabModel* model() { return model_; }

 private:
  friend WebContentsUserData;
  TabLookupFromWebContents(content::WebContents* contents, TabModel* model)
      : content::WebContentsUserData<TabLookupFromWebContents>(*contents),
        model_(model) {}

  // Semantically owns this class.
  raw_ptr<TabModel> model_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabLookupFromWebContents);

}  // namespace

TabModel::TabModel(std::unique_ptr<content::WebContents> contents,
                   bool is_in_normal_window)
    : contents_owned_(std::move(contents)),
      contents_(contents_owned_.get()),
      is_in_normal_window_(is_in_normal_window) {
  TabLookupFromWebContents::CreateForWebContents(contents_, this);

  tab_features_ = TabFeatures::CreateTabFeatures();

  // Once tabs are pulled into a standalone module, TabFeatures and its
  // initialization will need to be delegated back to the main module.
  tab_features_->Init(
      *this, Profile::FromBrowserContext(contents_->GetBrowserContext()));
}

TabModel::~TabModel() {
  contents_->RemoveUserData(TabLookupFromWebContents::UserDataKey());
}

void TabModel::OnAddedToModel(TabStripModel* owning_model) {
  CHECK(!owning_model_);
  CHECK(owning_model);
  owning_model_ = owning_model;
  owning_model_->AddObserver(this);

  // Being detached is equivalent to being in the background. So after
  // detachment, if the tab is in the foreground, we must send a notification.
  if (IsInForeground()) {
    did_enter_foreground_callback_list_.Notify(this);
  }
}

void TabModel::OnRemovedFromModel() {
  // Going through each field here:
  // Keep `contents_`, obviously.

  owning_model_->RemoveObserver(this);
  owning_model_ = nullptr;

  // Opener stuff doesn't make sense to transfer between browsers.
  opener_ = nullptr;
  reset_opener_on_active_tab_change_ = false;

  // Pinned state, blocked state, and group membership are all preserved, at
  // least in some cases, but for now let's leave that to the existing
  // mechanisms that were handling that.
  // TODO(tbergquist): Decide whether to stick with this approach or not.
  pinned_ = false;
  blocked_ = false;
  group_ = std::nullopt;
}

TabCollection* TabModel::GetParentCollection(
    base::PassKey<TabCollection>) const {
  CHECK(base::FeatureList::IsEnabled(tabs::kTabStripCollectionStorage));
  return parent_collection_;
}

void TabModel::OnReparented(TabCollection* parent,
                            base::PassKey<TabCollection>) {
  CHECK(base::FeatureList::IsEnabled(tabs::kTabStripCollectionStorage));
  parent_collection_ = parent;
}

void TabModel::WillEnterBackground(base::PassKey<TabStripModel>) {
  will_enter_background_callback_list_.Notify(this);
}

void TabModel::WillDetach(base::PassKey<TabStripModel>,
                          tabs::TabInterface::DetachReason reason) {
  will_detach_callback_list_.Notify(this, reason);
}

content::WebContents* TabModel::GetContents() const {
  return contents();
}

base::CallbackListSubscription TabModel::RegisterWillDiscardContents(
    TabInterface::WillDiscardContentsCallback callback) {
  return will_discard_contents_callback_list_.Add(std::move(callback));
}

bool TabModel::IsInForeground() const {
  return owning_model() && owning_model()->GetActiveTab() == this;
}

base::CallbackListSubscription TabModel::RegisterDidEnterForeground(
    TabInterface::DidEnterForegroundCallback callback) {
  return did_enter_foreground_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterWillEnterBackground(
    TabInterface::WillEnterBackgroundCallback callback) {
  return will_enter_background_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterWillDetach(
    TabInterface::WillDetach callback) {
  return will_detach_callback_list_.Add(std::move(callback));
}

bool TabModel::CanShowModalUI() const {
  return !showing_modal_ui_;
}

std::unique_ptr<ScopedTabModalUI> TabModel::ShowModalUI() {
  return std::make_unique<ScopedTabModalUIImpl>(this);
}

bool TabModel::IsInNormalWindow() const {
  return is_in_normal_window_;
}

BrowserWindowInterface* TabModel::GetBrowserWindowInterface() {
  return owning_model_->delegate()->GetBrowserWindowInterface();
}

tabs::TabFeatures* TabModel::GetTabFeatures() {
  return tab_features_.get();
}

void TabModel::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }

  if (selection.new_contents == contents()) {
    did_enter_foreground_callback_list_.Notify(this);
    return;
  }
}

TabModel::ScopedTabModalUIImpl::ScopedTabModalUIImpl(TabModel* tab)
    : tab_(tab) {
  CHECK(!tab_->showing_modal_ui_);
  tab_->showing_modal_ui_ = true;
}

TabModel::ScopedTabModalUIImpl::~ScopedTabModalUIImpl() {
  tab_->showing_modal_ui_ = false;
}

void TabModel::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("web_contents", contents());
  dict.Add("pinned", pinned());
  dict.Add("blocked", blocked());
}

std::unique_ptr<content::WebContents> TabModel::DiscardContents(
    std::unique_ptr<content::WebContents> contents) {
  will_discard_contents_callback_list_.Notify(this, contents_, contents.get());
  contents_->RemoveUserData(TabLookupFromWebContents::UserDataKey());
  std::unique_ptr<content::WebContents> old_contents =
      std::move(contents_owned_);
  contents_owned_ = std::move(contents);
  contents_ = contents_owned_.get();
  TabLookupFromWebContents::CreateForWebContents(contents_, this);
  return old_contents;
}

// static
std::unique_ptr<content::WebContents> TabModel::DestroyAndTakeWebContents(
    std::unique_ptr<TabModel> tab_model) {
  std::unique_ptr<content::WebContents> contents =
      std::move(tab_model->contents_owned_);
  return contents;
}

// static
TabInterface* TabInterface::GetFromContents(
    content::WebContents* web_contents) {
  return TabLookupFromWebContents::FromWebContents(web_contents)->model();
}

}  // namespace tabs
