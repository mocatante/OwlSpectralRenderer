# OwlSpectralRenderer
GPU port of spectral-ray-tracer (CPU version), built on OWL + NVIDIA OptiX, offloading ray-geometry intersection to hardware RT Cores.
spectral-ray-tracer（CPU 版）的 GPU 迁移版本，基于 OWL + NVIDIA OptiX，将光线求交迁移至 RT Core。

## 预览 / preview
<img width="1260" height="709" alt="preview_1" src="https://github.com/user-attachments/assets/6bf9e64a-11e7-4e66-928a-2756a89f0516" />
<img width="1024" height="1024" alt="preview_2" src="https://github.com/user-attachments/assets/d80379d9-256e-46ed-b2e6-d201299ac21f" />


## 依赖 / Dependencies
- OWL SDK（build from source / 需自行从源码编译）
- NVIDIA OptiX SDK 9.0.0
- CUDA Toolkit 13.x
- CMake ≥ 3.24，MSVC / Visual Studio 2022

仅在 Windows + MSVC + VS2022 上验证。/ Tested on Windows + MSVC + VS2022 only.

## 相对 CPU 版的改动 / Changes from the CPU version
The following simplifications were made relative to the CPU version.
NEE, GAS reuse, and sphere primitives are known gaps; direction-guided sampling
with a mixture PDF is implemented for diffuse surfaces as a first step.

以下几处相比 CPU 版做了简化。NEE、GAS 复用与球体图元是已知缺口；
漫反射表面已加入方向引导采样与混合 PDF，作为后续演进的第一步。


### 仅三角面片/Triangles only: 
All geometry is converted to triangle meshes so that intersection runs through OptiX's triangle pipeline on RT Cores. 
This is the project's primary goal; custom primitives would bypass the hardware intersection units.

图元改为三角网格，使求交走 OptiX 三角形管线并由 RT Core 执行——这是本项目的核心目的。


### 无 NEE，改用方向引导采样 / No NEE; direction-guided sampling instead:
No direct light sampling (NEE). Diffuse surfaces may instead draw first-bounce
directions from a mixture of cosine-weighted hemisphere and cones toward
bounding-sphere targets `{center, radius, weight}`, combined under a
balance-heuristic PDF. Rays are still traced in full, so this is not NEE.
Enabled only in nested_glass; other scenes fall back to pure BSDF sampling.

未实现直接光采样（NEE）。漫反射表面可将首次散射方向改为「余弦加权半球 +
指向包围球目标 `{center, radius, weight}` 的锥」的混合采样，按 balance
heuristic 合成 PDF。光线仍完整追踪，并非 NEE。仅 nested_glass 启用，其他
场景退化为纯 BSDF 采样。

### 暴力 GAS 构建 / Naive GAS construction:
One GAS per geometry, with no reuse or instance merging. For the current scene sizes the overhead is negligible. 
The simplest construction is used for now; reuse is left as a potential long-term optimization.

每个几何体独立建 GAS，未做复用或实例化合并。当前场景规模下构建开销可忽略。
先采用最简构建方式，复用留作后续可能的长期优化。
