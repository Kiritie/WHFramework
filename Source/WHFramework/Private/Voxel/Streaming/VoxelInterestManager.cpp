#include "Voxel/Streaming/VoxelInterestManager.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	constexpr int32 InterestSectionSide = 16;

	int32 CeilDividePositive(
		const int32 InValue,
		const int32 InDivisor)
	{
		return FMath::Max(
			0,
			(InValue + InDivisor - 1) /
			InDivisor);
	}

	FIntVector InterestToSection(
		const FIntVector& InCell)
	{
		return FIntVector(
			VoxelGeneration::FloorDivide(
				InCell.X,
				InterestSectionSide),
			VoxelGeneration::FloorDivide(
				InCell.Y,
				InterestSectionSide),
			VoxelGeneration::FloorDivide(
				InCell.Z,
				InterestSectionSide));
	}

	bool IsInsideRadius(
		const FIntVector& InDeltaCells,
		const int32 InRadius)
	{
		if (InRadius < 0)
		{
			return false;
		}

		const int64 RadiusSquared =
			static_cast<int64>(InRadius) *
			InRadius;

		return
			static_cast<int64>(InDeltaCells.X) *
				InDeltaCells.X +
			static_cast<int64>(InDeltaCells.Y) *
				InDeltaCells.Y +
			static_cast<int64>(InDeltaCells.Z) *
				InDeltaCells.Z <=
			RadiusSquared;
	}

	uint8 ResolveScreenErrorLevel(
		const int32 InDistanceCells,
		const FVoxelStreamingSource& InSource,
		const FVoxelViewSettings& InSettings,
		const uint8 InMaximumLevel)
	{
		const double Distance =
			FMath::Max(1, InDistanceCells);

		const double HalfFov =
			FMath::DegreesToRadians(
				FMath::Clamp(
					InSource.VerticalFovDegrees,
					1.0f,
					179.0f) *
				0.5f);

		const double ProjectionScale =
			FMath::Max(
				1,
				InSource.ViewportHeightPixels) /
			(2.0 * FMath::Tan(HalfFov));

		const double Target =
			FMath::Max(
				0.1f,
				InSettings.TargetScreenErrorPixels);

		uint8 Level = 0;

		while (Level < InMaximumLevel)
		{
			const double NextLevelError =
				static_cast<double>(
					1 << (Level + 1)) *
				ProjectionScale /
				Distance;

			if (NextLevelError > Target)
			{
				break;
			}

			++Level;
		}

		return Level;
	}

	template<typename KeyType>
	void AddTwoDimensionalTiles(
		const FIntVector& InCenter,
		const int32 InOuterRadius,
		const int32 InInnerRadius,
		const int32 InTileSide,
		const uint8 InLevel,
		TSet<KeyType>& OutKeys)
	{
		if (InOuterRadius <= 0 ||
			InTileSide <= 0)
		{
			return;
		}

		const int32 CenterX =
			VoxelGeneration::FloorDivide(
				InCenter.X,
				InTileSide);

		const int32 CenterY =
			VoxelGeneration::FloorDivide(
				InCenter.Y,
				InTileSide);

		const int32 TileRadius =
			CeilDividePositive(
				InOuterRadius,
				InTileSide) +
			1;

		const double HalfDiagonal =
			static_cast<double>(InTileSide) *
			0.7071067811865476;

		const double InnerRadius =
			FMath::Clamp(
				static_cast<double>(InInnerRadius),
				0.0,
				static_cast<double>(InOuterRadius));

		for (int32 Y = -TileRadius;
			Y <= TileRadius;
			++Y)
		{
			for (int32 X = -TileRadius;
				X <= TileRadius;
				++X)
			{
				const FIntPoint Coordinate(
					CenterX + X,
					CenterY + Y);

				const FVector2D TileCenter(
					Coordinate.X * InTileSide +
						InTileSide * 0.5,
					Coordinate.Y * InTileSide +
						InTileSide * 0.5);

				const FVector2D Delta =
					TileCenter -
					FVector2D(
						InCenter.X,
						InCenter.Y);

				const double Distance =
					Delta.Size();

				if (Distance - HalfDiagonal >
					InOuterRadius)
				{
					continue;
				}

				if (InnerRadius > 0.0 &&
					Distance + HalfDiagonal <=
						InnerRadius)
				{
					continue;
				}

				OutKeys.Add({
					Coordinate,
					InLevel
				});
			}
		}
	}
}

FVoxelInterestSet FVoxelInterestManager::Compute(
	TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings) const
{
	FVoxelInterestSet Result;

	for (const FVoxelStreamingSource& Source :
		InSources)
	{
		AddExactSource(
			Source,
			InManifest,
			InViewSettings,
			Result);

		AddViewSource(
			Source,
			InManifest,
			InViewSettings,
			Result);
	}

	return Result;
}

void FVoxelInterestManager::AddExactSource(
	const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings,
	FVoxelInterestSet& InOutInterest) const
{
	const int32 ExactRadius =
		FMath::Max(0, InSource.ExactRadius);

	const int32 CollisionRadius =
		InSource.bCollision
			? FMath::Max(
				0,
				InSource.CollisionRadius)
			: 0;

	const int32 SimulationRadius =
		InSource.bSimulation
			? FMath::Max(
				0,
				InSource.SimulationRadius)
			: 0;

	const int32 FineRadius =
		InSource.bRender
			? FMath::Min(
				FMath::Max(
					0,
					InViewSettings.FineRadius),
				ExactRadius)
			: 0;

	const int32 DataRadius =
		FMath::Max(
			FMath::Max(
				ExactRadius,
				CollisionRadius),
			SimulationRadius);

	const int32 HorizontalSections =
		CeilDividePositive(
			DataRadius,
			InterestSectionSide);

	const int32 VerticalSections =
		CeilDividePositive(
			FMath::Max(
				0,
				InSource.VerticalExactRadius),
			InterestSectionSide);

	const FIntVector CenterSection =
		InterestToSection(
			InSource.Center);

	const FVector Direction =
		InSource.Direction.GetSafeNormal();

	const int32 MinSectionZ =
		VoxelGeneration::FloorDivide(
			InManifest.Settings.MinZ,
			InterestSectionSide);

	const int32 MaxSectionZ =
		VoxelGeneration::FloorDivide(
			InManifest.Settings.MaxZ - 1,
			InterestSectionSide);

	for (int32 Z = -VerticalSections;
		Z <= VerticalSections;
		++Z)
	{
		for (int32 Y = -HorizontalSections;
			Y <= HorizontalSections;
			++Y)
		{
			for (int32 X = -HorizontalSections;
				X <= HorizontalSections;
				++X)
			{
				const FIntVector Key =
					CenterSection +
					FIntVector(X, Y, Z);

				if (Key.Z < MinSectionZ ||
					Key.Z > MaxSectionZ)
				{
					continue;
				}

				const FIntVector SectionCenterCells =
					Key * InterestSectionSide +
					FIntVector(
						InterestSectionSide / 2);

				const FIntVector Delta =
					SectionCenterCells -
					InSource.Center;

				const bool bExact =
					IsInsideRadius(
						Delta,
						ExactRadius);

				const bool bCollision =
					InSource.bCollision &&
					IsInsideRadius(
						Delta,
						CollisionRadius);

				const bool bSimulation =
					InSource.bSimulation &&
					IsInsideRadius(
						Delta,
						SimulationRadius);

				const bool bFineRender =
					InSource.bRender &&
					IsInsideRadius(
						Delta,
						FineRadius);

				if (!bExact &&
					!bCollision &&
					!bSimulation &&
					!bFineRender)
				{
					continue;
				}

				FVoxelExactDemand& Demand =
					InOutInterest.Exact.
						FindOrAdd(Key);

				Demand.bData = true;
				Demand.bExact |= bExact;
				Demand.bCollision |= bCollision;
				Demand.bSimulation |= bSimulation;
				Demand.bFineRender |= bFineRender;

				const FVector DeltaVector(
					Delta);

				const double Distance =
					DeltaVector.Size();

				const double Forward =
					Distance >
					UE_DOUBLE_SMALL_NUMBER
						? FVector::DotProduct(
							Direction,
							DeltaVector /
								Distance)
						: 1.0;

				const double Priority =
					1.0 /
						(1.0 + Distance) +
					FMath::Max(
						0.0,
						Forward) *
						0.25;

				Demand.Priority =
					FMath::Max(
						Demand.Priority,
						Priority);
			}
		}
	}
}

void FVoxelInterestManager::AddViewSource(
	const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings,
	FVoxelInterestSet& InOutInterest) const
{
	if (!InSource.bRender)
	{
		return;
	}

	const int32 FineRadius =
		FMath::Min(
			FMath::Max(
				0,
				InViewSettings.FineRadius),
			FMath::Max(
				0,
				InSource.ExactRadius));

	const int32 ProxyRange =
		FMath::Max(
			FineRadius,
			InViewSettings.VoxelProxyRadius);

	if (ProxyRange > FineRadius)
	{
		const uint8 ProxyLevel =
			FMath::Max<uint8>(
				1,
				ResolveScreenErrorLevel(
					FMath::Max(
						FineRadius,
						ProxyRange / 2),
					InSource,
					InViewSettings,
					InViewSettings.
						MaximumVoxelProxyLevel));

		const int32 ProxySide =
			FMath::Max(
				InterestSectionSide,
				InViewSettings.
					VoxelProxyTileSide <<
				ProxyLevel);

		const int32 HorizontalRadius =
			CeilDividePositive(
				ProxyRange,
				ProxySide) +
			1;

		const int32 VerticalRadius =
			FMath::Max(
				1,
				CeilDividePositive(
					FMath::Max(
						0,
						InSource.
							VerticalExactRadius),
					ProxySide));

		const FIntVector ProxyCenter(
			VoxelGeneration::FloorDivide(
				InSource.Center.X,
				ProxySide),
			VoxelGeneration::FloorDivide(
				InSource.Center.Y,
				ProxySide),
			VoxelGeneration::FloorDivide(
				InSource.Center.Z,
				ProxySide));

		const int32 MinZ =
			VoxelGeneration::FloorDivide(
				InManifest.Settings.MinZ,
				ProxySide);

		const int32 MaxZ =
			VoxelGeneration::FloorDivide(
				InManifest.Settings.MaxZ - 1,
				ProxySide);

		const double HalfDiagonal =
			static_cast<double>(ProxySide) *
			0.8660254037844386;

		for (int32 Z = -VerticalRadius;
			Z <= VerticalRadius;
			++Z)
		{
			for (int32 Y = -HorizontalRadius;
				Y <= HorizontalRadius;
				++Y)
			{
				for (int32 X = -HorizontalRadius;
					X <= HorizontalRadius;
					++X)
				{
					const FIntVector Coordinate =
						ProxyCenter +
						FIntVector(X, Y, Z);

					if (Coordinate.Z < MinZ ||
						Coordinate.Z > MaxZ)
					{
						continue;
					}

					const FVector TileCenter =
						FVector(
							Coordinate *
								ProxySide) +
						FVector(
							ProxySide * 0.5);

					const double Distance =
						FVector::Distance(
							TileCenter,
							FVector(
								InSource.Center));

					if (Distance -
						HalfDiagonal >
						ProxyRange)
					{
						continue;
					}

					if (FineRadius > 0 &&
						Distance +
							HalfDiagonal <=
						FineRadius)
					{
						continue;
					}

					InOutInterest.
						VoxelProxy.Add({
							Coordinate,
							ProxyLevel
						});
				}
			}
		}
	}

	const int32 SurfaceRange =
		FMath::Max(
			ProxyRange,
			InViewSettings.SurfaceRadius);

	if (SurfaceRange > ProxyRange)
	{
		const uint8 SurfaceLevel =
			ResolveScreenErrorLevel(
				FMath::Max(
					ProxyRange,
					SurfaceRange / 2),
				InSource,
				InViewSettings,
				InViewSettings.
					MaximumSurfaceLevel);

		const int32 SurfaceSide =
			FMath::Max(
				1,
				InViewSettings.
					SurfaceTileSide <<
				SurfaceLevel);

		AddTwoDimensionalTiles(
			InSource.Center,
			SurfaceRange,
			ProxyRange,
			SurfaceSide,
			SurfaceLevel,
			InOutInterest.Surface);
	}

	const int32 MacroRange =
		FMath::Max(
			SurfaceRange,
			InViewSettings.MacroRadius);

	if (MacroRange > SurfaceRange)
	{
		const uint8 MacroLevel =
			ResolveScreenErrorLevel(
				FMath::Max(
					SurfaceRange,
					MacroRange / 2),
				InSource,
				InViewSettings,
				InViewSettings.
					MaximumMacroLevel);

		const int32 MacroSide =
			FMath::Max(
				1,
				InViewSettings.
					MacroTileSide <<
				MacroLevel);

		AddTwoDimensionalTiles(
			InSource.Center,
			MacroRange,
			SurfaceRange,
			MacroSide,
			MacroLevel,
			InOutInterest.Macro);
	}
}
