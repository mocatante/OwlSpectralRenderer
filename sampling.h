#pragma once
#include "shared.h"

#ifdef __CUDA_ARCH__
constexpr float MY_PI = 3.14159265358979323846f;

inline __device__ vec3f random_in_hemisphere(vec3f n, Random& rng) {
    float z = 1.0f - 2.0f * rng();
    float r = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    float phi = 2.0f * MY_PI * rng();
    vec3f local_dir(r * cosf(phi), r * sinf(phi), z);

    float n_len2 = dot(n, n);
    if (n_len2 < 1e-20f) return vec3f(0, 1, 0);
    vec3f w = n * rsqrtf(n_len2);

    vec3f fallback = (fabsf(w.x) > 0.9f) ? vec3f(0, 1, 0) : vec3f(1, 0, 0);
    vec3f u_raw = cross(fallback, w);
    float u_len2 = dot(u_raw, u_raw);
    if (u_len2 < 1e-20f) return w;
    vec3f u = u_raw * rsqrtf(u_len2);
    vec3f v = cross(w, u);

    vec3f world_dir = u * local_dir.x + v * local_dir.y + w * local_dir.z;
    if (dot(world_dir, n) < 0.0f) world_dir = -world_dir;

    float d_len2 = dot(world_dir, world_dir);
    if (d_len2 < 1e-20f) return w;
    return world_dir * rsqrtf(d_len2);
}

inline __device__ bool scatter_diffuse(float albedo, const vec3f& P, const vec3f& N, PerRayData& prd) {
    float r1 = prd.random();
    float r2 = prd.random();
    float z = sqrtf(1.0f - r1);
    float phi = 2.0f * 3.14159265f * r2;
    float x = sqrtf(r1) * cosf(phi);
    float y = sqrtf(r1) * sinf(phi);

    vec3f w = N;
    vec3f fallback = (fabsf(w.x) > 0.9f) ? vec3f(0, 1, 0) : vec3f(1, 0, 0);
    vec3f u_raw = cross(fallback, w);
    float u_len2 = dot(u_raw, u_raw);
    if (u_len2 < 1e-20f) return false;  // ¼«¶ËÍË»¯
    vec3f u = u_raw * rsqrtf(u_len2);
    vec3f v = cross(w, u);
    vec3f world_dir = u * x + v * y + w * z;

    float d_len2 = dot(world_dir, world_dir);
    if (d_len2 < 1e-20f) return false;
    prd.direction = world_dir * rsqrtf(d_len2);
    prd.origin = P;
    prd.throughput *= albedo;
    return true;
}
#endif