
cbuffer cbuffer_a : register(b0) {
  uint4 a[8];
};
RWByteAddressBuffer s : register(u1);
static int counter = 0;
int i() {
  counter = (counter + 1);
  return counter;
}

float2x3 v(uint start_byte_offset) {
  float3 v_1 = asfloat(a[(start_byte_offset / 16u)].xyz);
  return float2x3(v_1, asfloat(a[((16u + start_byte_offset) / 16u)].xyz));
}

typedef float2x3 ary_ret[4];
ary_ret v_2(uint start_byte_offset) {
  float2x3 a[4] = (float2x3[4])0;
  {
    uint v_3 = 0u;
    v_3 = 0u;
    while(true) {
      uint v_4 = v_3;
      if ((v_4 >= 4u)) {
        break;
      }
      a[v_4] = v((start_byte_offset + (v_4 * 32u)));
      {
        v_3 = (v_4 + 1u);
      }
      continue;
    }
  }
  float2x3 v_5[4] = a;
  return v_5;
}

[numthreads(1, 1, 1)]
void f() {
  uint v_6 = (32u * uint(i()));
  uint v_7 = (16u * uint(i()));
  float2x3 v_8[4] = v_2(0u);
  float2x3 l_a_i = v(v_6);
  float3 l_a_i_i = asfloat(a[((v_6 + v_7) / 16u)].xyz);
  float2x3 l_a[4] = v_8;
  s.Store(0u, asuint((((asfloat(a[((v_6 + v_7) / 16u)][(((v_6 + v_7) % 16u) / 4u)]) + l_a[0][0][0u]) + l_a_i[0][0u]) + l_a_i_i[0u])));
}

