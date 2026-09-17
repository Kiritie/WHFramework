#pragma once
#include "Voxel/Generation/Kernel/VoxelGenTypes.h"
namespace VoxelGen
{
WHFRAMEWORK_API int32_t FloorDiv(int32_t value,int32_t divisor);
WHFRAMEWORK_API int32_t CeilDivPositive(int32_t value,int32_t divisor);
WHFRAMEWORK_API uint64_t Mix(uint64_t value);
WHFRAMEWORK_API uint64_t Seed(int32_t world,I3 anchor,uint64_t salt);
WHFRAMEWORK_API Id MakeId(int32_t seed,I3 anchor,uint64_t kind,uint64_t ordinal=0);
WHFRAMEWORK_API int32_t Range(uint64_t seed,int32_t lo,int32_t hi);
WHFRAMEWORK_API int32_t Noise(int32_t seed,I3 p,int32_t period,uint64_t salt);
WHFRAMEWORK_API int32_t Noise2(int32_t seed,int32_t x,int32_t y,int32_t period,uint64_t salt);
WHFRAMEWORK_API int64_t Dot(I3 a,I3 b);
WHFRAMEWORK_API I3 RotateCell(I3 p,uint8_t yaw);
WHFRAMEWORK_API I3 RotateCorner(I3 p,uint8_t yaw);
WHFRAMEWORK_API Box TransformBox(Box b,I3 origin,int32_t scale,uint8_t yaw);
WHFRAMEWORK_API bool InEllipsoid(I3 p,I3 center,I3 radius);
WHFRAMEWORK_API bool InCapsule(I3 p,I3 a,I3 b,int32_t radius);
WHFRAMEWORK_API uint64_t HashBytes(const std::vector<uint8_t>& bytes);
}
