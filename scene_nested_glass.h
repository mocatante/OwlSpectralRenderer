#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <random>
#include <owl/owl.h>
#include "shared.h"
#include "mat4.h"
#include "camera.h"
#include "sphere.h"   // 引入球和四边形的生成
#include "meshGAS.h"

inline void render_nested_glass() {
    OWLContext owl = owlContextCreate(nullptr, 1);
    std::ifstream ptxFile("shaders.ptx");
    if (!ptxFile.is_open()) return;
    std::string ptxCode((std::istreambuf_iterator<char>(ptxFile)), std::istreambuf_iterator<char>());
    OWLModule module = owlModuleCreate(owl, ptxCode.c_str());

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    auto rnd = [&]() { return dis(gen); };

    const float R_HALF = 2.5f;
    const float R_H = 5.0f;

    // 1. 材质声明
    OWLVarDecl diffuseVars[] = {
        { "mesh.vertices", OWL_BUFPTR, OWL_OFFSETOF(DiffuseTriangleData, mesh.vertices) },
        { "mesh.indices",  OWL_BUFPTR, OWL_OFFSETOF(DiffuseTriangleData, mesh.indices) },
        { "albedo",        OWL_FLOAT3, OWL_OFFSETOF(DiffuseTriangleData, albedo) },
        { nullptr }
    };
    OWLVarDecl blackbodyVars[] = {
        { "mesh.vertices", OWL_BUFPTR, OWL_OFFSETOF(BlackbodyTriangleData, mesh.vertices) },
        { "mesh.indices",  OWL_BUFPTR, OWL_OFFSETOF(BlackbodyTriangleData, mesh.indices) },
        { "temperature",   OWL_FLOAT,  OWL_OFFSETOF(BlackbodyTriangleData, temperature_K) },
        { "intensity",     OWL_FLOAT,  OWL_OFFSETOF(BlackbodyTriangleData, intensity) },
        { nullptr }
    };
    OWLVarDecl glassVars[] = {
        { "mesh.vertices", OWL_BUFPTR, OWL_OFFSETOF(GlassTriangleData, mesh.vertices) },
        { "mesh.indices",  OWL_BUFPTR, OWL_OFFSETOF(GlassTriangleData, mesh.indices) },
        { "ior",           OWL_FLOAT,  OWL_OFFSETOF(GlassTriangleData, ior) },
        { "dispersion",    OWL_FLOAT,  OWL_OFFSETOF(GlassTriangleData, dispersion) },
        { nullptr }
    };
    OWLVarDecl metalVars[] = {
        { "mesh.vertices", OWL_BUFPTR, OWL_OFFSETOF(MetalTriangleData, mesh.vertices) },
        { "mesh.indices",  OWL_BUFPTR, OWL_OFFSETOF(MetalTriangleData, mesh.indices) },
        { "albedo",        OWL_FLOAT3, OWL_OFFSETOF(MetalTriangleData, albedo) },
        { "fuzz",          OWL_FLOAT,  OWL_OFFSETOF(MetalTriangleData, fuzz) },
        { nullptr }
    };

    // 2. 注册几何类型
    OWLGeomType diffuseType = owlGeomTypeCreate(owl, OWL_TRIANGLES, sizeof(DiffuseTriangleData), diffuseVars, 3);
    owlGeomTypeSetClosestHit(diffuseType, 0, module, "DiffuseHit");

    OWLGeomType blackbodyType = owlGeomTypeCreate(owl, OWL_TRIANGLES, sizeof(BlackbodyTriangleData), blackbodyVars, 4);
    owlGeomTypeSetClosestHit(blackbodyType, 0, module, "BlackbodyHit");

    OWLGeomType glassType = owlGeomTypeCreate(owl, OWL_TRIANGLES, sizeof(GlassTriangleData), glassVars, 4);
    owlGeomTypeSetClosestHit(glassType, 0, module, "GlassHit");

    OWLGeomType metalType = owlGeomTypeCreate(owl, OWL_TRIANGLES, sizeof(MetalTriangleData), metalVars, 4);
    owlGeomTypeSetClosestHit(metalType, 0, module, "MetalHit");

    // 3. 准备几何数据
    std::vector<vec3f> sphereVerts;
    std::vector<vec3i> sphereIdxs;
    generate_unit_sphere(128, 256, sphereVerts, sphereIdxs);

    std::vector<vec3f> floorVerts, wallVerts, ceilVerts;
    std::vector<vec3i> floorIdx, wallIdx, ceilIdx;

    // 直接调用 sphere.h 里的 generate_quad_mesh
    generate_quad_mesh(vec3f(-R_HALF, 0, -R_HALF), vec3f(2 * R_HALF, 0, 0), vec3f(0, 0, 2 * R_HALF), floorVerts, floorIdx);
    generate_quad_mesh(vec3f(-R_HALF, R_H, -R_HALF), vec3f(2 * R_HALF, 0, 0), vec3f(0, 0, 2 * R_HALF), ceilVerts, ceilIdx);
    generate_quad_mesh(vec3f(-R_HALF, 0, -R_HALF), vec3f(0, R_H, 0), vec3f(0, 0, 2 * R_HALF), wallVerts, wallIdx);
    generate_quad_mesh(vec3f(R_HALF, 0, -R_HALF), vec3f(0, 0, 2 * R_HALF), vec3f(0, R_H, 0), wallVerts, wallIdx);
    generate_quad_mesh(vec3f(-R_HALF, 0, -R_HALF), vec3f(2 * R_HALF, 0, 0), vec3f(0, R_H, 0), wallVerts, wallIdx);

    // 4. 构建 GAS
    OWLGroup floorGAS = buildTriangleMeshGAS(owl, floorVerts, floorIdx, diffuseType, "albedo", vec3f(0.8f));
    OWLGroup ceilGAS = buildTriangleMeshGAS(owl, ceilVerts, ceilIdx, diffuseType, "albedo", vec3f(0.6f));
    OWLGroup wallGAS = buildTriangleMeshGAS(owl, wallVerts, wallIdx, diffuseType, "albedo", vec3f(0.45f));

    OWLGroup innerGlassGAS = buildGlassGAS(owl, sphereVerts, sphereIdxs, glassType, 1.9f, 0.045f);
    OWLGroup outerGlassGAS = buildGlassGAS(owl, sphereVerts, sphereIdxs, glassType, 1.9f, 0.045f);

    OWLGroup lightGAS = buildBlackbodyGAS(owl, sphereVerts, sphereIdxs, blackbodyType, 5778.0f, 10.0f);

    // ==================================================================
    // 场景装配：先收集所有 instance，再按实际数量建 TAS
    // ==================================================================

    vec3f center(0.6f, 0.4f, -0.6f);

    // 5. 收集 10 个额外随机小球（可能放不满 10 个，全部先收集好）
    std::vector<vec3f> placed_centers;
    std::vector<float> placed_radii;

    struct PendingBall { OWLGroup gas; Mat4 xf; };
    std::vector<PendingBall> extra_balls;

    for (int n = 0; n < 10; ++n) {
        const float radius = 0.36f;
        vec3f p(0, 0, 0);
        bool placed = false;

        for (int attempt = 0; attempt < 64 && !placed; ++attempt) {
            float x = rnd() * (R_HALF - radius - 0.05f - (-R_HALF + radius + 0.05f)) + (-R_HALF + radius + 0.05f);
            float z = rnd() * (R_HALF - radius - 0.05f - (-R_HALF + radius + 0.05f)) + (-R_HALF + radius + 0.05f);
            p = vec3f(x, radius + 1e-3f, z);

            // 避开外层玻璃球
            if (length(p - center) < 0.4f + radius + 0.05f) continue;

            // 避免与已放置的小球重叠
            bool ok = true;
            for (size_t k = 0; k < placed_centers.size(); ++k) {
                if (length(p - placed_centers[k]) < radius + placed_radii[k] + 0.02f) {
                    ok = false; break;
                }
            }
            if (ok) placed = true;
        }
        if (!placed) continue;

        placed_centers.push_back(p);
        placed_radii.push_back(radius);

        float t = rnd();
        OWLGroup ballGAS;
        if (t < 0.4f) {
            vec3f c(rnd() * 0.8f + 0.15f, rnd() * 0.8f + 0.15f, rnd() * 0.8f + 0.15f);
            ballGAS = buildTriangleMeshGAS(owl, sphereVerts, sphereIdxs, diffuseType, "albedo", c);
        }
        else if (t < 0.7f) {
            vec3f c(rnd() * 0.65f + 0.3f, rnd() * 0.65f + 0.3f, rnd() * 0.65f + 0.3f);
            ballGAS = buildMetalGAS(owl, sphereVerts, sphereIdxs, metalType, c, rnd() * 0.3f);
        }
        else {
            ballGAS = buildGlassGAS(owl, sphereVerts, sphereIdxs, glassType, rnd() * 0.6f + 1.4f, rnd() * 0.02f + 0.04f);
        }

        Mat4 xf = Mat4::translate(p) * Mat4::scale(vec3f(radius));
        extra_balls.push_back({ ballGAS, xf });
    }

    // 6. 建 TAS：数量 = 5 墙 + 2 玻璃 + 25 光球 + extra_balls.size()
    int actualTotal = 5 + 2 + 25 + (int)extra_balls.size();
    OWLGroup sceneTAS = owlInstanceGroupCreate(owl, actualTotal, nullptr);
    int idx = 0;

    // 6.1 五面墙（世界坐标，identity 变换）
    owlInstanceGroupSetChild(sceneTAS, idx, floorGAS); ++idx;
    owlInstanceGroupSetChild(sceneTAS, idx, ceilGAS);  ++idx;
    owlInstanceGroupSetChild(sceneTAS, idx, wallGAS);  ++idx;
    owlInstanceGroupSetChild(sceneTAS, idx, wallGAS);  ++idx;
    owlInstanceGroupSetChild(sceneTAS, idx, wallGAS);  ++idx;

    Mat4 identity = Mat4::identity();
    for (int i = 0; i < 5; ++i)
        owlInstanceGroupSetTransform(sceneTAS, i, identity.m, OWL_MATRIX_FORMAT_OWL);

    // 6.2 两层嵌套玻璃（同心球）
    Mat4 innerXF = Mat4::translate(center) * Mat4::scale(vec3f(0.3f));
    owlInstanceGroupSetChild(sceneTAS, idx, innerGlassGAS);
    owlInstanceGroupSetTransform(sceneTAS, idx++, innerXF.m, OWL_MATRIX_FORMAT_OWL);

    Mat4 outerXF = Mat4::translate(center) * Mat4::scale(vec3f(0.4f));
    owlInstanceGroupSetChild(sceneTAS, idx, outerGlassGAS);
    owlInstanceGroupSetTransform(sceneTAS, idx++, outerXF.m, OWL_MATRIX_FORMAT_OWL);

    // 6.3 25 个发光小球（在玻璃球内部）
    for (int i = 0; i < 25; ++i) {
        float r = rnd() * (0.03f - 0.015f) + 0.015f;
        float z = rnd() * 2.0f - 1.0f;
        float phi = rnd() * 2.0f * M_PI;
        float rxy = sqrtf(1.0f - z * z);
        vec3f dir(rxy * cosf(phi), z, rxy * sinf(phi));

        float dist = rnd() * (0.3f - r - 0.02f - (0.1f + r + 0.02f)) + (0.1f + r + 0.02f);
        vec3f p = center + dir * dist;

        Mat4 xf = Mat4::translate(p) * Mat4::scale(vec3f(r));
        owlInstanceGroupSetChild(sceneTAS, idx, lightGAS);
        owlInstanceGroupSetTransform(sceneTAS, idx++, xf.m, OWL_MATRIX_FORMAT_OWL);
    }

    // 6.4 额外随机小球
    for (const auto& b : extra_balls) {
        owlInstanceGroupSetChild(sceneTAS, idx, b.gas);
        owlInstanceGroupSetTransform(sceneTAS, idx++, b.xf.m, OWL_MATRIX_FORMAT_OWL);
    }

    owlGroupBuildAccel(sceneTAS);

    // 断言：所有槽位都应该被填满
    std::cout << "[nested_glass] TAS instances: " << idx << " / " << actualTotal << "\n";

    // 7. 相机与渲染
    vec3f cam_pos(0.0f, 2.5f, 5.0f);
    vec3f cam_tgt(0.0f, 1.5f, 0.0f);

    Camera cam(owl, module,
        cam_pos,
        cam_tgt,
        vec3f(0, 1, 0),
        owl::common::length(cam_pos - cam_tgt),
        55.0f, 1.0f, 1024, 1024, 10
    );

    cam.render(sceneTAS, "scene_nested_glass.bmp");

    owlModuleRelease(module);
    owlContextDestroy(owl);
}