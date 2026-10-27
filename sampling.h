#pragma once
#include "shared.h"

#ifdef __CUDA_ARCH__
constexpr float MY_PI = 3.14159265358979323846f;

inline __device__ vec3f random_in_hemisphere(vec3f n, Random& rng) {
    float z = 1.0f - 2.0f * rng();
    float r = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    float phi = 2.0f * MY_PI * rng();
    vec3f local_dir(r * cosf(phi), r * sinf(phi), z);

    vec3f w = normalize(n);
    vec3f u = normalize(cross(fabs(w.x) > 0.9f ? vec3f(0, 1, 0) : vec3f(1, 0, 0), w));
    vec3f v = cross(w, u);

    vec3f world_dir = u * local_dir.x + v * local_dir.y + w * local_dir.z;
    if (dot(world_dir, n) < 0.0f) world_dir = -world_dir;
    return normalize(world_dir);
}


inline __device__ bool scatter_diffuse(float albedo, const vec3f& P, const vec3f& N, PerRayData& prd) {
    prd.direction = random_in_hemisphere(N, prd.random);
    prd.origin = P;
    prd.throughput *= albedo * 2.0f * MY_PI;
    return true;
}
#endif