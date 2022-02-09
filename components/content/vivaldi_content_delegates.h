// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CONTENT_VIVALDI_CONTENT_DELEGATES_H_
#define COMPONENTS_CONTENT_VIVALDI_CONTENT_DELEGATES_H_

#include "content/common/content_export.h"

// This file contains delegates necessary to communicate from content code back
// to Vivaldi code. This MUST NOT include non-content classes or references.

namespace content {
class WebContents;
}

namespace vivaldi_content {

class CONTENT_EXPORT TabActivationDelegate {
 public:
  virtual ~TabActivationDelegate() = default;
  TabActivationDelegate() = default;

  // Activate the given tab
  virtual void ActivateTab(content::WebContents* contents) = 0;
};

}  // namespace vivaldi_content

#endif  // COMPONENTS_CONTENT_VIVALDI_CONTENT_DELEGATES_H_
