SKIP: FAILED


[numthreads(1, 1, 1)]
void main() {
  {
    int i = 0;
    while(true) {
      if ((i < 2)) {
      } else {
        break;
      }
      {
        int j = 0;
        while(true) {
          if ((j < 2)) {
          } else {
            break;
          }
          switch(i) {
            case 0:
            {
              {
                j = (j + 2);
              }
              continue;
            }
            default:
            {
              break;
            }
          }
          {
            j = (j + 2);
          }
          continue;
        }
      }
      {
        i = (i + 2);
      }
      continue;
    }
  }
}

FXC validation failure:
<scrubbed_path>(24,15-23): error X3708: continue cannot be used in a switch


tint executable returned error: exit status 1
