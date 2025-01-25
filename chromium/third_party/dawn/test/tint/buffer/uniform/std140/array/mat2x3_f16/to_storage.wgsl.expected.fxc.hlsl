SKIP: INVALID

cbuffer cbuffer_u : register(b0) {
  uint4 u[4];
};
RWByteAddressBuffer s : register(u1);

void s_store_1(uint offset, matrix<float16_t, 2, 3> value) {
  s.Store<vector<float16_t, 3> >((offset + 0u), value[0u]);
  s.Store<vector<float16_t, 3> >((offset + 8u), value[1u]);
}

void s_store(uint offset, matrix<float16_t, 2, 3> value[4]) {
  matrix<float16_t, 2, 3> array_1[4] = value;
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      s_store_1((offset + (i * 16u)), array_1[i]);
    }
  }
}

matrix<float16_t, 2, 3> u_load_1(uint offset) {
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
  return matrix<float16_t, 2, 3>(vector<float16_t, 3>(ubo_load_xz[0], ubo_load_y, ubo_load_xz[1]), vector<float16_t, 3>(ubo_load_2_xz[0], ubo_load_2_y, ubo_load_2_xz[1]));
}

typedef matrix<float16_t, 2, 3> u_load_ret[4];
u_load_ret u_load(uint offset) {
  matrix<float16_t, 2, 3> arr[4] = (matrix<float16_t, 2, 3>[4])0;
  {
    for(uint i_1 = 0u; (i_1 < 4u); i_1 = (i_1 + 1u)) {
      arr[i_1] = u_load_1((offset + (i_1 * 16u)));
    }
  }
  return arr;
}

[numthreads(1, 1, 1)]
void f() {
  s_store(0u, u_load(0u));
  s_store_1(16u, u_load_1(32u));
  uint2 ubo_load_4 = u[0].zw;
  vector<float16_t, 2> ubo_load_4_xz = vector<float16_t, 2>(f16tof32(ubo_load_4 & 0xFFFF));
  float16_t ubo_load_4_y = f16tof32(ubo_load_4[0] >> 16);
  s.Store<vector<float16_t, 3> >(16u, vector<float16_t, 3>(ubo_load_4_xz[0], ubo_load_4_y, ubo_load_4_xz[1]).zxy);
  s.Store<float16_t>(16u, float16_t(f16tof32(((u[0].z) & 0xFFFF))));
  return;
}
FXC validation failure:
<scrubbed_path>(6,36-44): error X3000: syntax error: unexpected token 'float16_t'
<scrubbed_path>(7,3-9): error X3018: invalid subscript 'Store'


tint executable returned error: exit status 1
