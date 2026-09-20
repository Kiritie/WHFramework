#include "Voxel/Streaming/VoxelInterestManager.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelViewLod.h"

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

	template<typename KeyType>
	void AddAdaptiveTwoDimensionalTiles(
		const FIntVector& InCenter,
		const int32 InOuterRadius,
		const int32 InInnerRadius,
		const int32 InBaseTileSide,
		const int32 InBaseSampleStepCells,
		const uint8 InMaximumLevel,
		const int32 InMaximumTiles,
		const FVoxelStreamingSource& InSource,
		const FVoxelViewSettings& InSettings,
		TSet<KeyType>& OutKeys)
	{
		if (InOuterRadius <= 0 ||
			InBaseTileSide <= 0)
		{
			return;
		}

		struct FNode
		{
			FIntPoint Coordinate;
			uint8 Level = 0;
		};

		const int32 RootSide =
			InBaseTileSide << InMaximumLevel;
		const int32 RootX =
			VoxelGeneration::FloorDivide(InCenter.X, RootSide);
		const int32 RootY =
			VoxelGeneration::FloorDivide(InCenter.Y, RootSide);
		const int32 RootRadius =
			CeilDividePositive(
				InOuterRadius,
				RootSide) +
			1;

		TArray<FNode> Stack;

		for (int32 Y = -RootRadius;
			Y <= RootRadius;
			++Y)
		{
			for (int32 X = -RootRadius;
				X <= RootRadius;
				++X)
			{
				Stack.Add({ FIntPoint(RootX + X, RootY + Y), InMaximumLevel });
			}
		}

		int32 Added = 0;
		while (!Stack.IsEmpty())
		{
			const FNode Node = Stack.Pop(EAllowShrinking::No);
			const int32 Side = InBaseTileSide << Node.Level;
			const FVector2D TileCenter(
				Node.Coordinate.X * Side + Side * 0.5,
				Node.Coordinate.Y * Side + Side * 0.5);
			const double Distance =
				(TileCenter - FVector2D(InCenter.X, InCenter.Y)).Size();
			const double HalfDiagonal = static_cast<double>(Side) * 0.7071067811865476;

			if (Distance - HalfDiagonal > InOuterRadius ||
				(InInnerRadius > 0 && Distance + HalfDiagonal <= InInnerRadius))
			{
				continue;
			}

			const int32 NearDistance = FMath::Max(
				1,
				FMath::FloorToInt(FMath::Max(0.0, Distance - HalfDiagonal)));
			const uint8 DesiredLevel = VoxelViewLod::ResolveScreenErrorLevel(
				NearDistance,
				InBaseSampleStepCells,
				InSource,
				InSettings,
				InMaximumLevel);
			const bool bCanSubdivide =
				Node.Level > DesiredLevel &&
				Node.Level > 0 &&
				Added + Stack.Num() + 4 < InMaximumTiles;

			if (bCanSubdivide)
			{
				const uint8 ChildLevel = Node.Level - 1;
				const FIntPoint ChildBase = Node.Coordinate * 2;
				Stack.Add({ ChildBase + FIntPoint(0, 0), ChildLevel });
				Stack.Add({ ChildBase + FIntPoint(1, 0), ChildLevel });
				Stack.Add({ ChildBase + FIntPoint(0, 1), ChildLevel });
				Stack.Add({ ChildBase + FIntPoint(1, 1), ChildLevel });
				continue;
			}

			OutKeys.Add({ Node.Coordinate, Node.Level });
			++Added;
		}
	}
}

FVoxelInterestSet FVoxelInterestManager::Compute(
	TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewInterestPlan);

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

	const bool bWantsFine =
		InSource.RenderMode != EVoxelStreamingRenderMode::None;

	const int32 FineRadius =
		bWantsFine
			? FMath::Min(
				FMath::Max(
					0,
					InViewSettings.FineRadius),
				ExactRadius)
			: 0;

	const int32 WarmupDataRadius =
		FMath::Min(
			FMath::Max(0, InViewSettings.WarmupDataRadius),
			ExactRadius);

	const int32 WarmupCollisionRadius =
		InSource.bCollision
			? FMath::Min(
				FMath::Max(0, InViewSettings.WarmupCollisionRadius),
				CollisionRadius)
			: 0;

	const int32 MovementCriticalCollisionRadius =
		InSource.bCollision
			? FMath::Min(
				CollisionRadius,
				FMath::Max(0, InSource.MovementCriticalCollisionRadius))
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
					bWantsFine &&
					IsInsideRadius(
						Delta,
						FineRadius);

				const bool bWarmupData =
					IsInsideRadius(Delta, WarmupDataRadius);

				const bool bWarmupCollision =
					InSource.bCollision &&
					IsInsideRadius(Delta, WarmupCollisionRadius);

				const bool bMovementCriticalCollision =
					InSource.bCollision &&
					MovementCriticalCollisionRadius > 0 &&
					IsInsideRadius(Delta, MovementCriticalCollisionRadius);

				if (!bExact &&
					!bCollision &&
					!bSimulation &&
					!bFineRender &&
					!bWarmupData &&
					!bWarmupCollision &&
					!bMovementCriticalCollision)
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
				Demand.bWarmupData |= bWarmupData;
				Demand.bWarmupCollision |= bWarmupCollision;
				Demand.bMovementCriticalCollision |= bMovementCriticalCollision;

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
	if (InSource.RenderMode != EVoxelStreamingRenderMode::Full)
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
				VoxelViewLod::ResolveScreenErrorLevel(
					FMath::Max(
						FineRadius,
						ProxyRange / 2),
					1,
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
		AddAdaptiveTwoDimensionalTiles(
			InSource.Center,
			SurfaceRange,
			ProxyRange,
			InViewSettings.SurfaceTileSide,
			1,
			InViewSettings.MaximumSurfaceLevel,
			InViewSettings.MaximumSurfaceTilesPerSource,
			InSource,
			InViewSettings,
			InOutInterest.Surface);
	}

	const int32 MacroRange =
		FMath::Max(
			SurfaceRange,
			InViewSettings.MacroRadius);

	if (MacroRange > SurfaceRange)
	{
		AddAdaptiveTwoDimensionalTiles(
			InSource.Center,
			MacroRange,
			SurfaceRange,
			InViewSettings.MacroTileSide,
			FVoxelMacroTileData::BaseStep,
			InViewSettings.MaximumMacroLevel,
			InViewSettings.MaximumMacroTilesPerSource,
			InSource,
			InViewSettings,
			InOutInterest.Macro);
	}
}
