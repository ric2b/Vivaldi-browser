
cbuffer cbuffer_x : register(b0) {
  uint4 x[1];
};
[numthreads(1, 1, 1)]
void main() {
  switch(asint(x[0u].x)) {
    case int(0):
    {
      {
        while(true) {
          return;
        }
      }
      break;
    }
    default:
    {
      break;
    }
  }
}

