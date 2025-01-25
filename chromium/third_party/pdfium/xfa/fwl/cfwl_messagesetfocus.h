// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_MESSAGESETFOCUS_H_
#define XFA_FWL_CFWL_MESSAGESETFOCUS_H_

#include "xfa/fwl/cfwl_message.h"

namespace pdfium {

class CFWL_MessageSetFocus final : public CFWL_Message {
 public:
  explicit CFWL_MessageSetFocus(CFWL_Widget* pDstTarget);
  ~CFWL_MessageSetFocus() override;
};

}  // namespace pdfium

// TODO(crbug.com/42271761): Remove.
using pdfium::CFWL_MessageSetFocus;

#endif  // XFA_FWL_CFWL_MESSAGESETFOCUS_H_
