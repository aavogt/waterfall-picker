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
    camdirty = true;
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
      if (camdirty) {
        InsertCam(camera, selected_stl_id, &cameraid);
        printf("dirty cam, camid %d\n", cameraid);
        camdirty = false;
      }
      InsertPick(mouse_pos, hit.point, cameraid);
    }
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && npicks > 0 && deletionmode) {
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

  if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && npicks > 0 &&
      !deletionmode) {
    Vector2 mouse_pos = GetMousePosition();
    Ray ray = GetScreenToWorldRay(mouse_pos, camera);

    // index with the minimum distance
    // that also has the same camera id
    int min_index = -1;
    for (int i = 0; i < npicks; i++) {
      if (picks2cam[i] == cameraid) {
        min_index = i;
        break;
      }
    }

    if (min_index == -1) {
      // nobody uses the camera so delete it
      RemoveCameraFromDB(cameraid);
      LoadCameraIDWithDirection(true);
    } else {
      float min_distance = RayVector3Distance(ray, picks[min_index]);
      for (int i = min_index + 1; i < npicks; i++) {
        if (picks2cam[i] != cameraid)
          continue;
        float distance = RayVector3Distance(ray, picks[i]);
        if (distance < min_distance) {
          min_distance = distance;
          min_index = i;
        }
      }
      DeletePick(min_index);
    }
  }

  if (IsKeyPressed(KEY_PERIOD)) {
    LoadCameraIDWithDirection(true);
  }
  if (IsKeyPressed(KEY_COMMA)) {
    LoadCameraIDWithDirection(false);
  }

  if (IsKeyPressed(KEY_SLASH)) {
    deletionmode = (1 + deletionmode) % 2;
  }
}

void DrawPicks() {
  for (int i = 0; i < npicks; i++) {
    Color col = (!camdirty && picks2cam[i] == cameraid) ? RED : BLUE;
    DrawSphere(picks[i], 1.f, col);
  }
}

void DrawUI(void) {
  DrawText("Controls:", 10, 10, 20, BLACK);
  DrawText(", (KEY_COMMA): previous camera", 10, 40, 16,
           IsKeyDown(KEY_COMMA) ? RED : DARKGRAY);
  DrawText(". (KEY_PERIOD): next camera", 10, 60, 16,
           IsKeyDown(KEY_PERIOD) ? RED : DARKGRAY);
  DrawText("/ (KEY_SLASH): toggle middle click deletion mode", 10, 80, 16,
           IsKeyDown(KEY_SLASH) ? RED : DARKGRAY);
  DrawText("Left Click: Add point", 10, 100, 16,
           IsMouseButtonDown(MOUSE_LEFT_BUTTON) ? RED : DARKGRAY);
  DrawText("Middle Click: Delete closest point", 10, 120, 16,
           IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) ? RED : DARKGRAY);
  DrawText("Right drag: Rotate camera", 10, 140, 16,
           IsMouseButtonDown(MOUSE_RIGHT_BUTTON) ? RED : DARKGRAY);
  DrawText(TextFormat("npicks: %d", npicks), 10, GetScreenHeight() - 100, 16,
           BLACK);
  DrawText(TextFormat("STL ID: %d", selected_stl_id), 10,
           GetScreenHeight() - 80, 16, BLACK);
  DrawText(camdirty ? "CAM ID: ..." : TextFormat("CAM ID: %d", cameraid), 10,
           GetScreenHeight() - 60, 16, BLACK);
  DrawText(deletionmode
               ? "Middle click deletes all"
               : "Middle click deletes only for current CAM ID (red points)",
           10, GetScreenHeight() - 40, 16, BLACK);
}
