// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FDE_CFDE_WORDBREAK_DATA_H_
#define XFA_FDE_CFDE_WORDBREAK_DATA_H_

#include <stdint.h>

namespace pdfium {

enum class WordBreakProperty : uint8_t {
  // Internal tables depend on constants computed from these values, so do
  // not re-order.
  kNone = 0,
  kCR,
  kLF,
  kNewLine,
  kExtend,
  kFormat,
  kKataKana,
  kALetter,
  kMidLetter,
  kMidNum,
  kMidNumLet,
  kNumeric,
  kExtendNumLet,
};

bool FX_CheckStateChangeForWordBreak(WordBreakProperty from,
                                     WordBreakProperty to);
WordBreakProperty FX_GetWordBreakProperty(wchar_t wcCodePoint);

}  // namespace pdfium

#endif  // XFA_FDE_CFDE_WORDBREAK_DATA_H_
