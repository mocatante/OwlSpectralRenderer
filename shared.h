#pragma once
#include <owl/owl.h>
#include <owl/common/math/vec.h>
#include <owl/common/math/random.h>

using namespace owl;
typedef owl::common::LCG<4> Random;

// 一组三角面片的顶点与索引
struct TriangleMeshGeometry {
    vec3f* vertices;
    vec3i* indices;
};

// 漫反射材质的三角面片数据
struct DiffuseTriangleData {
    TriangleMeshGeometry mesh;
    float albedo;
};

// 发光材质的三角面片数据
struct EmissiveTriangleData {
    TriangleMeshGeometry mesh;
    float emission;
};

// 线程PRD
struct PerRayData {
    Random random;
    float throughput;
    float emission;
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