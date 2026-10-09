#pragma once
#include <cstdint>
#include <cstddef>
using GX2RResourceFlags = unsigned;
using GX2SurfaceFormat = unsigned;
constexpr unsigned GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8 = 0x1a;
constexpr unsigned GX2_SURFACE_FORMAT_UNORM_R8_G8 = 7;
constexpr unsigned GX2_SURFACE_USE_TEXTURE = 1, GX2_SURFACE_DIM_TEXTURE_2D = 1, GX2_AA_MODE1X = 0;
constexpr unsigned GX2_TILE_MODE_LINEAR_ALIGNED = 1;
constexpr unsigned GX2R_RESOURCE_BIND_TEXTURE = 1, GX2R_RESOURCE_USAGE_CPU_WRITE = 0x1000;
constexpr unsigned GX2R_RESOURCE_USAGE_GPU_READ = 0x2000, GX2_INVALIDATE_MODE_CPU_TEXTURE = 0x40;
constexpr unsigned GX2_TEX_CLAMP_MODE_WRAP = 0, GX2_TEX_CLAMP_MODE_CLAMP = 2;
constexpr unsigned GX2_TEX_XY_FILTER_MODE_POINT = 0, GX2_TEX_XY_FILTER_MODE_LINEAR = 1;
struct GX2Surface {
    unsigned use,dim,width,height,depth,mipLevels,format,aa,tileMode;
    unsigned pitch,imageSize,alignment,mipLevelOffset[13];
    void* image;
};
struct GX2Texture { GX2Surface surface; unsigned viewFirstMip,viewNumMips,viewFirstSlice,viewNumSlices,compMap; };
struct GX2Sampler {unsigned Clamp,Filter;};
void GX2CalcSurfaceSizeAndAlignment(GX2Surface*);
bool GX2RCreateSurface(GX2Surface*,unsigned);
void* GX2RLockSurfaceEx(GX2Surface*,unsigned,unsigned);
void GX2RUnlockSurfaceEx(GX2Surface*,unsigned,unsigned);
void GX2RInvalidateSurface(GX2Surface*,unsigned,unsigned);
void GX2Invalidate(unsigned,void*,unsigned);
void GX2RDestroySurfaceEx(GX2Surface*,unsigned);
void GX2InitTextureRegs(GX2Texture*);
void GX2InitSampler(GX2Sampler*,unsigned,unsigned);
void GX2DrawDone();
