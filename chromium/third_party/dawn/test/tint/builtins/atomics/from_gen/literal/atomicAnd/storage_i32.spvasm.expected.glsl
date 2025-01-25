#version 310 es
precision highp float;
precision highp int;


struct SB_RW_atomic {
  int arg_0;
};

layout(binding = 0, std430)
buffer sb_rw_block_1_ssbo {
  SB_RW_atomic inner;
} v;
void atomicAnd_152966() {
  int res = 0;
  int x_9 = atomicAnd(v.inner.arg_0, 1);
  res = x_9;
}
void fragment_main_1() {
  atomicAnd_152966();
}
void main() {
  fragment_main_1();
}
#version 310 es


struct SB_RW_atomic {
  int arg_0;
};

layout(binding = 0, std430)
buffer sb_rw_block_1_ssbo {
  SB_RW_atomic inner;
} v;
void atomicAnd_152966() {
  int res = 0;
  int x_9 = atomicAnd(v.inner.arg_0, 1);
  res = x_9;
}
void compute_main_1() {
  atomicAnd_152966();
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main_1();
}
