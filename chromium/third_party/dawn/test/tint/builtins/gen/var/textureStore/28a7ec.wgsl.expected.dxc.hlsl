RWTexture2DArray<int4> arg_0 : register(u0, space1);

void textureStore_28a7ec() {
  uint2 arg_1 = (1u).xx;
  int arg_2 = 1;
  int4 arg_3 = (1).xxxx;
  arg_0[uint3(arg_1, uint(arg_2))] = arg_3;
}

void fragment_main() {
  textureStore_28a7ec();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_28a7ec();
  return;
}
