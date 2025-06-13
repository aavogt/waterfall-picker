#ifndef MAIN_ONCE
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
static bool camdirty = true;
static Vector2 picks2[MAX_PTS];
static Vector3 picks[MAX_PTS];
static int npicks = 0;
static Shader shader;
const char *db_path = "stl.sqlite3"; // Default database path

bool InitDatabase(const char *db_path);
bool LoadSTLFromDB(int stl_id);
bool LoadCameraFromDB(int stl_id);
bool LoadPicksFromDB(int stl_id);
bool SavePolylineToScreenSpace(int drag_id);
bool SavePolylineTo3D(int drag_id);
void ProcessInput();
void DrawPicks();
void DrawUI(void);
bool DeletePick(int i);
#define MAIN_ONCE
#endif
