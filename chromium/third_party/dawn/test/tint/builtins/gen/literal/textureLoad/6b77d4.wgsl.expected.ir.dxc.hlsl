struct VertexOutput {
  float4 pos;
  uint4 prevent_dce;
};

struct vertex_main_outputs {
  nointerpolation uint4 VertexOutput_prevent_dce : TEXCOORD0;
  float4 VertexOutput_pos : SV_Position;
};


RWByteAddressBuffer prevent_dce : register(u0);
Texture1D<uint4> arg_0 : register(t0, space1);
uint4 textureLoad_6b77d4() {
  Texture1D<uint4> v = arg_0;
  int v_1 = int(1);
  uint4 res = uint4(v.Load(int2(v_1, int(1u))));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, textureLoad_6b77d4());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, textureLoad_6b77d4());
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = textureLoad_6b77d4();
  VertexOutput v_2 = tint_symbol;
  return v_2;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_3 = vertex_main_inner();
  VertexOutput v_4 = v_3;
  VertexOutput v_5 = v_3;
  vertex_main_outputs v_6 = {v_5.prevent_dce, v_4.pos};
  return v_6;
}

