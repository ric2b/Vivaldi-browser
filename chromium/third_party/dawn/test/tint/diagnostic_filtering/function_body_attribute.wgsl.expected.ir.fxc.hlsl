SKIP: FAILED

<dawn>/test/tint/diagnostic_filtering/function_body_attribute.wgsl:7:9 warning: 'textureSample' must only be called from uniform control flow
    _ = textureSample(t, s, vec2(0, 0));
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

<dawn>/test/tint/diagnostic_filtering/function_body_attribute.wgsl:6:3 note: control flow depends on possibly non-uniform value
  if (x > 0) {
  ^^

<dawn>/test/tint/diagnostic_filtering/function_body_attribute.wgsl:6:7 note: user-defined input 'x' of 'main' may be non-uniform
  if (x > 0) {
      ^

<dawn>/src/tint/lang/hlsl/writer/printer/printer.cc:198 internal compiler error: Switch() matched no cases. Type: tint::core::ir::If
********************************************************************
*  The tint shader compiler has encountered an unexpected error.   *
*                                                                  *
*  Please help us fix this issue by submitting a bug report at     *
*  crbug.com/tint with the source program that triggered the bug.  *
********************************************************************
