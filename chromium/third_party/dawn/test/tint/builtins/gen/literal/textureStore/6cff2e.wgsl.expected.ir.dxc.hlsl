
RWTexture2D<uint4> arg_0 : register(u0, space1);
void textureStore_6cff2e() {
  arg_0[(1).xx] = (1u).xxxx;
}

void fragment_main() {
  textureStore_6cff2e();
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_6cff2e();
}

