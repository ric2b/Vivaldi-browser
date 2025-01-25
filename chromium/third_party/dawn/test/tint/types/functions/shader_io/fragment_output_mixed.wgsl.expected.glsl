#version 310 es
#extension GL_OES_sample_variables: require
precision highp float;
precision highp int;


struct tint_push_constant_struct {
  float tint_frag_depth_min;
  float tint_frag_depth_max;
};

struct FragmentOutputs {
  int loc0;
  float frag_depth;
  uint loc1;
  float loc2;
  uint sample_mask;
  vec4 loc3;
};

layout(location = 0) uniform tint_push_constant_struct tint_push_constants;
layout(location = 0) out int tint_symbol_loc0_Output;
layout(location = 1) out uint tint_symbol_loc1_Output;
layout(location = 2) out float tint_symbol_loc2_Output;
layout(location = 3) out vec4 tint_symbol_loc3_Output;
FragmentOutputs tint_symbol_inner() {
  return FragmentOutputs(1, 2.0f, 1u, 1.0f, 2u, vec4(1.0f, 2.0f, 3.0f, 4.0f));
}
void main() {
  FragmentOutputs v = tint_symbol_inner();
  tint_symbol_loc0_Output = v.loc0;
  gl_FragDepth = clamp(v.frag_depth, tint_push_constants.tint_frag_depth_min, tint_push_constants.tint_frag_depth_max);
  tint_symbol_loc1_Output = v.loc1;
  tint_symbol_loc2_Output = v.loc2;
  gl_SampleMask[0u] = int(v.sample_mask);
  tint_symbol_loc3_Output = v.loc3;
}
