void radians_44a9f8() {
  float2 res = (0.01745329238474369049f).xx;
}

void fragment_main() {
  radians_44a9f8();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  radians_44a9f8();
  return;
}

struct VertexOutput {
  float4 pos;
};
struct tint_symbol_1 {
  float4 pos : SV_Position;
};

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  radians_44a9f8();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  return wrapper_result;
}
