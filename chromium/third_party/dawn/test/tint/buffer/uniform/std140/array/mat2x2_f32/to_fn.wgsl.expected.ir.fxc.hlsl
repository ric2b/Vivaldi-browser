
cbuffer cbuffer_u : register(b0) {
  uint4 u[4];
};
RWByteAddressBuffer s : register(u1);
float a(float2x2 a_1[4]) {
  return a_1[0][0][0u];
}

float b(float2x2 m) {
  return m[0][0u];
}

float c(float2 v) {
  return v[0u];
}

float d(float f) {
  return f;
}

float2x2 v_1(uint start_byte_offset) {
  uint4 v_2 = u[(start_byte_offset / 16u)];
  float2 v_3 = asfloat((((((start_byte_offset % 16u) / 4u) == 2u)) ? (v_2.zw) : (v_2.xy)));
  uint4 v_4 = u[((8u + start_byte_offset) / 16u)];
  return float2x2(v_3, asfloat(((((((8u + start_byte_offset) % 16u) / 4u) == 2u)) ? (v_4.zw) : (v_4.xy))));
}

typedef float2x2 ary_ret[4];
ary_ret v_5(uint start_byte_offset) {
  float2x2 a[4] = (float2x2[4])0;
  {
    uint v_6 = 0u;
    v_6 = 0u;
    while(true) {
      uint v_7 = v_6;
      if ((v_7 >= 4u)) {
        break;
      }
      a[v_7] = v_1((start_byte_offset + (v_7 * 16u)));
      {
        v_6 = (v_7 + 1u);
      }
      continue;
    }
  }
  float2x2 v_8[4] = a;
  return v_8;
}

[numthreads(1, 1, 1)]
void f() {
  float2x2 v_9[4] = v_5(0u);
  float v_10 = a(v_9);
  float v_11 = (v_10 + b(v_1(16u)));
  float v_12 = (v_11 + c(asfloat(u[1u].xy).yx));
  s.Store(0u, asuint((v_12 + d(asfloat(u[1u].xy).yx[0u]))));
}

