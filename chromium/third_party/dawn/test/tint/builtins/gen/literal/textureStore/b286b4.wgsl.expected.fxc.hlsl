RWTexture2DArray<uint4> arg_0 : register(u0, space1);

void textureStore_b286b4() {
  arg_0[int3((1).xx, 1)] = (1u).xxxx;
}

void fragment_main() {
  textureStore_b286b4();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_b286b4();
  return;
}
