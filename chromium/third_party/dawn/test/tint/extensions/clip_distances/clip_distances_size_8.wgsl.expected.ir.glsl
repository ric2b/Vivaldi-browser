SKIP: INVALID

#version 310 es

struct VertexOutputs {
  vec4 position;
  float clipDistance[8];
};

VertexOutputs main() {
  return VertexOutputs(vec4(1.0f, 2.0f, 3.0f, 4.0f), float[8](0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
}
error: Error parsing GLSL shader:
ERROR: 0:8: 'structure' :  entry point cannot return a value
ERROR: 0:8: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.




tint executable returned error: exit status 1
