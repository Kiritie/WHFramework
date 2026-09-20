#include "Voxel/Streaming/VoxelInterestManager.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	constexpr int32 InterestSectionSide = 16;

	int32 CeilDividePositive(const int32 InValue, const int32 InDivisor)
	{
		return FMath::Max(0, (InValue + InDivisor - 1) / InDivisor);
	}

	FIntVector InterestToSection(const FIntVector& InCell)
	{
		return FIntVector(
			VoxelGeneration::FloorDivide(InCell.X, InterestSectionSide),
			VoxelGeneration::FloorDivide(InCell.Y, InterestSectionSide),
			VoxelGeneration::FloorDivide(InCell.Z, InterestSectionSide));
	}

	bool IsInsideRadius(const FIntVector& InDeltaCells, const int32 InRadius)
	{
		if (InRadius < 0)
		{
			return false;
		}
		const int64 RadiusSquared = static_cast<int64>(InRadius) * InRadius;
		return static_cast<int64>(InDeltaCells.X) * InDeltaCells.X +
			static_cast<int64>(InDeltaCells.Y) * InDeltaCells.Y +
			static_cast<int64>(InDeltaCells.Z) * InDeltaCells.Z <= RadiusSquared;
	}

	uint8 ResolveScreenErrorLevel(
		const int32 InDistanceCells,
		const FVoxelStreamingSource& InSource,
		const FVoxelViewSettings& InSettings,
		const uint8 InMaximumLevel)
	{
		const double Distance = FMath::Max(1, InDistanceCells);
		const double HalfFov = FMath::DegreesToRadians(
			FMath::Clamp(InSource.VerticalFovDegrees, 1.0f, 179.0f) * 0.5f);
		const double ProjectionScale =
			FMath::Max(1, InSource.ViewportHeightPixels) /
			(2.0 * FMath::Tan(HalfFov));
		const double Target = FMath::Max(0.1f, InSettings.TargetScreenErrorPixels);
		uint8 Level = 0;
		while (Level < InMaximumLevel)
		{
			const double NextLevelError = static_cast<double>(1 << (Level + 1)) * ProjectionScale / Distance;
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
		const int32 InRadius,
		const int32 InTileSide,
		const uint8 InLevel,
		TSet<KeyType>& OutKeys)
	{
		const int32 CenterX = VoxelGeneration::FloorDivide(InCenter.X, InTileSide);
		const int32 CenterY = VoxelGeneration::FloorDivide(InCenter.Y, InTileSide);
		const int32 TileRadius = CeilDividePositive(InRadius, InTileSide);
		for (int32 Y = -TileRadius; Y <= TileRadius; ++Y)
		{
			for (int32 X = -TileRadius; X <= TileRadius; ++X)
			{
				const FIntPoint DeltaCells(X * InTileSide, Y * InTileSide);
				if (static_cast<int64>(DeltaCells.X) * DeltaCells.X +
					static_cast<int64>(DeltaCells.Y) * DeltaCells.Y >
					static_cast<int64>(InRadius + InTileSide) * (InRadius + InTileSide))
				{
					continue;
				}
				OutKeys.Add({ FIntPoint(CenterX + X, CenterY + Y), InLevel });
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
	for (const FVoxelStreamingSource& Source : InSources)
	{
		AddExactSource(Source, InManifest, InViewSettings, Result);
		AddViewSource(Source, InManifest, InViewSettings, Result);
	}
	return Result;
}

void FVoxelInterestManager::AddExactSource(
	const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings,
	FVoxelInterestSet& InOutInterest) const
{
	const int32 FineRadius = InViewSettings.FineRadius;
	const int32 Radius = FMath::Max(
		FMath::Max(InSource.ExactRadius, InSource.CollisionRadius),
		FMath::Max(InSource.SimulationRadius, FineRadius));
	const int32 HorizontalSections = CeilDividePositive(Radius, InterestSectionSide);
	const int32 VerticalSections = CeilDividePositive(InSource.VerticalExactRadius, InterestSectionSide);
	const FIntVector CenterSection = InterestToSection(InSource.Center);
	const FVector Direction = InSource.Direction.GetSafeNormal();
	const int32 MinSectionZ = VoxelGeneration::FloorDivide(InManifest.Settings.MinZ, InterestSectionSide);
	const int32 MaxSectionZ = VoxelGeneration::FloorDivide(InManifest.Settings.MaxZ - 1, InterestSectionSide);

	for (int32 Z = -VerticalSections; Z <= VerticalSections; ++Z)
	{
		for (int32 Y = -HorizontalSections; Y <= HorizontalSections; ++Y)
		{
			for (int32 X = -HorizontalSections; X <= HorizontalSections; ++X)
			{
				const FIntVector Key = CenterSection + FIntVector(X, Y, Z);
				if (Key.Z < MinSectionZ || Key.Z > MaxSectionZ)
				{
					continue;
				}

				const FIntVector SectionCenterCells = Key * InterestSectionSide + FIntVector(InterestSectionSide / 2);
				const FIntVector Delta = SectionCenterCells - InSource.Center;
				const bool bExact = IsInsideRadius(Delta, InSource.ExactRadius);
				const bool bCollision = InSource.bCollision && IsInsideRadius(Delta, InSource.CollisionRadius);
				const bool bSimulation = InSource.bSimulation && IsInsideRadius(Delta, InSource.SimulationRadius);
				const bool bFineRender = InSource.bRender && IsInsideRadius(Delta, FineRadius);
				if (!bExact && !bCollision && !bSimulation && !bFineRender)
				{
					continue;
				}

				FVoxelExactDemand& Demand = InOutInterest.Exact.FindOrAdd(Key);
				Demand.bData |= bExact || bCollision || bSimulation || bFineRender;
				Demand.bExact |= bExact;
				Demand.bCollision |= bCollision;
				Demand.bSimulation |= bSimulation;
				Demand.bFineRender |= bFineRender;
				const FVector DeltaVector(Delta);
				const double Distance = DeltaVector.Size();
				const double Forward = Distance > UE_DOUBLE_SMALL_NUMBER
					? FVector::DotProduct(Direction, DeltaVector / Distance)
					: 1.0;
				const double Priority = 1.0 / (1.0 + Distance) + FMath::Max(0.0, Forward) * 0.25;
				Demand.Priority = FMath::Max(Demand.Priority, Priority);
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

	const int32 ProxyRange = InViewSettings.VoxelProxyRadius;
	const uint8 ProxyLevel = ResolveScreenErrorLevel(
		FMath::Max(InViewSettings.FineRadius, ProxyRange / 2),
		InSource,
		InViewSettings,
		InViewSettings.MaximumVoxelProxyLevel);
	const int32 ProxySide = FMath::Max(InterestSectionSide, InViewSettings.VoxelProxyTileSide << ProxyLevel);
	const int32 ProxyRadius = CeilDividePositive(ProxyRange, ProxySide);
	const FIntVector ProxyCenter(
		VoxelGeneration::FloorDivide(InSource.Center.X, ProxySide),
		VoxelGeneration::FloorDivide(InSource.Center.Y, ProxySide),
		VoxelGeneration::FloorDivide(InSource.Center.Z, ProxySide));
	const int32 MinZ = VoxelGeneration::FloorDivide(InManifest.Settings.MinZ, ProxySide);
	const int32 MaxZ = VoxelGeneration::FloorDivide(InManifest.Settings.MaxZ - 1, ProxySide);
	for (int32 Z = -ProxyRadius; Z <= ProxyRadius; ++Z)
	{
		for (int32 Y = -ProxyRadius; Y <= ProxyRadius; ++Y)
		{
			for (int32 X = -ProxyRadius; X <= ProxyRadius; ++X)
			{
				if (X * X + Y * Y + Z * Z > ProxyRadius * ProxyRadius)
				{
					continue;
				}
				const FIntVector Coordinate = ProxyCenter + FIntVector(X, Y, Z);
				if (Coordinate.Z >= MinZ && Coordinate.Z <= MaxZ)
				{
					InOutInterest.VoxelProxy.Add({ Coordinate, ProxyLevel });
				}
			}
		}
	}

	AddTwoDimensionalTiles(
		InSource.Center,
		InViewSettings.SurfaceRadius,
		FMath::Max(1, InViewSettings.SurfaceTileSide << ResolveScreenErrorLevel(
			FMath::Max(InViewSettings.VoxelProxyRadius, InViewSettings.SurfaceRadius / 2),
			InSource,
			InViewSettings,
			InViewSettings.MaximumSurfaceLevel)),
		ResolveScreenErrorLevel(
			FMath::Max(InViewSettings.VoxelProxyRadius, InViewSettings.SurfaceRadius / 2),
			InSource,
			InViewSettings,
			InViewSettings.MaximumSurfaceLevel),
		InOutInterest.Surface);
	AddTwoDimensionalTiles(
		InSource.Center,
		InViewSettings.MacroRadius,
		FMath::Max(1, InViewSettings.MacroTileSide << ResolveScreenErrorLevel(
			FMath::Max(InViewSettings.SurfaceRadius, InViewSettings.MacroRadius / 2),
			InSource,
			InViewSettings,
			InViewSettings.MaximumMacroLevel)),
		ResolveScreenErrorLevel(
			FMath::Max(InViewSettings.SurfaceRadius, InViewSettings.MacroRadius / 2),
			InSource,
			InViewSettings,
			InViewSettings.MaximumMacroLevel),
		InOutInterest.Macro);
}
