
cbuffer cbuffer_m : register(b0) {
  uint4 m[1];
};
static int counter = 0;
int i() {
  counter = (counter + 1);
  return counter;
}

vector<float16_t, 4> tint_bitcast_to_f16(uint4 src) {
  uint4 v = src;
  uint4 mask = (65535u).xxxx;
  uint4 shift = (16u).xxxx;
  float4 t_low = f16tof32((v & mask));
  float4 t_high = f16tof32(((v >> shift) & mask));
  float16_t v_1 = float16_t(t_low.x);
  float16_t v_2 = float16_t(t_high.x);
  float16_t v_3 = float16_t(t_low.y);
  return vector<float16_t, 4>(v_1, v_2, v_3, float16_t(t_high.y));
}

matrix<float16_t, 2, 4> v_4(uint start_byte_offset) {
  vector<float16_t, 4> v_5 = tint_bitcast_to_f16(m[(start_byte_offset / 16u)]);
  return matrix<float16_t, 2, 4>(v_5, tint_bitcast_to_f16(m[((8u + start_byte_offset) / 16u)]));
}

[numthreads(1, 1, 1)]
void f() {
  matrix<float16_t, 2, 4> l_m = v_4(0u);
  vector<float16_t, 4> l_m_1 = tint_bitcast_to_f16(m[0u]);
}

