#include "geometry.h"
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <random>

Vector3 ArbitraryVector3() {
  return {static_cast<float>(std::rand()) / RAND_MAX,
          static_cast<float>(std::rand()) / RAND_MAX,
          static_cast<float>(std::rand()) / RAND_MAX};
}

// Generate an array of arbitrary Vector3 values
void GenerateArbitraryVector3Array(Vector3 p[3]) {
  for (int i = 0; i < 3; ++i) {
    p[i] = ArbitraryVector3();
  }
}
void GenerateArbitraryVector2Array(Vector3 p[2]) {
  for (int i = 0; i < 2; ++i) {
    p[i] = ArbitraryVector3();
  }
}

// EyeSide(plane, eye, x) + if same eye and x are on the same side, - if
// different
float EyeSide(Vector3 p[3], Vector3 eye, Vector3 x) {
  Vector3 n = Vector3CrossProduct(p[2] - p[0], p[1] - p[0]);
  return Vector3DotProduct(n, eye - p[0]) * Vector3DotProduct(n, x - p[0]);
}

float EyeSide2(Vector3 p[2], Vector3 eye, Vector3 x) {
  Vector3 n = Vector3CrossProduct(eye - p[1], eye - p[0]);
  Vector3 b = Vector3CrossProduct(eye - p[1], n);
  return Vector3DotProduct(x - p[0], b) * Vector3DotProduct(eye - p[0], b);
}

TEST(AdvanceSegTest, UpdatesSegmentPoints) {
  Vector3 eye = {0, 0, 5};
  Vector3 ab[2] = {{0, 0, 0}, {1, 0, 0}};
  Vector3 x = {0.5, 0.5, 0};

  AdvanceSeg(eye, ab, x);

  EXPECT_TRUE(Vector3Distance(ab[0], x) < 1e-5 ||
              Vector3Distance(ab[1], x) < 1e-5);
}

TEST(AdvancePlaneTest, RandomizedPlanePointUpdate) {
  std::srand(std::time(nullptr)); // Seed for random number generation

  Vector3 p[3], q[3], eye = {0, 0, 5};
  for (int k = 0; k < 100; k++) {
    GenerateArbitraryVector3Array(p);
    for (int m = 0; m < 3; m++)
      q[m] = p[m];

    int i = std::rand() % 3; // Random index in range 0..2
    float factor =
        (std::rand() % 2) ? 0.9f : 1.1f; // Randomly choose 0.9 or 1.1
    Vector3 x = Vector3Lerp(eye, p[i], factor);

    AdvancePlane(eye, p, x);

    for (int j = 0; j < 2; j++) {
      EXPECT_GE(EyeSide(p, eye, q[j]), -1e-5);
    }
  }
}

TEST(AdvanceSegTest, RandomizedSegPointUpdate) {
  std::srand(std::time(nullptr)); // Seed for random number generation

  Vector3 eye = {0, 0, 5};
  Vector3 ab[2], ab0[2];
  for (int k = 0; k < 100; k++) {
    GenerateArbitraryVector2Array(ab);
    ab0[0] = ab[0];
    ab0[1] = ab[1];

    int i = std::rand() % 2; // Random index in range 0..1
    float factor =
        (std::rand() % 2) ? 0.9f : 1.1f; // Randomly choose 0.9 or 1.1
    Vector3 x = Vector3Lerp(eye, ab[i], factor);

    AdvanceSeg(eye, ab, x);

    EXPECT_GE(EyeSide2(ab, eye, ab0[0]), 0.f);
    EXPECT_GE(EyeSide2(ab, eye, ab0[1]), 0.f);
  }
}

TEST(AdvancePlaneTest, Hardcoded) {
  Vector3 eye = {0, 0, 5}, p[3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}},
          x = {0, 0, -1}, y = {0, 0, 1}, p0[3];

  for (int i = 0; i < 2; i++)
    p0[i] = p[i];

  AdvancePlane(eye, p, x);

  for (int i = 0; i < 2; i++) {
    EXPECT_NEAR(p0[i].x, p[i].x, 1e-5);
    EXPECT_NEAR(p0[i].y, p[i].y, 1e-5);
    EXPECT_NEAR(p0[i].z, p[i].z, 1e-5);
  }
}

TEST(GetRayCollisionPlaneTest, Hardcoded) {
  // Define the XY plane
  Vector3 plane[3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};

  // Define the ray starting at (0, 0, 1) and aiming at the origin
  Ray ray = {{0, 0, 1}, {0, 0, -1}};

  // Call GetRayCollisionPlane
  RayCollision collision = GetRayCollisionPlane(ray, plane, 3);

  // Verify the collision point is at the origin
  EXPECT_TRUE(collision.hit);
  EXPECT_NEAR(collision.point.x, 0.0f, 1e-5);
  EXPECT_NEAR(collision.point.y, 0.0f, 1e-5);
  EXPECT_NEAR(collision.point.z, 0.0f, 1e-5);
  EXPECT_NEAR(collision.distance, 1.0f, 1e-5);
}

TEST(GetRayCollisionPlaneTest, RandomizedTransform) {
  // Define the XY plane
  Vector3 plane[3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};

  // Define the ray starting at (0, 0, 1) and aiming at the origin
  Ray ray = {{0, 0, 1}, {0, 0, -1}};
  Matrix rotation = MatrixRotateXYZ(ArbitraryVector3());
  Vector3 translation = ArbitraryVector3();
  // Apply rotation to the plane points
  for (int i = 0; i < 3; i++) {
    plane[i] = translation + Vector3Transform(plane[i], rotation);
  }

  // Apply rotation to the ray origin and direction
  ray.position = translation + Vector3Transform(ray.position, rotation);
  ray.direction = Vector3Transform(ray.direction, rotation);

  // Call GetRayCollisionPlane
  RayCollision collision = GetRayCollisionPlane(ray, plane, 3);

  // Verify the collision point is at the transformed origin
  EXPECT_TRUE(collision.hit);
  EXPECT_NEAR(collision.point.x, translation.x, 1e-5);
  EXPECT_NEAR(collision.point.y, translation.y, 1e-5);
  EXPECT_NEAR(collision.point.z, translation.z, 1e-5);
}
