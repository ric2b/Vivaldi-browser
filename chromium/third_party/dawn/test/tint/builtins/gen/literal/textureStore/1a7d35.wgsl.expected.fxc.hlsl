RWTexture2DArray<int4> arg_0 : register(u0, space1);

void textureStore_1a7d35() {
  arg_0[uint3((1u).xx, 1u)] = (1).xxxx;
}

void fragment_main() {
  textureStore_1a7d35();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_1a7d35();
  return;
}
