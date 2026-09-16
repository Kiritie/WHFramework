#include "Voxel/Generation/VoxelTerrainStage.h"
#include "Voxel/Generation/VoxelBiomeResolver.h"
int64 FVoxelTerrainStage::HeightQ16(int32 X, int32 Y) const
{
	const int32 C = Noise.Noise2D(X, Y, S.ContinentalPeriod, 0x1001);
	const int32 E = Noise.Noise2D(X, Y, S.ErosionPeriod, 0x1002);
	const int32 M = FMath::Abs(Noise.Noise2D(X, Y, S.MountainPeriod, 0x1003));
	const int64 Ridge = FMath::Clamp<int64>((int64(M) - 10000) * 65536 / 20000, 0, 65536);
	const int64 Erosion = FMath::Clamp<int64>(32768 - int64(E), 9830, 65536);
	return int64(S.BaseHeight) * 65536 + int64(C) * 2 * S.ContinentalAmplitude + (((Ridge * Erosion) / 65536) * S.MountainAmplitude);
}
int64 FVoxelTerrainStage::DensityQ16(int32 X, int32 Y, int32 Z, int64 H) const
{
	return H - int64(Z) * 65536 + int64(Noise.Noise3D(X, Y, Z, S.DetailPeriod, 0x1006)) * 2 * S.DetailAmplitude;
}
FVoxelColumnSample FVoxelTerrainStage::SampleColumn(int32 X, int32 Y) const
{
	FVoxelColumnSample O;
	O.HeightQ16 = HeightQ16(X, Y);
	O.SurfaceZ = S.MinZ - 1;
	// The bounded window includes every possible density-positive top cell.
	int64 FloorH = O.HeightQ16 / 65536;
	if (O.HeightQ16 % 65536 < 0)
		--FloorH;
	int32 Top = int32(FMath::Clamp<int64>(FloorH + S.DetailAmplitude + 2, S.MinZ, S.MaxZ - 1));
	int32 Bottom = int32(FMath::Clamp<int64>(FloorH - S.DetailAmplitude - 2, S.MinZ, S.MaxZ - 1));
	for (int32 Z = Top; Z >= Bottom; --Z)
		if (DensityQ16(X, Y, Z, O.HeightQ16) > 0)
		{
			O.SurfaceZ = Z;
			break;
		}
	O.Biome = FVoxelBiomeResolver::Resolve(S, Noise, X, Y, O.SurfaceZ);
	return O;
}
