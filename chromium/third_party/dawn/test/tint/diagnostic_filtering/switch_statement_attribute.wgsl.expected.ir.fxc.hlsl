SKIP: FAILED

<dawn>/test/tint/diagnostic_filtering/switch_statement_attribute.wgsl:7:27 warning: 'dpdx' must only be called from uniform control flow
  switch (i32(x == 0.0 && dpdx(1.0) == 0.0)) {
                          ^^^^^^^^^

<dawn>/test/tint/diagnostic_filtering/switch_statement_attribute.wgsl:7:15 note: control flow depends on possibly non-uniform value
  switch (i32(x == 0.0 && dpdx(1.0) == 0.0)) {
              ^^^^^^^^^^^^^^^^^^^^^^^^^^^^

<dawn>/test/tint/diagnostic_filtering/switch_statement_attribute.wgsl:7:15 note: user-defined input 'x' of 'main' may be non-uniform
  switch (i32(x == 0.0 && dpdx(1.0) == 0.0)) {
              ^

<dawn>/src/tint/lang/hlsl/writer/printer/printer.cc:198 internal compiler error: Switch() matched no cases. Type: tint::core::ir::If
********************************************************************
*  The tint shader compiler has encountered an unexpected error.   *
*                                                                  *
*  Please help us fix this issue by submitting a bug report at     *
*  crbug.com/tint with the source program that triggered the bug.  *
********************************************************************
