#version 310 es

void deref_const() {
  int a[10] = int[10](0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  int b = a[0];
  a[0] = 42;
}
void no_deref_const() {
  int a[10] = int[10](0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  int b = a[0];
  a[0] = 42;
}
void deref_let() {
  int a[10] = int[10](0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  int i = 0;
  int b = a[i];
  a[0] = 42;
}
void no_deref_let() {
  int a[10] = int[10](0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  int i = 0;
  int b = a[i];
  a[0] = 42;
}
void deref_var() {
  int a[10] = int[10](0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  int i = 0;
  int v = i;
  int b = a[v];
  a[0] = 42;
}
void no_deref_var() {
  int a[10] = int[10](0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  int i = 0;
  int v_1 = i;
  int b = a[v_1];
  a[0] = 42;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  deref_const();
  no_deref_const();
  deref_let();
  no_deref_let();
  deref_var();
  no_deref_var();
}
