SKIP: FAILED

<dawn>/test/tint/diagnostic_filtering/while_loop_attribute.wgsl:5:21 warning: 'dpdx' must only be called from uniform control flow
  while (x > 0.0 && dpdx(1.0) > 0.0)  {
                    ^^^^^^^^^

<dawn>/test/tint/diagnostic_filtering/while_loop_attribute.wgsl:5:3 note: control flow depends on possibly non-uniform value
  while (x > 0.0 && dpdx(1.0) > 0.0)  {
  ^^^^^

<dawn>/test/tint/diagnostic_filtering/while_loop_attribute.wgsl:5:21 note: return value of 'dpdx' may be non-uniform
  while (x > 0.0 && dpdx(1.0) > 0.0)  {
                    ^^^^^^^^^

<dawn>/src/tint/lang/hlsl/writer/printer/printer.cc:198 internal compiler error: Switch() matched no cases. Type: tint::core::ir::Loop
********************************************************************
*  The tint shader compiler has encountered an unexpected error.   *
*                                                                  *
*  Please help us fix this issue by submitting a bug report at     *
*  crbug.com/tint with the source program that triggered the bug.  *
********************************************************************
