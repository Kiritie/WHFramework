#pragma once
#include "CoreMinimal.h"
class WHFRAMEWORK_API FVoxelNoiseRouter
{
public:
	explicit FVoxelNoiseRouter(int32 InSeed)
	    : Seed(InSeed)
	{
	}
	int32 Noise2D(int32 X, int32 Y, int32 Period, uint32 Salt) const;
	int32 Noise3D(int32 X, int32 Y, int32 Z, int32 Period, uint32 Salt) const;
	static int64 LerpQ16(int64 A, int64 B, int32 T);

private:
	int32 Lattice(int32 X, int32 Y, int32 Z, uint32 Salt) const;
	static int32 Fraction(int32 Value, int32 Cell, int32 Period);
	int32 Seed;
};
