RWTexture2DArray<int4> arg_0 : register(u0, space1);

void textureStore_a0022f() {
  int2 arg_1 = (1).xx;
  int arg_2 = 1;
  int4 arg_3 = (1).xxxx;
  arg_0[int3(arg_1, arg_2)] = arg_3;
}

void fragment_main() {
  textureStore_a0022f();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_a0022f();
  return;
}
