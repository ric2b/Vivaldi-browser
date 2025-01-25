// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"

TestBrowser::TestBrowser(
    ChromeBrowserState* browser_state,
    SceneState* scene_state,
    std::unique_ptr<WebStateListDelegate> web_state_list_delegate,
    Type type)
    : type_(type),
      browser_state_(browser_state),
      scene_state_(scene_state),
      web_state_list_delegate_(std::move(web_state_list_delegate)),
      command_dispatcher_([[CommandDispatcher alloc] init]) {
  DCHECK(browser_state_);
  DCHECK(web_state_list_delegate_);
  web_state_list_ =
      std::make_unique<WebStateList>(web_state_list_delegate_.get());
}

TestBrowser::TestBrowser(ChromeBrowserState* browser_state,
                         SceneState* scene_state)
    : TestBrowser(browser_state,
                  scene_state,
                  std::make_unique<FakeWebStateListDelegate>(),
                  browser_state->IsOffTheRecord() ? Type::kIncognito
                                                  : Type::kRegular) {}

TestBrowser::TestBrowser(
    ChromeBrowserState* browser_state,
    std::unique_ptr<WebStateListDelegate> web_state_list_delegate)
    : TestBrowser(browser_state,
                  nil,
                  std::move(web_state_list_delegate),
                  browser_state->IsOffTheRecord() ? Type::kIncognito
                                                  : Type::kRegular) {}

TestBrowser::TestBrowser(ChromeBrowserState* browser_state)
    : TestBrowser(browser_state,
                  nil,
                  std::make_unique<FakeWebStateListDelegate>(),
                  browser_state->IsOffTheRecord() ? Type::kIncognito
                                                  : Type::kRegular) {}

TestBrowser::~TestBrowser() {
  for (auto& observer : observers_) {
    observer.BrowserDestroyed(this);
  }
}

#pragma mark - Browser

Browser::Type TestBrowser::type() const {
  return type_;
}

// TODO(crbug.com/358301380): After all usage has changed to GetProfile(),
// remove this method.
ChromeBrowserState* TestBrowser::GetBrowserState() {
  return GetProfile();
}

ChromeBrowserState* TestBrowser::GetProfile() {
  return browser_state_;
}

WebStateList* TestBrowser::GetWebStateList() {
  return web_state_list_.get();
}

CommandDispatcher* TestBrowser::GetCommandDispatcher() {
  return command_dispatcher_;
}

SceneState* TestBrowser::GetSceneState() {
  return scene_state_;
}

void TestBrowser::AddObserver(BrowserObserver* observer) {
  observers_.AddObserver(observer);
}

void TestBrowser::RemoveObserver(BrowserObserver* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<Browser> TestBrowser::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool TestBrowser::IsInactive() const {
  return type_ == Type::kInactive;
}

Browser* TestBrowser::GetActiveBrowser() {
  return this;
}

Browser* TestBrowser::GetInactiveBrowser() {
  return nullptr;
}

Browser* TestBrowser::CreateInactiveBrowser() {
  CHECK_EQ(type_, Type::kRegular);
  inactive_browser_ = std::make_unique<TestBrowser>(
      browser_state_, scene_state_,
      std::make_unique<FakeWebStateListDelegate>(), Type::kInactive);
  SnapshotBrowserAgent::CreateForBrowser(inactive_browser_.get());
  SnapshotBrowserAgent::FromBrowser(inactive_browser_.get())
      ->SetSessionID("some_id");
  return inactive_browser_.get();
}

void TestBrowser::DestroyInactiveBrowser() {
  NOTREACHED();
}
