struct VertexOutput {
  float4 pos;
  uint4 prevent_dce;
};

struct vertex_main_outputs {
  nointerpolation uint4 VertexOutput_prevent_dce : TEXCOORD0;
  float4 VertexOutput_pos : SV_Position;
};


RWByteAddressBuffer prevent_dce : register(u0);
Texture2DArray<uint4> arg_1 : register(t1, space1);
SamplerState arg_2 : register(s2, space1);
uint4 textureGather_1bf0ab() {
  float2 arg_3 = (1.0f).xx;
  uint arg_4 = 1u;
  float2 v = arg_3;
  uint4 res = arg_1.GatherGreen(arg_2, float3(v, float(arg_4)));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, textureGather_1bf0ab());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, textureGather_1bf0ab());
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = textureGather_1bf0ab();
  VertexOutput v_1 = tint_symbol;
  return v_1;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_2 = vertex_main_inner();
  vertex_main_outputs v_3 = {v_2.prevent_dce, v_2.pos};
  return v_3;
}

