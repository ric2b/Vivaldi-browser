// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BROWSER_LANGUAGE_VIVALDI_TRANSLATE_FRAME_BINDER_H_
#define BROWSER_LANGUAGE_VIVALDI_TRANSLATE_FRAME_BINDER_H_

#include "build/build_config.h"
#include "components/translate/content/common/translate.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace vivaldi {

void BindVivaldiContentTranslateDriver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver);
}

#endif  // BROWSER_LANGUAGE_VIVALDI_TRANSLATE_FRAME_BINDER_H_
