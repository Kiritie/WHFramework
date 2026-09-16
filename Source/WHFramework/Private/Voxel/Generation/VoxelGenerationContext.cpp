#include "Voxel/Generation/VoxelGenerationContext.h"
bool FVoxelGenerationSettings::Validate(FString& E) const
{
	if (MinZ >= MaxZ || MinZ < -32768 || MaxZ > 32768 || int64(MaxZ) - MinZ > 4096)
	{
		E = TEXT("Invalid height range");
		return false;
	}
	if (SeaLevel < MinZ || SeaLevel >= MaxZ || BaseHeight < MinZ || BaseHeight >= MaxZ)
	{
		E = TEXT("Invalid sea/base height");
		return false;
	}
	const int32 P[] = {ContinentalPeriod, ErosionPeriod, MountainPeriod, ClimatePeriod, DetailPeriod};
	for (int32 V : P)
		if (V < 2 || V > 1048576)
		{
			E = TEXT("Noise period out of range");
			return false;
		}
	if (ContinentalAmplitude < 0 || ContinentalAmplitude > 512 || MountainAmplitude < 0 || MountainAmplitude > 512 || DetailAmplitude < 0 ||
	    DetailAmplitude > 64)
	{
		E = TEXT("Noise amplitude out of range");
		return false;
	}
	E.Reset();
	return true;
}
