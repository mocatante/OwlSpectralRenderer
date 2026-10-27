#include <optix_device.h>
#include "shared.h"
#include "sampling.h"
#include "spectral.h"

using namespace owl;

// 辅助函数, 内联
inline __device__ vec3f safe_normalize(const vec3f& v, const vec3f& fallback = vec3f(0, 1, 0)) {
    float len2 = dot(v, v);
    if (len2 < 1e-20f) return fallback;
    return v * rsqrtf(len2);
}

inline __device__ vec3f get_triangle_normal(const TriangleMeshGeometry& mesh, int primID, const vec3f& rayDir) {
    const vec3i idx = mesh.indices[primID];
    const vec3f A = mesh.vertices[idx.x];
    const vec3f B = mesh.vertices[idx.y];
    const vec3f C = mesh.vertices[idx.z];
    vec3f n_raw = cross(B - A, C - A);
    float len2 = dot(n_raw, n_raw);
    if (len2 < 1e-20f) {
        // sphere 极点退化三角形, 用射线反向当法线
        return -rayDir;
    }
    vec3f N = n_raw * rsqrtf(len2);
    if (dot(N, rayDir) > 0.0f) N = -N;
    return N;
}
inline __device__ vec3f get_triangle_hitP() {
    return vec3f(optixGetWorldRayOrigin()) + vec3f(optixGetWorldRayDirection()) * optixGetRayTmax();
}
inline __device__ vec3f reflect(const vec3f& v, const vec3f& n) {
    return v - 2.0f * dot(v, n) * n;
}

inline __device__ bool refract(const vec3f& v, const vec3f& n, float eta, vec3f& out_dir) {
    float cos_i = -dot(v, n);
    cos_i = fminf(fmaxf(cos_i, -1.0f), 1.0f);
    float sin2_t = eta * eta * (1.0f - cos_i * cos_i);
    sin2_t = fmaxf(sin2_t, 0.0f);
    if (sin2_t >= 1.0f) return false;
    float cos_t = sqrtf(1.0f - sin2_t);
    out_dir = eta * v + (eta * cos_i - cos_t) * n;

    float len2 = dot(out_dir, out_dir);
    if (len2 < 1e-12f) return false;
    out_dir = out_dir * rsqrtf(len2);
    return true;
}

inline __device__ float fresnel_schlick(float cosine, float n1, float n2) {
    float r0 = (n1 - n2) / (n1 + n2);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cosine, 5.0f);
}

inline __device__ float cauchy_dispersion(float ior, float dispersion, float lambda_nm) {
    float lambda_um = lambda_nm * 0.001f; // 将 nm 转为 um
    return ior + dispersion / (lambda_um * lambda_um);
}


/*=============================================================================================*/

// CH1: RGB 漫反射
OPTIX_CLOSEST_HIT_PROGRAM(DiffuseHit)() {
    const int primID = optixGetPrimitiveIndex();
    const DiffuseTriangleData& self = owl::getProgramData<DiffuseTriangleData>();

    vec3f dir = vec3f(optixGetWorldRayDirection());
    vec3f P = get_triangle_hitP();
    vec3f N = get_triangle_normal(self.mesh, primID, dir);

    PerRayData& prd = owl::getPRD<PerRayData>();

    // 把 RGB 反解成当前波长下的标量反照率
    float refl = Spectral::rgbToReflectanceAtWavelength(self.albedo, prd.lambda);

    prd.emission = 0.0f;
    prd.isScattered = scatter_diffuse(refl, P, N, prd); // 标量散射
}

// CH2: 黑体光源 / 平谱光源
OPTIX_CLOSEST_HIT_PROGRAM(BlackbodyHit)() {
    const BlackbodyTriangleData& self = owl::getProgramData<BlackbodyTriangleData>();
    PerRayData& prd = owl::getPRD<PerRayData>();

    if (self.temperature_K <= 0.0f) {
        prd.emission = self.intensity;
    }
    else {
        prd.emission = Spectral::blackbodyRadiance(prd.lambda, self.temperature_K) * self.intensity;
    }
    prd.isScattered = false;
}

// CH3: 光谱玻璃
OPTIX_CLOSEST_HIT_PROGRAM(GlassHit)() {
    const int primID = optixGetPrimitiveIndex();
    const GlassTriangleData& self = owl::getProgramData<GlassTriangleData>();
    PerRayData& prd = owl::getPRD<PerRayData>();

    vec3f dir = vec3f(optixGetWorldRayDirection());
    vec3f P = get_triangle_hitP();

    // 计算绝对外法线
    const vec3i idx = self.mesh.indices[primID];
    const vec3f A = self.mesh.vertices[idx.x];
    const vec3f B = self.mesh.vertices[idx.y];
    const vec3f C = self.mesh.vertices[idx.z];
    vec3f n_raw = cross(B - A, C - A);
    if (dot(n_raw, n_raw) < 1e-20f) {   // 退化三角形：直接吃掉
        prd.isScattered = false;
        prd.emission = 0.0f;
        return;
    }
    vec3f outward_N = safe_normalize(n_raw);

    // 判断光线是从外部射入，还是从内部射出
    bool front_face = dot(dir, outward_N) < 0.0f;
    vec3f N = front_face ? outward_N : -outward_N;

    // 使用提取的柯西色散函数计算当前波长的折射率
    float n_lambda = cauchy_dispersion(self.ior, self.dispersion, prd.lambda);

    float n1 = front_face ? 1.0f : n_lambda;
    float n2 = front_face ? n_lambda : 1.0f;
    float eta = n1 / n2;

    vec3f refracted_dir;
    bool can_refract = refract(dir, N, eta, refracted_dir);
    vec3f reflected_dir = reflect(dir, N);

    // 菲涅尔概率决定是反射还是折射
    float reflect_prob = can_refract ? fresnel_schlick(fmaxf(0.0f, -dot(dir, N)), n1, n2) : 1.0f;

    if (prd.random() < reflect_prob) {
        prd.direction = reflected_dir;
    }
    else {
        prd.direction = refracted_dir;
    }

    prd.origin = P + prd.direction * 1e-3f;
    prd.emission = 0.0f;
    prd.isScattered = true;
}

// CH4: 金属
OPTIX_CLOSEST_HIT_PROGRAM(MetalHit)() {
    const int primID = optixGetPrimitiveIndex();
    const MetalTriangleData& self = owl::getProgramData<MetalTriangleData>();
    PerRayData& prd = owl::getPRD<PerRayData>();

    vec3f dir = vec3f(optixGetWorldRayDirection());
    vec3f P = get_triangle_hitP();
    vec3f N = get_triangle_normal(self.mesh, primID, dir);

    // 随机单位球扰动
    float z = 1.0f - 2.0f * prd.random();
    float r = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    float phi = 2.0f * 3.14159265f * prd.random();
    vec3f random_unit(z, r * cosf(phi), r * sinf(phi));

    vec3f reflected = reflect(dir, N);

    vec3f scattered_raw = reflected + random_unit * self.fuzz;
    float s_len2 = dot(scattered_raw, scattered_raw);
    prd.emission = 0.0f;
    if (s_len2 < 1e-20f) {
        prd.isScattered = false;
        return;
    }
    vec3f scattered_dir = scattered_raw * rsqrtf(s_len2);


    if (dot(scattered_dir, N) <= 0.0f) {
        prd.isScattered = false;
    }
    else {
        prd.direction = scattered_dir;
        prd.origin = P;
        
        float refl = Spectral::rgbToReflectanceAtWavelength(self.albedo, prd.lambda);
        prd.throughput *= refl;
        prd.isScattered = true;
    }
}

// RG
OPTIX_RAYGEN_PROGRAM(rayGen)() {
    const RayGenData& self = owl::getProgramData<RayGenData>();
    const vec2i pixelID = owl::getLaunchIndex();
    const int pixelIdx = pixelID.x + self.fbSize.x * pixelID.y;

    // 累加器是 XYZ 向量
    vec3f final_xyz = vec3f(0.0f);

    for (int s = 0; s < self.samples_per_pixel; s++) {
        PerRayData prd;
        prd.throughput = 1.0f;
        prd.emission = 0.0f;
        prd.random.init(pixelID.x * 1973 + s, pixelID.y * 9277 + s);

        // 随机波长
        int channelIndex = min((int)(prd.random() * SPECTRAL_CHANNEL_COUNT), SPECTRAL_CHANNEL_COUNT - 1);
        prd.lambda = SPECTRAL_WAVELENGTH_MIN_NM + channelIndex * SPECTRAL_WAVELENGTH_STEP_NM;

        float u_offset = prd.random() - 0.5f;
        float v_offset = prd.random() - 0.5f;

        vec3f sample_point = self.camera.pixel00_center
            + self.camera.u_step * (pixelID.x + u_offset)
            + self.camera.v_step * (pixelID.y + v_offset);

        vec3f direction = normalize(sample_point - self.camera.origin);

        owl::Ray ray;
        ray.origin = self.camera.origin;
        ray.direction = direction;
        ray.tmin = 1e-3f;
        ray.tmax = 1e10f;

        float path_radiance = 0.0f;

        for (int depth = 0; depth < self.max_depth; depth++) {
            owl::traceRay(self.world, ray, prd);
            path_radiance += prd.throughput * prd.emission;
            if (!prd.isScattered) break;
            ray.origin = prd.origin;
            ray.direction = prd.direction;
        }

        // 乘上当前波长对应的 CIE 权重
        final_xyz += Spectral::wavelength_to_xyz(prd.lambda) * path_radiance * (float)SPECTRAL_CHANNEL_COUNT;
    }

    // 蒙特卡洛平均及 XYZ 归一化
    final_xyz /= (float)self.samples_per_pixel;
    final_xyz /= SPECTRAL_XYZ_INTEGRAL_NORMALIZATION;

    // XYZ 转 sRGB
    vec3f linear_rgb = vec3f(
        3.2406f * final_xyz.x - 1.5372f * final_xyz.y - 0.4986f * final_xyz.z,
        -0.9689f * final_xyz.x + 1.8758f * final_xyz.y + 0.0415f * final_xyz.z,
        0.0557f * final_xyz.x - 0.2040f * final_xyz.y + 1.0570f * final_xyz.z
    );


    // —— 调试：NaN 洋红、Inf 青、负值暗红（诊断完可以删）——
    if (isnan(linear_rgb.x) || isnan(linear_rgb.y) || isnan(linear_rgb.z)) {
        self.fbPtr[pixelIdx] = owl::make_rgba(vec3f(1.0f, 0.0f, 1.0f));  // 洋红
        return;
    }
    if (isinf(linear_rgb.x) || isinf(linear_rgb.y) || isinf(linear_rgb.z)) {
        self.fbPtr[pixelIdx] = owl::make_rgba(vec3f(0.0f, 1.0f, 1.0f));  // 青
        return;
    }

    // Clamp 并转 sRGB
    linear_rgb = vec3f(fminf(fmaxf(linear_rgb.x, 0.0f), 1.0f),
        fminf(fmaxf(linear_rgb.y, 0.0f), 1.0f),
        fminf(fmaxf(linear_rgb.z, 0.0f), 1.0f));

    // gamma 校正
    vec3f rgb_output = vec3f(powf(linear_rgb.x, 1.0f / 2.2f), powf(linear_rgb.y, 1.0f / 2.2f), powf(linear_rgb.z, 1.0f / 2.2f));

    self.fbPtr[pixelIdx] = owl::make_rgba(rgb_output);
}

// MS
OPTIX_MISS_PROGRAM(miss)() {
    PerRayData& prd = owl::getPRD<PerRayData>();
    prd.isScattered = false;
    prd.emission = 0.0f;
}