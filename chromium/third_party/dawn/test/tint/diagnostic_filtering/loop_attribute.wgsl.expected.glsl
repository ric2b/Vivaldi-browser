<dawn>/test/tint/diagnostic_filtering/loop_attribute.wgsl:5:9 warning: 'dpdx' must only be called from uniform control flow
    _ = dpdx(1.0);
        ^^^^^^^^^

<dawn>/test/tint/diagnostic_filtering/loop_attribute.wgsl:7:7 note: control flow depends on possibly non-uniform value
      break if x > 0.0;
      ^^^^^

<dawn>/test/tint/diagnostic_filtering/loop_attribute.wgsl:7:16 note: user-defined input 'x' of 'main' may be non-uniform
      break if x > 0.0;
               ^

#version 310 es
precision highp float;
precision highp int;

layout(location = 0) in float tint_symbol_loc0_Input;
void tint_symbol_inner(float x) {
  {
    while(true) {
      dFdx(1.0f);
      {
        if ((x > 0.0f)) { break; }
      }
      continue;
    }
  }
}
void main() {
  tint_symbol_inner(tint_symbol_loc0_Input);
}
