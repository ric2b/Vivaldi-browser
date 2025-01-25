struct frexp_result_vec3_f16 {
  vector<float16_t, 3> fract;
  int3 exp;
};

struct VertexOutput {
  float4 pos;
};

struct vertex_main_outputs {
  float4 VertexOutput_pos : SV_Position;
};


void frexp_ae4a66() {
  frexp_result_vec3_f16 res = {(float16_t(0.5h)).xxx, (1).xxx};
}

void fragment_main() {
  frexp_ae4a66();
}

[numthreads(1, 1, 1)]
void compute_main() {
  frexp_ae4a66();
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  frexp_ae4a66();
  VertexOutput v = tint_symbol;
  return v;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_1 = vertex_main_inner();
  vertex_main_outputs v_2 = {v_1.pos};
  return v_2;
}

