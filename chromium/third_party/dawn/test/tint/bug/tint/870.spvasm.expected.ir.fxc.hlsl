
ByteAddressBuffer sspp962805860buildInformation : register(t2);
typedef int ary_ret[6];
ary_ret v(uint offset) {
  int a[6] = (int[6])0;
  {
    uint v_1 = 0u;
    v_1 = 0u;
    while(true) {
      uint v_2 = v_1;
      if ((v_2 >= 6u)) {
        break;
      }
      a[v_2] = asint(sspp962805860buildInformation.Load((offset + (v_2 * 4u))));
      {
        v_1 = (v_2 + 1u);
      }
      continue;
    }
  }
  int v_3[6] = a;
  return v_3;
}

void main_1() {
  int orientation[6] = (int[6])0;
  int x_23[6] = v(36u);
  orientation[int(0)] = x_23[0u];
  orientation[int(1)] = x_23[1u];
  orientation[int(2)] = x_23[2u];
  orientation[int(3)] = x_23[3u];
  orientation[int(4)] = x_23[4u];
  orientation[int(5)] = x_23[5u];
}

void main() {
  main_1();
}

