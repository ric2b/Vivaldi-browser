RWTexture2DArray<uint4> arg_0 : register(u0, space1);

void textureStore_bc1423() {
  arg_0[uint3((1u).xx, uint(1))] = (1u).xxxx;
}

void fragment_main() {
  textureStore_bc1423();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_bc1423();
  return;
}
