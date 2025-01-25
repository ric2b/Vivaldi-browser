struct VertexOutput {
  float4 pos;
  uint4 prevent_dce;
};

struct vertex_main_outputs {
  nointerpolation uint4 VertexOutput_prevent_dce : TEXCOORD0;
  float4 VertexOutput_pos : SV_Position;
};


RWByteAddressBuffer prevent_dce : register(u0);
Texture3D<uint4> arg_0 : register(t0, space1);
uint4 textureLoad_f56e6f() {
  int3 arg_1 = (1).xxx;
  Texture3D<uint4> v = arg_0;
  uint4 res = uint4(v.Load(int4(int3(arg_1), 0)));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, textureLoad_f56e6f());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, textureLoad_f56e6f());
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = textureLoad_f56e6f();
  VertexOutput v_1 = tint_symbol;
  return v_1;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_2 = vertex_main_inner();
  VertexOutput v_3 = v_2;
  VertexOutput v_4 = v_2;
  vertex_main_outputs v_5 = {v_4.prevent_dce, v_3.pos};
  return v_5;
}

