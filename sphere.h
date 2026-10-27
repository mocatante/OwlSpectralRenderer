#pragma once
#include <vector>
#include <cmath>
#include <owl/common/math/vec.h>

using namespace owl;

// 生成一个单位球体网格
inline void generate_unit_sphere(int stacks, int slices,
    std::vector<vec3f>& vertices,
    std::vector<vec3i>& indices) {
    // ---- 顶点（不变）----
    for (int i = 0; i <= stacks; ++i) {
        float v = (float)i / stacks;
        float phi = v * M_PI;               // 0 → π
        for (int j = 0; j <= slices; ++j) {
            float u = (float)j / slices;
            float theta = u * 2.0f * M_PI;  // 0 → 2π
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

            if (i != 0) {
                indices.push_back(vec3i(first, first + 1, second));
            }
            if (i != stacks - 1) {
                indices.push_back(vec3i(second, first + 1, second + 1));
            }
        }
    }
}



// 辅助测试: 生成一个四边形平面
// 按理不应该放这里, 但是为了省事
inline void generate_quad_mesh(vec3f p, vec3f u, vec3f v, std::vector<vec3f>&verts, std::vector<vec3i>&idxs) {
    int base = (int)verts.size();
    verts.push_back(p);
    verts.push_back(p + u);
    verts.push_back(p + u + v);
    verts.push_back(p + v);
    idxs.push_back(vec3i(base, base + 1, base + 2));
    idxs.push_back(vec3i(base, base + 2, base + 3));
}