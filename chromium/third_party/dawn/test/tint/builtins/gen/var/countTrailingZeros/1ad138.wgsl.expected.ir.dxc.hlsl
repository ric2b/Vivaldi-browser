struct VertexOutput {
  float4 pos;
  uint2 prevent_dce;
};

struct vertex_main_outputs {
  nointerpolation uint2 VertexOutput_prevent_dce : TEXCOORD0;
  float4 VertexOutput_pos : SV_Position;
};


RWByteAddressBuffer prevent_dce : register(u0);
uint2 countTrailingZeros_1ad138() {
  uint2 arg_0 = (1u).xx;
  uint2 v = arg_0;
  uint2 v_1 = ((((v & (65535u).xx) == (0u).xx)) ? ((16u).xx) : ((0u).xx));
  uint2 v_2 = (((((v >> v_1) & (255u).xx) == (0u).xx)) ? ((8u).xx) : ((0u).xx));
  uint2 v_3 = ((((((v >> v_1) >> v_2) & (15u).xx) == (0u).xx)) ? ((4u).xx) : ((0u).xx));
  uint2 v_4 = (((((((v >> v_1) >> v_2) >> v_3) & (3u).xx) == (0u).xx)) ? ((2u).xx) : ((0u).xx));
  uint2 v_5 = ((((((((v >> v_1) >> v_2) >> v_3) >> v_4) & (1u).xx) == (0u).xx)) ? ((1u).xx) : ((0u).xx));
  uint2 res = ((v_1 | (v_2 | (v_3 | (v_4 | v_5)))) + (((((((v >> v_1) >> v_2) >> v_3) >> v_4) == (0u).xx)) ? ((1u).xx) : ((0u).xx)));
  return res;
}

void fragment_main() {
  prevent_dce.Store2(0u, countTrailingZeros_1ad138());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store2(0u, countTrailingZeros_1ad138());
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = countTrailingZeros_1ad138();
  VertexOutput v_6 = tint_symbol;
  return v_6;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_7 = vertex_main_inner();
  VertexOutput v_8 = v_7;
  VertexOutput v_9 = v_7;
  vertex_main_outputs v_10 = {v_9.prevent_dce, v_8.pos};
  return v_10;
}

