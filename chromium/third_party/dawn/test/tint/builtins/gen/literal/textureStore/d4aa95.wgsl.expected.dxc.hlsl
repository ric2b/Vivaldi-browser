RWTexture3D<uint4> arg_0 : register(u0, space1);

void textureStore_d4aa95() {
  arg_0[(1u).xxx] = (1u).xxxx;
}

void fragment_main() {
  textureStore_d4aa95();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_d4aa95();
  return;
}
