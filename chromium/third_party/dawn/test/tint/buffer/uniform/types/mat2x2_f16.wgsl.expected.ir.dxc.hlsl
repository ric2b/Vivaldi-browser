
cbuffer cbuffer_u : register(b0) {
  uint4 u[1];
};
RWByteAddressBuffer s : register(u1);
void v_1(uint offset, matrix<float16_t, 2, 2> obj) {
  s.Store<vector<float16_t, 2> >((offset + 0u), obj[0u]);
  s.Store<vector<float16_t, 2> >((offset + 4u), obj[1u]);
}

vector<float16_t, 2> tint_bitcast_to_f16(uint src) {
  uint v = src;
  float t_low = f16tof32((v & 65535u));
  float t_high = f16tof32(((v >> 16u) & 65535u));
  float16_t v_2 = float16_t(t_low);
  return vector<float16_t, 2>(v_2, float16_t(t_high));
}

matrix<float16_t, 2, 2> v_3(uint start_byte_offset) {
  uint4 v_4 = u[(start_byte_offset / 16u)];
  vector<float16_t, 2> v_5 = tint_bitcast_to_f16((((((start_byte_offset % 16u) / 4u) == 2u)) ? (v_4.z) : (v_4.x)));
  uint4 v_6 = u[((4u + start_byte_offset) / 16u)];
  return matrix<float16_t, 2, 2>(v_5, tint_bitcast_to_f16(((((((4u + start_byte_offset) % 16u) / 4u) == 2u)) ? (v_6.z) : (v_6.x))));
}

[numthreads(1, 1, 1)]
void main() {
  matrix<float16_t, 2, 2> x = v_3(0u);
  v_1(0u, x);
}

