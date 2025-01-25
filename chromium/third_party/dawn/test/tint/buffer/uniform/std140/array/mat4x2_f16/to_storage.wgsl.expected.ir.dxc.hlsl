
cbuffer cbuffer_u : register(b0) {
  uint4 u[4];
};
RWByteAddressBuffer s : register(u1);
vector<float16_t, 2> tint_bitcast_to_f16(uint src) {
  uint v = src;
  float t_low = f16tof32((v & 65535u));
  float t_high = f16tof32(((v >> 16u) & 65535u));
  float16_t v_1 = float16_t(t_low);
  return vector<float16_t, 2>(v_1, float16_t(t_high));
}

void v_2(uint offset, matrix<float16_t, 4, 2> obj) {
  s.Store<vector<float16_t, 2> >((offset + 0u), obj[0u]);
  s.Store<vector<float16_t, 2> >((offset + 4u), obj[1u]);
  s.Store<vector<float16_t, 2> >((offset + 8u), obj[2u]);
  s.Store<vector<float16_t, 2> >((offset + 12u), obj[3u]);
}

matrix<float16_t, 4, 2> v_3(uint start_byte_offset) {
  uint4 v_4 = u[(start_byte_offset / 16u)];
  vector<float16_t, 2> v_5 = tint_bitcast_to_f16((((((start_byte_offset % 16u) / 4u) == 2u)) ? (v_4.z) : (v_4.x)));
  uint4 v_6 = u[((4u + start_byte_offset) / 16u)];
  vector<float16_t, 2> v_7 = tint_bitcast_to_f16(((((((4u + start_byte_offset) % 16u) / 4u) == 2u)) ? (v_6.z) : (v_6.x)));
  uint4 v_8 = u[((8u + start_byte_offset) / 16u)];
  vector<float16_t, 2> v_9 = tint_bitcast_to_f16(((((((8u + start_byte_offset) % 16u) / 4u) == 2u)) ? (v_8.z) : (v_8.x)));
  uint4 v_10 = u[((12u + start_byte_offset) / 16u)];
  return matrix<float16_t, 4, 2>(v_5, v_7, v_9, tint_bitcast_to_f16(((((((12u + start_byte_offset) % 16u) / 4u) == 2u)) ? (v_10.z) : (v_10.x))));
}

void v_11(uint offset, matrix<float16_t, 4, 2> obj[4]) {
  {
    uint v_12 = 0u;
    v_12 = 0u;
    while(true) {
      uint v_13 = v_12;
      if ((v_13 >= 4u)) {
        break;
      }
      v_2((offset + (v_13 * 16u)), obj[v_13]);
      {
        v_12 = (v_13 + 1u);
      }
      continue;
    }
  }
}

typedef matrix<float16_t, 4, 2> ary_ret[4];
ary_ret v_14(uint start_byte_offset) {
  matrix<float16_t, 4, 2> a[4] = (matrix<float16_t, 4, 2>[4])0;
  {
    uint v_15 = 0u;
    v_15 = 0u;
    while(true) {
      uint v_16 = v_15;
      if ((v_16 >= 4u)) {
        break;
      }
      a[v_16] = v_3((start_byte_offset + (v_16 * 16u)));
      {
        v_15 = (v_16 + 1u);
      }
      continue;
    }
  }
  matrix<float16_t, 4, 2> v_17[4] = a;
  return v_17;
}

[numthreads(1, 1, 1)]
void f() {
  matrix<float16_t, 4, 2> v_18[4] = v_14(0u);
  v_11(0u, v_18);
  v_2(16u, v_3(32u));
  s.Store<vector<float16_t, 2> >(16u, tint_bitcast_to_f16(u[0u].x).yx);
  s.Store<float16_t>(16u, float16_t(f16tof32(u[0u].y)));
}

