struct S {
  int arr[4];
};


RWByteAddressBuffer s : register(u0);
int foo() {
  int v[4] = (int[4])0;
  int tint_symbol[4] = (int[4])0;
  S dst_struct = (S)0;
  int dst_array[2][4] = (int[2][4])0;
  int dst_ptr[4] = tint_symbol;
  S dst_struct_ptr = dst_struct;
  int dst_array_ptr[2][4] = dst_array;
  int src[4] = v;
  dst_struct.arr = src;
  int v_1[4] = v;
  dst_array[1] = v_1;
  int v_2[4] = v;
  dst_ptr = v_2;
  int v_3[4] = v;
  dst_struct_ptr.arr = v_3;
  int v_4[4] = v;
  dst_array_ptr[0] = v_4;
  return ((dst_ptr[0] + dst_struct_ptr.arr[0]) + dst_array_ptr[0][0]);
}

[numthreads(1, 1, 1)]
void main() {
  s.Store(0u, asuint(foo()));
}

