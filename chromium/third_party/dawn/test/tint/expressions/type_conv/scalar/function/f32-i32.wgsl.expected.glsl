#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
int tint_ftoi(float v) {
  return ((v <= 2147483520.0f) ? ((v < -2147483648.0f) ? (-2147483647 - 1) : int(v)) : 2147483647);
}

float t = 0.0f;
float m() {
  t = 1.0f;
  return float(t);
}

void f() {
  float tint_symbol = m();
  int v = tint_ftoi(tint_symbol);
}

