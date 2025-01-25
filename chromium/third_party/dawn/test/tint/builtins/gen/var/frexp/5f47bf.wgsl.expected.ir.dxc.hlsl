struct frexp_result_vec2_f16 {
  vector<float16_t, 2> fract;
  int2 exp;
};

struct VertexOutput {
  float4 pos;
};

struct vertex_main_outputs {
  float4 VertexOutput_pos : SV_Position;
};


void frexp_5f47bf() {
  vector<float16_t, 2> arg_0 = (float16_t(1.0h)).xx;
  vector<float16_t, 2> v = arg_0;
  vector<float16_t, 2> v_1 = (float16_t(0.0h)).xx;
  vector<float16_t, 2> v_2 = frexp(v, v_1);
  vector<float16_t, 2> v_3 = (vector<float16_t, 2>(sign(v)) * v_2);
  frexp_result_vec2_f16 res = {v_3, int2(v_1)};
}

void fragment_main() {
  frexp_5f47bf();
}

[numthreads(1, 1, 1)]
void compute_main() {
  frexp_5f47bf();
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  frexp_5f47bf();
  VertexOutput v_4 = tint_symbol;
  return v_4;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_5 = vertex_main_inner();
  vertex_main_outputs v_6 = {v_5.pos};
  return v_6;
}

