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
uint4 textureLoad_8527b1() {
  uint2 arg_1 = (1u).xx;
  uint arg_2 = 1u;
  uint arg_3 = 1u;
  Texture2DArray<uint4> v = arg_0;
  uint v_1 = arg_2;
  uint v_2 = arg_3;
  int2 v_3 = int2(arg_1);
  int v_4 = int(v_1);
  uint4 res = uint4(v.Load(int4(v_3, v_4, int(v_2))));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, textureLoad_8527b1());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, textureLoad_8527b1());
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = textureLoad_8527b1();
  VertexOutput v_5 = tint_symbol;
  return v_5;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_6 = vertex_main_inner();
  VertexOutput v_7 = v_6;
  VertexOutput v_8 = v_6;
  vertex_main_outputs v_9 = {v_8.prevent_dce, v_7.pos};
  return v_9;
}

