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
    if (u_len2 < 1e-20f) return false;  // 极端退化
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



// 包围球直接采样：在指向包围球的立体角锥内均匀采样
// 退化情况(origin在球内/半径过小)退回全球面均匀采样
inline __device__ vec3f sample_bounding_sphere(
    vec3f center, float radius, vec3f origin, Random& rng)
{
    vec3f to_center = center - origin;
    float dist2 = dot(to_center, to_center);

    // 退化：均匀全球面采样
    if (radius <= 1e-6f || dist2 <= radius * radius) {
        float z = 1.0f - 2.0f * rng();
        float r = sqrtf(fmaxf(0.0f, 1.0f - z * z));
        float phi = 2.0f * MY_PI * rng();
        return vec3f(r * cosf(phi), r * sinf(phi), z);
    }

    vec3f w = to_center * rsqrtf(dist2);
    vec3f fallback = (fabsf(w.x) > 0.9f) ? vec3f(0, 1, 0) : vec3f(1, 0, 0);
    vec3f u_raw = cross(fallback, w);
    vec3f u = u_raw * rsqrtf(dot(u_raw, u_raw));
    vec3f v = cross(w, u);

    float cos_theta_max = sqrtf(fmaxf(0.0f, 1.0f - radius * radius / dist2));
    float cos_theta = 1.0f - rng() * (1.0f - cos_theta_max);
    float sin_theta = sqrtf(fmaxf(0.0f, 1.0f - cos_theta * cos_theta));
    float phi = 2.0f * MY_PI * rng();

    vec3f local_dir(sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta);
    vec3f out_dir = u * local_dir.x + v * local_dir.y + w * local_dir.z;

    float d_len2 = dot(out_dir, out_dir);
    if (d_len2 < 1e-20f) return w;
    return out_dir * rsqrtf(d_len2);
}

// 包围球直接采样的 pdf
inline __device__ float pdf_bounding_sphere(
    vec3f center, float radius, vec3f origin, vec3f dir){
    vec3f to_center = center - origin;
    float dist2 = dot(to_center, to_center);

    if (radius <= 1e-6f || dist2 <= radius * radius) {
        return 1.0f / (4.0f * MY_PI);
    }

    float cos_theta_max = sqrtf(fmaxf(0.0f, 1.0f - radius * radius / dist2));
    float solid_angle = 2.0f * MY_PI * (1.0f - cos_theta_max);
    float cos_angle = dot(to_center * rsqrtf(dist2), dir);
    return (cos_angle > cos_theta_max - 1e-4f) ? 1.0f / solid_angle : 0.0f;
}

#endif