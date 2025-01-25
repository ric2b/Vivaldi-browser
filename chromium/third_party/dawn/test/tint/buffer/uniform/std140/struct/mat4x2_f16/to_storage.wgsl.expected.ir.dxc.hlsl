struct S {
  int before;
  matrix<float16_t, 4, 2> m;
  int after;
};


cbuffer cbuffer_u : register(b0) {
  uint4 u[32];
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

void v_11(uint offset, S obj) {
  s.Store((offset + 0u), asuint(obj.before));
  v_2((offset + 4u), obj.m);
  s.Store((offset + 64u), asuint(obj.after));
}

S v_12(uint start_byte_offset) {
  int v_13 = asint(u[(start_byte_offset / 16u)][((start_byte_offset % 16u) / 4u)]);
  matrix<float16_t, 4, 2> v_14 = v_3((4u + start_byte_offset));
  S v_15 = {v_13, v_14, asint(u[((64u + start_byte_offset) / 16u)][(((64u + start_byte_offset) % 16u) / 4u)])};
  return v_15;
}

void v_16(uint offset, S obj[4]) {
  {
    uint v_17 = 0u;
    v_17 = 0u;
    while(true) {
      uint v_18 = v_17;
      if ((v_18 >= 4u)) {
        break;
      }
      S v_19 = obj[v_18];
      v_11((offset + (v_18 * 128u)), v_19);
      {
        v_17 = (v_18 + 1u);
      }
      continue;
    }
  }
}

typedef S ary_ret[4];
ary_ret v_20(uint start_byte_offset) {
  S a[4] = (S[4])0;
  {
    uint v_21 = 0u;
    v_21 = 0u;
    while(true) {
      uint v_22 = v_21;
      if ((v_22 >= 4u)) {
        break;
      }
      S v_23 = v_12((start_byte_offset + (v_22 * 128u)));
      a[v_22] = v_23;
      {
        v_21 = (v_22 + 1u);
      }
      continue;
    }
  }
  S v_24[4] = a;
  return v_24;
}

[numthreads(1, 1, 1)]
void f() {
  S v_25[4] = v_20(0u);
  v_16(0u, v_25);
  S v_26 = v_12(256u);
  v_11(128u, v_26);
  v_2(388u, v_3(260u));
  s.Store<vector<float16_t, 2> >(132u, tint_bitcast_to_f16(u[0u].z).yx);
}

