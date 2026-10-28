#pragma once
#include <cmath>
#include <algorithm>
#include <owl/owl.h>
#include "shared.h"
#include "toBMP.h"

class Camera {
public:
    // 相机构造
    Camera(OWLContext owl, OWLModule module,
        vec3f position, vec3f target, vec3f up,
        float focus_distance, float fov_x_deg, float ratio,
        int w, int spp, int max_depth)
        : owl(owl), module(module), width(w), samples_per_pixel(spp), maxDepth(max_depth) {

        vec3f lookAt = normalize(target - position);
        if (std::abs(dot(lookAt, up)) > 0.999f) {
            up = (std::abs(lookAt.x) < 0.9f) ? vec3f(1, 0, 0) : vec3f(0, 1, 0);
        }
        vec3f right = normalize(cross(lookAt, up));
        up = normalize(cross(right, lookAt));

        float fov_x_rad = fov_x_deg * M_PI / 180.0f;
        float viewport_width = 2.0f * tanf(fov_x_rad * 0.5f) * focus_distance;
        float pixel_size = viewport_width / static_cast<float>(width);

        height = std::max(1, static_cast<int>(lroundf((viewport_width / ratio) / pixel_size)));
        float viewport_height = pixel_size * static_cast<float>(height);

        cam_data.origin = position;
        cam_data.u_step = right * pixel_size;
        cam_data.v_step = up * pixel_size;

        vec3f view_center = position + lookAt * focus_distance;
        vec3f viewport_bottom_left =
            view_center - right * (viewport_width * 0.5f) - up * (viewport_height * 0.5f);

        cam_data.pixel00_center = viewport_bottom_left
            + right * (pixel_size * 0.5f)
            + up * (pixel_size * 0.5f);
    }

    void render(OWLGroup world,
        OWLBuffer targetBuffer,
        int   targetCount,
        float targetTotalWeight,
        const char* out_file) {
        OWLBuffer frameBuffer = owlHostPinnedBufferCreate(owl, OWL_INT, width * height);

        // RG参数
        OWLVarDecl rayGenVars[] = {
            { "fbPtr",         OWL_BUFPTR, OWL_OFFSETOF(RayGenData, fbPtr)},
            { "fbSize",        OWL_INT2,   OWL_OFFSETOF(RayGenData, fbSize)},
            { "spp",           OWL_INT,    OWL_OFFSETOF(RayGenData, samples_per_pixel)},
            { "maxDepth",      OWL_INT,    OWL_OFFSETOF(RayGenData, max_depth)},
            { "world",         OWL_GROUP,  OWL_OFFSETOF(RayGenData, world)},
            { "camera.org",    OWL_FLOAT3, OWL_OFFSETOF(RayGenData, camera.origin)},
            { "camera.p00",    OWL_FLOAT3, OWL_OFFSETOF(RayGenData, camera.pixel00_center)},
            { "camera.ustep",  OWL_FLOAT3, OWL_OFFSETOF(RayGenData, camera.u_step)},
            { "camera.vstep",  OWL_FLOAT3, OWL_OFFSETOF(RayGenData, camera.v_step)},
            // 直接采样目标, raw pointer 透传
            { "targets",           OWL_RAW_POINTER, OWL_OFFSETOF(RayGenData, targets)},
            { "targetCount",       OWL_INT,         OWL_OFFSETOF(RayGenData, target_count)},
            { "targetTotalWeight", OWL_FLOAT,       OWL_OFFSETOF(RayGenData, target_total_weight)},
            { nullptr }
        };
        // 绑定RG
        OWLRayGen rayGen = owlRayGenCreate(owl, module, "rayGen", sizeof(RayGenData), rayGenVars, -1);
        // 绑定MS
        OWLVarDecl missVars[] = { { nullptr } };
        OWLMissProg miss = owlMissProgCreate(owl, module, "miss", sizeof(MissProgData), missVars, -1);
        // MS需要设置槽位
        owlMissProgSet(owl, 0, miss);

        owlRayGenSetBuffer(rayGen, "fbPtr", frameBuffer);
        owlRayGenSet2i(rayGen, "fbSize", (const owl2i&)vec2i(width, height));
        owlRayGenSet1i(rayGen, "spp", samples_per_pixel);
        owlRayGenSet1i(rayGen, "maxDepth", maxDepth);
        owlRayGenSetGroup(rayGen, "world", world);
        owlRayGenSet3f(rayGen, "camera.org", (const owl3f&)cam_data.origin);
        owlRayGenSet3f(rayGen, "camera.p00", (const owl3f&)cam_data.pixel00_center);
        owlRayGenSet3f(rayGen, "camera.ustep", (const owl3f&)cam_data.u_step);
        owlRayGenSet3f(rayGen, "camera.vstep", (const owl3f&)cam_data.v_step);

        // 直接采样目标, 允许传 nullptr
        owlRayGenSetPointer(rayGen, "targets",
            targetBuffer ? owlBufferGetPointer(targetBuffer, 0) : nullptr);
        owlRayGenSet1i(rayGen, "targetCount", targetCount);
        owlRayGenSet1f(rayGen, "targetTotalWeight", targetTotalWeight);

        // 逻辑上, 此时场景已构建完毕, 即注册了材质 PGs
        // 因此, 这是一次最终构建PGs, 把所有 PG 链接成完整的硬件状态机
        owlBuildPrograms(owl);
        // 编译Pipeline, 构建底层状态机
        owlBuildPipeline(owl);
        // 最终提交SBT
        owlBuildSBT(owl);

        // 入口, owlRayGenLaunch2D->RG
        owlRayGenLaunch2D(rayGen, width, height);

        const uint32_t* fb = (const uint32_t*)owlBufferGetPointer(frameBuffer, 0);
        saveBMP(out_file, fb, width, height);

        owlRayGenRelease(rayGen);
        owlBufferRelease(frameBuffer);
    }

private:
    OWLContext owl;
    OWLModule module;
    int width, height;
    int samples_per_pixel;
    int maxDepth;
    struct {
        vec3f origin;
        vec3f pixel00_center;
        vec3f u_step;
        vec3f v_step;
    } cam_data;
};