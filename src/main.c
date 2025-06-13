#include "main.h"
#include "initdb.h"
#include "initshader.h"
#include "inittexture.h"

int main(int argc, char *argv[]) {
  // if (argc < 2) {
  //   printf("Usage: %s <database_path> [stl_id]\n", argv[0]);
  //   return 1;
  // }
  selected_stl_id = 1;

  if (argc > 1) {
    const char *db_path = argv[1];
  }
  if (argc > 2) {
    selected_stl_id = atoi(argv[2]);
  }

  // Initialize Raylib
  const int screenWidth = 1200;
  const int screenHeight = 800;
  InitWindow(screenWidth, screenHeight, "STL Viewer with Point Editor");
  SetTargetFPS(30);

  InitializeLoadDB();
  InitializeShader();
  InitializeTexture();

  // Main game loop
  while (!WindowShouldClose()) {
    ProcessInput();

    BeginDrawing();
    ClearBackground(RAYWHITE);
    BeginMode3D(camera);
    BeginShaderMode(shader);
    DrawModel(stl_model, Vector3Zero(), 1.0f, (Color){0, 255, 255, 128});
    EndShaderMode();
    DrawPicks();
    EndMode3D();
    DrawUI();
    EndDrawing();
  }

  // Cleanup
  sqlite3_close(db);

  UnloadShader(shader);
  UninitializeTexture();
  // UnloadModel(stl_model);
  CloseWindow();

  return 0;
}

// Plucker coordinate
float RayVector3Distance(Ray r, Vector3 x) {
  Vector3 originToPoint = Vector3Subtract(x, r.position);
  return Vector3Length(Vector3CrossProduct(r.direction, originToPoint));
}

void ProcessInput() {
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    UpdateCamera(&camera, CAMERA_THIRD_PERSON);
    cameramatrix = GetCameraMatrix(camera);
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    Vector2 mouse_pos = GetMousePosition();
    Ray ray = GetScreenToWorldRay(mouse_pos, camera);

    // if there are multiple meshes, consider only the first one to be hit
    RayCollision hit;
    for (int i = 0; i < stl_model.meshCount; i++) {
      hit = GetRayCollisionMesh(ray, stl_model.meshes[i], stl_model.transform);
      if (hit.hit)
        break;
    }
    if (hit.hit) {
      picks2[npicks] = mouse_pos;
      picks[npicks++] = hit.point;
    }
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && npicks > 0) {
    Vector2 mouse_pos = GetMousePosition();
    Ray ray = GetScreenToWorldRay(mouse_pos, camera);

    // index with the minimum distance
    int min_index = 0;
    float min_distance = RayVector3Distance(ray, picks[0]);
    for (int i = 1; i < npicks; i++) {
      float distance = RayVector3Distance(ray, picks[i]);
      if (distance < min_distance) {
        min_distance = distance;
        min_index = i;
      }
    }

    DeletePick(min_index);
  }
}

void DrawPicks() {
  for (int i = 0; i < npicks; i++) {
    DrawSphere(picks[i], 1.f, RED);
  }
}

void DrawUI(void) {
  DrawText("Controls:", 10, 10, 20, BLACK);
  DrawText("Left Click: Add point", 10, 75, 16, DARKGRAY);
  DrawText("Middle Click: Delete closest point", 10, 75 + 20, 16, DARKGRAY);
  DrawText("Right drag: Rotate camera", 10, 75 + 40, 16, DARKGRAY);
  DrawText(TextFormat("npicks: %d", npicks), 10, 165, 16, BLACK);
  DrawText(TextFormat("STL ID: %d", selected_stl_id), 10, 185, 16, BLACK);
}
