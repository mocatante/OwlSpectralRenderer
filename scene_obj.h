#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <random>
#include <chrono> // 计时器头文件
#include <owl/owl.h>
#include "shared.h"
#include "mat4.h"
#include "camera.h"
#include "sphere.h"
#include "meshGAS.h"

inline void render_obj() {
    auto t_start = std::chrono::high_resolution_clock::now();

    OWLContext owl = owlContextCreate(nullptr, 1);

    auto t_ctx = std::chrono::high_resolution_clock::now();
    std::cerr << "[TIME] 0. OWLContext 初始化耗时: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t_ctx - t_start).count() << " ms\n";

    std::ifstream ptxFile("shaders.ptx");
    if (!ptxFile.is_open()) return;
    std::string ptxCode((std::istreambuf_iterator<char>(ptxFile)), std::istreambuf_iterator<char>());
    OWLModule module = owlModuleCreate(owl, ptxCode.c_str());

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

    // 3. 准备球体几何
    std::vector<vec3f> sphereVerts;
    std::vector<vec3i> sphereIdxs;
    generate_unit_sphere(64, 128, sphereVerts, sphereIdxs);

    auto t_load_start = std::chrono::high_resolution_clock::now();

    // 4. 加载 OBJ (设定目标对角线长度为 3.0)
    std::string obj_path = "D:/object.obj";
    float target_extent = 3.0f;
    Mat4 obj_xf;
    float scaled_extent = target_extent;

    OWLGroup objGAS = buildObjGAS(owl, obj_path, glassType, 1.5f, 0.05f, target_extent, obj_xf, scaled_extent);
    if (!objGAS) {
        std::cerr << "FATAL: Failed to load OBJ: " << obj_path << "\n";
        owlModuleRelease(module);
        owlContextDestroy(owl);
        return;
    }

    auto t_load_end = std::chrono::high_resolution_clock::now();
    std::cerr << "[TIME] 1. 解析 OBJ + 构建 BVH 耗时: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t_load_end - t_load_start).count() << " ms\n";

    // 5. 计算场景尺寸 (模型中心在原点，scaled_extent 为对角线长)
    float ground_y = -scaled_extent * 0.5f - scaled_extent * 0.02f;
    float scene_size = scaled_extent * 20.0f;
    float wall_z = -scaled_extent * 2.0f;

    // 6. 构建静态场景 GAS
    // 地板
    std::vector<vec3f> floorVerts; std::vector<vec3i> floorIdx;
    generate_quad_mesh(vec3f(-scene_size * 0.5f, ground_y, -scene_size * 0.5f),
        vec3f(scene_size, 0, 0), vec3f(0, 0, scene_size), floorVerts, floorIdx);
    OWLGroup floorGAS = buildTriangleMeshGAS(owl, floorVerts, floorIdx, diffuseType, "albedo", vec3f(0.15f, 0.15f, 0.18f));

    // 后墙
    std::vector<vec3f> wallVerts; std::vector<vec3i> wallIdx;
    generate_quad_mesh(vec3f(-scene_size * 0.5f, ground_y, wall_z),
        vec3f(scene_size, 0, 0), vec3f(0, scene_size, 0), wallVerts, wallIdx);
    OWLGroup wallGAS = buildTriangleMeshGAS(owl, wallVerts, wallIdx, diffuseType, "albedo", vec3f(0.05f, 0.05f, 0.08f));

    // 顶部大光源
    std::vector<vec3f> topLightVerts; std::vector<vec3i> topLightIdx;
    generate_quad_mesh(vec3f(-scaled_extent * 2.0f, scaled_extent * 3.0f, -scaled_extent * 1.0f),
        vec3f(scaled_extent * 4.0f, 0, 0), vec3f(0, 0, scaled_extent * 4.0f), topLightVerts, topLightIdx);
    OWLGroup topLightGAS = buildBlackbodyGAS(owl, topLightVerts, topLightIdx, blackbodyType, 4500.0f, 3.0f);

    // 背景条状光源 (10条)
    std::vector<vec3f> stripVerts; std::vector<vec3i> stripIdx;
    float strip_w = scaled_extent * 0.02f, strip_h = scaled_extent * 6.0f;
    for (int i = -5; i <= 4; ++i) {
        float x = i * scaled_extent * 0.4f;
        generate_quad_mesh(vec3f(x - strip_w * 0.5f, ground_y, wall_z + scaled_extent * 0.1f),
            vec3f(strip_w, 0, 0), vec3f(0, strip_h, 0), stripVerts, stripIdx);
    }
    OWLGroup stripGAS = buildBlackbodyGAS(owl, stripVerts, stripIdx, blackbodyType, 6500.0f, 70.0f);

    // 发光小球 (20个)
    OWLGroup glowGAS = buildBlackbodyGAS(owl, sphereVerts, sphereIdxs, blackbodyType, 3000.0f, 12.0f);

    // 漂浮玻璃球
    OWLGroup floatGlassGAS = buildGlassGAS(owl, sphereVerts, sphereIdxs, glassType, 1.5f, 0.05f);

    // ==================================================================
    // 组装 TAS
    // ==================================================================
    int totalInstances = 1 + 1 + 1 + 1 + 1 + 20 + 4;
    OWLGroup sceneTAS = owlInstanceGroupCreate(owl, totalInstances, nullptr);
    int idx = 0;
    Mat4 identity = Mat4::identity();

    // OBJ (使用 out_xf 居中并缩放)
    owlInstanceGroupSetChild(sceneTAS, idx, objGAS);
    owlInstanceGroupSetTransform(sceneTAS, idx++, obj_xf.m, OWL_MATRIX_FORMAT_OWL);

    // 地板、墙、顶光、条光
    owlInstanceGroupSetChild(sceneTAS, idx, floorGAS); owlInstanceGroupSetTransform(sceneTAS, idx++, identity.m, OWL_MATRIX_FORMAT_OWL);
    owlInstanceGroupSetChild(sceneTAS, idx, wallGAS); owlInstanceGroupSetTransform(sceneTAS, idx++, identity.m, OWL_MATRIX_FORMAT_OWL);
    owlInstanceGroupSetChild(sceneTAS, idx, topLightGAS); owlInstanceGroupSetTransform(sceneTAS, idx++, identity.m, OWL_MATRIX_FORMAT_OWL);
    owlInstanceGroupSetChild(sceneTAS, idx, stripGAS); owlInstanceGroupSetTransform(sceneTAS, idx++, identity.m, OWL_MATRIX_FORMAT_OWL);

    // 发光小球
    for (int i = 0; i < 20; ++i) {
        float x = (rnd() - 0.5f) * scaled_extent * 4.0f;
        float y = ground_y + scaled_extent * 0.5f + rnd() * scaled_extent * 2.5f;
        float r = scaled_extent * (0.02f + rnd() * 0.06f);
        Mat4 xf = Mat4::translate(vec3f(x, y, wall_z + scaled_extent * 0.1f)) * Mat4::scale(vec3f(r));
        owlInstanceGroupSetChild(sceneTAS, idx, glowGAS);
        owlInstanceGroupSetTransform(sceneTAS, idx++, xf.m, OWL_MATRIX_FORMAT_OWL);
    }

    // 漂浮玻璃球
    vec3f f_pos[] = {
        vec3f(-scaled_extent * 0.9f, scaled_extent * 0.5f, -scaled_extent * 0.2f),
        vec3f(scaled_extent * 0.8f, scaled_extent * 0.1f, scaled_extent * 0.4f),
        vec3f(-scaled_extent * 0.7f, -scaled_extent * 0.4f, scaled_extent * 0.6f),
        vec3f(scaled_extent * 0.6f, scaled_extent * 0.8f, -scaled_extent * 0.5f)
    };
    float f_r[] = { 0.15f, 0.18f, 0.12f, 0.2f };
    for (int i = 0; i < 4; ++i) {
        Mat4 xf = Mat4::translate(f_pos[i]) * Mat4::scale(vec3f(scaled_extent * f_r[i]));
        owlInstanceGroupSetChild(sceneTAS, idx, floatGlassGAS);
        owlInstanceGroupSetTransform(sceneTAS, idx++, xf.m, OWL_MATRIX_FORMAT_OWL);
    }

    auto t_tas_start = std::chrono::high_resolution_clock::now();
    owlGroupBuildAccel(sceneTAS);
    auto t_tas_end = std::chrono::high_resolution_clock::now();

    std::cerr << "[TIME] 2. 组装全部场景 TAS 耗时: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t_tas_end - t_tas_start).count() << " ms\n";
    std::cerr << "[INFO] TAS instances: " << idx << " / " << totalInstances << "\n";

    // 7. 相机与渲染 (看向原点)
    vec3f cam_lookat = vec3f(0, 0, 0);
    vec3f cam_lookfrom = vec3f(0, scaled_extent * 0.2f, scaled_extent * 2.6f);

    Camera cam(owl, module,
        cam_lookfrom,
        cam_lookat,
        vec3f(0, 1, 0),
        length(cam_lookfrom - cam_lookat),
        45.0f, 16.0f / 9.0f,
        1260, 1240, 20
    );

    auto t_render_start = std::chrono::high_resolution_clock::now();
    cam.render(sceneTAS, nullptr, 0, 0.0f, "scene_obj_gpu.bmp");
    auto t_render_end = std::chrono::high_resolution_clock::now();

    std::cerr << "[TIME] 3. Render 内部 (JIT编译+渲染+保存) 耗时: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t_render_end - t_render_start).count() << " ms\n";

    owlModuleRelease(module);
    owlContextDestroy(owl);
}