#pragma once
#include <owl/common/math/vec.h>
#include <owl/owl.h>

using namespace owl;


// 加这个类是因为owl的矩阵运算有些转换问题
// 用自己写的不烧心


struct Mat4 {
    float m[12];

    static Mat4 identity() {
        Mat4 mat;
        mat.m[0] = 1; mat.m[1] = 0; mat.m[2] = 0;
        mat.m[3] = 0; mat.m[4] = 1; mat.m[5] = 0;
        mat.m[6] = 0; mat.m[7] = 0; mat.m[8] = 1;
        mat.m[9] = 0; mat.m[10] = 0; mat.m[11] = 0;
        return mat;
    }

    static Mat4 translate(const vec3f& t) {
        Mat4 mat = identity();
        mat.m[9] = t.x;
        mat.m[10] = t.y;
        mat.m[11] = t.z;
        return mat;
    }

    static Mat4 scale(const vec3f& s) {
        Mat4 mat = identity();
        mat.m[0] = s.x;
        mat.m[4] = s.y;
        mat.m[8] = s.z;
        return mat;
    }

    static Mat4 rotate_y(float angle_deg) {
        float rad = angle_deg * 3.14159265358979323846f / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);
        Mat4 mat = identity();
        mat.m[0] = c;  mat.m[2] = -s;
        mat.m[6] = s;  mat.m[8] = c;
        return mat;
    }


    Mat4 operator*(const Mat4& other) const {
        Mat4 res;
        res.m[0] = m[0] * other.m[0] + m[3] * other.m[1] + m[6] * other.m[2];
        res.m[1] = m[1] * other.m[0] + m[4] * other.m[1] + m[7] * other.m[2];
        res.m[2] = m[2] * other.m[0] + m[5] * other.m[1] + m[8] * other.m[2];

        res.m[3] = m[0] * other.m[3] + m[3] * other.m[4] + m[6] * other.m[5];
        res.m[4] = m[1] * other.m[3] + m[4] * other.m[4] + m[7] * other.m[5];
        res.m[5] = m[2] * other.m[3] + m[5] * other.m[4] + m[8] * other.m[5];

        res.m[6] = m[0] * other.m[6] + m[3] * other.m[7] + m[6] * other.m[8];
        res.m[7] = m[1] * other.m[6] + m[4] * other.m[7] + m[7] * other.m[8];
        res.m[8] = m[2] * other.m[6] + m[5] * other.m[7] + m[8] * other.m[8];

        res.m[9] = m[0] * other.m[9] + m[3] * other.m[10] + m[6] * other.m[11] + m[9];
        res.m[10] = m[1] * other.m[9] + m[4] * other.m[10] + m[7] * other.m[11] + m[10];
        res.m[11] = m[2] * other.m[9] + m[5] * other.m[10] + m[8] * other.m[11] + m[11];
        return res;
    }
};