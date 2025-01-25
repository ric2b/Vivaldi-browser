struct VertexOutput {
  float4 pos;
  uint4 prevent_dce;
};

struct vertex_main_outputs {
  nointerpolation uint4 VertexOutput_prevent_dce : TEXCOORD0;
  float4 VertexOutput_pos : SV_Position;
};


RWByteAddressBuffer prevent_dce : register(u0);
Texture2DArray<uint4> arg_0 : register(t0, space1);
uint4 textureLoad_7ab4df() {
  Texture2DArray<uint4> v = arg_0;
  int2 v_1 = int2((1).xx);
  int v_2 = int(1u);
  uint4 res = uint4(v.Load(int4(v_1, v_2, int(1))));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, textureLoad_7ab4df());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, textureLoad_7ab4df());
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = textureLoad_7ab4df();
  VertexOutput v_3 = tint_symbol;
  return v_3;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_4 = vertex_main_inner();
  VertexOutput v_5 = v_4;
  VertexOutput v_6 = v_4;
  vertex_main_outputs v_7 = {v_6.prevent_dce, v_5.pos};
  return v_7;
}

