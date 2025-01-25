struct main_inputs {
  uint3 local_id : SV_GroupThreadID;
  uint tint_local_index : SV_GroupIndex;
  uint3 global_id : SV_DispatchThreadID;
};


ByteAddressBuffer firstMatrix : register(t0);
ByteAddressBuffer secondMatrix : register(t1);
RWByteAddressBuffer resultMatrix : register(u2);
cbuffer cbuffer_uniforms : register(b3) {
  uint4 uniforms[1];
};
groupshared float mm_Asub[64][64];
groupshared float mm_Bsub[64][64];
float mm_readA(uint row, uint col) {
  bool v = false;
  if ((row < uniforms[0u].x)) {
    v = (col < uniforms[0u].y);
  } else {
    v = false;
  }
  if (v) {
    float result = asfloat(firstMatrix.Load((0u + (uint(((row * uniforms[0u].y) + col)) * 4u))));
    return result;
  }
  return 0.0f;
}

float mm_readB(uint row, uint col) {
  bool v_1 = false;
  if ((row < uniforms[0u].y)) {
    v_1 = (col < uniforms[0u].z);
  } else {
    v_1 = false;
  }
  if (v_1) {
    float result = asfloat(secondMatrix.Load((0u + (uint(((row * uniforms[0u].z) + col)) * 4u))));
    return result;
  }
  return 0.0f;
}

void mm_write(uint row, uint col, float value) {
  bool v_2 = false;
  if ((row < uniforms[0u].x)) {
    v_2 = (col < uniforms[0u].z);
  } else {
    v_2 = false;
  }
  if (v_2) {
    uint index = (col + (row * uniforms[0u].z));
    uint v_3 = (0u + (uint(index) * 4u));
    resultMatrix.Store(v_3, asuint(value));
  }
}

uint tint_div_u32(uint lhs, uint rhs) {
  return (lhs / (((rhs == 0u)) ? (1u) : (rhs)));
}

void main_inner(uint3 local_id, uint3 global_id, uint tint_local_index) {
  {
    uint v_4 = 0u;
    v_4 = tint_local_index;
    while(true) {
      uint v_5 = v_4;
      if ((v_5 >= 4096u)) {
        break;
      }
      mm_Asub[(v_5 / 64u)][(v_5 % 64u)] = 0.0f;
      mm_Bsub[(v_5 / 64u)][(v_5 % 64u)] = 0.0f;
      {
        v_4 = (v_5 + 256u);
      }
      continue;
    }
  }
  GroupMemoryBarrierWithGroupSync();
  uint tileRow = (local_id[1u] * 4u);
  uint tileCol = (local_id[0u] * 4u);
  uint globalRow = (global_id[1u] * 4u);
  uint globalCol = (global_id[0u] * 4u);
  uint numTiles = (tint_div_u32((uniforms[0u].y - 1u), 64u) + 1u);
  float acc[16] = (float[16])0;
  float ACached = 0.0f;
  float BCached[4] = (float[4])0;
  {
    uint index = 0u;
    while(true) {
      if ((index < 16u)) {
      } else {
        break;
      }
      acc[index] = 0.0f;
      {
        index = (index + 1u);
      }
      continue;
    }
  }
  uint ColPerThreadA = 4u;
  uint tileColA = (local_id[0u] * ColPerThreadA);
  uint RowPerThreadB = 4u;
  uint tileRowB = (local_id[1u] * RowPerThreadB);
  {
    uint t = 0u;
    while(true) {
      if ((t < numTiles)) {
      } else {
        break;
      }
      {
        uint innerRow = 0u;
        while(true) {
          if ((innerRow < 4u)) {
          } else {
            break;
          }
          {
            uint innerCol = 0u;
            while(true) {
              if ((innerCol < ColPerThreadA)) {
              } else {
                break;
              }
              uint inputRow = (tileRow + innerRow);
              uint inputCol = (tileColA + innerCol);
              mm_Asub[inputRow][inputCol] = mm_readA((globalRow + innerRow), ((t * 64u) + inputCol));
              {
                innerCol = (innerCol + 1u);
              }
              continue;
            }
          }
          {
            innerRow = (innerRow + 1u);
          }
          continue;
        }
      }
      {
        uint innerRow = 0u;
        while(true) {
          if ((innerRow < RowPerThreadB)) {
          } else {
            break;
          }
          {
            uint innerCol = 0u;
            while(true) {
              if ((innerCol < 4u)) {
              } else {
                break;
              }
              uint inputRow = (tileRowB + innerRow);
              uint inputCol = (tileCol + innerCol);
              float v_6 = mm_Bsub[innerCol][inputCol];
              v_6 = mm_readB(((t * 64u) + inputRow), (globalCol + innerCol));
              {
                innerCol = (innerCol + 1u);
              }
              continue;
            }
          }
          {
            innerRow = (innerRow + 1u);
          }
          continue;
        }
      }
      GroupMemoryBarrierWithGroupSync();
      {
        uint k = 0u;
        while(true) {
          if ((k < 64u)) {
          } else {
            break;
          }
          {
            uint inner = 0u;
            while(true) {
              if ((inner < 4u)) {
              } else {
                break;
              }
              BCached[inner] = mm_Bsub[k][(tileCol + inner)];
              {
                inner = (inner + 1u);
              }
              continue;
            }
          }
          {
            uint innerRow = 0u;
            while(true) {
              if ((innerRow < 4u)) {
              } else {
                break;
              }
              ACached = mm_Asub[(tileRow + innerRow)][k];
              {
                uint innerCol = 0u;
                while(true) {
                  if ((innerCol < 4u)) {
                  } else {
                    break;
                  }
                  uint index = ((innerRow * 4u) + innerCol);
                  acc[index] = (acc[index] + (ACached * BCached[innerCol]));
                  {
                    innerCol = (innerCol + 1u);
                  }
                  continue;
                }
              }
              {
                innerRow = (innerRow + 1u);
              }
              continue;
            }
          }
          {
            k = (k + 1u);
          }
          continue;
        }
      }
      GroupMemoryBarrierWithGroupSync();
      {
        t = (t + 1u);
      }
      continue;
    }
  }
  {
    uint innerRow = 0u;
    while(true) {
      if ((innerRow < 4u)) {
      } else {
        break;
      }
      {
        uint innerCol = 0u;
        while(true) {
          if ((innerCol < 4u)) {
          } else {
            break;
          }
          uint index = ((innerRow * 4u) + innerCol);
          mm_write((globalRow + innerRow), (globalCol + innerCol), acc[index]);
          {
            innerCol = (innerCol + 1u);
          }
          continue;
        }
      }
      {
        innerRow = (innerRow + 1u);
      }
      continue;
    }
  }
}

[numthreads(16, 16, 1)]
void main(main_inputs inputs) {
  main_inner(inputs.local_id, inputs.global_id, inputs.tint_local_index);
}

