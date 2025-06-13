#include "main.h"

void SetLightPosition(Vector3 lightPosition) {
  int shaderLightPositionLoc = GetShaderLocation(shader, "lightPosition");
  SetShaderValue(shader, shaderLightPositionLoc, &lightPosition,
                 SHADER_UNIFORM_VEC3);
}

bool InitializeShader() {
  shader = LoadShader("vs.glsl", "fs.glsl");
  if (shader.id == 0) {
    printf("Failed to load shader\n");
    return 1;
  }
  Vector3 ambientColor = {0.5f, 0.5f, 0.4f}; // Example ambient color
  int shaderAmbientLoc = GetShaderLocation(shader, "ambientColor");
  SetShaderValue(shader, shaderAmbientLoc, &ambientColor, SHADER_UNIFORM_VEC3);

  SetLightPosition((Vector3){0.0, -10.0, 10.0});
  return 0;
}
