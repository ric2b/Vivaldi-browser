RWTexture2DArray<int4> arg_0 : register(u0, space1);

void textureStore_1f1ef8() {
  arg_0[int3((1).xx, int(1u))] = (1).xxxx;
}

void fragment_main() {
  textureStore_1f1ef8();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_1f1ef8();
  return;
}
