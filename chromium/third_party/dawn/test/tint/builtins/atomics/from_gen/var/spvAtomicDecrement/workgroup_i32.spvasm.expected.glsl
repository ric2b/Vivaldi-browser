#version 310 es

uint local_invocation_index_1 = 0u;
shared int arg_0;
void atomicAdd_794055() {
  int arg_1 = 0;
  int res = 0;
  arg_1 = 1;
  int x_15 = atomicAdd(arg_0, -(1));
  res = x_15;
}
void compute_main_inner(uint local_invocation_index_2) {
  atomicExchange(arg_0, 0);
  barrier();
  atomicAdd_794055();
}
void compute_main_1() {
  uint x_33 = local_invocation_index_1;
  compute_main_inner(x_33);
}
void compute_main_inner_1(uint local_invocation_index_1_param) {
  if ((local_invocation_index_1_param == 0u)) {
    atomicExchange(arg_0, 0);
  }
  barrier();
  local_invocation_index_1 = local_invocation_index_1_param;
  compute_main_1();
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main_inner_1(gl_LocalInvocationIndex);
}
