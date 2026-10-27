#include <optix_device.h>
#include "shared.h"
#include "sampling.h"

using namespace owl;

// 材质相关的 PGs
inline __device__ vec3f get_triangle_normal(const TriangleMeshGeometry& mesh, int primID, const vec3f& rayDir) {
    const vec3i idx = mesh.indices[primID];
    const vec3f A = mesh.vertices[idx.x];
    const vec3f B = mesh.vertices[idx.y];
    const vec3f C = mesh.vertices[idx.z];
    vec3f N = normalize(cross(B - A, C - A));
    if (dot(N, rayDir) > 0.0f) N = -N;
    return N;
}
inline __device__ vec3f get_triangle_hitP() {
    return vec3f(optixGetWorldRayOrigin()) + vec3f(optixGetWorldRayDirection()) * optixGetRayTmax();
}

// CH, 漫反射
OPTIX_CLOSEST_HIT_PROGRAM(DiffuseHit)() {
    const int primID = optixGetPrimitiveIndex();
    const DiffuseTriangleData& self = owl::getProgramData<DiffuseTriangleData>();

    vec3f dir = vec3f(optixGetWorldRayDirection());
    vec3f P = get_triangle_hitP();
    vec3f N = get_triangle_normal(self.mesh, primID, dir);

    PerRayData& prd = owl::getPRD<PerRayData>();
    prd.emission = 0.0f;
    prd.isScattered = scatter_diffuse(self.albedo, P, N, prd);
}

// CH, 纯自发光
OPTIX_CLOSEST_HIT_PROGRAM(EmissiveHit)() {
    const EmissiveTriangleData& self = owl::getProgramData<EmissiveTriangleData>();

    PerRayData& prd = owl::getPRD<PerRayData>();
    prd.emission = self.emission;
    prd.isScattered = false;
}

// 相机相关PGs
// 
// RG
OPTIX_RAYGEN_PROGRAM(rayGen)() {
    const RayGenData& self = owl::getProgramData<RayGenData>();
    const vec2i pixelID = owl::getLaunchIndex();
    const int pixelIdx = pixelID.x + self.fbSize.x * pixelID.y;

    // 波长辐射度
    float final_radiance = 0.0f;

    for (int s = 0; s < self.samples_per_pixel; s++) {
        PerRayData prd;
        prd.throughput = 1.0f;
        prd.emission = 0.0f;
        prd.random.init(pixelID.x * 1973 + s, pixelID.y * 9277 + s);

        // float lambda = 380.0f + prd.random() * 400.0f; 

        float u_offset = prd.random() - 0.5f;
        float v_offset = prd.random() - 0.5f;

        vec3f sample_point = self.camera.pixel00_center
            + self.camera.u_step * (pixelID.x + u_offset)
            + self.camera.v_step * (pixelID.y + v_offset);

        vec3f direction = normalize(sample_point - self.camera.origin);

        owl::Ray ray;
        ray.origin = self.camera.origin;
        ray.direction = direction;
        ray.tmin = 1e-3f;
        ray.tmax = 1e10f;

        float path_radiance = 0.0f;

        for (int depth = 0; depth < self.max_depth; depth++) {
            // BVH->SBT->PGs
            owl::traceRay(self.world, ray, prd);
            path_radiance += prd.throughput * prd.emission;
            if (!prd.isScattered) break;
            ray.origin = prd.origin;
            ray.direction = prd.direction;
        }
        final_radiance += path_radiance;
    }

    final_radiance /= (float)self.samples_per_pixel;
    final_radiance = fminf(fmaxf(final_radiance, 0.0f), 1.0f);
    vec3f rgb_output = vec3f(final_radiance, final_radiance, final_radiance);
    self.fbPtr[pixelIdx] = owl::make_rgba(rgb_output);
}
// MS
OPTIX_MISS_PROGRAM(miss)() {
    PerRayData& prd = owl::getPRD<PerRayData>();
    prd.isScattered = false;
    prd.emission = 0.0f;
}