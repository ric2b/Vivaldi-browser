void smoothstep_0c4ffc() {
  float4 res = (0.5f).xxxx;
}

void fragment_main() {
  smoothstep_0c4ffc();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  smoothstep_0c4ffc();
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
  smoothstep_0c4ffc();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  return wrapper_result;
}
