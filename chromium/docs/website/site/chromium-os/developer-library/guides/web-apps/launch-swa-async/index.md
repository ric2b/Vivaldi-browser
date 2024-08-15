---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: launch-swa-async
title: How to finish a task before launching a System web app
---

**Prerequisites**

System Web App, WebUI

**Problem**

I wanted to defer the launch of my SWA until a task is completed. For example,
for the feedback tool SWA, I need to take a screenshot for all screens first
before showing the UI to users.

**Solution**

-   **Step 1**: Implement a method that will complete a task async and once done
    will invoke the callback passed in.

> ```
> // Take a screenshot and invoke the callback when completed or failed.
> void OsFeedbackScreenshotManager::TakeScreenshot(ScreenshotCallback callback) {
>   // SKip taking if one has been taken already. Although the feedback tool only
>   // allows one instance, the user could request it multiple times while one
>   // instance is still running.
>   if (screenshot_png_data_ == nullptr) {
>     // In unit tests, shell is not created.
>     aura::Window* primary_window = ash::Shell::HasInstance()
>                                        ? ash::Shell::GetPrimaryRootWindow()
>                                        : nullptr;
>     if (primary_window) {
>       gfx::Rect rect = primary_window->bounds();
>       ui::GrabWindowSnapshotAsPNG(
>           primary_window, rect,
>           base::BindOnce(&OsFeedbackScreenshotManager::OnScreenshotTaken,
>                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
>     } else {
>       std::move(callback).Run(false);
>     }
>   } else {
>     std::move(callback).Run(false);
>   }
> }
> ```

-   **Step 2**: Override the `LaunchAndNavigateSystemWebApp` of the class
    `ash::SystemWebAppDelegate`
    *   If not there yet, add a class 'OSFeedbackAppDelegate' header file in
        `chrome/browser/ash/web_applications/os_feedback_system_web_app_info.h`
        that extends `ash::SystemWebAppDelegate`.
    *   Override the `LaunchAndNavigateSystemWebApp`

> ```
> #include "chrome/browser/profiles/profile.h"
> #include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
> #include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
> #include "ui/gfx/geometry/rect.h"
>
> class Browser;
> struct WebAppInstallInfo;
>
> namespace web_app {
> class WebAppProvider;
> }  // namespace web_app
>
> class OSFeedbackAppDelegate : public web_app::SystemWebAppDelegate {
>  public:
>   explicit OSFeedbackAppDelegate(Profile* profile);
>
>   // web_app::SystemWebAppDelegate overrides:
>   std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
>   bool IsAppEnabled() const override;
>   bool ShouldAllowScriptsToCloseWindows() const override;
>   bool ShouldCaptureNavigations() const override;
>   bool ShouldAllowMaximize() const override;
>   bool ShouldAllowResize() const override;
>   gfx::Rect GetDefaultBounds(Browser*) const override;
>   Browser* LaunchAndNavigateSystemWebApp(
>       Profile* profile,
>       web_app::WebAppProvider* provider,
>       const GURL& url,
>       const apps::AppLaunchParams& params) const override;
> };
>
> // Returns a WebAppInstallInfo used to install the app.
> std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForOSFeedbackSystemWebApp();
>
> // Returns the default bounds.
> gfx::Rect GetDefaultBoundsForOSFeedbackApp(Browser*);
> ```

-   **Step 3**: Implement the `LaunchAndNavigateSystemWebApp` in
    `chrome/browser/ash/web_applications/os_feedback_system_web_app_info.cc`

  1. Launch and hide the SWA.
  2. Start the task with a callback to show the SWA.

> ```
> Browser* OSFeedbackAppDelegate::LaunchAndNavigateSystemWebApp(
>     Profile* profile,
>     web_app::WebAppProvider* provider,
>     const GURL& url,
>     const apps::AppLaunchParams& params) const {
>   // This check is needed to enforce the policy no matter how and from where the
>   // feedback tool is to be launched.
>   if (IsUserFeedbackAllowed(profile)) {
>     Browser* browser = SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(
>         profile, provider, url, params);
>     if (browser && browser->window()) {
>       // Hide the app, take a screenshot async, then show it afterward.
>       browser->window()->Hide();
>       ash::OsFeedbackScreenshotManager::GetInstance()->TakeScreenshot(
>           base::BindOnce(&OSFeedbackAppDelegate::OnScreenshotTaken,
>                          weak_ptr_factory_.GetWeakPtr(), browser));
>     }
>     return browser;
>   }
>   return nullptr;
> }
>
> void OSFeedbackAppDelegate::OnScreenshotTaken(Browser* browser,
>                                               bool status) const {
>   if (browser && browser->window()) {
>     browser->window()->Show();
>   }
> }
> ```

**Example CL**

-   https://crrev.com/c/3645699
    *   Parent change: https://crrev.com/c/3642641

**Comment/Discussion**

> Note: Simply fires an async operation when starting to launch the SWA may not
> work because we have no control over the timing. If the UI is being drawn
> while the screenshot taking is in progress, the scrennshot will include some
> of the UI elements of the feedback tool.
