// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_MESSAGE_H_
#define XFA_FWL_CFWL_MESSAGE_H_

#include "core/fxcrt/mask.h"
#include "core/fxcrt/unowned_ptr.h"
#include "v8/include/cppgc/macros.h"

namespace pdfium {

class CFWL_Widget;

class CFWL_Message {
  CPPGC_STACK_ALLOCATED();  // Allow Raw/Unowned pointers.

 public:
  enum class Type { kKey, kKillFocus, kMouse, kMouseWheel, kSetFocus };

  virtual ~CFWL_Message();

  Type GetType() const { return m_type; }
  CFWL_Widget* GetDstTarget() const { return m_pDstTarget; }
  void SetDstTarget(CFWL_Widget* pWidget) { m_pDstTarget = pWidget; }

 protected:
  CFWL_Message(Type type, CFWL_Widget* pDstTarget);
  CFWL_Message(const CFWL_Message& that) = delete;
  CFWL_Message& operator=(const CFWL_Message& that) = delete;

 private:
  const Type m_type;
  UnownedPtr<CFWL_Widget> m_pDstTarget;
};

}  // namespace pdfium

// TODO(crbug.com/42271761): Remove.
using pdfium::CFWL_Message;

#endif  // XFA_FWL_CFWL_MESSAGE_H_
