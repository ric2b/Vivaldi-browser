RWTexture2DArray<int4> arg_0 : register(u0, space1);

void textureStore_1c02e7() {
  arg_0[int3((1).xx, 1)] = (1).xxxx;
}

void fragment_main() {
  textureStore_1c02e7();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_1c02e7();
  return;
}
