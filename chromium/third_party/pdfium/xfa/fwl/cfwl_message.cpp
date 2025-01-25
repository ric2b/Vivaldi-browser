// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/cfwl_message.h"

namespace pdfium {

CFWL_Message::CFWL_Message(Type type, CFWL_Widget* pDstTarget)
    : m_type(type), m_pDstTarget(pDstTarget) {}

CFWL_Message::~CFWL_Message() = default;

}  // namespace pdfium
