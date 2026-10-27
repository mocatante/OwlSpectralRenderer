#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <owl/owl.h>
#include "shared.h"
#include "mat4.h"
#include "camera.h"

inline void generate_unit_sphere(int stacks, int slices, std::vector<vec3f>& vertices, std::vector<vec3i>& indices) {
    for (int i = 0; i <= stacks; ++i) {
        float v = (float)i / stacks;
        float phi = v * M_PI;
        for (int j = 0; j <= slices; ++j) {
            float u = (float)j / slices;
            float theta = u * 2.0f * M_PI;
            float x = sinf(phi) * cosf(theta);
            float y = cosf(phi);
            float z = sinf(phi) * sinf(theta);
            vertices.push_back(vec3f(x, y, z));
        }
    }
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;
            indices.push_back(vec3i(first, second, first + 1));
            indices.push_back(vec3i(second, second + 1, first + 1));
        }
    }
}

// 辅助构建GAS
inline OWLGroup buildTriangleMeshGAS(
    OWLContext owl,
    std::vector<vec3f>& verts,
    std::vector<vec3i>& idxs,
    OWLGeomType type,
    const char* materialParamName,
    float materialParamValue) {
    // 复制到显存
    OWLBuffer vertexBuffer = owlDeviceBufferCreate(owl, OWL_FLOAT3, verts.size(), verts.data());
    OWLBuffer indexBuffer = owlDeviceBufferCreate(owl, OWL_INT3, idxs.size(), idxs.data());
    // 构建一系列三角面片
    OWLGeom geom = owlGeomCreate(owl, type);
    owlTrianglesSetVertices(geom, vertexBuffer, verts.size(), sizeof(vec3f), 0);
    owlTrianglesSetIndices(geom, indexBuffer, idxs.size(), sizeof(vec3i), 0);
    // 填写SBT, 绑定材质参数
    owlGeomSetBuffer(geom, "mesh.vertices", vertexBuffer);
    owlGeomSetBuffer(geom, "mesh.indices", indexBuffer);
    owlGeomSet1f(geom, materialParamName, materialParamValue);
    // 构建GAS
    OWLGroup group = owlTrianglesGeomGroupCreate(owl, 1, &geom);
    owlGroupBuildAccel(group);
    return group;
}

inline void render_ball() {
    OWLContext owl = owlContextCreate(nullptr, 1);

    // .cu -> .ptx ->编译到显存, 返回句柄
    std::ifstream ptxFile("shaders.ptx");
    if (!ptxFile.is_open()) {
        std::cerr << "Failed to open shaders.ptx!" << std::endl;
        return;
    }
    std::string ptxCode((std::istreambuf_iterator<char>(ptxFile)), std::istreambuf_iterator<char>());
    OWLModule module = owlModuleCreate(owl, ptxCode.c_str());

    // 球和平面数据
    std::vector<vec3f> sphereVertices;
    std::vector<vec3i> sphereIndices;
    generate_unit_sphere(32, 64, sphereVertices, sphereIndices);

    std::vector<vec3f> planeVertices = { vec3f(-10, 0, -10), vec3f(10, 0, -10), vec3f(10, 0, 10), vec3f(-10, 0, 10) };
    std::vector<vec3i> planeIndices = { vec3i(0, 1, 2), vec3i(0, 2, 3) };

    // 材质参数声明
    OWLVarDecl diffuseTriangleVars[] = {
        { "mesh.vertices", OWL_BUFPTR, OWL_OFFSETOF(DiffuseTriangleData, mesh.vertices) },
        { "mesh.indices",  OWL_BUFPTR, OWL_OFFSETOF(DiffuseTriangleData, mesh.indices) },
        { "albedo",        OWL_FLOAT,  OWL_OFFSETOF(DiffuseTriangleData, albedo) },
        { nullptr }
    };
    OWLVarDecl emissiveTriangleVars[] = {
        { "mesh.vertices", OWL_BUFPTR, OWL_OFFSETOF(EmissiveTriangleData, mesh.vertices) },
        { "mesh.indices",  OWL_BUFPTR, OWL_OFFSETOF(EmissiveTriangleData, mesh.indices) },
        { "emission",      OWL_FLOAT,  OWL_OFFSETOF(EmissiveTriangleData, emission) },
        { nullptr }
    };

    // 注册几何类型，并绑定 CH
    OWLGeomType diffuseGeomType = owlGeomTypeCreate(owl, OWL_TRIANGLES, sizeof(DiffuseTriangleData), diffuseTriangleVars, 3);
    owlGeomTypeSetClosestHit(diffuseGeomType, 0, module, "DiffuseHit");

    OWLGeomType emissiveGeomType = owlGeomTypeCreate(owl, OWL_TRIANGLES, sizeof(EmissiveTriangleData), emissiveTriangleVars, 3);
    owlGeomTypeSetClosestHit(emissiveGeomType, 0, module, "EmissiveHit");

    // 第一次 build (确保底层有材质求交/包围盒程序，建树才不报错)
    owlBuildPrograms(owl);

    // 构建资产 GAS
    OWLGroup diffuseSphereGAS = buildTriangleMeshGAS(owl, sphereVertices, sphereIndices, diffuseGeomType, "albedo", 0.5f);
    OWLGroup emissiveSphereGAS = buildTriangleMeshGAS(owl, sphereVertices, sphereIndices, emissiveGeomType, "emission", 15.0f);
    OWLGroup planeGAS = buildTriangleMeshGAS(owl, planeVertices, planeIndices, diffuseGeomType, "albedo", 0.5f);

    // 构建场景 TAS
    OWLGroup sceneTAS = owlInstanceGroupCreate(owl, 4, nullptr);

    // 大球
    owlInstanceGroupSetChild(sceneTAS, 0, diffuseSphereGAS);
    Mat4 diffuseSphereTransform = Mat4::translate(vec3f(1.5f, 1.0f, 0.0f)) * Mat4::scale(vec3f(1.0f));
    owlInstanceGroupSetTransform(sceneTAS, 0, diffuseSphereTransform.m, OWL_MATRIX_FORMAT_OWL);

    // 小球 (复用)
    owlInstanceGroupSetChild(sceneTAS, 1, diffuseSphereGAS);
    Mat4 secondDiffuseSphereTransform = Mat4::translate(vec3f(-1.0f, 1.0f, 0.5f)) * Mat4::scale(vec3f(0.6f));
    owlInstanceGroupSetTransform(sceneTAS, 1, secondDiffuseSphereTransform.m, OWL_MATRIX_FORMAT_OWL);

    // 发光球
    owlInstanceGroupSetChild(sceneTAS, 2, emissiveSphereGAS);
    Mat4 emissiveSphereTransform = Mat4::translate(vec3f(-3.0f, 4.0f, -1.0f)) * Mat4::scale(vec3f(1.5f));
    owlInstanceGroupSetTransform(sceneTAS, 2, emissiveSphereTransform.m, OWL_MATRIX_FORMAT_OWL);

    // 地面
    owlInstanceGroupSetChild(sceneTAS, 3, planeGAS);
    Mat4 groundTransform = Mat4::identity();
    owlInstanceGroupSetTransform(sceneTAS, 3, groundTransform.m, OWL_MATRIX_FORMAT_OWL);

    owlGroupBuildAccel(sceneTAS);

    vec3f cam_pos(0.0f, 2.5f, 5.0f);
    vec3f cam_tgt(0.0f, 1.5f, 0.0f);

    // 传给相机全局唯一的 module
    Camera cam(owl, module,
        cam_pos,
        cam_tgt,
        vec3f(0, 1, 0),
        owl::common::length(cam_pos - cam_tgt),
        55.0f, 1.0f, 800, 64, 10
    );

    cam.render(sceneTAS, "scene_ball.bmp");

    owlModuleRelease(module);
    owlContextDestroy(owl);
}