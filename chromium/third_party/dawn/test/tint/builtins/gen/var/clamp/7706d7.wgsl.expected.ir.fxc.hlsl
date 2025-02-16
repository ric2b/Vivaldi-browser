struct VertexOutput {
  float4 pos;
  uint2 prevent_dce;
};

struct vertex_main_outputs {
  nointerpolation uint2 VertexOutput_prevent_dce : TEXCOORD0;
  float4 VertexOutput_pos : SV_Position;
};


RWByteAddressBuffer prevent_dce : register(u0);
uint2 clamp_7706d7() {
  uint2 arg_0 = (1u).xx;
  uint2 arg_1 = (1u).xx;
  uint2 arg_2 = (1u).xx;
  uint2 v = arg_2;
  uint2 res = min(max(arg_0, arg_1), v);
  return res;
}

void fragment_main() {
  prevent_dce.Store2(0u, clamp_7706d7());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store2(0u, clamp_7706d7());
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = clamp_7706d7();
  VertexOutput v_1 = tint_symbol;
  return v_1;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_2 = vertex_main_inner();
  vertex_main_outputs v_3 = {v_2.prevent_dce, v_2.pos};
  return v_3;
}

