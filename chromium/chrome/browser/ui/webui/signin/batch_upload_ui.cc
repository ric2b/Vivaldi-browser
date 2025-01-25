// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/ABC): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/signin/batch_upload_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/batch_upload_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/batch_upload_resources.h"
#include "chrome/grit/batch_upload_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

BatchUploadUI::BatchUploadUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  // Set up the chrome://batch-upload source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIBatchUploadHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::make_span(kBatchUploadResources, kBatchUploadResourcesSize),
      IDR_BATCH_UPLOAD_BATCH_UPLOAD_HTML);

  // Temporary code.
  source->AddString("message", "Hello World!");
}

BatchUploadUI::~BatchUploadUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BatchUploadUI)

void BatchUploadUI::Initialize(
    const std::vector<raw_ptr<const BatchUploadDataProvider>>&
        data_providers_list,
    base::RepeatingCallback<void(int)> update_view_height_callback,
    SelectedDataTypeItemsCallback completion_callback) {
  initialize_handler_callback_ = base::BindOnce(
      &BatchUploadUI::OnMojoHandlersReady, base::Unretained(this),
      data_providers_list, update_view_height_callback,
      std::move(completion_callback));
}

void BatchUploadUI::Clear() {
  handler_.reset();
}

void BatchUploadUI::BindInterface(
    mojo::PendingReceiver<batch_upload::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void BatchUploadUI::CreateBatchUploadHandler(
    mojo::PendingRemote<batch_upload::mojom::Page> page,
    mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver) {
  CHECK(initialize_handler_callback_);
  std::move(initialize_handler_callback_)
      .Run(std::move(page), std::move(receiver));
}

void BatchUploadUI::OnMojoHandlersReady(
    std::vector<raw_ptr<const BatchUploadDataProvider>> data_providers_list,
    base::RepeatingCallback<void(int)> update_view_height_callback,
    SelectedDataTypeItemsCallback completion_callback,
    mojo::PendingRemote<batch_upload::mojom::Page> page,
    mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver) {
  CHECK(!handler_);
  handler_ = std::make_unique<BatchUploadHandler>(
      std::move(receiver), std::move(page), data_providers_list,
      update_view_height_callback, std::move(completion_callback));
}
