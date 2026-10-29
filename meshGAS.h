#pragma once
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <iostream>
#include <owl/owl.h>
#include "shared.h"
#include "mat4.h"

using namespace owl;

// 辅助构建GAS (传入 vec3f 材质参数，比如 albedo)
inline OWLGroup buildTriangleMeshGAS(
    OWLContext owl,
    std::vector<vec3f>& verts,
    std::vector<vec3i>& idxs,
    OWLGeomType type,
    const char* materialParamName,
    vec3f materialParamValue) {

    // 复制到显存
    OWLBuffer vertexBuffer = owlDeviceBufferCreate(owl, OWL_FLOAT3, verts.size(), verts.data());
    OWLBuffer indexBuffer = owlDeviceBufferCreate(owl, OWL_INT3, idxs.size(), idxs.data());

    // 构建一系列三角面片
    OWLGeom geom = owlGeomCreate(owl, type);
    owlTrianglesSetVertices(geom, vertexBuffer, verts.size(), sizeof(vec3f), 0);
    owlTrianglesSetIndices(geom, indexBuffer, idxs.size(), sizeof(vec3i), 0);

    // 填写SBT
    owlGeomSetBuffer(geom, "mesh.vertices", vertexBuffer);
    owlGeomSetBuffer(geom, "mesh.indices", indexBuffer);
    // 绑定材质参数 (vec3f)
    owlGeomSet3f(geom, materialParamName, owl3f{ materialParamValue.x, materialParamValue.y, materialParamValue.z });

    // 构建GAS
    OWLGroup group = owlTrianglesGeomGroupCreate(owl, 1, &geom);
    owlGroupBuildAccel(group);
    return group;
}

// 用于黑体光源
inline OWLGroup buildBlackbodyGAS(
    OWLContext owl,
    std::vector<vec3f>& verts,
    std::vector<vec3i>& idxs,
    OWLGeomType type,
    float temperature,
    float intensity) {

    OWLBuffer vertexBuffer = owlDeviceBufferCreate(owl, OWL_FLOAT3, verts.size(), verts.data());
    OWLBuffer indexBuffer = owlDeviceBufferCreate(owl, OWL_INT3, idxs.size(), idxs.data());

    OWLGeom geom = owlGeomCreate(owl, type);
    owlTrianglesSetVertices(geom, vertexBuffer, verts.size(), sizeof(vec3f), 0);
    owlTrianglesSetIndices(geom, indexBuffer, idxs.size(), sizeof(vec3i), 0);

    owlGeomSetBuffer(geom, "mesh.vertices", vertexBuffer);
    owlGeomSetBuffer(geom, "mesh.indices", indexBuffer);
    owlGeomSet1f(geom, "temperature", temperature);
    owlGeomSet1f(geom, "intensity", intensity);

    OWLGroup group = owlTrianglesGeomGroupCreate(owl, 1, &geom);
    owlGroupBuildAccel(group);
    return group;
}

// 用于光谱玻璃
inline OWLGroup buildGlassGAS(
    OWLContext owl,
    std::vector<vec3f>& verts,
    std::vector<vec3i>& idxs,
    OWLGeomType type,
    float ior,
    float dispersion) {

    OWLBuffer vertexBuffer = owlDeviceBufferCreate(owl, OWL_FLOAT3, verts.size(), verts.data());
    OWLBuffer indexBuffer = owlDeviceBufferCreate(owl, OWL_INT3, idxs.size(), idxs.data());

    OWLGeom geom = owlGeomCreate(owl, type);
    owlTrianglesSetVertices(geom, vertexBuffer, verts.size(), sizeof(vec3f), 0);
    owlTrianglesSetIndices(geom, indexBuffer, idxs.size(), sizeof(vec3i), 0);

    owlGeomSetBuffer(geom, "mesh.vertices", vertexBuffer);
    owlGeomSetBuffer(geom, "mesh.indices", indexBuffer);
    owlGeomSet1f(geom, "ior", ior);
    owlGeomSet1f(geom, "dispersion", dispersion);

    OWLGroup group = owlTrianglesGeomGroupCreate(owl, 1, &geom);
    owlGroupBuildAccel(group);
    return group;
}

// 用于金属
inline OWLGroup buildMetalGAS(
    OWLContext owl, std::vector<vec3f>& verts, std::vector<vec3i>& idxs, OWLGeomType type,
    vec3f albedo, float fuzz) {
    OWLBuffer vertexBuffer = owlDeviceBufferCreate(owl, OWL_FLOAT3, verts.size(), verts.data());
    OWLBuffer indexBuffer = owlDeviceBufferCreate(owl, OWL_INT3, idxs.size(), idxs.data());
    OWLGeom geom = owlGeomCreate(owl, type);
    owlTrianglesSetVertices(geom, vertexBuffer, verts.size(), sizeof(vec3f), 0);
    owlTrianglesSetIndices(geom, indexBuffer, idxs.size(), sizeof(vec3i), 0);
    owlGeomSetBuffer(geom, "mesh.vertices", vertexBuffer);
    owlGeomSetBuffer(geom, "mesh.indices", indexBuffer);
    owlGeomSet3f(geom, "albedo", owl3f{ albedo.x, albedo.y, albedo.z });
    owlGeomSet1f(geom, "fuzz", fuzz);
    OWLGroup group = owlTrianglesGeomGroupCreate(owl, 1, &geom);
    owlGroupBuildAccel(group);
    return group;
}

// 新增：加载 OBJ 并构建 GAS，自动居中、缩放，并返回变换矩阵
inline OWLGroup buildObjGAS(
    OWLContext owl,
    const std::string& filename,
    OWLGeomType type,
    float ior,
    float dispersion,
    float target_extent,  // 期望缩放后的对角线长度
    Mat4& out_xf,         // 输出：平移到原点并缩放的变换矩阵
    float& out_scaled_extent) // 输出：缩放后的对角线长度 (即 target_extent)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    std::vector<vec3f> verts;
    std::vector<vec3i> idxs;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open OBJ file: " << filename << "\n";
        out_xf = Mat4::identity();
        out_scaled_extent = target_extent;
        return nullptr;
    }

    vec3f bmin(1e30f), bmax(-1e30f);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string typeStr;
        iss >> typeStr;
        if (typeStr == "v") {
            float x, y, z;
            if (iss >> x >> y >> z) {
                verts.push_back(vec3f(x, y, z));
                bmin.x = std::fmin(bmin.x, x); bmin.y = std::fmin(bmin.y, y); bmin.z = std::fmin(bmin.z, z);
                bmax.x = std::fmax(bmax.x, x); bmax.y = std::fmax(bmax.y, y); bmax.z = std::fmax(bmax.z, z);
            }
        }
        else if (typeStr == "f") {
            std::vector<int> face_indices;
            std::string token;
            while (iss >> token) {
                size_t pos = token.find('/');
                std::string idx_str = (pos == std::string::npos) ? token : token.substr(0, pos);
                if (!idx_str.empty()) face_indices.push_back(std::stoi(idx_str) - 1);
            }
            for (size_t i = 1; i + 1 < face_indices.size(); ++i) {
                idxs.push_back(vec3i(face_indices[0], face_indices[i], face_indices[i + 1]));
            }
        }
    }
    file.close();

    auto t1 = std::chrono::high_resolution_clock::now();


    if (verts.empty() || idxs.empty()) {
        out_xf = Mat4::identity();
        out_scaled_extent = target_extent;
        return nullptr;
    }

    vec3f center = (bmin + bmax) * 0.5f;

    // 计算包围盒对角线长度
    float dx = bmax.x - bmin.x;
    float dy = bmax.y - bmin.y;
    float dz = bmax.z - bmin.z;
    float raw_extent = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (raw_extent <= 1e-6f) raw_extent = 1.0f;

    float scale_factor = target_extent / raw_extent;
    out_scaled_extent = target_extent;

    // 先平移到中心，再缩放
    out_xf = Mat4::scale(vec3f(scale_factor)) * Mat4::translate(-center);

    OWLBuffer vertexBuffer = owlDeviceBufferCreate(owl, OWL_FLOAT3, verts.size(), verts.data());
    OWLBuffer indexBuffer = owlDeviceBufferCreate(owl, OWL_INT3, idxs.size(), idxs.data());
    OWLGeom geom = owlGeomCreate(owl, type);
    owlTrianglesSetVertices(geom, vertexBuffer, verts.size(), sizeof(vec3f), 0);
    owlTrianglesSetIndices(geom, indexBuffer, idxs.size(), sizeof(vec3i), 0);
    owlGeomSetBuffer(geom, "mesh.vertices", vertexBuffer);
    owlGeomSetBuffer(geom, "mesh.indices", indexBuffer);
    owlGeomSet1f(geom, "ior", ior);
    owlGeomSet1f(geom, "dispersion", dispersion);
    OWLGroup group = owlTrianglesGeomGroupCreate(owl, 1, &geom);

    auto t2 = std::chrono::high_resolution_clock::now();
    owlGroupBuildAccel(group); // 这一步是在 CPU 多线程建树
    auto t3 = std::chrono::high_resolution_clock::now();

    std::cerr << "   [OBJ] 纯解析耗时: " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms | ";
    std::cerr << "   [OBJ] GAS 构建耗时:  (GPU 硬件加速):: " << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count() << " ms\n";
    std::cerr << "   [OBJ] 顶点数: " << verts.size() << " | 三角形数: " << idxs.size() << "\n";
    return group;
}