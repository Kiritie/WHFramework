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
			double Error = 0.0;
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

		TArray<FNode> Leaves;
		auto AddNode = [&](const FIntPoint& Coordinate, const uint8 Level, TArray<FNode>& Nodes)
		{
			const int32 Side = InBaseTileSide << Level;
			const FVector2D Delta(
				Coordinate.X * Side + Side * 0.5 - InCenter.X,
				Coordinate.Y * Side + Side * 0.5 - InCenter.Y);
			const FVector2D NearDelta(
				FMath::Max(0.0, FMath::Abs(Delta.X) - Side * 0.5),
				FMath::Max(0.0, FMath::Abs(Delta.Y) - Side * 0.5));
			const FVector2D FarDelta(FMath::Abs(Delta.X) + Side * 0.5, FMath::Abs(Delta.Y) + Side * 0.5);
			if (NearDelta.SizeSquared() > FMath::Square(static_cast<double>(InOuterRadius)) ||
				(InInnerRadius > 0 && FarDelta.SizeSquared() <= FMath::Square(static_cast<double>(InInnerRadius))))
			{
				return;
			}
			const double Distance = FMath::Max(1.0, NearDelta.Size());
			const uint8 DesiredLevel = VoxelViewLod::ResolveScreenErrorLevel(
				FMath::FloorToInt(Distance), InBaseSampleStepCells, InSource, InSettings, InMaximumLevel);
			const double Error = Level > DesiredLevel ? static_cast<double>(InBaseSampleStepCells << Level) / Distance : 0.0;
			Nodes.Add({ Coordinate, Level, Error });
		};

		for (int32 Y = -RootRadius;
			Y <= RootRadius;
			++Y)
		{
			for (int32 X = -RootRadius;
				X <= RootRadius;
				++X)
			{
				AddNode(FIntPoint(RootX + X, RootY + Y), InMaximumLevel, Leaves);
			}
		}

		while (!Leaves.IsEmpty())
		{
			double MaximumError = 0.0;
			for (const FNode& Node : Leaves)
			{
				MaximumError = FMath::Max(MaximumError, Node.Error);
			}
			if (MaximumError <= 0.0)
			{
				break;
			}
			TArray<FNode> Children;
			int32 ParentCount = 0;
			for (const FNode& Node : Leaves)
			{
				if (!FMath::IsNearlyEqual(Node.Error, MaximumError, UE_DOUBLE_SMALL_NUMBER))
				{
					continue;
				}
				++ParentCount;
				const FIntPoint ChildBase = Node.Coordinate * 2;
				for (int32 Y = 0; Y < 2; ++Y)
				{
					for (int32 X = 0; X < 2; ++X)
					{
						AddNode(ChildBase + FIntPoint(X, Y), Node.Level - 1, Children);
					}
				}
			}
			if (Leaves.Num() - ParentCount + Children.Num() > InMaximumTiles)
			{
				break;
			}
			Leaves.RemoveAll([MaximumError](const FNode& Node)
			{
				return FMath::IsNearlyEqual(Node.Error, MaximumError, UE_DOUBLE_SMALL_NUMBER);
			});
			Leaves.Append(MoveTemp(Children));
		}
		for (const FNode& Node : Leaves)
		{
			OutKeys.Add({ Node.Coordinate, Node.Level });
		}
	}
}

FVoxelInterestSet FVoxelInterestManager::Compute(
	TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings,
	const FVoxelInterestSet* InPrevious) const
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
			Result,
			InPrevious);

		AddViewSource(
			Source,
			InManifest,
			InViewSettings,
			Result);
	}

	if (!Result.VoxelProxy.IsEmpty())
	{
		TSet<FIntVector> FineSections;
		for (const TPair<FIntVector, FVoxelExactDemand>& Pair : Result.Exact)
		{
			if (Pair.Value.bFineRender)
			{
				FineSections.Add(Pair.Key);
			}
		}
		Result.TerrainPlan.Build(InSources, FineSections, Result.VoxelProxy, InViewSettings, InViewSettings.MaximumTerrainLeaves);
		Result.VoxelProxy.Reset();
		for (const FVoxelViewKey& Key : Result.TerrainPlan.Leaves)
		{
			if (Key.Level > 0)
			{
				Result.VoxelProxy.Add(Key);
				continue;
			}
			FVoxelExactDemand& Demand = Result.Exact.FindOrAdd(Key.Coordinate);
			Demand.bData = true;
			Demand.bFineRender = true;
			const FVector Center = FVector(Key.Coordinate * InterestSectionSide) + FVector(InterestSectionSide * 0.5);
			for (const FVoxelStreamingSource& Source : InSources)
			{
				if (Source.RenderMode != EVoxelStreamingRenderMode::None)
				{
					Demand.DistanceCells = FMath::Min(Demand.DistanceCells, FVector::Distance(Center, FVector(Source.Center)));
				}
			}
		}
	}
	// 精细边界需要真实相邻数据；只增加数据依赖，不扩张碰撞、模拟或可见几何。
	TArray<FIntVector> FineKeys;
	for (const auto& Pair : Result.Exact)
	{
		if (Pair.Value.bFineRender)
		{
			FineKeys.Add(Pair.Key);
		}
	}
	const auto FineSnapshot = MakeShared<TSet<FIntVector>, ESPMode::ThreadSafe>();
	FineSnapshot->Append(FineKeys);
	Result.FineSections = FineSnapshot;
	for (const FIntVector& Key : FineKeys)
	{
		const double Distance = Result.Exact.FindChecked(Key).DistanceCells;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Sign : { -1, 1 })
			{
				FIntVector Neighbor = Key;
				Neighbor[Axis] += Sign;
				if (Neighbor.Z * InterestSectionSide >= InManifest.Settings.MaxZ ||
					(Neighbor.Z + 1) * InterestSectionSide <= InManifest.Settings.MinZ)
				{
					continue;
				}
				FVoxelExactDemand& Demand = Result.Exact.FindOrAdd(Neighbor);
				Demand.bData = true;
				Demand.DistanceCells = FMath::Min(Demand.DistanceCells, Distance + InterestSectionSide);
			}
		}
	}

	// 排序随不可变范围快照在后台完成，主线程只消费索引，不重新分配、排序数万个节点。
	auto Distance = [&InSources](const FBox& Bounds, const bool bColumn)
	{
		double Best = MAX_dbl;
		for (const FVoxelStreamingSource& Source : InSources)
		{
			if (Source.RenderMode == EVoxelStreamingRenderMode::None) continue;
			FVector Position(Source.Center);
			if (bColumn) Position.Z = 0.0;
			Best = FMath::Min(Best, FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(Position)));
		}
		return Best;
	};
	for (const auto& Pair : Result.Exact)
	{
		if (!Pair.Value.bFineRender) continue;
		FVoxelViewAdmission& A = Result.Admissions.AddDefaulted_GetRef();
		A.Kind = EVoxelViewAdmissionKind::Fine;
		A.FineKey = Pair.Key;
		const FVector Center(Pair.Key * InterestSectionSide + FIntVector(InterestSectionSide / 2));
		A.DistanceCells = Distance(FBox(Center, Center), false);
	}
	for (const FVoxelViewKey& Key : Result.VoxelProxy)
	{
		FVoxelViewAdmission& A = Result.Admissions.AddDefaulted_GetRef();
		A.Kind = EVoxelViewAdmissionKind::VoxelProxy;
		A.ProxyKey = Key;
		const FVoxelGenerationBounds Bounds = Key.GetBounds();
		A.DistanceCells = Distance(FBox(FVector(Bounds.Min), FVector(Bounds.Max)), false);
	}
	for (const FVoxelSurfaceTileKey& Key : Result.Surface)
	{
		FVoxelViewAdmission& A = Result.Admissions.AddDefaulted_GetRef();
		A.Kind = EVoxelViewAdmissionKind::Surface;
		A.SurfaceKey = Key;
		const int32 Side = InViewSettings.SurfaceTileSide << Key.Level;
		const FVector Center((Key.Coordinate.X + 0.5) * Side, (Key.Coordinate.Y + 0.5) * Side, 0.0);
		A.DistanceCells = Distance(FBox(Center, Center), true);
	}
	for (const FVoxelMacroTileKey& Key : Result.Macro)
	{
		FVoxelViewAdmission& A = Result.Admissions.AddDefaulted_GetRef();
		A.Kind = EVoxelViewAdmissionKind::Macro;
		A.MacroKey = Key;
		const int32 Side = InViewSettings.MacroTileSide << Key.Level;
		const FVector Center((Key.Coordinate.X + 0.5) * Side, (Key.Coordinate.Y + 0.5) * Side, 0.0);
		A.DistanceCells = Distance(FBox(Center, Center), true);
	}
	Result.Admissions.Sort();
	for (int32 Index = 0; Index < Result.Admissions.Num(); ++Index)
	{
		Result.AdmissionLanes[static_cast<uint8>(Result.Admissions[Index].Kind)].Add(Index);
	}

	for (const auto& Pair : Result.Exact)
	{
		if (Pair.Value.bWarmupData) Result.Warmup.Add(Pair.Key, Pair.Value);
	}
	Result.Exact.GetKeys(Result.ExactOrder);
	Result.ExactOrder.Sort([&Result](const FIntVector& Left, const FIntVector& Right)
	{
		const FVoxelExactDemand& A = Result.Exact.FindChecked(Left);
		const FVoxelExactDemand& B = Result.Exact.FindChecked(Right);
		const bool bGameplayA = A.bExact || A.bCollision || A.bSimulation || A.bWarmupData;
		const bool bGameplayB = B.bExact || B.bCollision || B.bSimulation || B.bWarmupData;
		if (bGameplayA != bGameplayB) return bGameplayA;
		if (A.DistanceCells != B.DistanceCells) return A.DistanceCells < B.DistanceCells;
		if (A.ForwardScore != B.ForwardScore) return A.ForwardScore > B.ForwardScore;
		if (Left.X != Right.X) return Left.X < Right.X;
		if (Left.Y != Right.Y) return Left.Y < Right.Y;
		return Left.Z < Right.Z;
	});

	return Result;
}

void FVoxelInterestManager::AddExactSource(
	const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings,
	FVoxelInterestSet& InOutInterest,
	const FVoxelInterestSet* InPrevious) const
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

	const int32 FineRadius = bWantsFine ? FMath::Max(0, InViewSettings.FineRadius) +
		FMath::Max(0, InViewSettings.FinePreload) : 0;

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

	const int32 FineRetainRadius = bWantsFine && FineRadius > 0 ? FineRadius + InterestSectionSide : 0;
	const int32 DataRadius = FMath::Max(FineRetainRadius,
		FMath::Max(
			FMath::Max(
				ExactRadius,
				CollisionRadius),
			SimulationRadius));

	const int32 HorizontalSections =
		CeilDividePositive(
			DataRadius,
			InterestSectionSide);

	const int32 VerticalSections = CeilDividePositive(
		FMath::Max(FineRetainRadius, FMath::Max(0, InSource.VerticalExactRadius)), InterestSectionSide);

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

				const FVoxelExactDemand* Previous = InPrevious ? InPrevious->Exact.Find(Key) : nullptr;
				const bool bRetainFine = InPrevious && InPrevious->FineSections
					? InPrevious->FineSections->Contains(Key) : Previous && Previous->bFineRender;
				const bool bFineRender =
					bWantsFine &&
					IsInsideRadius(
						Delta,
						bRetainFine ? FineRetainRadius : FineRadius);

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

				Demand.DistanceCells = FMath::Min(Demand.DistanceCells, Distance);
				Demand.ForwardScore = FMath::Max(Demand.ForwardScore, Forward);
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

	const int32 FineRadius = FMath::Max(0, InViewSettings.FineRadius) + FMath::Max(0, InViewSettings.FinePreload);

	const int32 ProxyRange =
		FMath::Max(
			FineRadius,
			InViewSettings.VoxelProxyRadius);

	if (ProxyRange > FineRadius)
	{
		const uint8 ProxyLevel = FMath::Max<uint8>(1, InViewSettings.MaximumVoxelProxyLevel);

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
