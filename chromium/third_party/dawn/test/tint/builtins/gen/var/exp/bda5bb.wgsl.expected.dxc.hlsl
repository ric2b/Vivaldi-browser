void exp_bda5bb() {
  float3 res = (2.71828174591064453125f).xxx;
}

void fragment_main() {
  exp_bda5bb();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  exp_bda5bb();
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
  exp_bda5bb();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  return wrapper_result;
}
