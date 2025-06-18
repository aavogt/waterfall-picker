#include "main.h"

// Plucker coordinate
inline float RayVector3Distance(Ray r, Vector3 x) {
  return Vector3Length(Vector3CrossProduct(r.direction, x - r.position));
}

// z component of the cross product, + if a-b-c makes a left turn
// https://en.wikipedia.org/wiki/Graham_scan
inline float Turn(Vector2 a, Vector2 b, Vector2 c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

inline RayCollision GetRayCollisionPlane(Ray ray, Vector3 *p, int np) {
  if (np == 3) {
    Vector3 n = Vector3CrossProduct(p[2] - p[0], p[1] - p[0]);

    float den = Vector3DotProduct(n, ray.direction);
    float t = (Vector3DotProduct(ray.direction, p[0]) -
               Vector3DotProduct(n, ray.position)) /
              den;

    return (RayCollision){.hit = den != 0,
                          .distance = t,
                          .point = ray.position + ray.direction * t};
  } else if (np == 2) {
    // return the point on the line
    // Vector3 n = Vector3CrossProduct(p[0] - ray.position, p[1] -
    // ray.position);

    // Vector3 d = Vector3Reject(ray.direction, n);
    //  d*t + ray.position = s( p[1]-p[0]) + p[0]
    //  3 equations, 2 unknowns, but they should be redundant
    //  so I should repeat the UV coordinates to get a 2 equation, 2 unknown
    //  option? Or just do
    auto t = Vector3Length(Vector3Reject(ray.direction, p[1] - p[0]));

    auto x = ray.direction * t + ray.position;
    return (RayCollision){.hit = true, .distance = t, .point = x};
  } else {
    return (RayCollision){.hit = false, .distance = 0, .point = {}};
  }
}

// AdvancePlane(eye, plane, x) tries to moves the plane towards eye.
inline void AdvancePlane(Vector3 eye, Vector3 p[3], Vector3 x) {
  Vector3 n = Vector3Normalize(Vector3CrossProduct(p[2] - p[0], p[1] - p[0]));
  if (Vector3DotProduct(eye - p[0], n) < 0)
    n *= -1;

  auto hit = GetRayCollisionPlane((Ray){.position = eye, .direction = n}, p, 3);

  if (!hit.hit)
    return;

  Vector3 xp = x + n * hit.distance;

  // replace the point closest
  float d_min = INFINITY;
  float d_minp = INFINITY;
  int i_min = 0;
  for (int i = 0; i < 3; i++) {
    float d = Vector3DistanceSqr(p[i], xp);
    float d2 = Vector3DistanceSqr(p[i], eye);
    if (d < d_min) {
      d_min = d;
      i_min = i;
      if (d2 > d)
        d_minp = d2;
    }
  }
  if (d_minp < d_min)
    p[i_min] = x;
}

// AdvanceSeg(eye, ab, x) tries to move the line segment towards eye.
inline void AdvanceSeg(Vector3 eye, Vector3 ab[2], Vector3 x) {
  Vector3 n = Vector3CrossProduct(ab[1] - eye, ab[0] - eye);
  Vector3 xp0 = Vector3Reject(x - ab[0], n);

  // UV corrdinates of xp in the plane
  // U is the a-b direction
  // V is towards eye
  // can likely be done in fewer operations
  float u = Vector3Length(Vector3Project(xp0, eye - ab[0]));
  float v = Vector3Length(Vector3Project(xp0, ab[1] - ab[0]));

  float dab = Vector3Distance(ab[1], ab[0]);

  if (u < 0)
    return;

  if (v < dab / 2) {
    ab[0] = x;
  } else {
    ab[1] = x;
  }
}
