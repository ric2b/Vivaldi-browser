uint4 tint_unpack_4xu8(uint a) {
  uint4 a_vec4u = (uint4((a).xxxx) >> uint4(0u, 8u, 16u, 24u));
  return (a_vec4u & (255u).xxxx);
}

RWByteAddressBuffer prevent_dce : register(u0);

uint4 unpack4xU8_a5ea55() {
  uint arg_0 = 1u;
  uint4 res = tint_unpack_4xu8(arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(unpack4xU8_a5ea55()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(unpack4xU8_a5ea55()));
  return;
}

struct VertexOutput {
  float4 pos;
  uint4 prevent_dce;
};
struct tint_symbol_1 {
  nointerpolation uint4 prevent_dce : TEXCOORD0;
  float4 pos : SV_Position;
};

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = unpack4xU8_a5ea55();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  wrapper_result.prevent_dce = inner_result.prevent_dce;
  return wrapper_result;
}
