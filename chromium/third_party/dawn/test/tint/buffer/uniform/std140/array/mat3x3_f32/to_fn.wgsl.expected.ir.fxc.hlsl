
cbuffer cbuffer_u : register(b0) {
  uint4 u[12];
};
RWByteAddressBuffer s : register(u1);
float a(float3x3 a_1[4]) {
  return a_1[0][0][0u];
}

float b(float3x3 m) {
  return m[0][0u];
}

float c(float3 v) {
  return v[0u];
}

float d(float f) {
  return f;
}

float3x3 v_1(uint start_byte_offset) {
  float3 v_2 = asfloat(u[(start_byte_offset / 16u)].xyz);
  float3 v_3 = asfloat(u[((16u + start_byte_offset) / 16u)].xyz);
  return float3x3(v_2, v_3, asfloat(u[((32u + start_byte_offset) / 16u)].xyz));
}

typedef float3x3 ary_ret[4];
ary_ret v_4(uint start_byte_offset) {
  float3x3 a[4] = (float3x3[4])0;
  {
    uint v_5 = 0u;
    v_5 = 0u;
    while(true) {
      uint v_6 = v_5;
      if ((v_6 >= 4u)) {
        break;
      }
      a[v_6] = v_1((start_byte_offset + (v_6 * 48u)));
      {
        v_5 = (v_6 + 1u);
      }
      continue;
    }
  }
  float3x3 v_7[4] = a;
  return v_7;
}

[numthreads(1, 1, 1)]
void f() {
  float3x3 v_8[4] = v_4(0u);
  float v_9 = a(v_8);
  float v_10 = (v_9 + b(v_1(48u)));
  float v_11 = (v_10 + c(asfloat(u[3u].xyz).zxy));
  s.Store(0u, asuint((v_11 + d(asfloat(u[3u].xyz).zxy[0u]))));
}

