RWTexture2DArray<int4> arg_0 : register(u0, space1);

void textureStore_7bb211() {
  arg_0[int3((1).xx, int(1u))] = (1).xxxx;
}

void fragment_main() {
  textureStore_7bb211();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_7bb211();
  return;
}
