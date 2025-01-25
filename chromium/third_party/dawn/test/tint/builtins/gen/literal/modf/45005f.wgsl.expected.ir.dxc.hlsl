struct modf_result_vec3_f16 {
  vector<float16_t, 3> fract;
  vector<float16_t, 3> whole;
};

struct VertexOutput {
  float4 pos;
};

struct vertex_main_outputs {
  float4 VertexOutput_pos : SV_Position;
};


void modf_45005f() {
  modf_result_vec3_f16 res = {(float16_t(-0.5h)).xxx, (float16_t(-1.0h)).xxx};
}

void fragment_main() {
  modf_45005f();
}

[numthreads(1, 1, 1)]
void compute_main() {
  modf_45005f();
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  modf_45005f();
  VertexOutput v = tint_symbol;
  return v;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_1 = vertex_main_inner();
  vertex_main_outputs v_2 = {v_1.pos};
  return v_2;
}

