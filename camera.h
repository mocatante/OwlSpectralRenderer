#pragma once
#include <cmath>
#include <algorithm>
#include <owl/owl.h>
#include "shared.h"
#include "toBMP.h"

class Camera {
public:
    Camera(OWLContext owl, OWLModule module,
        vec3f position, vec3f target, vec3f up,
        float focus_distance, float fov_x_deg, float ratio,
        int w, int spp, int max_depth)
        : owl(owl), module(module), width(w), samples_per_pixel(spp), maxDepth(max_depth) {

        vec3f lookAt = normalize(target - position);
        if (std::abs(dot(lookAt, up)) > 0.999f) {
            up = (std::abs(lookAt.x) < 0.9f) ? vec3f(1, 0, 0) : vec3f(0, 1, 0);
        }
        vec3f right = normalize(cross(up, lookAt));
        up = normalize(cross(lookAt, right));

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

    void render(OWLGroup world, const char* out_file) {
        OWLBuffer frameBuffer = owlHostPinnedBufferCreate(owl, OWL_INT, width * height);

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
            { nullptr }
        };
        OWLRayGen rayGen = owlRayGenCreate(owl, module, "rayGen", sizeof(RayGenData), rayGenVars, -1);

        OWLVarDecl missVars[] = { { nullptr } };
        OWLMissProg miss = owlMissProgCreate(owl, module, "miss", sizeof(MissProgData), missVars, -1);
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

        owlBuildPrograms(owl);
        owlBuildPipeline(owl);
        owlBuildSBT(owl);

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