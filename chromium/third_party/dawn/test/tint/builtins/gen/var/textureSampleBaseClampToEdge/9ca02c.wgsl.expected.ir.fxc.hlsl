struct VertexOutput {
  float4 pos;
  float4 prevent_dce;
};

struct vertex_main_outputs {
  nointerpolation float4 VertexOutput_prevent_dce : TEXCOORD0;
  float4 VertexOutput_pos : SV_Position;
};


RWByteAddressBuffer prevent_dce : register(u0);
Texture2D<float4> arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);
float4 textureSampleBaseClampToEdge_9ca02c() {
  float2 arg_2 = (1.0f).xx;
  Texture2D<float4> v = arg_0;
  SamplerState v_1 = arg_1;
  float2 v_2 = arg_2;
  uint2 v_3 = (0u).xx;
  v.GetDimensions(v_3[0u], v_3[1u]);
  float2 v_4 = ((0.5f).xx / float2(v_3));
  float2 v_5 = clamp(v_2, v_4, ((1.0f).xx - v_4));
  float4 res = v.SampleLevel(v_1, v_5, float(0.0f));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureSampleBaseClampToEdge_9ca02c()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(textureSampleBaseClampToEdge_9ca02c()));
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = textureSampleBaseClampToEdge_9ca02c();
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

