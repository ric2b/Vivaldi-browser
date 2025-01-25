uint tint_dot4_u8_packed(uint a, uint b) {
  uint4 a_u8 = ((uint4((a).xxxx) >> uint4(24u, 16u, 8u, 0u)) & (255u).xxxx);
  uint4 b_u8 = ((uint4((b).xxxx) >> uint4(24u, 16u, 8u, 0u)) & (255u).xxxx);
  return dot(a_u8, b_u8);
}

RWByteAddressBuffer prevent_dce : register(u0);

uint dot4U8Packed_fbed7b() {
  uint arg_0 = 1u;
  uint arg_1 = 1u;
  uint res = tint_dot4_u8_packed(arg_0, arg_1);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(dot4U8Packed_fbed7b()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(dot4U8Packed_fbed7b()));
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
  tint_symbol.prevent_dce = dot4U8Packed_fbed7b();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  wrapper_result.prevent_dce = inner_result.prevent_dce;
  return wrapper_result;
}
