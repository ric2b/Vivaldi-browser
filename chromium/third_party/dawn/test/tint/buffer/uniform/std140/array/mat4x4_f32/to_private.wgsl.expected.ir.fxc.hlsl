
cbuffer cbuffer_u : register(b0) {
  uint4 u[16];
};
RWByteAddressBuffer s : register(u1);
static float4x4 p[4] = (float4x4[4])0;
float4x4 v(uint start_byte_offset) {
  float4 v_1 = asfloat(u[(start_byte_offset / 16u)]);
  float4 v_2 = asfloat(u[((16u + start_byte_offset) / 16u)]);
  float4 v_3 = asfloat(u[((32u + start_byte_offset) / 16u)]);
  return float4x4(v_1, v_2, v_3, asfloat(u[((48u + start_byte_offset) / 16u)]));
}

typedef float4x4 ary_ret[4];
ary_ret v_4(uint start_byte_offset) {
  float4x4 a[4] = (float4x4[4])0;
  {
    uint v_5 = 0u;
    v_5 = 0u;
    while(true) {
      uint v_6 = v_5;
      if ((v_6 >= 4u)) {
        break;
      }
      a[v_6] = v((start_byte_offset + (v_6 * 64u)));
      {
        v_5 = (v_6 + 1u);
      }
      continue;
    }
  }
  float4x4 v_7[4] = a;
  return v_7;
}

[numthreads(1, 1, 1)]
void f() {
  float4x4 v_8[4] = v_4(0u);
  p = v_8;
  p[1] = v(128u);
  p[1][0] = asfloat(u[1u]).ywxz;
  p[1][0][0u] = asfloat(u[1u].x);
  s.Store(0u, asuint(p[1][0].x));
}

