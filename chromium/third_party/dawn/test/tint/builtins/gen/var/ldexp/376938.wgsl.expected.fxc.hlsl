void ldexp_376938() {
  int4 arg_1 = (1).xxxx;
  float4 res = ldexp((1.0f).xxxx, arg_1);
}

void fragment_main() {
  ldexp_376938();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  ldexp_376938();
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
  ldexp_376938();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  return wrapper_result;
}
