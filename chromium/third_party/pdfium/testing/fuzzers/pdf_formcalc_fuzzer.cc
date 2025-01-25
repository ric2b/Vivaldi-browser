// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/compiler_specific.h"
#include "core/fxcrt/fx_string.h"
#include "testing/fuzzers/pdfium_fuzzer_util.h"
#include "testing/fuzzers/xfa_process_state.h"
#include "xfa/fxfa/formcalc/cxfa_fmparser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto* state = static_cast<XFAProcessState*>(FPDF_GetFuzzerPerProcessState());
  // SAFETY: required from fuzzer.
  WideString input =
      WideString::FromUTF8(UNSAFE_BUFFERS(ByteStringView(data, size)));
  CXFA_FMLexer lexer(input.AsStringView());
  CXFA_FMParser parser(state->GetHeap(), &lexer);
  parser.Parse();
  state->ForceGCAndPump();
  return 0;
}
