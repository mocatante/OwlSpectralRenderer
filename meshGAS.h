#pragma once
#include <vector>
#include <owl/owl.h>
#include "shared.h"

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