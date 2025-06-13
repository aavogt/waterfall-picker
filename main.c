#include "raylib.h"
#include "raymath.h"

#include "sqlite3.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_TRIANGLES 80000
#define MAX_PTS 1000

// Global variables
static sqlite3 *db = NULL;
static Camera3D camera = {0};
static Matrix cameramatrix = {0};
static int selected_stl_id = 0;
static Model stl_model = {0};
static int editing_mode = 0;

static int picksid[MAX_PTS];
static int picks2cam[MAX_PTS];
static Vector2 picks2[MAX_PTS];
static Vector3 picks[MAX_PTS];
static int npicks = 0;
static Shader shader;

// Function prototypes
bool InitDatabase(const char *db_path);
bool LoadSTLFromDB(int stl_id);
bool LoadCameraFromDB(int stl_id);
bool LoadPicksFromDB(int stl_id);
bool SavePolylineToScreenSpace(int drag_id);
bool SavePolylineTo3D(int drag_id);
void ProcessInput();
void DrawPicks();
void DrawUI(void);

int main(int argc, char *argv[]) {
  // if (argc < 2) {
  //   printf("Usage: %s <database_path> [stl_id]\n", argv[0]);
  //   return 1;
  // }
  const char *db_path = "/tmp/db.stl"; // Default database path
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

  // Initialize database
  if (!InitDatabase(db_path)) {
    printf("Failed to initialize database\n");
    CloseWindow();
    return 1;
  }

  // Load STL model
  if (!LoadSTLFromDB(selected_stl_id)) {
    printf("Failed to load STL model\n");
    sqlite3_close(db);
    CloseWindow();
    return 1;
  }

  // Load picks
  if (!LoadPicksFromDB(selected_stl_id)) {
    printf("Failed to load picks\n");
    sqlite3_close(db);
    CloseWindow();
    return 1;
  }

  // Load camera settings
  LoadCameraFromDB(selected_stl_id);

  shader = LoadShader("vs.glsl", "fs.glsl");
  if (shader.id == 0) {
    printf("Failed to load shader\n");
    return 1;
  }
  Vector3 ambientColor = {0.5f, 0.5f, 0.4f}; // Example ambient color
  int shaderAmbientLoc = GetShaderLocation(shader, "ambientColor");
  SetShaderValue(shader, shaderAmbientLoc, &ambientColor, SHADER_UNIFORM_VEC3);

  Vector3 lightPosition = {0.0, -10.0, 10.0};
  int shaderLightPositionLoc = GetShaderLocation(shader, "lightPosition");
  SetShaderValue(shader, shaderLightPositionLoc, &lightPosition,
                 SHADER_UNIFORM_VEC3);

  // Load texture and assign it to the model
  // convert -size 800x800 pattern:checkerboard -colors 2 checkerboard.png
  Texture2D checkerboard = LoadTexture("checkerboard.png");
  if (checkerboard.id == 0) {
    printf("Failed to load texture\n");
    return 1;
  }
  stl_model.materials =
      &(Material){.shader = shader,
                  .params = {1.f, 1.f, 1.f, 1.f},
                  .maps = &(MaterialMap){
                      .texture = checkerboard, .color = GRAY, .value = 50.f}};

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
  UnloadModel(stl_model);
  sqlite3_close(db);
  UnloadTexture(checkerboard);
  CloseWindow();

  return 0;
}

bool InitDatabase(const char *db_path) {
  int rc = sqlite3_open(db_path, &db);
  if (rc != SQLITE_OK) {
    printf("Cannot open database: %s\n", sqlite3_errmsg(db));
    return false;
  }
  return true;
}

bool LoadSTLFromDB(int stl_id) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT data FROM stls WHERE rowid = ?;";

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_int(stmt, 1, stl_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const void *data = sqlite3_column_blob(stmt, 0);
    int data_size = sqlite3_column_bytes(stmt, 0);

    // Parse STL data (simplified binary STL parsing)
    if (data_size > 80) { // STL header is 80 bytes
      const char *stl_data = (const char *)data;
      uint32_t triangle_count = *(uint32_t *)(stl_data + 80);

      if (triangle_count > MAX_TRIANGLES)
        triangle_count = MAX_TRIANGLES;

      // Create mesh from STL data
      Mesh mesh = {0};
      mesh.triangleCount = triangle_count;
      mesh.vertexCount = triangle_count * 3;
      mesh.vertices = (float *)malloc(mesh.vertexCount * 3 * sizeof(float));
      mesh.normals = (float *)malloc(mesh.vertexCount * 3 * sizeof(float));

      const char *triangle_data = stl_data + 84;
      for (int i = 0; i < triangle_count; i++) {
        // Normal vector (12 bytes)
        float nx = *(float *)(triangle_data + i * 50);
        float ny = *(float *)(triangle_data + i * 50 + 4);
        float nz = *(float *)(triangle_data + i * 50 + 8);

        // Vertices (36 bytes)
        for (int j = 0; j < 3; j++) {
          int vertex_idx = i * 3 + j;
          mesh.vertices[vertex_idx * 3] =
              *(float *)(triangle_data + i * 50 + 12 + j * 12);
          mesh.vertices[vertex_idx * 3 + 1] =
              *(float *)(triangle_data + i * 50 + 16 + j * 12);
          mesh.vertices[vertex_idx * 3 + 2] =
              *(float *)(triangle_data + i * 50 + 20 + j * 12);

          mesh.normals[vertex_idx * 3] = nx;
          mesh.normals[vertex_idx * 3 + 1] = ny;
          mesh.normals[vertex_idx * 3 + 2] = nz;
        }
      }

      UploadMesh(&mesh, false);
      stl_model = LoadModelFromMesh(mesh);
      sqlite3_finalize(stmt);
      return true;
    }
  }
  sqlite3_finalize(stmt);
  return false;
}

bool LoadPicksFromDB(int stl_id) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT picks.cam, picks.mx, picks.my, picks.x, picks.y, "
                    "picks.z, picks.rowid "
                    "FROM picks "
                    "INNER JOIN cams ON picks.cam = cams.rowid "
                    "WHERE cams.stl = ?;";
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_int(stmt, 1, stl_id);
  npicks = 0; // Reset the number of picks
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (npicks >= MAX_PTS) {
      printf("Error: Exceeded maximum number of picks (%d).\n", MAX_PTS);
      break;
    }

    // Assign values from the database row to the static variables
    picks2cam[npicks] = sqlite3_column_int(stmt, 0); // cam

    picks2[npicks] = (Vector2){
        .x = (float)sqlite3_column_double(stmt, 1), // mx
        .y = (float)sqlite3_column_double(stmt, 2)  // my
    };

    picks[npicks] = (Vector3){
        .x = (float)sqlite3_column_double(stmt, 3), // x
        .y = (float)sqlite3_column_double(stmt, 4), // y
        .z = (float)sqlite3_column_double(stmt, 5)  // z
    };

    picksid[npicks] = sqlite3_column_int(stmt, 6); // rowid

    npicks++; // Increment the number of picks
  }

  sqlite3_finalize(stmt);
  return true;
}

bool LoadCameraFromDB(int stl_id) {
  sqlite3_stmt *stmt;
  const char *sql =
      "SELECT posx, posy, posz, tx, ty, tz, upx, upy, upz, fovy, proj "
      "FROM cams WHERE stl = ?;";
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_int(stmt, 1, stl_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    // Assign values from the database row to the camera structure
    camera = (Camera3D){
        .position =
            {
                .x = (float)sqlite3_column_double(stmt, 0), // posx
                .y = (float)sqlite3_column_double(stmt, 1), // posy
                .z = (float)sqlite3_column_double(stmt, 2)  // posz
            },
        .target =
            {
                .x = (float)sqlite3_column_double(stmt, 3), // tx
                .y = (float)sqlite3_column_double(stmt, 4), // ty
                .z = (float)sqlite3_column_double(stmt, 5)  // tz
            },
        .up =
            {
                .x = (float)sqlite3_column_double(stmt, 6), // upx
                .y = (float)sqlite3_column_double(stmt, 7), // upy
                .z = (float)sqlite3_column_double(stmt, 8)  // upz
            },
        .fovy = (float)sqlite3_column_double(stmt, 9), // fovy
        .projection = sqlite3_column_int(stmt, 10)     // proj
    };
    cameramatrix = GetCameraMatrix(camera);

    sqlite3_finalize(stmt);
    return true;
  } else {
    printf("No camera found for stl_id: %d\n", stl_id);
    sqlite3_finalize(stmt);
    return false;
  }
}

// Plucker coordinate
float RayVector3Distance(Ray r, Vector3 x) {
  Vector3 originToPoint = Vector3Subtract(x, r.position);
  return Vector3Length(Vector3CrossProduct(r.direction, originToPoint));
}

bool DeletePick(int i) {
  // delete it from the arrays
  int delid = picksid[i];
  picks[i] = picks[npicks - 1];
  picks2cam[i] = picks2cam[npicks - 1];
  picksid[i] = picks2cam[npicks - 1];
  picks2[i] = picks2[--npicks];

  // delete it from the database
  const char *sql = "DELETE FROM picks WHERE rowid = ?;";
  sqlite3_stmt *stmt;

  // Prepare the SQL statement
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  // Bind the rowid value to the SQL statement
  sqlite3_bind_int(stmt, 1, i);

  // Execute the SQL statement
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    printf("Failed to delete row: %s\n", sqlite3_errmsg(db));
  } else {
    printf("Row with rowid = %d deleted successfully.\n", i);
  }

  // Finalize the statement
  sqlite3_finalize(stmt);
  return true;
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
