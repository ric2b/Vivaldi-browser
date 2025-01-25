// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_TEST_AX_MEDIA_APP_UNTRUSTED_HANDLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_TEST_AX_MEDIA_APP_UNTRUSTED_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/accessibility/ax_tree_manager.h"

namespace content {

class BrowserContext;

}  // namespace content

namespace ui {

class AXTreeID;
class AXNode;

}  // namespace ui

namespace ash::test {

class TestAXMediaAppUntrustedHandler : public AXMediaAppUntrustedHandler {
 public:
  TestAXMediaAppUntrustedHandler(
      content::BrowserContext& context,
      gfx::NativeWindow native_window,
      mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page);
  TestAXMediaAppUntrustedHandler(const TestAXMediaAppUntrustedHandler&) =
      delete;
  TestAXMediaAppUntrustedHandler& operator=(
      const TestAXMediaAppUntrustedHandler&) = delete;
  ~TestAXMediaAppUntrustedHandler() override;

  void SetMediaAppForTesting(AXMediaApp* media_app) { media_app_ = media_app; }
  std::string GetDocumentTreeToStringForTesting() const;
  void EnablePendingSerializedUpdatesForTesting();

  const ui::AXNode* GetDocumentRootNodeForTesting() const {
    return document_.GetRoot();
  }

  const ui::AXTreeID& GetDocumentTreeIDForTesting() const {
    return document_.GetTreeID();
  }

  std::map<const std::string, AXMediaAppPageMetadata>&
  GetPageMetadataForTesting() {
    return page_metadata_;
  }

  const std::map<const std::string, std::unique_ptr<ui::AXTreeManager>>&
  GetPagesForTesting() {
    return pages_;
  }

  const std::vector<ui::AXTreeUpdate>& GetPendingSerializedUpdatesForTesting()
      const {
    return *pending_serialized_updates_for_testing_;
  }

  void SetIsOcrServiceEnabledForTesting() {
    is_ocr_service_enabled_for_testing_ = true;
  }

  // Whether to allow tests to manually allow the OcrNextDirtyPageIfAny() method
  // to be called to better control the order of execution.
  void SetDelayCallingOcrNextDirtyPage(bool enabled) {
    delay_calling_ocr_next_dirty_page_ = enabled;
  }

  void SetMinPagesPerBatchForTesting(size_t min_pages) {
    min_pages_per_batch_ = min_pages;
  }

  void DisableStatusNodesForTesting() { has_landmark_node_ = false; }

  void DisablePostamblePageForTesting() { has_postamble_page_ = false; }

  void CreateFakeOpticalCharacterRecognizerForTesting(bool return_empty);
  void FlushForTesting();

  void PushDirtyPageForTesting(const std::string& dirty_page_id);
  std::string PopDirtyPageForTesting();

  // AXMediaAppUntrustedHandler:
  bool IsOcrServiceEnabled() const override;
  void OcrNextDirtyPageIfAny() override;

 private:
  bool is_ocr_service_enabled_for_testing_ = false;
  bool delay_calling_ocr_next_dirty_page_ = false;
};

}  // namespace ash::test

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_TEST_AX_MEDIA_APP_UNTRUSTED_HANDLER_H_
