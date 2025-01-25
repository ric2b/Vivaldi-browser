vector<float16_t, 2> tint_bitcast_to_f16(int src) {
  uint v = asuint(src);
  float t_low = f16tof32(v & 0xffff);
  float t_high = f16tof32((v >> 16) & 0xffff);
  return vector<float16_t, 2>(t_low.x, t_high.x);
}

RWByteAddressBuffer prevent_dce : register(u0);

vector<float16_t, 2> bitcast_674557() {
  int arg_0 = 1;
  vector<float16_t, 2> res = tint_bitcast_to_f16(arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store<vector<float16_t, 2> >(0u, bitcast_674557());
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store<vector<float16_t, 2> >(0u, bitcast_674557());
  return;
}

struct VertexOutput {
  float4 pos;
  vector<float16_t, 2> prevent_dce;
};
struct tint_symbol_1 {
  nointerpolation vector<float16_t, 2> prevent_dce : TEXCOORD0;
  float4 pos : SV_Position;
};

VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = (VertexOutput)0;
  tint_symbol.pos = (0.0f).xxxx;
  tint_symbol.prevent_dce = bitcast_674557();
  return tint_symbol;
}

tint_symbol_1 vertex_main() {
  VertexOutput inner_result = vertex_main_inner();
  tint_symbol_1 wrapper_result = (tint_symbol_1)0;
  wrapper_result.pos = inner_result.pos;
  wrapper_result.prevent_dce = inner_result.prevent_dce;
  return wrapper_result;
}
