RWByteAddressBuffer prevent_dce : register(u0);

uint4 select_087ea4() {
  uint4 arg_0 = (1u).xxxx;
  uint4 arg_1 = (1u).xxxx;
  bool arg_2 = true;
  uint4 res = (arg_2 ? arg_1 : arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(select_087ea4()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(select_087ea4()));
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
  tint_symbol.prevent_dce = select_087ea4();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  wrapper_result.prevent_dce = inner_result.prevent_dce;
  return wrapper_result;
}
