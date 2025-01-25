struct VertexOutput {
  float4 pos;
  uint prevent_dce;
};

struct vertex_main_outputs {
  nointerpolation uint VertexOutput_prevent_dce : TEXCOORD0;
  float4 VertexOutput_pos : SV_Position;
};


RWByteAddressBuffer prevent_dce : register(u0);
uint pack4xI8_bfce01() {
  int4 arg_0 = (1).xxxx;
  int4 v = arg_0;
  uint4 v_1 = uint4(0u, 8u, 16u, 24u);
  uint4 v_2 = asuint(v);
  uint4 v_3 = ((v_2 & uint4((255u).xxxx)) << v_1);
  uint res = dot(v_3, uint4((1u).xxxx));
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, pack4xI8_bfce01());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, pack4xI8_bfce01());
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = pack4xI8_bfce01();
  VertexOutput v_4 = tint_symbol;
  return v_4;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_5 = vertex_main_inner();
  VertexOutput v_6 = v_5;
  VertexOutput v_7 = v_5;
  vertex_main_outputs v_8 = {v_7.prevent_dce, v_6.pos};
  return v_8;
}

