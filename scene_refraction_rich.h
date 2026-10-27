#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <random>
#include <owl/owl.h>
#include "shared.h"
#include "mat4.h"
#include "camera.h"
#include "sphere.h"
#include "meshGAS.h"

inline void render_refraction_rich() {
    OWLContext owl = owlContextCreate(nullptr, 1);
    std::ifstream ptxFile("shaders.ptx");
    if (!ptxFile.is_open()) return;
    std::string ptxCode((std::istreambuf_iterator<char>(ptxFile)), std::istreambuf_iterator<char>());
    OWLModule module = owlModuleCreate(owl, ptxCode.c_str());

    // 使用固定种子，方便每次布局一致
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    auto rnd = [&]() { return dis(gen); };

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

    // 3. 准备几何数据 (用 32x64 的球)
    std::vector<vec3f> sphereVerts;
    std::vector<vec3i> sphereIdxs;
    generate_unit_sphere(128, 256, sphereVerts, sphereIdxs);

    // 4. 构建 GAS
    // 4.1 背景墙与地板
    std::vector<vec3f> backVerts; std::vector<vec3i> backIdx;
    generate_quad_mesh(vec3f(-15.0f, -2.0f, -12.0f), vec3f(30.0f, 0, 0), vec3f(0, 15.0f, 0), backVerts, backIdx);
    OWLGroup backWallGAS = buildTriangleMeshGAS(owl, backVerts, backIdx, diffuseType, "albedo", vec3f(0.4f, 0.4f, 0.45f));

    std::vector<vec3f> floorVerts; std::vector<vec3i> floorIdx;
    generate_quad_mesh(vec3f(-15.0f, -1.2f, -15.0f), vec3f(30.0f, 0, 0), vec3f(0, 0, 30.0f), floorVerts, floorIdx);
    OWLGroup floorGAS = buildTriangleMeshGAS(owl, floorVerts, floorIdx, diffuseType, "albedo", vec3f(0.6f, 0.6f, 0.6f));

    // 4.2 光源 (使用 0.0f 色温复用 BlackbodyGAS)
    std::vector<vec3f> topLightVerts; std::vector<vec3i> topLightIdx;
    generate_quad_mesh(vec3f(-5.0f, 6.0f, -8.0f), vec3f(10.0f, 0, 0), vec3f(0, 0, 10.0f), topLightVerts, topLightIdx);
    OWLGroup topLightGAS = buildBlackbodyGAS(owl, topLightVerts, topLightIdx, blackbodyType, 0.0f, 3.0f);

    std::vector<vec3f> stripVerts; std::vector<vec3i> stripIdx;
    float strip_w = 0.08f, strip_h = 7.0f;
    for (int i = -2; i <= 2; ++i) {
        float x = i * 1.2f;
        generate_quad_mesh(vec3f(x - strip_w * 0.5f, -1.2f, -9.0f), vec3f(strip_w, 0, 0), vec3f(0, strip_h, 0), stripVerts, stripIdx);
    }
    OWLGroup stripGAS = buildBlackbodyGAS(owl, stripVerts, stripIdx, blackbodyType, 0.0f, 8.0f);

    OWLGroup glowGAS = buildBlackbodyGAS(owl, sphereVerts, sphereIdxs, blackbodyType, 0.0f, 8.0f);

    // 4.3 其他材质
    OWLGroup glassGAS = buildGlassGAS(owl, sphereVerts, sphereIdxs, glassType, 1.5f, 0.04f);
    OWLGroup metal1GAS = buildMetalGAS(owl, sphereVerts, sphereIdxs, metalType, vec3f(1.0f, 0.5f, 0.0f), 0.05f);
    OWLGroup metal2GAS = buildMetalGAS(owl, sphereVerts, sphereIdxs, metalType, vec3f(0.9f, 0.9f, 0.9f), 0.2f);
    OWLGroup diff1GAS = buildTriangleMeshGAS(owl, sphereVerts, sphereIdxs, diffuseType, "albedo", vec3f(0.8f, 0.1f, 0.1f));
    OWLGroup diff2GAS = buildTriangleMeshGAS(owl, sphereVerts, sphereIdxs, diffuseType, "albedo", vec3f(0.1f, 0.3f, 0.9f));

    // ==================================================================
    // 组装 TAS
    // ==================================================================
    int totalInstances = 2 + 1 + 1 + 6 + 6 + 2 + 2; // 墙+地(2) + 顶光(1) + 条光(1) + 发光球(6) + 玻璃(6) + 金属(2) + 漫反(2)
    OWLGroup sceneTAS = owlInstanceGroupCreate(owl, totalInstances, nullptr);
    int idx = 0;

    Mat4 identity = Mat4::identity();
    // 墙与地板
    owlInstanceGroupSetChild(sceneTAS, idx, backWallGAS); owlInstanceGroupSetTransform(sceneTAS, idx++, identity.m, OWL_MATRIX_FORMAT_OWL);
    owlInstanceGroupSetChild(sceneTAS, idx, floorGAS);    owlInstanceGroupSetTransform(sceneTAS, idx++, identity.m, OWL_MATRIX_FORMAT_OWL);
    // 顶光
    owlInstanceGroupSetChild(sceneTAS, idx, topLightGAS); owlInstanceGroupSetTransform(sceneTAS, idx++, identity.m, OWL_MATRIX_FORMAT_OWL);
    // 背景条（合并为1个GAS，所以只用1个Instance）
    owlInstanceGroupSetChild(sceneTAS, idx, stripGAS);    owlInstanceGroupSetTransform(sceneTAS, idx++, identity.m, OWL_MATRIX_FORMAT_OWL);

    // 发光小球（随机位置）
    for (int i = 0; i < 6; ++i) {
        float x = rnd() * 12.0f - 6.0f; // [-6, 6]
        float y = rnd() * 5.0f - 1.0f;  // [-1, 4]
        float r = rnd() * 0.15f + 0.1f; // [0.1, 0.25]
        Mat4 xf = Mat4::translate(vec3f(x, y, -10.0f)) * Mat4::scale(vec3f(r));
        owlInstanceGroupSetChild(sceneTAS, idx, glowGAS);
        owlInstanceGroupSetTransform(sceneTAS, idx++, xf.m, OWL_MATRIX_FORMAT_OWL);
    }

    // 玻璃球 (6个)
    vec3f g_pos[] = { {-2.5f, 0.2f, -3.0f}, {0.0f, 0.8f, -5.0f}, {2.0f, -0.2f, -2.5f}, {-0.8f, -0.5f, -2.0f}, {1.2f, 1.2f, -4.0f}, {-1.8f, 1.0f, -6.0f} };
    float g_r[] = { 1.0f, 0.8f, 0.6f, 0.5f, 0.4f, 0.7f };
    for (int i = 0; i < 6; ++i) {
        Mat4 xf = Mat4::translate(g_pos[i]) * Mat4::scale(vec3f(g_r[i]));
        owlInstanceGroupSetChild(sceneTAS, idx, glassGAS);
        owlInstanceGroupSetTransform(sceneTAS, idx++, xf.m, OWL_MATRIX_FORMAT_OWL);
    }

    // 金属球
    Mat4 m1xf = Mat4::translate(vec3f(-4.0f, -0.7f, -4.0f)) * Mat4::scale(vec3f(0.5f));
    owlInstanceGroupSetChild(sceneTAS, idx, metal1GAS); owlInstanceGroupSetTransform(sceneTAS, idx++, m1xf.m, OWL_MATRIX_FORMAT_OWL);
    Mat4 m2xf = Mat4::translate(vec3f(4.0f, -0.7f, -3.0f)) * Mat4::scale(vec3f(0.5f));
    owlInstanceGroupSetChild(sceneTAS, idx, metal2GAS); owlInstanceGroupSetTransform(sceneTAS, idx++, m2xf.m, OWL_MATRIX_FORMAT_OWL);

    // 漫反射球
    Mat4 d1xf = Mat4::translate(vec3f(-3.0f, -0.9f, -1.0f)) * Mat4::scale(vec3f(0.3f));
    owlInstanceGroupSetChild(sceneTAS, idx, diff1GAS); owlInstanceGroupSetTransform(sceneTAS, idx++, d1xf.m, OWL_MATRIX_FORMAT_OWL);
    Mat4 d2xf = Mat4::translate(vec3f(3.5f, -0.9f, -1.5f)) * Mat4::scale(vec3f(0.3f));
    owlInstanceGroupSetChild(sceneTAS, idx, diff2GAS); owlInstanceGroupSetTransform(sceneTAS, idx++, d2xf.m, OWL_MATRIX_FORMAT_OWL);

    owlGroupBuildAccel(sceneTAS);
    std::cout << "[refraction_rich] TAS instances: " << idx << " / " << totalInstances << "\n";

    // 5. 相机与渲染
    vec3f cam_pos(0.0f, 0.5f, 2.5f);
    vec3f cam_tgt(0.0f, 0.2f, -4.0f);

    Camera cam(owl, module,
        cam_pos,
        cam_tgt,
        vec3f(0, 1, 0),
        1.0f, 50.0f, 16.0f / 9.0f,
        1260, 1024, 20
    );

    cam.render(sceneTAS, "scene_refraction_rich.bmp");

    owlModuleRelease(module);
    owlContextDestroy(owl);
}