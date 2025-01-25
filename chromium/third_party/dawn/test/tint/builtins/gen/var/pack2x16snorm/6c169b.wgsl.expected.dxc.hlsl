uint tint_pack2x16snorm(float2 param_0) {
  int2 i = int2(round(clamp(param_0, -1.0, 1.0) * 32767.0)) & 0xffff;
  return asuint(i.x | i.y << 16);
}

RWByteAddressBuffer prevent_dce : register(u0);

uint pack2x16snorm_6c169b() {
  float2 arg_0 = (1.0f).xx;
  uint res = tint_pack2x16snorm(arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(pack2x16snorm_6c169b()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(pack2x16snorm_6c169b()));
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
  tint_symbol.prevent_dce = pack2x16snorm_6c169b();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  wrapper_result.prevent_dce = inner_result.prevent_dce;
  return wrapper_result;
}
