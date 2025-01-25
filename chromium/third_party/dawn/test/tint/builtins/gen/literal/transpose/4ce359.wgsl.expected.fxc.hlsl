RWByteAddressBuffer prevent_dce : register(u0);

int transpose_4ce359() {
  float4x2 res = float4x2((1.0f).xx, (1.0f).xx, (1.0f).xx, (1.0f).xx);
  return ((res[0][0] == 0.0f) ? 1 : 0);
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(transpose_4ce359()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(transpose_4ce359()));
  return;
}

struct VertexOutput {
  float4 pos;
  int prevent_dce;
};
struct tint_symbol_1 {
  nointerpolation int prevent_dce : TEXCOORD0;
  float4 pos : SV_Position;
};

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = transpose_4ce359();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  wrapper_result.prevent_dce = inner_result.prevent_dce;
  return wrapper_result;
}
