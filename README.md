# OwlSpectralRenderer
GPU port of spectral-ray-tracer (CPU version), built on OWL + NVIDIA OptiX, offloading ray-geometry intersection to hardware RT Cores.
spectral-ray-tracer（CPU 版）的 GPU 迁移版本，基于 OWL + NVIDIA OptiX，将光线求交迁移至 RT Core。

## 预览 / preview
<img width="1024" height="1024" alt="dc20eb3d010c1f9abf7185a243d6b546" src="https://github.com/user-attachments/assets/1503764d-7d00-41d0-a50a-dff7d23aebba" />
<img width="1260" height="709" alt="0903bfc72f5dbfa0521d309f7758afd0" src="https://github.com/user-attachments/assets/421fcb9b-ca9b-4f0f-a804-b8c7fd156dc9" />

## 依赖 / Dependencies
- OWL SDK（build from source / 需自行从源码编译）
- NVIDIA OptiX SDK 9.0.0
- CUDA Toolkit 13.x
- CMake ≥ 3.24，MSVC / Visual Studio 2022

仅在 Windows + MSVC + VS2022 上验证。/ Tested on Windows + MSVC + VS2022 only.

## 相对 CPU 版的改动 / Changes from the CPU version
The following simplifications were made relative to the CPU version. At this stage my understanding of renderer architecture is not deep enough to judge what the optimal implementation would be, so the simplest viable approach is chosen uniformly, with targeted optimization deferred until that understanding improves.
以下几处相比 CPU 版做了简化。当前对渲染器架构的理解尚不足以判断何为最优实现，因此统一选择最简可行方案，待后续理解加深再做针对性优化。
### 仅三角面片/Triangles only: 
All geometry is converted to triangle meshes so that intersection runs through OptiX's triangle pipeline on RT Cores. This is the project's primary goal; custom primitives would bypass the hardware intersection units.
除自定义图元，全部改为三角网格，使求交走 OptiX 三角形管线并由 RT Core 执行——这是本项目的核心目的。
### 纯路径追踪 / Pure path tracing:
Direct light sampling and MIS are removed. NEE and MIS interact with spectral sampling in non-obvious ways, and it is unclear which combination yields the best noise/effort trade-off here. Pure path tracing is unbiased and simple to verify, so it serves as the baseline; light sampling will be revisited once the base pipeline is solid.
validating the base pipeline first makes those additions far less error-prone later.
移除直接光采样，未实现 MIS。NEE 与 MIS 和光谱采样的耦合方式并不直观，何种组合在此场景下噪声与实现成本的权衡最优也尚不清楚。
纯路径追踪无偏且易于验证，先作为基线，待基础管线稳定后再评估是否引入光采样。
### 暴力 GAS 构建 / Naive GAS construction:
One GAS per geometry, with no reuse or instance merging. For the current scene sizes the overhead is negligible, and it is unclear whether BLAS/TLAS separation or instance merging would actually pay off at this scale. The simplest construction is used for now; reuse is left as a potential long-term optimization.
每个几何体独立建 GAS，未做复用或实例化合并。当前场景规模下构建开销可忽略，而 BLAS/TLAS 拆分或实例合并在这一规模下是否真正划算也尚不清楚。
先采用最简构建方式，复用留作后续可能的长期优化。
