// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/cfwl_messagemouse.h"

namespace pdfium {

CFWL_MessageMouse::CFWL_MessageMouse(CFWL_Widget* pDstTarget,
                                     MouseCommand cmd,
                                     Mask<XFA_FWL_KeyFlag> flags,
                                     CFX_PointF pos)
    : CFWL_Message(CFWL_Message::Type::kMouse, pDstTarget),
      m_dwCmd(cmd),
      m_dwFlags(flags),
      m_pos(pos) {}

CFWL_MessageMouse::~CFWL_MessageMouse() = default;

}  // namespace pdfium
