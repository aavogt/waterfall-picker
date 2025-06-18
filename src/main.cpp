#include "main.h"
#include "geometry.h"
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
  for (int i = 0; i < stl_model.meshCount; i++) {
    free(stl_model.meshes[i].vertices);
    free(stl_model.meshes[i].normals);
  }
  RL_FREE(stl_model.meshes);
  CloseWindow();

  return 0;
}

// interpret all points in the given camera as defining a (planar) polygonal
// pyramid cast from the camera, whose base touches but
// does not penetrate into stl_model. The polygon's normal (ie. of the
// pyramid's base) should point
// roughly at the camera, but in this first attempt there is no way to express a
// preference for skewness vs. distance:
//
// consider the stl_model as a sphere, then some traversal orders will
// be different? In all cases the polygon will be tangent to the sphere?
void AttachPolygon1(int nx, int ny) {
  // bounding box
  Vector2 upperLeft = {INFINITY, INFINITY}, lowerRight = {-INFINITY, -INFINITY};

  // compute the axis aligned bounding box (aabb)
  int nbb = 0;
  for (int i = 0; i < npicks; i++) {
    if (cameraid != picks2cam[i])
      continue;
    nbb++;
    if (picks2[i].x < upperLeft.x) {
      upperLeft.x = picks2[i].x;
    }
    if (picks2[i].y < upperLeft.y) {
      upperLeft.y = picks2[i].y;
    }
    if (picks2[i].x > lowerRight.x) {
      lowerRight.x = picks2[i].x;
    }
    if (picks2[i].y > lowerRight.y) {
      lowerRight.y = picks2[i].y;
    }
  }
  float dx = (lowerRight.x - upperLeft.x) / (nx - 1);
  float dy = (lowerRight.y - upperLeft.y) / (ny - 1);

  Vector3 boundary[3];
  int iboundary = 0;

  // nothing or point
  if (nbb < 2)
    return;

  // line segment
  if (nbb == 2) {
    for (int i2 = nx / 2; i2 >= 0; i2--)
      for (int si = -1; si <= 1; si += 2) {
        int i = nx / 2 + si * i2;
        Vector2 p = {upperLeft.x + i * dx, upperLeft.y + i * dy};
        Ray ray = GetScreenToWorldRay(p, camera);
        RayCollision hit =
            GetRayCollisionMesh(ray, stl_model.meshes[0], stl_model.transform);

        if (hit.hit) {
          if (iboundary < 3) {
            boundary[iboundary++] = hit.point;
          }
          AdvanceSeg(camera.position, boundary, hit.point);
        }
      }
  }

  // plane
  if (nbb > 2) {
    Ray ray;
    RayCollision hit;
    // traverse from outside the aabb towards the center
    for (int i2 = nx / 2; i2 >= 0; i2--)
      for (int j2 = ny / 2; j2 >= 0; j2--)
        for (int sj = -1; sj <= 1; sj += 2)
          for (int si = -1; si <= 1; si += 2) {
            int i = nx / 2 + si * i2;
            int j = ny / 2 + sj * j2;
            Vector2 p = {upperLeft.x + i * dx, upperLeft.y + j * dy};
            // check that p is inside the polygon, assuming that points are
            // counterclockwise
            // TODO Nef polygon would be
            // float sign = 1;
            // for (...) sign = copysign(sign, Turn( .. ))
            // if (sign < 0) goto outside;
            for (int k = 1; k < npicks; k++) {
              if (Turn(picks2[k - 1], picks2[k], p) < 0)
                goto outside;
            }
            // inside
            ray = GetScreenToWorldRay(p, camera);
            hit = GetRayCollisionMesh(ray, stl_model.meshes[0],
                                      stl_model.transform);

            if (iboundary < 3) {
              boundary[iboundary++] = hit.point;
            } else {
              AdvancePlane(camera.position, boundary, hit.point);
            }
          outside:
            continue;
          }
  }

  for (int i = 0; i < npicks; i++) {
    if (cameraid != picks2cam[i])
      continue;
    Ray ray = GetScreenToWorldRay(picks2[i], camera);
    RayCollision hit = GetRayCollisionPlane(ray, boundary, iboundary);
    if (hit.hit) {
      picks[i] = hit.point;
      // do it with a single transaction?
      DeletePick(i);
      InsertPick(picks2[i], hit.point, picks2cam[i]);
    }
  }
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
    if (cameraattachment == 1)
      AttachPolygon1(10, 10);
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
  if (IsKeyPressed(KEY_M)) {
    cameraattachment = (1 + cameraattachment) % 2;
    if (cameraattachment == 1)
      AttachPolygon1(10, 10);
  }
}

void DrawPicks() {
  for (int i = 0; i < npicks; i++) {
    Color col = (!camdirty && picks2cam[i] == cameraid) ? RED : BLUE;
    DrawSphere(picks[i], 1.f, col);
  }
}

void DrawUI(void) {
  // instructions
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
  DrawText(cameraattachment ? "M: camera attachment mode point"
                            : "M: camera attachment mode envelope",
           10, 160, 16, IsKeyDown(KEY_M) ? RED : DARKGRAY);

  // status
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
