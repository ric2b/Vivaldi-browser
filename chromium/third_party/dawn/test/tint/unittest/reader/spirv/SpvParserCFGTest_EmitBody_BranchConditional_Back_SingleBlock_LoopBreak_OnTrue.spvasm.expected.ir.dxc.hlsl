SKIP: FAILED


static uint var_1 = 0u;
void main_1() {
  var_1 = 0u;
  {
    while(true) {
      var_1 = 1u;
      if (false) {
        break;
      }
      {
      }
      continue;
    }
  }
  var_1 = 5u;
}

void main() {
  main_1();
}

DXC validation failure:
error: validation errors
hlsl.hlsl:19: error: Loop must have break.
Validation failed.



tint executable returned error: exit status 1
