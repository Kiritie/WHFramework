#include "Voxel/Generation/VoxelBiomeResolver.h"
EVoxelBiomeId FVoxelBiomeResolver::Resolve(const FVoxelGenerationSettings& S, const FVoxelNoiseRouter& N, int32 X, int32 Y, int32 H)
{
	if (H < S.SeaLevel - 4)
		return EVoxelBiomeId::Ocean;
	int32 T = N.Noise2D(X, Y, S.ClimatePeriod, 0x1004), U = N.Noise2D(X, Y, S.ClimatePeriod, 0x1005);
	if (H > S.BaseHeight + S.MountainAmplitude / 2 && H > S.SeaLevel + 16)
		return EVoxelBiomeId::Mountain;
	if (T < -14418)
		return EVoxelBiomeId::Snow;
	if (T > 11796 && U < -7864)
		return EVoxelBiomeId::Desert;
	if (U > 7864)
		return EVoxelBiomeId::Forest;
	return EVoxelBiomeId::Plains;
}
