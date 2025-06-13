#include "checkerboard.h"
#include "main.h"

Image checkerboard_img;
Texture2D checkerboard;
static Material materials;

bool InitializeTexture() {
  // Load texture and assign it to the model
  // convert -size 800x800 pattern:checkerboard -colors 2 checkerboard.png
  // xxd -i checkerboard.png > checkerboard.h
  checkerboard_img =
      LoadImageFromMemory(".png", checkerboard_png, checkerboard_png_len);
  checkerboard = LoadTextureFromImage(checkerboard_img);
  if (checkerboard.id == 0) {
    printf("Failed to load texture\n");
    return 1;
  }
  materials =
      (Material){.shader = shader,
                 .params = {1.f, 1.f, 1.f, 1.f},
                 .maps = &(MaterialMap){
                     .texture = checkerboard, .color = GRAY, .value = 50.f}};
  stl_model.materials = &materials;
  return 0;
}

void UninitializeTexture() {
  stl_model.materials = NULL;
  UnloadImage(checkerboard_img);
  UnloadTexture(checkerboard);
}
