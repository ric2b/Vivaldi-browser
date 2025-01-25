
RWByteAddressBuffer tint_symbol : register(u0);
void v(uint offset, matrix<float16_t, 4, 3> obj) {
  tint_symbol.Store<vector<float16_t, 3> >((offset + 0u), obj[0u]);
  tint_symbol.Store<vector<float16_t, 3> >((offset + 8u), obj[1u]);
  tint_symbol.Store<vector<float16_t, 3> >((offset + 16u), obj[2u]);
  tint_symbol.Store<vector<float16_t, 3> >((offset + 24u), obj[3u]);
}

[numthreads(1, 1, 1)]
void f() {
  matrix<float16_t, 4, 3> m = matrix<float16_t, 4, 3>((float16_t(0.0h)).xxx, (float16_t(0.0h)).xxx, (float16_t(0.0h)).xxx, (float16_t(0.0h)).xxx);
  v(0u, matrix<float16_t, 4, 3>(m));
}

