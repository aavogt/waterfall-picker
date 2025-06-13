#include "main.h"

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

bool LoadCameraID(int cam_id) {
  sqlite3_stmt *stmt;
  const char *sql =
      "SELECT posx, posy, posz, tx, ty, tz, upx, upy, upz, fovy, proj "
      "FROM cams WHERE rowid = ?;";
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_int(stmt, 1, cam_id);

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
    printf("No camera found with rowid: %d\n", cam_id);
    sqlite3_finalize(stmt);
    return false;
  }
}

bool LoadCameraIDWithDirection(bool asc) {
  sqlite3_stmt *stmt;
  const char *sql =
      asc ? "SELECT rowid FROM cams WHERE rowid > ? ORDER BY rowid ASC LIMIT 1;"
          : "SELECT rowid FROM cams WHERE rowid < ? ORDER BY rowid DESC LIMIT "
            "1;";
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_int(stmt, 1, cameraid);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    cameraid = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return LoadCameraID(cameraid);
  }

  sqlite3_finalize(stmt);

  // Wrap to the first or last camera if no next/previous camera is found
  sql = asc ? "SELECT rowid FROM cams ORDER BY rowid ASC LIMIT 1;"
            : "SELECT rowid FROM cams ORDER BY rowid DESC LIMIT 1;";
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    cameraid = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return LoadCameraID(cameraid);
  }

  sqlite3_finalize(stmt);
  printf("No cameras found in the database.\n");
  return false;
}

bool LoadCameraFromDB(int stl_id) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT rowid FROM cams WHERE stl = ?;";
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_int(stmt, 1, stl_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int cam_id = sqlite3_column_int(stmt, 0); // Retrieve the camera ID
    sqlite3_finalize(stmt);
    return LoadCameraID(cam_id); // Use LoadCameraID to load the camera data
  }

  sqlite3_finalize(stmt);
  printf("No camera found for stl_id: %d\n", stl_id);
  return false;
}

bool InitializeLoadDB() {
  // Initialize database
  if (!InitDatabase(db_path)) {
    printf("Failed to initialize database\n");
    CloseWindow();
    return 1;
  }

  // Load STL model
  if (!LoadSTLFromDB(selected_stl_id)) {
    printf("Failed to load STL model from DB\n");
    sqlite3_close(db);
    CloseWindow();
    return 1;
  }

  // Load picks
  if (!LoadPicksFromDB(selected_stl_id)) {
    printf("Failed to load picks from DB\n");
    sqlite3_close(db);
    CloseWindow();
    return 1;
  }

  // Load camera settings
  if (!LoadCameraFromDB(selected_stl_id)) {
    printf("Failed to load camera from DB\n");
    sqlite3_close(db);
    CloseWindow();
    return 1;
  }
  return 0;
}

bool DeletePick(int i) {
  // delete it from the arrays
  int delid = picksid[i];
  picks[i] = picks[npicks - 1];
  picks2cam[i] = picks2cam[npicks - 1];
  picksid[i] = picksid[npicks - 1];
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
  sqlite3_bind_int(stmt, 1, delid);

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

bool InsertPick(Vector2 mouse_pos, Vector3 world_pos, int cam_id) {
  // Ensure we don't exceed the maximum number of picks
  if (npicks >= MAX_PTS) {
    printf("Error: Exceeded maximum number of picks (%d).\n", MAX_PTS);
    return false;
  }

  // Prepare the SQL statement for insertion
  const char *sql =
      "INSERT INTO picks (cam, mx, my, x, y, z) VALUES (?, ?, ?, ?, ?, ?);";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  // Bind values to the SQL statement
  sqlite3_bind_int(stmt, 1, cam_id);         // cam
  sqlite3_bind_double(stmt, 2, mouse_pos.x); // mx
  sqlite3_bind_double(stmt, 3, mouse_pos.y); // my
  sqlite3_bind_double(stmt, 4, world_pos.x); // x
  sqlite3_bind_double(stmt, 5, world_pos.y); // y
  sqlite3_bind_double(stmt, 6, world_pos.z); // z

  // Execute the SQL statement
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    printf("Failed to insert pick: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return false;
  }

  // Get the rowid of the inserted pick
  int rowid = (int)sqlite3_last_insert_rowid(db);

  // Update the static arrays
  picks2[npicks] = mouse_pos;
  picks[npicks] = world_pos;
  picks2cam[npicks] = cam_id;
  picksid[npicks] = rowid;
  npicks++; // Increment the number of picks

  // Finalize the statement
  sqlite3_finalize(stmt);

  printf("Pick inserted successfully with rowid = %d.\n", rowid);
  return true;
}

bool InsertCam(Camera3D camera, int stl_id, int *cam_id) {
  // Prepare the SQL statement for insertion
  const char *sql = "INSERT INTO cams (posx, posy, posz, tx, ty, tz, upx, upy, "
                    "upz, fovy, proj, stl) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  // Bind values to the SQL statement
  sqlite3_bind_double(stmt, 1, camera.position.x); // posx
  sqlite3_bind_double(stmt, 2, camera.position.y); // posy
  sqlite3_bind_double(stmt, 3, camera.position.z); // posz
  sqlite3_bind_double(stmt, 4, camera.target.x);   // tx
  sqlite3_bind_double(stmt, 5, camera.target.y);   // ty
  sqlite3_bind_double(stmt, 6, camera.target.z);   // tz
  sqlite3_bind_double(stmt, 7, camera.up.x);       // upx
  sqlite3_bind_double(stmt, 8, camera.up.y);       // upy
  sqlite3_bind_double(stmt, 9, camera.up.z);       // upz
  sqlite3_bind_double(stmt, 10, camera.fovy);      // fovy
  sqlite3_bind_int(stmt, 11, camera.projection);   // proj
  sqlite3_bind_int(stmt, 12, stl_id);              // stl

  // Execute the SQL statement
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    printf("Failed to insert camera: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return false;
  }

  *cam_id = (int)sqlite3_last_insert_rowid(db);

  // Finalize the statement
  sqlite3_finalize(stmt);

  printf("Camera inserted successfully for stl_id = %d.\n", stl_id);
  return true;
}

bool RemoveCameraFromDB(int cam_id) {
  // Check the number of rows in the cams table
  const char *count_sql = "SELECT COUNT(*) FROM cams;";
  sqlite3_stmt *count_stmt;

  int rc = sqlite3_prepare_v2(db, count_sql, -1, &count_stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  rc = sqlite3_step(count_stmt);
  if (rc != SQLITE_ROW) {
    printf("Failed to count rows: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(count_stmt);
    return false;
  }

  int row_count = sqlite3_column_int(count_stmt, 0);
  sqlite3_finalize(count_stmt);

  if (row_count <= 1) {
    printf("Cannot delete the last row in the database.\n");
    return false;
  }

  // Proceed with deletion if there is more than one row
  const char *sql = "DELETE FROM cams WHERE rowid = ?;";
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_int(stmt, 1, cam_id);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    printf("Failed to delete camera: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return false;
  }

  sqlite3_finalize(stmt);

  printf("Camera with ID %d deleted successfully.\n", cam_id);
  return true;
}
