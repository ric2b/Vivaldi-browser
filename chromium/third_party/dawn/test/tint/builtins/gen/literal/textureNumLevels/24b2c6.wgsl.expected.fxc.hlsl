RWByteAddressBuffer prevent_dce : register(u0);
Texture2D<float4> arg_0 : register(t0, space1);

uint textureNumLevels_24b2c6() {
  uint3 tint_tmp;
  arg_0.GetDimensions(0, tint_tmp.x, tint_tmp.y, tint_tmp.z);
  uint res = tint_tmp.z;
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(textureNumLevels_24b2c6()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(textureNumLevels_24b2c6()));
  return;
}

struct VertexOutput {
  float4 pos;
  uint prevent_dce;
};
struct tint_symbol_1 {
  nointerpolation uint prevent_dce : TEXCOORD0;
  float4 pos : SV_Position;
};

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = textureNumLevels_24b2c6();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  wrapper_result.prevent_dce = inner_result.prevent_dce;
  return wrapper_result;
}
