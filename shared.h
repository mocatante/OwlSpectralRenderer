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

struct PerRayData {
    Random random;
    float throughput;
    float emission;    // 标量辐射度
    float lambda;      // 波长
    vec3f origin;
    vec3f direction;
    bool isScattered;
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
};

struct MissProgData {};