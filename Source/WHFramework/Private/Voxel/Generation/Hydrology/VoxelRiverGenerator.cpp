#include "Voxel/Generation/Hydrology/VoxelRiverGenerator.h"

#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	int32 ApproxLength(
		const int32 InX,
		const int32 InY)
	{
		const int32 AbsX = FMath::Abs(InX);
		const int32 AbsY = FMath::Abs(InY);

		return
			FMath::Max(AbsX, AbsY) +
			FMath::Min(AbsX, AbsY) * 3 / 8;
	}

	int32 SmoothRiverQ16(
		const int32 InAlphaQ16)
	{
		const int64 X = FMath::Clamp(InAlphaQ16, 0, 65536);
		const int64 X2 = X * X / 65536;
		const int64 X3 = X2 * X / 65536;

		return static_cast<int32>(3 * X2 - 2 * X3);
	}
}

FVoxelRiverGenerator::FVoxelRiverGenerator(
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
	TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain)
	: Recipe(InRecipe)
	, Terrain(InTerrain)
{
}

bool FVoxelRiverGenerator::ApplyToColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& InOutColumn) const
{
	const FVoxelGenerationSettings& Settings = Recipe->Settings;
	const int32 WaterHeight = SampleWaterHeight();

	if (InOutColumn.bOcean ||
		InOutColumn.SurfaceZ <= Settings.SeaLevel ||
		(InOutColumn.Climate.ContinentalnessQ15 < 2621 &&
		 InOutColumn.SurfaceZ < WaterHeight))
	{
		return false;
	}

	const int32 Distance =
		SampleClosedRiverDistance(InX, InY);

	const int32 RiverHalfWidth =
		FMath::Max(1, Settings.RiverBaseHalfWidth);

	const int32 BankWidth =
		FMath::Max(1, RiverHalfWidth);

	const int32 OuterWidth =
		RiverHalfWidth + BankWidth;

	if (Distance < OuterWidth)
	{
		return ApplyRiverProfile(
			InX,
			InY,
			Distance,
			InOutColumn);
	}

	return ApplyRiverShore(
		InX,
		InY,
		Distance,
		InOutColumn);
}

int32 FVoxelRiverGenerator::SampleRiverDistance(
	const int32 InX,
	const int32 InY) const
{
	const int32 ChannelDistance =
		FMath::Abs(
			SampleTerrainAwareRiverField(
				InX,
				InY));

	constexpr int32 GradientRadius = 2;

	const int32 GradientX =
		SampleBaseRiverField(
			InX + GradientRadius,
			InY) -
		SampleBaseRiverField(
			InX - GradientRadius,
			InY);

	const int32 GradientY =
		SampleBaseRiverField(
			InX,
			InY + GradientRadius) -
		SampleBaseRiverField(
			InX,
			InY - GradientRadius);

	const int32 Gradient =
		FMath::Max(
			1,
			ApproxLength(
				GradientX,
				GradientY));

	return FMath::Clamp(
		ChannelDistance *
			GradientRadius *
			2 /
			Gradient,
		0,
		65536);
}

int32 FVoxelRiverGenerator::SampleNormalizedDistanceQ16(
	const int32 InX,
	const int32 InY) const
{
	const int32 RiverHalfWidth =
		FMath::Max(
			1,
			Recipe->Settings.RiverBaseHalfWidth);

	const int32 BankWidth =
		FMath::Max(
			1,
			RiverHalfWidth);

	const int32 OuterWidth =
		RiverHalfWidth +
		BankWidth;

	return FMath::Clamp(
		SampleClosedRiverDistance(
			InX,
			InY) *
			65536 /
			FMath::Max(
				1,
				OuterWidth),
		0,
		MAX_int32);
}

FVector2D FVoxelRiverGenerator::SampleRiverDirection(
	const int32 InX,
	const int32 InY) const
{
	constexpr int32 SampleRadius = 3;

	const double GradientX =
		static_cast<double>(
			SampleTerrainAwareRiverField(
				InX + SampleRadius,
				InY) -
			SampleTerrainAwareRiverField(
				InX - SampleRadius,
				InY));

	const double GradientY =
		static_cast<double>(
			SampleTerrainAwareRiverField(
				InX,
				InY + SampleRadius) -
			SampleTerrainAwareRiverField(
				InX,
				InY - SampleRadius));

	const FVector2D Tangent(
		-GradientY,
		GradientX);

	return Tangent.IsNearlyZero()
		? FVector2D(1.0, 0.0)
		: Tangent.GetSafeNormal();
}

int32 FVoxelRiverGenerator::SampleWaterHeight() const
{
	return
		Recipe->Settings.SeaLevel +
		RiverHeightAboveSea;
}

int32 FVoxelRiverGenerator::SampleBaseRiverField(
	const int32 InX,
	const int32 InY) const
{
	const FVoxelGenerationSettings& Settings =
		Recipe->Settings;

	const int32 WarpX =
		VoxelGeneration::Noise2D(
			Settings.Seed,
			InX,
			InY,
			WarpPeriod,
			0xA9F21B330C11F973ull) *
		32 /
		32768;

	const int32 WarpY =
		VoxelGeneration::Noise2D(
			Settings.Seed,
			InX,
			InY,
			WarpPeriod,
			0xD17AC99E528703B1ull) *
		32 /
		32768;

	const int32 WarpedX =
		InX +
		WarpX;

	const int32 WarpedY =
		InY +
		WarpY;

	const int32 MainChannel =
		VoxelGeneration::Noise2D(
			Settings.Seed,
			WarpedX,
			WarpedY,
			RiverPeriod,
			0x7D58A33B40B99817ull);

	const int32 DetailChannel =
		VoxelGeneration::Noise2D(
			Settings.Seed,
			WarpedX,
			WarpedY,
			DetailRiverPeriod,
			0x83B7C132193F4E67ull);

	return
		MainChannel +
		DetailChannel *
			22 /
			100;
}

int32 FVoxelRiverGenerator::SampleTerrainAwareRiverField(
	const int32 InX,
	const int32 InY) const
{
	int32 Field =
		SampleBaseRiverField(
			InX,
			InY);

	const int32 DifficultyQ15 =
		SampleTerrainDifficultyQ15(
			InX,
			InY);

	const int32 AvoidanceNoise =
		VoxelGeneration::Noise2D(
			Recipe->Settings.Seed,
			InX,
			InY,
			RiverPeriod * 4,
			0x4B6510D627D33E19ull);

	Field +=
		static_cast<int32>(
			static_cast<int64>(
				AvoidanceNoise) *
			DifficultyQ15 *
			22 /
			100 /
			32768);

	return Field;
}

int32 FVoxelRiverGenerator::SampleTerrainDifficultyQ15(
	const int32 InX,
	const int32 InY) const
{
	const FVoxelMacroTerrainSample Center =
		Terrain->SampleMacro(
			InX,
			InY);

	int32 MinHeight = Center.SurfaceZ;
	int32 MaxHeight = Center.SurfaceZ;

	static const FIntPoint Samples[] =
	{
		FIntPoint(4, 0),
		FIntPoint(-4, 0),
		FIntPoint(0, 4),
		FIntPoint(0, -4),
		FIntPoint(3, 3),
		FIntPoint(-3, 3),
		FIntPoint(3, -3),
		FIntPoint(-3, -3)
	};

	for (const FIntPoint& Offset : Samples)
	{
		const int32 Height =
			Terrain->SampleMacro(
				InX + Offset.X,
				InY + Offset.Y).
				SurfaceZ;

		MinHeight =
			FMath::Min(
				MinHeight,
				Height);

		MaxHeight =
			FMath::Max(
				MaxHeight,
				Height);
	}

	const int32 Relief =
		MaxHeight -
		MinHeight;

	const int32 ReliefQ15 =
		FMath::Clamp(
			(Relief - 2) *
				32768 /
				7,
			0,
			32767);

	const int32 Altitude =
		Center.SurfaceZ -
		Recipe->Settings.SeaLevel;

	const int32 AltitudeQ15 =
		FMath::Clamp(
			(Altitude - 16) *
				32768 /
				28,
			0,
			32767);

	const int32 SlopeQ15 =
		FMath::Clamp(
			Center.SlopePermille *
				32767 /
				1000,
			0,
			32767);

	return FMath::Max3(
		ReliefQ15,
		AltitudeQ15 *
			65 /
			100,
		SlopeQ15);
}

int32 FVoxelRiverGenerator::SampleBankNoiseQ15(
	const int32 InX,
	const int32 InY) const
{
	const int32 Large =
		VoxelGeneration::Noise2D(
			Recipe->Settings.Seed,
			InX,
			InY,
			29,
			0x9B6164F91022398Dull);

	const int32 Detail =
		VoxelGeneration::Noise2D(
			Recipe->Settings.Seed,
			InX,
			InY,
			13,
			0x01B83AC7D1ED443Full);

	return FMath::Clamp(
		Large *
			72 /
			100 +
		Detail *
			28 /
			100,
		-32768,
		32767);
}

int32 FVoxelRiverGenerator::SampleClosedRiverDistance(
	const int32 InX,
	const int32 InY) const
{
	const int32 Distance =
		SampleRiverDistance(
			InX,
			InY);

	const int32 RiverHalfWidth =
		FMath::Max(
			1,
			Recipe->Settings.RiverBaseHalfWidth);

	const int32 OuterWidth =
		RiverHalfWidth * 2;

	if (Distance <
			OuterWidth ||
		Distance >=
			OuterWidth + 8)
	{
		return Distance;
	}

	const int32 Left =
		SampleRiverDistance(
			InX - 1,
			InY);

	const int32 Right =
		SampleRiverDistance(
			InX + 1,
			InY);

	const int32 Backward =
		SampleRiverDistance(
			InX,
			InY - 1);

	const int32 Forward =
		SampleRiverDistance(
			InX,
			InY + 1);

	if ((Left < OuterWidth &&
		 Right < OuterWidth) ||
		(Backward < OuterWidth &&
		 Forward < OuterWidth))
	{
		return OuterWidth - 1;
	}

	return Distance;
}

bool FVoxelRiverGenerator::ApplyRiverProfile(
	const int32 InX,
	const int32 InY,
	const int32 InDistance,
	FVoxelColumnSample& InOutColumn) const
{
	const FVoxelGenerationSettings& Settings =
		Recipe->Settings;

	const int32 WaterHeight =
		SampleWaterHeight();

	const int32 RiverHalfWidth =
		FMath::Max(
			1,
			Settings.RiverBaseHalfWidth);

	const int32 BankWidth =
		FMath::Max(
			1,
			RiverHalfWidth);

	const int32 OuterWidth =
		RiverHalfWidth +
		BankWidth;

	const int32 OriginalHeight =
		InOutColumn.SurfaceZ;

	const int32 DepthNoise =
		VoxelGeneration::Noise2D(
			Settings.Seed,
			InX,
			InY,
			DepthNoisePeriod,
			0xD20177B64A82F693ull);

	const int32 MinRiverDepth =
		FMath::Max(
			1,
			Settings.RiverBaseDepth -
			2);

	const int32 LocalRiverDepth =
		MinRiverDepth +
		(DepthNoise + 32768) *
			(Settings.RiverBaseDepth -
			 MinRiverDepth) /
			65536;

	const int32 BedHeight =
		FMath::Max(
			Settings.MinZ + 1,
			WaterHeight -
				LocalRiverDepth);

	const int32 CrossSectionAlphaQ16 =
		FMath::Clamp(
			InDistance *
				65536 /
				FMath::Max(
					1,
					OuterWidth),
			0,
			65536);

	const int32 CoreAlphaQ16 =
		FMath::Clamp(
			RiverHalfWidth *
				65536 /
				FMath::Max(
					1,
					OuterWidth),
			0,
			62259);

	int32 TargetHeight =
		BedHeight;

	if (CrossSectionAlphaQ16 >
		CoreAlphaQ16)
	{
		const int32 BankAlphaQ16 =
			FMath::Clamp(
				(CrossSectionAlphaQ16 -
				 CoreAlphaQ16) *
				65536 /
				FMath::Max(
					1,
					65536 -
					CoreAlphaQ16),
				0,
				65536);

		TargetHeight =
			BedHeight +
			(LocalRiverDepth + 1) *
				SmoothRiverQ16(
					BankAlphaQ16) /
				65536;

		TargetHeight =
			FMath::Min(
				TargetHeight,
				WaterHeight);
	}

	InOutColumn.SurfaceZ =
		FMath::Min(
			OriginalHeight,
			TargetHeight);

	InOutColumn.bRiver = true;

	if (InOutColumn.SurfaceZ <=
		WaterHeight)
	{
		InOutColumn.SurfaceWaterZ =
			FMath::Max(
				InOutColumn.SurfaceWaterZ,
				WaterHeight);
	}

	return true;
}

bool FVoxelRiverGenerator::ApplyRiverShore(
	const int32 InX,
	const int32 InY,
	const int32 InDistance,
	FVoxelColumnSample& InOutColumn) const
{
	const FVoxelGenerationSettings& Settings =
		Recipe->Settings;

	const int32 WaterHeight =
		SampleWaterHeight();

	const int32 RiverHalfWidth =
		FMath::Max(
			1,
			Settings.RiverBaseHalfWidth);

	const int32 OuterWidth =
		RiverHalfWidth * 2;

	const int32 BankNoiseQ15 =
		SampleBankNoiseQ15(
			InX,
			InY);

	const int64 ShoreProgressQ16 =
		static_cast<int64>(
			InDistance -
			OuterWidth) *
			10 *
			65536 /
			FMath::Max(
				1,
				OuterWidth) +
		static_cast<int64>(
			BankNoiseQ15) *
			28 /
			10;

	const int32 ShoreStep =
		FMath::Max(
			1,
			static_cast<int32>(
				FMath::Clamp<int64>(
					ShoreProgressQ16 /
						65536 +
					1,
					1,
					MAX_int32)));

	const int32 MaximumShoreStep =
		FMath::Max(
			Settings.RiverBaseDepth *
				4,
			8);

	if (ShoreStep >
		MaximumShoreStep)
	{
		return false;
	}

	const int32 MinimumHeight =
		WaterHeight -
		ShoreStep +
		1;

	const int32 MaximumHeight =
		WaterHeight +
		ShoreStep;

	InOutColumn.SurfaceZ =
		FMath::Clamp(
			InOutColumn.SurfaceZ,
			MinimumHeight,
			MaximumHeight);

	if (InOutColumn.SurfaceZ <=
		WaterHeight)
	{
		InOutColumn.SurfaceWaterZ =
			FMath::Max(
				InOutColumn.SurfaceWaterZ,
				WaterHeight);

		InOutColumn.bRiver = true;
	}

	return true;
}
