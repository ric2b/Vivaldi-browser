RWTexture1D<int4> arg_0 : register(u0, space1);

void textureStore_958353() {
  arg_0[1u] = (1).xxxx;
}

void fragment_main() {
  textureStore_958353();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_958353();
  return;
}
