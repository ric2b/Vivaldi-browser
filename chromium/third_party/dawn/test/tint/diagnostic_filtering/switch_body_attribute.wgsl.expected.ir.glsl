SKIP: FAILED

<dawn>/test/tint/diagnostic_filtering/switch_body_attribute.wgsl:5:11 warning: 'dpdx' must only be called from uniform control flow
      _ = dpdx(1.0);
          ^^^^^^^^^

<dawn>/test/tint/diagnostic_filtering/switch_body_attribute.wgsl:3:3 note: control flow depends on possibly non-uniform value
  switch (i32(x)) @diagnostic(warning, derivative_uniformity) {
  ^^^^^^

<dawn>/test/tint/diagnostic_filtering/switch_body_attribute.wgsl:3:15 note: user-defined input 'x' of 'main' may be non-uniform
  switch (i32(x)) @diagnostic(warning, derivative_uniformity) {
              ^

<dawn>/src/tint/lang/glsl/writer/printer/printer.cc:1116 internal compiler error: TINT_UNREACHABLE unhandled core builtin: select
********************************************************************
*  The tint shader compiler has encountered an unexpected error.   *
*                                                                  *
*  Please help us fix this issue by submitting a bug report at     *
*  crbug.com/tint with the source program that triggered the bug.  *
********************************************************************

tint executable returned error: signal: trace/BPT trap
