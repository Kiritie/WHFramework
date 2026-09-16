#include "Voxel/Generation/VoxelSurfaceStage.h"
FVoxelBlockState FVoxelSurfaceStage::Resolve(const FVoxelGenerationRuntimeConfig& C, const FVoxelColumnSample& P, int32 Z, int64 D)
{
	if (Z < C.Settings.MinZ || Z >= C.Settings.MaxZ)
		return {};
	if (D <= 0)
		return Z <= C.Settings.SeaLevel ? FVoxelBlockState{C.Water, 0} : FVoxelBlockState{};
	const int32 Depth = P.SurfaceZ - Z;
	if (P.Biome == EVoxelBiomeId::Desert || P.Biome == EVoxelBiomeId::Ocean)
		return {Depth >= 0 && Depth <= 4 ? C.Sand : C.Stone, 0};
	if (Depth == 0)
		return {P.Biome == EVoxelBiomeId::Snow ? C.Snow : C.Grass, 0};
	if (Depth > 0 && Depth <= 3)
		return {C.Dirt, 0};
	return {C.Stone, 0};
}
