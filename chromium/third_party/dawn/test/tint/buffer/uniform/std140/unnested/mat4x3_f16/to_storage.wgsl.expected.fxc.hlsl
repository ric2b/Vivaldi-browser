SKIP: INVALID

cbuffer cbuffer_u : register(b0) {
  uint4 u[2];
};
RWByteAddressBuffer s : register(u1);

void s_store(uint offset, matrix<float16_t, 4, 3> value) {
  s.Store<vector<float16_t, 3> >((offset + 0u), value[0u]);
  s.Store<vector<float16_t, 3> >((offset + 8u), value[1u]);
  s.Store<vector<float16_t, 3> >((offset + 16u), value[2u]);
  s.Store<vector<float16_t, 3> >((offset + 24u), value[3u]);
}

matrix<float16_t, 4, 3> u_load(uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  uint4 ubo_load_1 = u[scalar_offset / 4];
  uint2 ubo_load = ((scalar_offset & 2) ? ubo_load_1.zw : ubo_load_1.xy);
  vector<float16_t, 2> ubo_load_xz = vector<float16_t, 2>(f16tof32(ubo_load & 0xFFFF));
  float16_t ubo_load_y = f16tof32(ubo_load[0] >> 16);
  const uint scalar_offset_1 = ((offset + 8u)) / 4;
  uint4 ubo_load_3 = u[scalar_offset_1 / 4];
  uint2 ubo_load_2 = ((scalar_offset_1 & 2) ? ubo_load_3.zw : ubo_load_3.xy);
  vector<float16_t, 2> ubo_load_2_xz = vector<float16_t, 2>(f16tof32(ubo_load_2 & 0xFFFF));
  float16_t ubo_load_2_y = f16tof32(ubo_load_2[0] >> 16);
  const uint scalar_offset_2 = ((offset + 16u)) / 4;
  uint4 ubo_load_5 = u[scalar_offset_2 / 4];
  uint2 ubo_load_4 = ((scalar_offset_2 & 2) ? ubo_load_5.zw : ubo_load_5.xy);
  vector<float16_t, 2> ubo_load_4_xz = vector<float16_t, 2>(f16tof32(ubo_load_4 & 0xFFFF));
  float16_t ubo_load_4_y = f16tof32(ubo_load_4[0] >> 16);
  const uint scalar_offset_3 = ((offset + 24u)) / 4;
  uint4 ubo_load_7 = u[scalar_offset_3 / 4];
  uint2 ubo_load_6 = ((scalar_offset_3 & 2) ? ubo_load_7.zw : ubo_load_7.xy);
  vector<float16_t, 2> ubo_load_6_xz = vector<float16_t, 2>(f16tof32(ubo_load_6 & 0xFFFF));
  float16_t ubo_load_6_y = f16tof32(ubo_load_6[0] >> 16);
  return matrix<float16_t, 4, 3>(vector<float16_t, 3>(ubo_load_xz[0], ubo_load_y, ubo_load_xz[1]), vector<float16_t, 3>(ubo_load_2_xz[0], ubo_load_2_y, ubo_load_2_xz[1]), vector<float16_t, 3>(ubo_load_4_xz[0], ubo_load_4_y, ubo_load_4_xz[1]), vector<float16_t, 3>(ubo_load_6_xz[0], ubo_load_6_y, ubo_load_6_xz[1]));
}

[numthreads(1, 1, 1)]
void f() {
  s_store(0u, u_load(0u));
  uint2 ubo_load_8 = u[0].xy;
  vector<float16_t, 2> ubo_load_8_xz = vector<float16_t, 2>(f16tof32(ubo_load_8 & 0xFFFF));
  float16_t ubo_load_8_y = f16tof32(ubo_load_8[0] >> 16);
  s.Store<vector<float16_t, 3> >(8u, vector<float16_t, 3>(ubo_load_8_xz[0], ubo_load_8_y, ubo_load_8_xz[1]));
  uint2 ubo_load_9 = u[0].xy;
  vector<float16_t, 2> ubo_load_9_xz = vector<float16_t, 2>(f16tof32(ubo_load_9 & 0xFFFF));
  float16_t ubo_load_9_y = f16tof32(ubo_load_9[0] >> 16);
  s.Store<vector<float16_t, 3> >(8u, vector<float16_t, 3>(ubo_load_9_xz[0], ubo_load_9_y, ubo_load_9_xz[1]).zxy);
  s.Store<float16_t>(2u, float16_t(f16tof32(((u[0].z) & 0xFFFF))));
  return;
}
FXC validation failure:
<scrubbed_path>(6,34-42): error X3000: syntax error: unexpected token 'float16_t'
<scrubbed_path>(7,3-9): error X3018: invalid subscript 'Store'


tint executable returned error: exit status 1
