#include "checkerboard.h"
#include "main.h"

static Image checkerboard_img;
static Texture2D checkerboard;
static Material materials;
static MaterialMap materialmap;

inline bool InitializeTexture() {
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
  materialmap = {.texture = checkerboard, .color = GRAY, .value = 50.f};
  materials = (Material){
      .shader = shader, .maps = &materialmap, .params = {1.f, 1.f, 1.f, 1.f}};
  stl_model.materials = &materials;
  return 0;
}

inline void UninitializeTexture() {
  stl_model.materials = NULL;
  UnloadImage(checkerboard_img);
  UnloadTexture(checkerboard);
}
