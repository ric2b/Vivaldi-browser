
[numthreads(1, 1, 1)]
void main() {
  {
    int i = int(0);
    while(true) {
      if ((i < int(2))) {
      } else {
        break;
      }
      {
        int j = int(0);
        while(true) {
          if ((j < int(2))) {
          } else {
            break;
          }
          bool tint_continue = false;
          switch(i) {
            case int(0):
            {
              tint_continue = true;
              break;
            }
            default:
            {
              break;
            }
          }
          if (tint_continue) {
            {
              j = (j + int(2));
            }
            continue;
          }
          {
            j = (j + int(2));
          }
          continue;
        }
      }
      {
        i = (i + int(2));
      }
      continue;
    }
  }
}

