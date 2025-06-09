#include "raylib.h"
#include "raymath.h"
#include "sqlite3.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TRIANGLES 100000
#define MAX_POLYLINE_POINTS 1000
#define MAX_POLYLINES 100

typedef struct {
  Vector3 v1, v2, v3;
  Vector3 normal;
} Triangle;

typedef struct {
  Vector2 points[MAX_POLYLINE_POINTS];
  int count;
  int drag_id;
  bool active;
} Polyline2D;

typedef struct {
  Vector3 points[MAX_POLYLINE_POINTS];
  Vector3 normals[MAX_POLYLINE_POINTS];
  int count;
  int mouse_id;
} Polyline3D;

typedef struct {
  Triangle triangles[MAX_TRIANGLES];
  int triangle_count;
  Model model;
  bool loaded;
} STLModel;

typedef struct {
  Vector3 position;
  Vector3 target;
  Vector3 up;
  float fovy;
  int projection;
  bool is_default;
} CameraData;

// Global variables
static sqlite3 *db = NULL;
static STLModel stl_model = {0};
static CameraData cam_data = {0};
static Polyline2D polylines_2d[MAX_POLYLINES] = {0};
static Polyline3D polylines_3d[MAX_POLYLINES] = {0};
static int polyline_count = 0;
static int current_polyline = -1;
static bool editing_mode = false;
static int selected_stl_id = 1;

// Function prototypes
bool InitDatabase(const char *db_path);
bool LoadSTLFromDB(int stl_id);
bool LoadCameraFromDB(int stl_id);
bool LoadPolylinesFromDB(int stl_id);
bool SavePolylineToScreenSpace(int drag_id);
bool SavePolylineTo3D(int drag_id);
void UpdatePolyline3DFromScreenSpace(int polyline_idx);
Vector3 ParseSTLTriangle(const char *line, int vertex_num);
void ProcessInput(Camera3D *camera);
void DrawPolylines2D(void);
void DrawPolylines3D(Camera3D camera);
void DrawUI(void);

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <database_path> [stl_id]\n", argv[0]);
    return 1;
  }

  const char *db_path = argv[1];
  if (argc > 2) {
    selected_stl_id = atoi(argv[2]);
  }

  // Initialize Raylib
  const int screenWidth = 1200;
  const int screenHeight = 800;
  InitWindow(screenWidth, screenHeight, "STL Viewer with Polyline Editor");
  SetTargetFPS(60);

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

  // Load camera settings
  LoadCameraFromDB(selected_stl_id);

  // Load existing polylines
  LoadPolylinesFromDB(selected_stl_id);

  // Initialize 3D camera
  Camera3D camera = {0};
  camera.position = cam_data.position;
  camera.target = cam_data.target;
  camera.up = cam_data.up;
  camera.fovy = cam_data.fovy;
  camera.projection = cam_data.projection;

  // Main game loop
  while (!WindowShouldClose()) {
    ProcessInput(&camera);

    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(camera);

    // Draw STL model
    if (stl_model.loaded) {
      DrawModel(stl_model.model, Vector3Zero(), 1.0f, GRAY);
    }

    // Draw 3D polylines
    DrawPolylines3D(camera);

    EndMode3D();

    // Draw 2D polylines overlay
    DrawPolylines2D();

    // Draw UI
    DrawUI();

    EndDrawing();
  }

  // Cleanup
  if (stl_model.loaded) {
    UnloadModel(stl_model.model);
  }
  sqlite3_close(db);
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
      stl_model.model = LoadModelFromMesh(mesh);
      stl_model.loaded = true;
      stl_model.triangle_count = triangle_count;
    }
  }

  sqlite3_finalize(stmt);
  return stl_model.loaded;
}

bool LoadCameraFromDB(int stl_id) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT posx, posy, posz, tx, ty, tz, upx, upy, upz, fovy, "
                    "proj FROM cams WHERE stl = ? ORDER BY isdef DESC LIMIT 1;";

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK)
    return false;

  sqlite3_bind_int(stmt, 1, stl_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    cam_data.position = (Vector3){sqlite3_column_double(stmt, 0),
                                  sqlite3_column_double(stmt, 1),
                                  sqlite3_column_double(stmt, 2)};
    cam_data.target = (Vector3){sqlite3_column_double(stmt, 3),
                                sqlite3_column_double(stmt, 4),
                                sqlite3_column_double(stmt, 5)};
    cam_data.up = (Vector3){sqlite3_column_double(stmt, 6),
                            sqlite3_column_double(stmt, 7),
                            sqlite3_column_double(stmt, 8)};
    cam_data.fovy = sqlite3_column_double(stmt, 9);
    cam_data.projection = sqlite3_column_int(stmt, 10);
  } else {
    // Default camera settings
    cam_data.position = (Vector3){10.0f, 10.0f, 10.0f};
    cam_data.target = (Vector3){0.0f, 0.0f, 0.0f};
    cam_data.up = (Vector3){0.0f, 1.0f, 0.0f};
    cam_data.fovy = 45.0f;
    cam_data.projection = CAMERA_PERSPECTIVE;
  }

  sqlite3_finalize(stmt);
  return true;
}

bool LoadPolylinesFromDB(int stl_id) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT d.rowid, d.desc FROM drags d JOIN cams c ON d.cam "
                    "= c.rowid WHERE c.stl = ?;";

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK)
    return false;

  sqlite3_bind_int(stmt, 1, stl_id);
  polyline_count = 0;

  while (sqlite3_step(stmt) == SQLITE_ROW && polyline_count < MAX_POLYLINES) {
    int drag_id = sqlite3_column_int(stmt, 0);

    // Load 2D points
    sqlite3_stmt *pts_stmt;
    const char *pts_sql = "SELECT x, y FROM two WHERE drag = ? ORDER BY rowid;";

    if (sqlite3_prepare_v2(db, pts_sql, -1, &pts_stmt, NULL) == SQLITE_OK) {
      sqlite3_bind_int(pts_stmt, 1, drag_id);

      int point_count = 0;
      while (sqlite3_step(pts_stmt) == SQLITE_ROW &&
             point_count < MAX_POLYLINE_POINTS) {
        polylines_2d[polyline_count].points[point_count] =
            (Vector2){sqlite3_column_double(pts_stmt, 0),
                      sqlite3_column_double(pts_stmt, 1)};
        point_count++;
      }

      polylines_2d[polyline_count].count = point_count;
      polylines_2d[polyline_count].drag_id = drag_id;
      polylines_2d[polyline_count].active = true;

      sqlite3_finalize(pts_stmt);
    }

    // Load 3D points
    sqlite3_stmt *pts3d_stmt;
    const char *pts3d_sql =
        "SELECT x, y, z, nx, ny, nz FROM three WHERE mouse = ? ORDER BY rowid;";

    if (sqlite3_prepare_v2(db, pts3d_sql, -1, &pts3d_stmt, NULL) == SQLITE_OK) {
      sqlite3_bind_int(pts3d_stmt, 1, drag_id);

      int point_count = 0;
      while (sqlite3_step(pts3d_stmt) == SQLITE_ROW &&
             point_count < MAX_POLYLINE_POINTS) {
        polylines_3d[polyline_count].points[point_count] =
            (Vector3){sqlite3_column_double(pts3d_stmt, 0),
                      sqlite3_column_double(pts3d_stmt, 1),
                      sqlite3_column_double(pts3d_stmt, 2)};
        polylines_3d[polyline_count].normals[point_count] =
            (Vector3){sqlite3_column_double(pts3d_stmt, 3),
                      sqlite3_column_double(pts3d_stmt, 4),
                      sqlite3_column_double(pts3d_stmt, 5)};
        point_count++;
      }

      polylines_3d[polyline_count].count = point_count;
      polylines_3d[polyline_count].mouse_id = drag_id;

      sqlite3_finalize(pts3d_stmt);
    }

    polyline_count++;
  }

  sqlite3_finalize(stmt);
  return true;
}

void ProcessInput(Camera3D *camera) {
  // Camera controls
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    UpdateCamera(camera, CAMERA_FREE);
  }

  // Toggle editing mode
  if (IsKeyPressed(KEY_E)) {
    editing_mode = !editing_mode;
    if (editing_mode && current_polyline == -1) {
      // Start new polyline
      if (polyline_count < MAX_POLYLINES) {
        current_polyline = polyline_count;
        polylines_2d[current_polyline].count = 0;
        polylines_2d[current_polyline].active = true;
        polylines_2d[current_polyline].drag_id = -1; // New polyline
      }
    }
  }

  // Add point to current polyline
  if (editing_mode && current_polyline >= 0 &&
      IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    Vector2 mouse_pos = GetMousePosition();
    if (polylines_2d[current_polyline].count < MAX_POLYLINE_POINTS) {
      polylines_2d[current_polyline]
          .points[polylines_2d[current_polyline].count] = mouse_pos;
      polylines_2d[current_polyline].count++;

      // Update 3D representation
      UpdatePolyline3DFromScreenSpace(current_polyline);
    }
  }

  // Finish current polyline
  if (editing_mode && current_polyline >= 0 && IsKeyPressed(KEY_ENTER)) {
    if (polylines_2d[current_polyline].count > 1) {
      // Save to database
      SavePolylineToScreenSpace(current_polyline);
      SavePolylineTo3D(current_polyline);

      if (current_polyline == polyline_count) {
        polyline_count++;
      }
    }
    current_polyline = -1;
    editing_mode = false;
  }

  // Cancel current polyline
  if (editing_mode && IsKeyPressed(KEY_ESCAPE)) {
    if (current_polyline == polyline_count) {
      polylines_2d[current_polyline].count = 0;
      polylines_2d[current_polyline].active = false;
    }
    current_polyline = -1;
    editing_mode = false;
  }
}

void UpdatePolyline3DFromScreenSpace(int polyline_idx) {
  Camera3D temp_camera = {.position = cam_data.position,
                          .target = cam_data.target,
                          .up = cam_data.up,
                          .fovy = cam_data.fovy,
                          .projection = cam_data.projection};

  for (int i = 0; i < polylines_2d[polyline_idx].count; i++) {
    Vector2 screen_pos = polylines_2d[polyline_idx].points[i];
    Ray ray = GetScreenToWorldRay(screen_pos, temp_camera);

    // Simple projection to Z=0 plane for demonstration
    // In a real application, you'd want to intersect with the STL model
    float t = -ray.position.z / ray.direction.z;
    Vector3 world_pos =
        Vector3Add(ray.position, Vector3Scale(ray.direction, t));

    polylines_3d[polyline_idx].points[i] = world_pos;
    polylines_3d[polyline_idx].normals[i] =
        (Vector3){0, 0, 1}; // Default normal
  }
  polylines_3d[polyline_idx].count = polylines_2d[polyline_idx].count;
}

bool SavePolylineToScreenSpace(int polyline_idx) {
  // Implementation for saving 2D polyline to database
  // This would insert into the 'two' table
  return true;
}

bool SavePolylineTo3D(int polyline_idx) {
  // Implementation for saving 3D polyline to database
  // This would insert into the 'three' table
  return true;
}

void DrawPolylines2D(void) {
  for (int i = 0; i < polyline_count; i++) {
    if (!polylines_2d[i].active)
      continue;

    Color color = (i == current_polyline) ? RED : BLUE;

    for (int j = 1; j < polylines_2d[i].count; j++) {
      DrawLineV(polylines_2d[i].points[j - 1], polylines_2d[i].points[j],
                color);
    }

    // Draw points
    for (int j = 0; j < polylines_2d[i].count; j++) {
      DrawCircleV(polylines_2d[i].points[j], 3, color);
    }
  }
}

void DrawPolylines3D(Camera3D camera) {
  for (int i = 0; i < polyline_count; i++) {
    Color color = (i == current_polyline) ? RED : GREEN;

    for (int j = 1; j < polylines_3d[i].count; j++) {
      DrawLine3D(polylines_3d[i].points[j - 1], polylines_3d[i].points[j],
                 color);
    }

    // Draw points
    for (int j = 0; j < polylines_3d[i].count; j++) {
      DrawSphere(polylines_3d[i].points[j], 0.1f, color);
    }
  }
}

void DrawUI(void) {
  DrawText("Controls:", 10, 10, 20, BLACK);
  DrawText("Right Mouse: Rotate camera", 10, 35, 16, DARKGRAY);
  DrawText("E: Toggle edit mode", 10, 55, 16, DARKGRAY);
  DrawText("Left Click: Add point (edit mode)", 10, 75, 16, DARKGRAY);
  DrawText("Enter: Finish polyline", 10, 95, 16, DARKGRAY);
  DrawText("Escape: Cancel polyline", 10, 115, 16, DARKGRAY);

  if (editing_mode) {
    DrawText("EDIT MODE", 10, 140, 20, RED);
  }

  DrawText(TextFormat("Polylines: %d", polyline_count), 10, 165, 16, BLACK);
  DrawText(TextFormat("STL ID: %d", selected_stl_id), 10, 185, 16, BLACK);
}
