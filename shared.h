#pragma once
#include <owl/owl.h>
#include <owl/common/math/vec.h>
#include <owl/common/math/random.h>

using namespace owl;
typedef owl::common::LCG<4> Random;

struct TriangleMeshGeometry {
    vec3f* vertices;
    vec3i* indices;
};

// 漫反射材质
struct DiffuseTriangleData {
    TriangleMeshGeometry mesh;
    vec3f albedo;
};

// 黑体发光材质
struct BlackbodyTriangleData {
    TriangleMeshGeometry mesh;
    float temperature_K;
    float intensity;
};

// 玻璃材质
struct GlassTriangleData {
    TriangleMeshGeometry mesh;
    float ior;
    float dispersion;
};

// 金属材质
struct MetalTriangleData {
    TriangleMeshGeometry mesh;
    vec3f albedo;
    float fuzz;
};

// 直接采样目标描述
// 包围球 + 采样权重
// 与场景实体解耦, 可以指向真实玻璃球, 也可以是纯虚拟引导
struct DirectTargetData {
    vec3f center;
    float radius;
    float weight;
};

struct PerRayData {
    Random random;
    float throughput;
    float emission;    // 标量辐射度
    float lambda;      // 波长
    vec3f origin;
    vec3f direction;
    bool isScattered;

    // 直接采样目标（设备端指针，由 RayGenData 透传）
    DirectTargetData* targets;
    int   target_count;
    float target_total_weight;
};

struct RayGenData {
    uint32_t* fbPtr;
    vec2i fbSize;
    int samples_per_pixel;
    int max_depth;
    OptixTraversableHandle world;
    struct {
        vec3f origin;
        vec3f pixel00_center;
        vec3f u_step;
        vec3f v_step;
    } camera;

    // 直接采样目标列表（OWL_RAW_POINTER）
    DirectTargetData* targets;
    int   target_count;
    float target_total_weight;
};

struct MissProgData {};