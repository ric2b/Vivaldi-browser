
RWTexture3D<int4> arg_0 : register(u0, space1);
void textureStore_a5b88e() {
  arg_0[(1u).xxx] = (1).xxxx;
}

void fragment_main() {
  textureStore_a5b88e();
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_a5b88e();
}

