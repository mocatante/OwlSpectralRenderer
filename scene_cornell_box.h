#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <owl/owl.h>
#include "shared.h"
#include "mat4.h"
#include "camera.h"
#include "sphere.h"   // 引入了球和 quad 的生成
#include "meshGAS.h"

// 生成盒子
inline void generate_box_mesh(vec3f min, vec3f max, std::vector<vec3f>& verts, std::vector<vec3i>& idxs) {
    int base = (int)verts.size();
    verts.push_back(vec3f(min.x, min.y, min.z)); // p0
    verts.push_back(vec3f(max.x, min.y, min.z)); // p1
    verts.push_back(vec3f(max.x, max.y, min.z)); // p2
    verts.push_back(vec3f(min.x, max.y, min.z)); // p3
    verts.push_back(vec3f(min.x, min.y, max.z)); // p4
    verts.push_back(vec3f(max.x, min.y, max.z)); // p5
    verts.push_back(vec3f(max.x, max.y, max.z)); // p6
    verts.push_back(vec3f(min.x, max.y, max.z)); // p7

    // 12 个三角形
    int faces[] = {
        0,2,1, 0,3,2, // 底面
        4,5,6, 4,6,7, // 顶面
        0,4,7, 0,7,3, // 左面
        1,2,6, 1,6,5, // 右面
        3,7,6, 3,6,2, // 前面
        0,1,5, 0,5,4  // 后面
    };
    for (int i = 0; i < 36; i += 3) {
        idxs.push_back(vec3i(base + faces[i], base + faces[i + 1], base + faces[i + 2]));
    }
}

inline void render_cornell_box() {
    OWLContext owl = owlContextCreate(nullptr, 1);
    std::ifstream ptxFile("shaders.ptx");
    if (!ptxFile.is_open()) return;
    std::string ptxCode((std::istreambuf_iterator<char>(ptxFile)), std::istreambuf_iterator<char>());
    OWLModule module = owlModuleCreate(owl, ptxCode.c_str());

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

    OWLGeomType diffuseType = owlGeomTypeCreate(owl, OWL_TRIANGLES, sizeof(DiffuseTriangleData), diffuseVars, 3);
    owlGeomTypeSetClosestHit(diffuseType, 0, module, "DiffuseHit");

    OWLGeomType blackbodyType = owlGeomTypeCreate(owl, OWL_TRIANGLES, sizeof(BlackbodyTriangleData), blackbodyVars, 4);
    owlGeomTypeSetClosestHit(blackbodyType, 0, module, "BlackbodyHit");

    // 2. 准备几何
    std::vector<vec3f> greenVerts, redVerts, whiteVerts, lightVerts;
    std::vector<vec3i> greenIdx, redIdx, whiteIdx, lightIdx;

    // 左右后墙 (使用 sphere.h 里的 generate_quad_mesh)
    generate_quad_mesh(vec3f(555, 0, 0), vec3f(0, 0, 555), vec3f(0, 555, 0), greenVerts, greenIdx);
    generate_quad_mesh(vec3f(0, 0, 555), vec3f(0, 0, -555), vec3f(0, 555, 0), redVerts, redIdx);

    // 天花板、地板、后墙
    generate_quad_mesh(vec3f(0, 555, 0), vec3f(555, 0, 0), vec3f(0, 0, 555), whiteVerts, whiteIdx);
    generate_quad_mesh(vec3f(0, 0, 555), vec3f(555, 0, 0), vec3f(0, 0, -555), whiteVerts, whiteIdx);
    generate_quad_mesh(vec3f(555, 0, 555), vec3f(-555, 0, 0), vec3f(0, 555, 0), whiteVerts, whiteIdx);

    // 发光面光源
    generate_quad_mesh(vec3f(213, 554, 227), vec3f(130, 0, 0), vec3f(0, 0, 105), lightVerts, lightIdx);

    // 两个盒子
    std::vector<vec3f> box1Verts, box2Verts;
    std::vector<vec3i> box1Idx, box2Idx;
    generate_box_mesh(vec3f(0, 0, 0), vec3f(165, 330, 165), box1Verts, box1Idx);
    generate_box_mesh(vec3f(0, 0, 0), vec3f(165, 165, 165), box2Verts, box2Idx);

    // 3. 建 GAS
    OWLGroup greenGAS = buildTriangleMeshGAS(owl, greenVerts, greenIdx, diffuseType, "albedo", vec3f(0.12f, 0.45f, 0.15f));
    OWLGroup redGAS = buildTriangleMeshGAS(owl, redVerts, redIdx, diffuseType, "albedo", vec3f(0.65f, 0.05f, 0.05f));
    OWLGroup whiteGAS = buildTriangleMeshGAS(owl, whiteVerts, whiteIdx, diffuseType, "albedo", vec3f(0.73f, 0.73f, 0.73f));

    // 光源：使用 6500K 的黑体，强度 15.0f
    OWLGroup lightGAS = buildBlackbodyGAS(owl, lightVerts, lightIdx, blackbodyType, 6500.0f, 15.0f);

    OWLGroup box1GAS = buildTriangleMeshGAS(owl, box1Verts, box1Idx, diffuseType, "albedo", vec3f(0.73f, 0.73f, 0.73f));
    OWLGroup box2GAS = buildTriangleMeshGAS(owl, box2Verts, box2Idx, diffuseType, "albedo", vec3f(0.73f, 0.73f, 0.73f));

    // 4. 构建 TAS
    OWLGroup sceneTAS = owlInstanceGroupCreate(owl, 8, nullptr);

    // 墙们 (世界坐标，Identity)
    Mat4 identity = Mat4::identity();
    owlInstanceGroupSetChild(sceneTAS, 0, greenGAS);
    owlInstanceGroupSetTransform(sceneTAS, 0, identity.m, OWL_MATRIX_FORMAT_OWL);

    owlInstanceGroupSetChild(sceneTAS, 1, redGAS);
    owlInstanceGroupSetTransform(sceneTAS, 1, identity.m, OWL_MATRIX_FORMAT_OWL);

    owlInstanceGroupSetChild(sceneTAS, 2, whiteGAS);
    owlInstanceGroupSetTransform(sceneTAS, 2, identity.m, OWL_MATRIX_FORMAT_OWL); // 天花板
    owlInstanceGroupSetChild(sceneTAS, 3, whiteGAS);
    owlInstanceGroupSetTransform(sceneTAS, 3, identity.m, OWL_MATRIX_FORMAT_OWL); // 地板
    owlInstanceGroupSetChild(sceneTAS, 4, whiteGAS);
    owlInstanceGroupSetTransform(sceneTAS, 4, identity.m, OWL_MATRIX_FORMAT_OWL); // 后墙

    // 光源
    owlInstanceGroupSetChild(sceneTAS, 5, lightGAS);
    owlInstanceGroupSetTransform(sceneTAS, 5, identity.m, OWL_MATRIX_FORMAT_OWL);

    // 盒子 1
    owlInstanceGroupSetChild(sceneTAS, 6, box1GAS);
    Mat4 box1XF = Mat4::translate(vec3f(265, 0, 295)) * Mat4::rotate_y(15.0f);
    owlInstanceGroupSetTransform(sceneTAS, 6, box1XF.m, OWL_MATRIX_FORMAT_OWL);

    // 盒子 2
    owlInstanceGroupSetChild(sceneTAS, 7, box2GAS);
    Mat4 box2XF = Mat4::translate(vec3f(130, 0, 65)) * Mat4::rotate_y(-18.0f);
    owlInstanceGroupSetTransform(sceneTAS, 7, box2XF.m, OWL_MATRIX_FORMAT_OWL);

    owlGroupBuildAccel(sceneTAS);

    // 5. 相机与渲染
    vec3f cam_pos(278.0f, 278.0f, -800.0f);
    vec3f cam_tgt(278.0f, 278.0f, 0.0f);

    Camera cam(owl, module,
        cam_pos, cam_tgt, vec3f(0, 1, 0),
        1.0f, 40.0f, 1.0f, 600, 512, 10
    );

    cam.render(sceneTAS, "scene_cornell_box.bmp");

    owlModuleRelease(module);
    owlContextDestroy(owl);
}