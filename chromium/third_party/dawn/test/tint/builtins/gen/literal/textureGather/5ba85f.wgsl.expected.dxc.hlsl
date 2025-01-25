RWByteAddressBuffer prevent_dce : register(u0);
TextureCube<int4> arg_1 : register(t1, space1);
SamplerState arg_2 : register(s2, space1);

int4 textureGather_5ba85f() {
  int4 res = arg_1.GatherGreen(arg_2, (1.0f).xxx);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureGather_5ba85f()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(textureGather_5ba85f()));
  return;
}

struct VertexOutput {
  float4 pos;
  int4 prevent_dce;
};
struct tint_symbol_1 {
  nointerpolation int4 prevent_dce : TEXCOORD0;
  float4 pos : SV_Position;
};

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = textureGather_5ba85f();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  wrapper_result.prevent_dce = inner_result.prevent_dce;
  return wrapper_result;
}
