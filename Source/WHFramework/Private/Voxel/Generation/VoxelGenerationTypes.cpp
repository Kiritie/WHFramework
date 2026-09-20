#include "Voxel/Generation/VoxelGenerationTypes.h"

bool FVoxelGenerationRange::Contains(int32 InValue) const
{
	return InValue >= Min && InValue <= Max;
}

bool FVoxelGenerationRange::Validate(FString& OutError) const
{
	if (Min > Max)
	{
		OutError = TEXT("Voxel generation range minimum exceeds maximum");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelTreeGenerationSettings::Validate(FString& OutError) const
{
	if (MinHeight <= 0 || MaxHeight < MinHeight)
	{
		OutError = TEXT("Voxel tree height range is invalid");
		return false;
	}
	if (Spacing < 4)
	{
		OutError = TEXT("Voxel tree spacing must be at least four cells");
		return false;
	}
	if (DensityPermille < 0 || DensityPermille > 2000)
	{
		OutError = TEXT("Voxel tree density must be in [0, 2000] permille");
		return false;
	}
	if (ChancePermille < 0 || ChancePermille > 1000)
	{
		OutError = TEXT("Voxel tree chance must be in [0, 1000] permille");
		return false;
	}
	if (CrownRadius <= 0 || CrownRadius > Spacing / 2)
	{
		OutError = TEXT("Voxel tree crown radius is invalid relative to spacing");
		return false;
	}
	if (MaxSlopePermille < 0)
	{
		OutError = TEXT("Voxel tree maximum slope cannot be negative");
		return false;
	}
	if (!Temperature.Validate(OutError) || !Moisture.Validate(OutError))
	{
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelGrassGenerationSettings::Validate(FString& OutError) const
{
	if (Spacing < 2)
	{
		OutError = TEXT("Voxel grass spacing must be at least two cells");
		return false;
	}
	if (DensityPermille < 0 || DensityPermille > 2000)
	{
		OutError = TEXT("Voxel grass density must be in [0, 2000] permille");
		return false;
	}
	if (ChancePermille < 0 || ChancePermille > 1000)
	{
		OutError = TEXT("Voxel grass chance must be in [0, 1000] permille");
		return false;
	}
	if (PatchRadius < 0 || PatchRadius > Spacing)
	{
		OutError = TEXT("Voxel grass patch radius is invalid");
		return false;
	}
	if (PatchFillPermille < 0 || PatchFillPermille > 1000)
	{
		OutError = TEXT("Voxel grass patch fill must be in [0, 1000] permille");
		return false;
	}
	if (MaxSlopePermille < 0)
	{
		OutError = TEXT("Voxel grass maximum slope cannot be negative");
		return false;
	}
	if (!Temperature.Validate(OutError) || !Moisture.Validate(OutError))
	{
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelEcologyGenerationSettings::Validate(FString& OutError) const
{
	return Tree.Validate(OutError) && Grass.Validate(OutError);
}

bool FVoxelGenerationSettings::Validate(FString& OutError) const
{
	if (MinZ >= MaxZ) { OutError = TEXT("Voxel generation MinZ must be lower than MaxZ"); return false; }
	const int64 WorldHeight = static_cast<int64>(MaxZ) - static_cast<int64>(MinZ);
	if (WorldHeight <= 0 || WorldHeight > 1024ll * 1024ll) { OutError = TEXT("Voxel generation world height is outside the supported range"); return false; }
	if (SeaLevel < MinZ || SeaLevel >= MaxZ) { OutError = TEXT("Voxel generation SeaLevel is outside world height"); return false; }
	if (BaseHeight < MinZ || BaseHeight >= MaxZ) { OutError = TEXT("Voxel generation BaseHeight is outside world height"); return false; }
	if (ContinentalPeriod <= 0 || ErosionPeriod <= 0 || MountainPeriod <= 0 || ClimatePeriod <= 0 || DetailPeriod <= 0) { OutError = TEXT("Voxel generation noise periods must be positive"); return false; }
	if (ContinentalAmplitude < 0 || MountainAmplitude < 0 || DetailAmplitude < 0) { OutError = TEXT("Voxel generation terrain amplitudes cannot be negative"); return false; }
	if (HydrologyCellSize <= 0) { OutError = TEXT("Voxel hydrology cell size must be positive"); return false; }
	if (HydrologyRegionSide < 8 || HydrologyRegionSide > 2048) { OutError = TEXT("Voxel hydrology region side is outside the supported range"); return false; }
	if (RiverSourceAccumulation <= 0) { OutError = TEXT("Voxel river source accumulation must be positive"); return false; }
	if (RiverBaseHalfWidth <= 0 || RiverBaseHalfWidth > 256) { OutError = TEXT("Voxel river base half width is outside the supported range"); return false; }
	if (RiverBaseDepth <= 0 || RiverBaseDepth > 256) { OutError = TEXT("Voxel river base depth is outside the supported range"); return false; }
	if (HydrologyHaloCells < 8 || HydrologyHaloCells > HydrologyRegionSide) { OutError = TEXT("Voxel hydrology halo is outside the supported range"); return false; }
	if (HydrologySinkSpacing < 8 || HydrologySinkSpacing > HydrologyRegionSide) { OutError = TEXT("Voxel hydrology sink spacing is outside the supported range"); return false; }
	if (RiverSourceSpacing <= 0 || RiverSourceSpacing > HydrologyRegionSide) { OutError = TEXT("Voxel river source spacing is invalid"); return false; }
	if (RiverTraceBudget < 128 || RiverTraceBudget > 1024 * 1024) { OutError = TEXT("Voxel river trace budget is invalid"); return false; }
	if (LakeMaxCells < 64 || LakeMaxCells > 1024 * 1024) { OutError = TEXT("Voxel lake max cell count is invalid"); return false; }
	if (CaveSpacing < 32) { OutError = TEXT("Voxel cave spacing is too small"); return false; }
	if (CaveMinDepth <= 0 || CaveMaxDepth < CaveMinDepth) { OutError = TEXT("Voxel cave depth settings are invalid"); return false; }
	if (CaveMainRadius < 2 || CaveBranchRadius < 2) { OutError = TEXT("Voxel cave radii must be at least two cells"); return false; }
	if (CaveMainRadius > CaveSpacing / 2 || CaveBranchRadius > CaveSpacing / 2) { OutError = TEXT("Voxel cave radius is too large relative to cave spacing"); return false; }
	if (CaveSystemChancePermille < 0 || CaveSystemChancePermille > 1000) { OutError = TEXT("Voxel cave system chance must be in [0,1000]"); return false; }
	if (CaveEntranceChancePermille < 0 || CaveEntranceChancePermille > 1000) { OutError = TEXT("Voxel cave entrance chance must be in [0,1000]"); return false; }
	if (CaveEntranceLength < 4 || CaveEntranceLength > 64) { OutError = TEXT("Voxel cave entrance length is outside the supported range"); return false; }
	if (CaveEntranceDropPerStep < 1 || CaveEntranceDropPerStep > 4) { OutError = TEXT("Voxel cave entrance drop per step is outside the supported range"); return false; }
	if (CaveEntranceTransitionDepth < 4 || CaveEntranceTransitionDepth > CaveMaxDepth) { OutError = TEXT("Voxel cave entrance transition depth is invalid"); return false; }
	if (CaveRoomChancePermille < 0 || CaveRoomChancePermille > 1000 || CaveBranchChancePermille < 0 || CaveBranchChancePermille > 1000) { OutError = TEXT("Voxel cave room/branch chance must be in [0,1000]"); return false; }
	if (AquiferSpacing <= 0) { OutError = TEXT("Voxel aquifer spacing must be positive"); return false; }
	if (AquiferRadius <= 0 || AquiferRadius >= AquiferSpacing) { OutError = TEXT("Voxel aquifer radius must be smaller than aquifer spacing"); return false; }
	if (LavaCeiling < MinZ || LavaCeiling >= MaxZ) { OutError = TEXT("Voxel lava ceiling is outside world height"); return false; }
	if (!Ecology.Validate(OutError)) { return false; }
	OutError.Reset();
	return true;
}

bool FVoxelGenerationBounds::IsValid() const
{
	return Min.X < Max.X && Min.Y < Max.Y && Min.Z < Max.Z;
}

bool FVoxelGenerationBounds::Contains(const FIntVector& InPosition) const
{
	return InPosition.X >= Min.X && InPosition.Y >= Min.Y && InPosition.Z >= Min.Z && InPosition.X < Max.X && InPosition.Y < Max.Y && InPosition.Z < Max.Z;
}

bool FVoxelGenerationBounds::Intersects(const FVoxelGenerationBounds& InOther) const
{
	return Min.X < InOther.Max.X && Min.Y < InOther.Max.Y && Min.Z < InOther.Max.Z && Max.X > InOther.Min.X && Max.Y > InOther.Min.Y && Max.Z > InOther.Min.Z;
}

FVoxelGenerationBounds FVoxelGenerationBounds::Expand(int32 InCells) const
{
	const int64 Expansion = FMath::Max<int64>(0, InCells);
	auto ExpandMin = [Expansion](int32 InValue) { return static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(InValue) - Expansion, MIN_int32, MAX_int32)); };
	auto ExpandMax = [Expansion](int32 InValue) { return static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(InValue) + Expansion, MIN_int32, MAX_int32)); };
	FVoxelGenerationBounds Result;
	Result.Min = FIntVector(ExpandMin(Min.X), ExpandMin(Min.Y), ExpandMin(Min.Z));
	Result.Max = FIntVector(ExpandMax(Max.X), ExpandMax(Max.Y), ExpandMax(Max.Z));
	return Result;
}

bool FVoxelStableId::IsValid() const
{
	return High != 0 || Low != 0;
}

bool FVoxelStableId::operator==(const FVoxelStableId& InOther) const
{
	return High == InOther.High && Low == InOther.Low;
}

bool FVoxelStableId::operator!=(const FVoxelStableId& InOther) const
{
	return !(*this == InOther);
}

bool FVoxelStableId::operator<(const FVoxelStableId& InOther) const
{
	if (High != InOther.High) return High < InOther.High;
	return Low < InOther.Low;
}
