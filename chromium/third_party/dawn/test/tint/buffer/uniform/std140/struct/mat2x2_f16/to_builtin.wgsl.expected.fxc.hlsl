SKIP: INVALID

cbuffer cbuffer_u : register(b0) {
  uint4 u[32];
};

matrix<float16_t, 2, 2> u_load(uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  uint ubo_load = u[scalar_offset / 4][scalar_offset % 4];
  const uint scalar_offset_1 = ((offset + 4u)) / 4;
  uint ubo_load_1 = u[scalar_offset_1 / 4][scalar_offset_1 % 4];
  return matrix<float16_t, 2, 2>(vector<float16_t, 2>(float16_t(f16tof32(ubo_load & 0xFFFF)), float16_t(f16tof32(ubo_load >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_1 & 0xFFFF)), float16_t(f16tof32(ubo_load_1 >> 16))));
}

[numthreads(1, 1, 1)]
void f() {
  matrix<float16_t, 2, 2> t = transpose(u_load(260u));
  uint ubo_load_2 = u[0].z;
  float16_t l = length(vector<float16_t, 2>(float16_t(f16tof32(ubo_load_2 & 0xFFFF)), float16_t(f16tof32(ubo_load_2 >> 16))).yx);
  uint ubo_load_3 = u[0].z;
  float16_t a = abs(vector<float16_t, 2>(float16_t(f16tof32(ubo_load_3 & 0xFFFF)), float16_t(f16tof32(ubo_load_3 >> 16))).yx.x);
  return;
}
FXC validation failure:
<scrubbed_path>(5,8-16): error X3000: syntax error: unexpected token 'float16_t'


tint executable returned error: exit status 1
