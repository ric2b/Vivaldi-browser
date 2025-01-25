struct VertexOutput {
  float4 pos;
  float16_t prevent_dce;
};

struct vertex_main_outputs {
  nointerpolation float16_t VertexOutput_prevent_dce : TEXCOORD0;
  float4 VertexOutput_pos : SV_Position;
};


RWByteAddressBuffer prevent_dce : register(u0);
float16_t degrees_5e9805() {
  float16_t arg_0 = float16_t(1.0h);
  float16_t res = (arg_0 * float16_t(57.28125h));
  return res;
}

void fragment_main() {
  prevent_dce.Store<float16_t>(0u, degrees_5e9805());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store<float16_t>(0u, degrees_5e9805());
}

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = degrees_5e9805();
  VertexOutput v = tint_symbol;
  return v;
}

vertex_main_outputs vertex_main() {
  VertexOutput v_1 = vertex_main_inner();
  VertexOutput v_2 = v_1;
  VertexOutput v_3 = v_1;
  vertex_main_outputs v_4 = {v_3.prevent_dce, v_2.pos};
  return v_4;
}

