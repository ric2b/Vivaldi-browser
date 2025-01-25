// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebUI;
}  // namespace content

namespace web_app {
class IwaSourceDevModeWithFileOp;
class ScopedTempWebBundleFile;
class WebAppInternalsIwaInstallationBrowserTest;
}  // namespace web_app

// Handles API requests from chrome://web-app-internals page by implementing
// mojom::WebAppInternalsHandler.
class WebAppInternalsHandler : public mojom::WebAppInternalsHandler {
 public:
  static void BuildDebugInfo(
      Profile* profile,
      base::OnceCallback<void(base::Value root)> callback);

  WebAppInternalsHandler(
      content::WebUI* web_ui,
      mojo::PendingReceiver<mojom::WebAppInternalsHandler> receiver);

  WebAppInternalsHandler(const WebAppInternalsHandler&) = delete;
  WebAppInternalsHandler& operator=(const WebAppInternalsHandler&) = delete;

  ~WebAppInternalsHandler() override;

  // mojom::WebAppInternalsHandler:
  void GetDebugInfoAsJsonString(
      GetDebugInfoAsJsonStringCallback callback) override;
  void InstallIsolatedWebAppFromDevProxy(
      const GURL& url,
      InstallIsolatedWebAppFromDevProxyCallback callback) override;
  void ParseUpdateManifestFromUrl(
      const GURL& update_manifest_url,
      ParseUpdateManifestFromUrlCallback callback) override;
  void InstallIsolatedWebAppFromBundleUrl(
      mojom::InstallFromBundleUrlParamsPtr params,
      InstallIsolatedWebAppFromBundleUrlCallback callback) override;
  void SelectFileAndInstallIsolatedWebAppFromDevBundle(
      SelectFileAndInstallIsolatedWebAppFromDevBundleCallback callback)
      override;
  void SelectFileAndUpdateIsolatedWebAppFromDevBundle(
      const webapps::AppId& app_id,
      SelectFileAndUpdateIsolatedWebAppFromDevBundleCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void ClearExperimentalWebAppIsolationData(
      ClearExperimentalWebAppIsolationDataCallback callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  void SearchForIsolatedWebAppUpdates(
      SearchForIsolatedWebAppUpdatesCallback callback) override;
  void GetIsolatedWebAppDevModeAppInfo(
      GetIsolatedWebAppDevModeAppInfoCallback callback) override;
  void UpdateDevProxyIsolatedWebApp(
      const webapps::AppId& app_id,
      UpdateDevProxyIsolatedWebAppCallback callback) override;
  void RotateKey(
      const std::string& web_bundle_id,
      const std::optional<std::vector<uint8_t>>& public_key) override;

 private:
  class IsolatedWebAppDevBundleSelectListener;
  friend class web_app::WebAppInternalsIwaInstallationBrowserTest;

  void DownloadWebBundleToFile(
      const GURL& web_bundle_url,
      InstallIsolatedWebAppFromBundleUrlCallback callback,
      web_app::ScopedTempWebBundleFile file);

  void OnWebBundleDownloaded(
      InstallIsolatedWebAppFromBundleUrlCallback callback,
      web_app::ScopedTempWebBundleFile bundle,
      int32_t result);

  void OnIsolatedWebAppDevModeBundleSelected(
      SelectFileAndInstallIsolatedWebAppFromDevBundleCallback callback,
      std::optional<base::FilePath> path);
  void OnIsolatedWebAppDevModeBundleSelectedForUpdate(
      const webapps::AppId& app_id,
      SelectFileAndUpdateIsolatedWebAppFromDevBundleCallback callback,
      std::optional<base::FilePath> path);

  void OnInstallIsolatedWebAppInDevMode(
      base::OnceCallback<void(mojom::InstallIsolatedWebAppResultPtr)> callback,
      web_app::IsolatedWebAppInstallationManager::
          MaybeInstallIsolatedWebAppCommandSuccess result);

  // Discovers and applies an update for a dev mode Isolated Web App identified
  // by its app id. If `location` is set, then the update will be read from the
  // provided location, otherwise the existing location will be used.
  void ApplyDevModeUpdate(
      const webapps::AppId& app_id,
      base::optional_ref<const web_app::IwaSourceDevModeWithFileOp> location,
      base::OnceCallback<void(const std::string&)> callback);

  const raw_ref<content::WebUI> web_ui_;
  const raw_ref<Profile> profile_;
  mojo::Receiver<mojom::WebAppInternalsHandler> receiver_;
  base::WeakPtrFactory<WebAppInternalsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_
