RWByteAddressBuffer prevent_dce : register(u0);

float4 max_a93419() {
  float4 res = (1.0f).xxxx;
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(max_a93419()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(max_a93419()));
  return;
}

struct VertexOutput {
  float4 pos;
  float4 prevent_dce;
};
struct tint_symbol_1 {
  nointerpolation float4 prevent_dce : TEXCOORD0;
  float4 pos : SV_Position;
};

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = max_a93419();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  wrapper_result.prevent_dce = inner_result.prevent_dce;
  return wrapper_result;
}
