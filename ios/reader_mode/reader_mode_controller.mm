// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#import "ios/reader_mode/reader_mode_controller.h"

#import "base/check.h"
#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/reader_mode/vivaldi_reader_mode_java_script_feature.h"
#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_animator.h"
#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_floating_ui.h"
#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_prefs.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs_helper.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/web/public/web_state.h"
#import "prefs/vivaldi_pref_names.h"

namespace {
// Returns the content view for the current WebState.
UIView* ContentViewForWebState(web::WebState* web_state) {
  return web_state ? web_state->GetView() : nil;
}

// Returns the container for a given content view.
UIView* ContainerViewForContent(UIView* content_view) {
  return content_view ? (content_view.superview ?: content_view) : nil;
}

// Applies saved preferences (font size/family/theme) to current page.
void ApplySavedPrefsToPage(ReaderModeController* controller, web::WebState* ws){
  if (!controller || !ws)
    return;
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(ws->GetBrowserState());
  PrefService* prefs = profile ? profile->GetPrefs() : nullptr;
  if (!prefs)
    return;
  int font_size =
      [VivaldiReaderModePrefs getReaderModeFontSizeWithPrefService:prefs];
  NSString* font_family =
      [VivaldiReaderModePrefs getReaderModeFontFamilyWithPrefService:prefs];
  NSString* theme =
      [VivaldiReaderModePrefs getReaderModeThemeWithPrefService:prefs];
  controller->SetFontSize(font_size);
  if (font_family) {
    controller->SetFontFamily(base::SysNSStringToUTF8(font_family));
  }
  if (theme) {
    controller->SetTheme(base::SysNSStringToUTF8(theme));
  }
}
}  // namespace

// Registry to map WebState to ReaderModeController
// Using base::NoDestructor to avoid exit-time destructor issues
static base::NoDestructor<std::map<web::WebState*,
                          ReaderModeController*>>& GetControllerRegistry() {
  static base::NoDestructor<std::map<web::WebState*,
                            ReaderModeController*>> registry;
  return registry;
}

// SetBrowser is static to apply browser instance globally to all controllers
// We don't need multiple objects in the registry/memory
void ReaderModeController::SetBrowser(Browser* browser) {
  auto& registry = *GetControllerRegistry();
  for (auto& pair : registry) {
    ReaderModeController* controller = pair.second;
    if (!controller) continue;
    controller->browser_ = browser;
  }
}

// Observer wrapper that routes callbacks to the appropriate controller
class ReaderModeControllerObserverBridge :
        public VivaldiReaderModeJavaScriptFeature::Observer {
 public:
  ReaderModeControllerObserverBridge() = default;
  ~ReaderModeControllerObserverBridge() override = default;

  void OnReaderModeAvailabilityResult(web::WebState* web_state,
                                      bool available) override {
    auto& registry = *GetControllerRegistry();
    auto it = registry.find(web_state);
    if (it != registry.end()) {
      it->second->OnReaderModeAvailabilityResultInternal(web_state, available);
    }
  }

  void OnReaderModeEnabledStateResult(web::WebState* web_state,
                                                  bool enabled) override {
    auto& registry = *GetControllerRegistry();
    auto it = registry.find(web_state);
    if (it != registry.end()) {
      it->second->OnReaderModeEnabledStateResultInternal(web_state, enabled);
    }
  }
};

static base::NoDestructor<
    std::unique_ptr<ReaderModeControllerObserverBridge>>& GetObserverBridge() {
  static base::NoDestructor<
    std::unique_ptr<ReaderModeControllerObserverBridge>> bridge;
  return bridge;
}

ReaderModeController::ReaderModeController(web::WebState* web_state)
    : web_state_(web_state),
      is_reader_mode_available_(false),
      is_reader_mode_enabled_(false),
      observers_([[NSMutableSet alloc] init]) {
  web_state_->AddObserver(this);

  // Initialize the observer bridge if needed
  auto& bridge_holder = GetObserverBridge();
  auto& bridge = *bridge_holder;
  if (!bridge) {
    bridge = std::make_unique<ReaderModeControllerObserverBridge>();
    VivaldiReaderModeJavaScriptFeature::GetInstance()
                                              ->SetObserver(bridge.get());
  }
  // Register this controller in the registry
  auto& registry = *GetControllerRegistry();
  registry[web_state] = this;
}

ReaderModeController::~ReaderModeController() {
  // Remove from registry
  if (web_state_) {
    auto& registry = *GetControllerRegistry();
    registry.erase(web_state_);
  }
  // Ensure any retained Obj-C UI state is torn down and released.
  RemoveFloatingUI();
  [observers_ removeAllObjects];
}

ReaderModeController*
            ReaderModeController::FromWebState(web::WebState* web_state) {
  // First try to get existing instance using WebStateUserData
  ReaderModeController* existing =
      web::WebStateUserData<ReaderModeController>::FromWebState(web_state);
  if (existing) {
    return existing;
  }

  // Create new instance if needed
  web::WebStateUserData<ReaderModeController>::CreateForWebState(web_state);
  return web::WebStateUserData<ReaderModeController>::FromWebState(web_state);
}

void ReaderModeController::AddObserver(
      id<ReaderModeControllerObserver> observer) {
  if (observer) {
    [observers_ addObject:observer];
  }
}

void ReaderModeController::RemoveObserver(
      id<ReaderModeControllerObserver> observer) {
  if (observer) {
    [observers_ removeObject:observer];
  }
}

void ReaderModeController::CheckReaderModeAvailability() {
  // Don't check if reader mode is disabled in settings
  if (!IsReaderModeEnabledInSettings()) {
    is_reader_mode_available_ = false;
    NotifyAvailabilityObservers(false);
    return;
  }

  VivaldiReaderModeJavaScriptFeature::
        GetInstance()->CheckReaderModeAvailability(web_state_);
}

void ReaderModeController::CheckReaderModeEnabledState() {
  VivaldiReaderModeJavaScriptFeature::
        GetInstance()->CheckReaderModeEnabledState(web_state_);
}

bool ReaderModeController::IsReaderModeEnabled() const {
  return is_reader_mode_enabled_;
}

bool ReaderModeController::ToggleReaderMode() {
  if (is_reader_mode_enabled_) {
    DisableReaderMode();
  } else {
    ApplyReaderMode();
  }
  return is_reader_mode_enabled_;
}

void ReaderModeController::ApplyReaderMode() {
  if (!IsReaderModeEnabledInSettings() || !web_state_) {
    return;
  }

  // Prepare container and content views for animation.
  UIView* contentView = ContentViewForWebState(web_state_);
  UIView* contentContainerView = ContainerViewForContent(contentView);

  base::WeakPtr<ReaderModeController> weak_ptr = weak_factory_.GetWeakPtr();
  [VivaldiReaderModeAnimator
    playTransitionOnContentContainerView:contentContainerView
                             contentView:contentView
                              startPoint:CGPointMake(0.5, 0.0)
                               wipeColor:web_state_->GetThemeColor()
                                 toggler:^{
                      if (!weak_ptr) return;
                      ReaderModeController* controller = weak_ptr.get();
                      VivaldiReaderModeJavaScriptFeature::GetInstance()
                          ->ApplyReaderMode(controller->web_state_);
                      ApplySavedPrefsToPage(controller, controller->web_state_);
                      controller->is_reader_mode_enabled_ = true;
                      controller->CreateFloatingUI();
                      controller->NotifyEnabledStateObservers(true);
                    }
   ];
}

void ReaderModeController::DisableReaderMode() {
  if (!web_state_)
    return;
  UIView* contentView = ContentViewForWebState(web_state_);
  UIView* contentContainerView = ContainerViewForContent(contentView);

  base::WeakPtr<ReaderModeController> weak_ptr = weak_factory_.GetWeakPtr();
  [VivaldiReaderModeAnimator
      playTransitionOnContentContainerView:contentContainerView
                               contentView:contentView
                                startPoint:CGPointMake(0.5, 0.0)
                                 wipeColor:web_state_->GetThemeColor()
                                   toggler:^{
                       if (!weak_ptr)
                         return;
                       ReaderModeController* controller = weak_ptr.get();
                       VivaldiReaderModeJavaScriptFeature::GetInstance()
                            ->DisableReaderMode(controller->web_state_);
                       controller->is_reader_mode_enabled_ = false;
                       controller->RemoveFloatingUI();
                       controller->NotifyEnabledStateObservers(false);
                     }
   ];
}

bool ReaderModeController::SetFontSize(int size) {
  return VivaldiReaderModeJavaScriptFeature::
      GetInstance()->SetFontSize(web_state_, size);
}

bool ReaderModeController::SetFontFamily(const std::string& family) {
  return VivaldiReaderModeJavaScriptFeature::
      GetInstance()->SetFontFamily(web_state_, family);
}

bool ReaderModeController::SetTheme(const std::string& theme) {
  return VivaldiReaderModeJavaScriptFeature::
      GetInstance()->SetTheme(web_state_, theme);
}

void ReaderModeController::
        OnReaderModeAvailabilityResultInternal(web::WebState* web_state,
                                                         bool available) {
  DCHECK_EQ(web_state_, web_state);
  is_reader_mode_available_ = available;
  NotifyAvailabilityObservers(available);
}

void ReaderModeController::
      OnReaderModeEnabledStateResultInternal(web::WebState* web_state,
                                                         bool enabled) {
  // This method is called by the observer bridge to
  // route callbacks to this controller
  DCHECK_EQ(web_state_, web_state);
  is_reader_mode_enabled_ = enabled;
  CreateFloatingUI();
  NotifyEnabledStateObservers(enabled);
}

#pragma mark - web::WebStateObserver
void ReaderModeController::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  // Remove from registry before nulling web_state_
  auto& registry = *GetControllerRegistry();
  registry.erase(web_state_);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  // Release UI state if any;
  RemoveFloatingUI();
}

void ReaderModeController::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    // Reset reader mode states when a new page loads
    is_reader_mode_available_ = false;
    is_reader_mode_enabled_ = false;
    // Check both availability and enabled state for the new page
    CheckReaderModeAvailability();
    CheckReaderModeEnabledState();
    CreateFloatingUI();
  }
}

#pragma mark - Private

bool ReaderModeController::IsReaderModeEnabledInSettings() const {
  if (!web_state_) {
    return false;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  return profile->GetPrefs()
                    ->GetBoolean(vivaldiprefs::kVivaldiReaderModeEnabled);
}

void ReaderModeController::NotifyAvailabilityObservers(bool available) {
  // Create a copy of observers to iterate over in case observers modify the set
  NSSet* observersCopy = [observers_ copy];
  for (id<ReaderModeControllerObserver> observer in observersCopy) {
    [observer onReaderModeAvailabilityChanged:available];
  }
}

void ReaderModeController::NotifyEnabledStateObservers(bool enabled) {
  // Create a copy of observers to iterate over in case observers modify the set
  NSSet* observersCopy = [observers_ copy];
  for (id<ReaderModeControllerObserver> observer in observersCopy) {
    [observer onReaderModeEnabledStateChanged:enabled];
  }
}

#pragma mark - Static methods used for update reader mode globally

void ReaderModeController::DisableReaderModeGlobally() {
  // Get all active controllers from the registry
  auto& registry = *GetControllerRegistry();

  // Iterate through all controllers and disable reader mode
  for (auto& pair : registry) {
    ReaderModeController* controller = pair.second;
    if (!controller) continue;

    // Disable reader mode if it's currently enabled
    if (controller->IsReaderModeEnabled()) {
      controller->DisableReaderMode();
    }

    // Update availability to false since the global setting is off
    controller->is_reader_mode_available_ = false;
    controller->NotifyAvailabilityObservers(false);
  }
}

void ReaderModeController::UpdateReaderModeAvailabilityGlobally() {
  // Get all active controllers from the registry
  auto& registry = *GetControllerRegistry();

  // Iterate through all controllers and re-check availability
  for (auto& pair : registry) {
    ReaderModeController* controller = pair.second;
    if (!controller) continue;

    // Re-check reader mode availability since the global setting is now on
    controller->CheckReaderModeAvailability();
  }
}
