
cbuffer cbuffer_a : register(b0) {
  uint4 a[8];
};
RWByteAddressBuffer s : register(u1);
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

matrix<float16_t, 4, 4> v_4(uint start_byte_offset) {
  vector<float16_t, 4> v_5 = tint_bitcast_to_f16(a[(start_byte_offset / 16u)]);
  vector<float16_t, 4> v_6 = tint_bitcast_to_f16(a[((8u + start_byte_offset) / 16u)]);
  vector<float16_t, 4> v_7 = tint_bitcast_to_f16(a[((16u + start_byte_offset) / 16u)]);
  return matrix<float16_t, 4, 4>(v_5, v_6, v_7, tint_bitcast_to_f16(a[((24u + start_byte_offset) / 16u)]));
}

typedef matrix<float16_t, 4, 4> ary_ret[4];
ary_ret v_8(uint start_byte_offset) {
  matrix<float16_t, 4, 4> a[4] = (matrix<float16_t, 4, 4>[4])0;
  {
    uint v_9 = 0u;
    v_9 = 0u;
    while(true) {
      uint v_10 = v_9;
      if ((v_10 >= 4u)) {
        break;
      }
      a[v_10] = v_4((start_byte_offset + (v_10 * 32u)));
      {
        v_9 = (v_10 + 1u);
      }
      continue;
    }
  }
  matrix<float16_t, 4, 4> v_11[4] = a;
  return v_11;
}

[numthreads(1, 1, 1)]
void f() {
  uint v_12 = (32u * uint(i()));
  uint v_13 = (8u * uint(i()));
  matrix<float16_t, 4, 4> v_14[4] = v_8(0u);
  matrix<float16_t, 4, 4> l_a_i = v_4(v_12);
  vector<float16_t, 4> l_a_i_i = tint_bitcast_to_f16(a[((v_12 + v_13) / 16u)]);
  uint v_15 = a[((v_12 + v_13) / 16u)][(((v_12 + v_13) % 16u) / 4u)];
  matrix<float16_t, 4, 4> l_a[4] = v_14;
  s.Store<float16_t>(0u, (((float16_t(f16tof32((v_15 >> (((((v_12 + v_13) % 4u) == 0u)) ? (0u) : (16u))))) + l_a[0][0][0u]) + l_a_i[0][0u]) + l_a_i_i[0u]));
}

