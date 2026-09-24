#include "Voxel/Generation/Landform/VoxelLandform.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	constexpr uint64 WarpXSalt = 0xB5370BBD82D93A47ull;
	constexpr uint64 WarpYSalt = 0x5EA72B630A403175ull;
	constexpr uint64 DomainSalt = 0x84971DE23A9A4211ull;
	constexpr uint64 ReliefSalt = 0x673DDA9941FC872Bull;
	constexpr uint64 PlateauSalt = 0x12B3BAA1E54C7F19ull;

	int32 BandWeight(const int32 InValue, const int32 InCenter, const int32 InRadius)
	{
		return static_cast<int32>(FMath::Max<int64>(
			0,
			32767 - static_cast<int64>(FMath::Abs(InValue - InCenter)) * 32767 / InRadius));
	}
}

FVoxelLandformGenerator::FVoxelLandformGenerator(
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe)
	: Recipe(InRecipe)
{
}

FVoxelLandformSample FVoxelLandformGenerator::Sample(
	const int32 InX,
	const int32 InY,
	const FVoxelClimateSample& InClimate) const
{
	const FVoxelGenerationSettings& Generation = Recipe->Settings;
	const FVoxelLandformGenerationSettings& Settings = Generation.Landform;
	const int32 WarpX = static_cast<int32>(
		static_cast<int64>(VoxelGeneration::Noise2D(
			Generation.Seed, InX, InY, Settings.DomainPeriod, WarpXSalt)) *
		Settings.DomainWarpCells / 32768);
	const int32 WarpY = static_cast<int32>(
		static_cast<int64>(VoxelGeneration::Noise2D(
			Generation.Seed, InX, InY, Settings.DomainPeriod, WarpYSalt)) *
		Settings.DomainWarpCells / 32768);
	const int32 DomainX = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(InX) + WarpX, MIN_int32, MAX_int32));
	const int32 DomainY = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(InY) + WarpY, MIN_int32, MAX_int32));
	const int32 Domain = VoxelGeneration::Noise2D(
		Generation.Seed, DomainX, DomainY, Settings.DomainPeriod, DomainSalt);
	const int32 Relief = VoxelGeneration::Noise2D(
		Generation.Seed, DomainX, DomainY, Settings.ReliefPeriod, ReliefSalt);
	const int32 Plateau = VoxelGeneration::Noise2D(
		Generation.Seed, DomainX, DomainY, Settings.PlateauPeriod, PlateauSalt);

	FVoxelLandformSample Result;
	Result.PlainQ15 = BandWeight(Domain, -20000, 23000);
	Result.HillsQ15 = BandWeight(Domain, -8000, 20000);
	Result.HighlandQ15 = BandWeight(Domain, 7000, 17000);
	Result.MountainQ15 = BandWeight(Domain, 21000, 23000);
	Result.PlateauQ15 = static_cast<int32>(
		static_cast<int64>(BandWeight(Domain, 12000, 20000)) *
		FMath::Clamp(Plateau - 1000, 0, 20000) / 8000);
	Result.BasinQ15 = static_cast<int32>(
		static_cast<int64>(BandWeight(Domain, -5000, 22000)) *
		FMath::Clamp(-Relief - 2000, 0, 16000) / 8000);

	const int32 Ridge = FMath::Clamp(FMath::Abs(InClimate.RidgeQ15), 0, 32767);
	Result.MountainQ15 = static_cast<int32>(
		static_cast<int64>(Result.MountainQ15) * 3 *
		(24576 + Ridge / 4) / 32767);

	const int64 Sum = static_cast<int64>(Result.PlainQ15) + Result.HillsQ15 +
		Result.HighlandQ15 + Result.MountainQ15 + Result.PlateauQ15 + Result.BasinQ15;
	if (Sum == 0)
	{
		Result.PlainQ15 = 32767;
	}
	else
	{
		Result.PlainQ15 = static_cast<int32>(Result.PlainQ15 * 32767ll / Sum);
		Result.HillsQ15 = static_cast<int32>(Result.HillsQ15 * 32767ll / Sum);
		Result.HighlandQ15 = static_cast<int32>(Result.HighlandQ15 * 32767ll / Sum);
		Result.MountainQ15 = static_cast<int32>(Result.MountainQ15 * 32767ll / Sum);
		Result.PlateauQ15 = static_cast<int32>(Result.PlateauQ15 * 32767ll / Sum);
		Result.BasinQ15 = 32767 - Result.PlainQ15 - Result.HillsQ15 -
			Result.HighlandQ15 - Result.MountainQ15 - Result.PlateauQ15;
	}

	const int32 Weights[] =
	{
		Result.PlainQ15,
		Result.HillsQ15,
		Result.HighlandQ15,
		Result.MountainQ15,
		Result.PlateauQ15,
		Result.BasinQ15
	};
	const EVoxelLandformKind Kinds[] =
	{
		EVoxelLandformKind::Plain,
		EVoxelLandformKind::RollingHills,
		EVoxelLandformKind::Highland,
		EVoxelLandformKind::MountainRange,
		EVoxelLandformKind::Plateau,
		EVoxelLandformKind::Basin
	};
	int32 BestIndex = 0;
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Weights); ++Index)
	{
		if (Weights[Index] > Weights[BestIndex])
		{
			BestIndex = Index;
		}
	}
	Result.Dominant = Kinds[BestIndex];
	Result.ReliefQ15 = Relief;
	return Result;
}
