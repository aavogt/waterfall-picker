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
    DrawPicks();
    EndShaderMode();
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
    printf("Mouse Left Button Pressed at Position: x=%.2f, y=%.2f\n",
           mouse_pos.x, mouse_pos.y);
    Ray ray = GetScreenToWorldRay(mouse_pos, camera);
    RayCollision hit =
        GetRayCollisionMesh(ray, stl_model.meshes[0], cameramatrix);
    if (hit.hit) {
      printf("Ray hit detected at Point: x=%.2f, y=%.2f, z=%.2f\n", hit.point.x,
             hit.point.y, hit.point.z);
      picks2[npicks] = GetMousePosition();
      picks[npicks++] = hit.point;
      printf("Pick added. Total picks: %d\n", npicks);
    } else {
      printf("No hit detected.\n");
    }
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
    Vector2 mouse_pos = GetMousePosition();
    printf("Mouse Middle Button Pressed at Position: x=%.2f, y=%.2f\n",
           mouse_pos.x, mouse_pos.y);
    Ray ray = GetScreenToWorldRay(mouse_pos, camera);

    // index with the minimum distance
    int min_index = 0;
    float min_distance = RayVector3Distance(ray, picks[0]);
    printf("Initial minimum distance: %.2f\n", min_distance);
    for (int i = 1; i < npicks; i++) {
      float distance = RayVector3Distance(ray, picks[i]);
      printf("Distance to pick[%d]: %.2f\n", i, distance);
      if (distance < min_distance) {
        min_distance = distance;
        min_index = i;
        printf("New minimum distance: %.2f at index %d\n", min_distance,
               min_index);
      }
    }

    DeletePick(min_index);
    printf("Pick at index %d deleted. Remaining picks: %d\n", min_index,
           npicks - 1);
  }
}

void DrawPicks() {
  for (int i = 0; i < npicks; i++) {
    DrawSphere(picks[i], 1.f, GREEN);
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
