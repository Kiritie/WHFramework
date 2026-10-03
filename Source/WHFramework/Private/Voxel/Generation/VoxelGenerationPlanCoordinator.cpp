#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"

#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"

namespace
{
	constexpr int32 MaximumHydrologyDependencies = 4096;

	FVoxelGenerationPlanDependency MakeDependency(
		const EVoxelGenerationPlanKind InKind,
		const FIntVector& InCoordinate)
	{
		FVoxelGenerationPlanDependency Dependency;
		Dependency.Kind = InKind;
		switch (InKind)
		{
		case EVoxelGenerationPlanKind::Hydrology:
			Dependency.HydrologyKey.Coordinate = FIntPoint(InCoordinate.X, InCoordinate.Y);
			break;
		case EVoxelGenerationPlanKind::Ecology:
			Dependency.EcologyKey.Coordinate = FIntPoint(InCoordinate.X, InCoordinate.Y);
			break;
		default:
			Dependency.TileKey.Coordinate = InCoordinate;
			break;
		}
		Dependency.StableHash = VoxelGeneration::MakeSeed(0, InCoordinate,
			0x965fe9283ac50d17ull ^ static_cast<uint64>(InKind));
		return Dependency;
	}
}

bool FVoxelGenerationPlanDependency::operator==(
	const FVoxelGenerationPlanDependency& InOther) const
{
	return Kind == InOther.Kind && Coordinate() == InOther.Coordinate();
}

FIntVector FVoxelGenerationPlanDependency::Coordinate() const
{
	switch (Kind)
	{
	case EVoxelGenerationPlanKind::Hydrology:
		return FIntVector(HydrologyKey.Coordinate.X, HydrologyKey.Coordinate.Y, 0);
	case EVoxelGenerationPlanKind::Ecology:
		return FIntVector(EcologyKey.Coordinate.X, EcologyKey.Coordinate.Y, 0);
	default:
		return TileKey.Coordinate;
	}
}

FVoxelGenerationPlanCoordinator::FVoxelGenerationPlanCoordinator(
	FVoxelTaskScheduler& InScheduler,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
	const uint64 InWorldEpoch)
	: Scheduler(InScheduler)
	, Config(InConfig)
	, Cache(InCache)
	, WorldEpoch(InWorldEpoch)
{
}

bool FVoxelGenerationPlanCoordinator::GatherForBounds(
	const FVoxelGenerationBounds& InBounds,
	TArray<FVoxelGenerationPlanDependency>& OutDependencies,
	FString& OutError) const
{
	OutDependencies.Reset();
	if (!InBounds.IsValid() || !Config->Recipe)
	{
		OutError = TEXT("Voxel plan preflight received invalid bounds or recipe");
		return false;
	}
	const FVoxelGenerationSettings& Settings = Config->Recipe->Settings;
	const FVoxelGenerationPlanKeys Keys = FVoxelGenerationQuery::GatherPlanKeys(
		InBounds, Settings);
	int64 MinX = InBounds.Min.X;
	int64 MinY = InBounds.Min.Y;
	int64 MaxX = static_cast<int64>(InBounds.Max.X) - 1;
	int64 MaxY = static_cast<int64>(InBounds.Max.Y) - 1;
	const int64 CaveReach = FVoxelCaveGenerator::GetMaximumReach(Settings);
	for (const FVoxelGenerationTileKey& Tile : Keys.Tiles)
	{
		const int64 TileX = static_cast<int64>(Tile.Coordinate.X) *
			FVoxelGenerationQuery::GenerationPlanTileSide;
		const int64 TileY = static_cast<int64>(Tile.Coordinate.Y) *
			FVoxelGenerationQuery::GenerationPlanTileSide;
		MinX = FMath::Min(MinX, TileX - CaveReach);
		MinY = FMath::Min(MinY, TileY - CaveReach);
		MaxX = FMath::Max(MaxX, TileX + FVoxelGenerationQuery::GenerationPlanTileSide - 1 + CaveReach);
		MaxY = FMath::Max(MaxY, TileY + FVoxelGenerationQuery::GenerationPlanTileSide - 1 + CaveReach);
	}
	for (const FVoxelEcologyTileKey& Tile : Keys.EcologyTiles)
	{
		const int64 TileX = static_cast<int64>(Tile.Coordinate.X) *
			FVoxelGenerationQuery::EcologyTileSide;
		const int64 TileY = static_cast<int64>(Tile.Coordinate.Y) *
			FVoxelGenerationQuery::EcologyTileSide;
		MinX = FMath::Min(MinX, TileX);
		MinY = FMath::Min(MinY, TileY);
		MaxX = FMath::Max(MaxX, TileX + FVoxelGenerationQuery::EcologyTileSide - 1);
		MaxY = FMath::Max(MaxY, TileY + FVoxelGenerationQuery::EcologyTileSide - 1);
	}
	const auto HydrologyAt = [&Settings](const int64 X, const int64 Y)
	{
		return FVoxelGenerationQuery::HydrologyKeyForVoxel(
			static_cast<int32>(FMath::Clamp<int64>(X, MIN_int32, MAX_int32)),
			static_cast<int32>(FMath::Clamp<int64>(Y, MIN_int32, MAX_int32)), Settings);
	};
	const FIntPoint MinRegion = HydrologyAt(MinX, MinY).Coordinate;
	const FIntPoint MaxRegion = HydrologyAt(MaxX, MaxY).Coordinate;
	const int64 HydrologyCount =
		(static_cast<int64>(MaxRegion.X) - MinRegion.X + 1) *
		(static_cast<int64>(MaxRegion.Y) - MinRegion.Y + 1);
	if (HydrologyCount > MaximumHydrologyDependencies)
	{
		OutError = FString::Printf(TEXT("Voxel plan preflight needs %lld hydrology regions; limit is %d"),
			HydrologyCount, MaximumHydrologyDependencies);
		return false;
	}
	OutDependencies.Reserve(static_cast<int32>(HydrologyCount) +
		Keys.Tiles.Num() * 3 + Keys.EcologyTiles.Num());
	for (int32 Y = MinRegion.Y; Y <= MaxRegion.Y; ++Y)
	{
		for (int32 X = MinRegion.X; X <= MaxRegion.X; ++X)
		{
			OutDependencies.Add(MakeDependency(EVoxelGenerationPlanKind::Hydrology,
				FIntVector(X, Y, 0)));
		}
	}
	for (const FVoxelGenerationTileKey& Tile : Keys.Tiles)
	{
		OutDependencies.Add(MakeDependency(EVoxelGenerationPlanKind::Cave, Tile.Coordinate));
		OutDependencies.Add(MakeDependency(EVoxelGenerationPlanKind::Structure, Tile.Coordinate));
		OutDependencies.Add(MakeDependency(EVoxelGenerationPlanKind::Feature, Tile.Coordinate));
	}
	for (const FVoxelEcologyTileKey& Tile : Keys.EcologyTiles)
	{
		OutDependencies.Add(MakeDependency(EVoxelGenerationPlanKind::Ecology,
				FIntVector(Tile.Coordinate.X, Tile.Coordinate.Y, 0)));
	}
	OutError.Reset();
	return true;
}

TSharedRef<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> FVoxelGenerationPlanCoordinator::RetainDependencies(
	TConstArrayView<FVoxelGenerationPlanDependency> InDependencies,
	const FIntPoint& InCenter) const
{
	FVoxelGenerationCacheRetentionPoint Point;
	Point.Center = InCenter;
	const auto& Settings = Config->Recipe->Settings;
	const int64 HydrologySide = static_cast<int64>(FMath::Max(8, Settings.HydrologyRegionSide)) * FMath::Max(1, Settings.HydrologyCellSize);
	for (const FVoxelGenerationPlanDependency& Dependency : InDependencies)
	{
		const FIntVector Coordinate = Dependency.Coordinate();
		const bool bHydrology = Dependency.Kind == EVoxelGenerationPlanKind::Hydrology;
		const bool bEcology = Dependency.Kind == EVoxelGenerationPlanKind::Ecology;
		const int64 Side = bHydrology ? HydrologySide : bEcology ? FVoxelGenerationQuery::EcologyTileSide : FVoxelGenerationQuery::GenerationPlanTileSide;
		// Cache 按水文/生态中心与其他计划原点回收；保留实际依赖键直到消费者完成。
		const int64 Offset = bHydrology || bEcology ? Side / 2 : 0;
		const int64 Distance = FMath::Max(FMath::Abs(static_cast<int64>(Coordinate.X) * Side + Offset - InCenter.X),
			FMath::Abs(static_cast<int64>(Coordinate.Y) * Side + Offset - InCenter.Y));
		int32& Radius = bHydrology ? Point.HydrologyRadiusCells : Point.PlanRadiusCells;
		Radius = FMath::Max(Radius, static_cast<int32>(FMath::Min<int64>(MAX_int32, Distance)));
	}
	return Cache->RetainForTask(Point);
}

FVoxelGenerationDependencyStatus FVoxelGenerationPlanCoordinator::EnsureEnvironment(const FIntPoint& InCell)
{
	const FVoxelHydrologyRegionKey Key = FVoxelGenerationQuery::HydrologyKeyForVoxel(
		InCell.X, InCell.Y, Config->Recipe->Settings);
	const FVoxelGenerationPlanDependency Dependency = MakeDependency(
		EVoxelGenerationPlanKind::Hydrology, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0));
	return Ensure(MakeArrayView(&Dependency, 1), EVoxelWorkClass::Interactive, 0, 0.0, 0.0,
		FIntVector(InCell.X, InCell.Y, MAX_int32));
}

FVoxelGenerationDependencyStatus FVoxelGenerationPlanCoordinator::EnsureEnvironmentBounds(
	const FVoxelGenerationBounds& InBounds, const EVoxelWorkClass InWorkClass,
	const int32 InSourcePriority, const double InDistanceScore, const double InForwardScore)
{
	check(IsInGameThread());
	if (!InBounds.IsValid() || !Config->Recipe)
		return { false, true, TEXT("Environment preflight received invalid bounds or recipe") };
	const auto& Settings = Config->Recipe->Settings;
	const FIntPoint Min = FVoxelGenerationQuery::HydrologyKeyForVoxel(
		InBounds.Min.X, InBounds.Min.Y, Settings).Coordinate;
	const FIntPoint Max = FVoxelGenerationQuery::HydrologyKeyForVoxel(
		InBounds.Max.X - 1, InBounds.Max.Y - 1, Settings).Coordinate;
	const int64 Count = (static_cast<int64>(Max.X) - Min.X + 1) *
		(static_cast<int64>(Max.Y) - Min.Y + 1);
	if (Count > MaximumHydrologyDependencies)
		return { false, true, TEXT("Environment preflight exceeds hydrology dependency limit") };
	TArray<FVoxelGenerationPlanDependency> Dependencies;
	Dependencies.Reserve(static_cast<int32>(Count));
	for (int64 Y = Min.Y; Y <= Max.Y; ++Y)
		for (int64 X = Min.X; X <= Max.X; ++X)
			Dependencies.Add(MakeDependency(EVoxelGenerationPlanKind::Hydrology,
				FIntVector(static_cast<int32>(X), static_cast<int32>(Y), 0)));
	return Ensure(Dependencies, InWorkClass, InSourcePriority, InDistanceScore, InForwardScore);
}

bool FVoxelGenerationPlanCoordinator::IsCached(
	const FVoxelGenerationPlanDependency& InDependency) const
{
	switch (InDependency.Kind)
	{
	case EVoxelGenerationPlanKind::Hydrology:
	{
		FVoxelHydrologyPlanPtr Plan;
		return Cache->FindHydrology(InDependency.HydrologyKey, Plan);
	}
	case EVoxelGenerationPlanKind::Cave:
	{
		FVoxelCavePlanPtr Plan;
		return Cache->FindCave(InDependency.TileKey, Plan);
	}
	case EVoxelGenerationPlanKind::Structure:
	{
		FVoxelStructurePlanPtr Plan;
		return Cache->FindStructure(InDependency.TileKey, Plan);
	}
	case EVoxelGenerationPlanKind::Feature:
	{
		FVoxelFeaturePlanPtr Plan;
		return Cache->FindFeature(InDependency.TileKey, Plan);
	}
	case EVoxelGenerationPlanKind::Ecology:
	{
		FVoxelEcologyPlanPtr Plan;
		return Cache->FindEcology(InDependency.EcologyKey, Plan);
	}
	default:
		return false;
	}
}

bool FVoxelGenerationPlanCoordinator::DonatePriority(
	FState& InOutState,
	const EVoxelWorkClass InWorkClass,
	const int32 InSourcePriority,
	const double InDistanceScore,
	const double InForwardScore)
{
	const EVoxelWorkClass PreviousClass = InOutState.WorkClass;
	const int32 PreviousPriority = InOutState.SourcePriority;
	const double PreviousDistance = InOutState.DistanceScore;
	const double PreviousForward = InOutState.ForwardScore;
	InOutState.WorkClass = static_cast<EVoxelWorkClass>(FMath::Min(
		static_cast<uint8>(InOutState.WorkClass), static_cast<uint8>(InWorkClass)));
	InOutState.SourcePriority = FMath::Min(InOutState.SourcePriority, InSourcePriority);
	InOutState.DistanceScore = FMath::Min(InOutState.DistanceScore, InDistanceScore);
	InOutState.ForwardScore = FMath::Max(InOutState.ForwardScore, InForwardScore);
	return InOutState.WorkClass != PreviousClass || InOutState.SourcePriority != PreviousPriority ||
		InOutState.DistanceScore != PreviousDistance || InOutState.ForwardScore != PreviousForward;
}

void FVoxelGenerationPlanCoordinator::RefreshQueuedPriorities()
{
	Scheduler.UpdatePriorities([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, int32& SourcePriority,
		double& DistanceScore, double& ForwardScore)
	{
		if (Kind != EVoxelTaskKind::BuildGenerationPlan || Stamp.WorldEpoch != WorldEpoch) return;
		const uint64 KindValue = Stamp.Revision;
		if (KindValue > static_cast<uint64>(EVoxelGenerationPlanKind::Ecology)) return;
		const FVoxelGenerationPlanDependency Dependency = MakeDependency(
			static_cast<EVoxelGenerationPlanKind>(KindValue), Stamp.Section);
		if (const FState* State = States.Find(Dependency))
		{
			WorkClass = State->WorkClass;
			SourcePriority = State->SourcePriority;
			DistanceScore = State->DistanceScore;
			ForwardScore = State->ForwardScore;
		}
	});
}

bool FVoxelGenerationPlanCoordinator::Queue(
	const FVoxelGenerationPlanDependency& InDependency,
	FState& InOutState)
{
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildGenerationPlan;
	Request.TerrainStage = 0;
	Request.WorkClass = InOutState.WorkClass;
	Request.SourcePriority = InOutState.SourcePriority;
	Request.DistanceScore = InOutState.DistanceScore;
	Request.ForwardScore = InOutState.ForwardScore;
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.Token = InDependency.StableHash;
	Request.Stamp.Revision = static_cast<uint8>(InDependency.Kind);
	Request.Stamp.Section = InDependency.Coordinate();
	Request.ReservedBytes = InDependency.Kind == EVoxelGenerationPlanKind::Hydrology
		? 8ull * 1024ull * 1024ull : 4ull * 1024ull * 1024ull;
	Request.Execute = [Dependency = InDependency, CapturedConfig = Config,
		CapturedCache = Cache](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		FVoxelGenerationQuery Query;
		if (!FVoxelGenerationQuery::Create(CapturedConfig, CapturedCache,
			Query, Result.Error, false, true)) return Result;
		switch (Dependency.Kind)
		{
		case EVoxelGenerationPlanKind::Hydrology:
		{
			FVoxelHydrologyPlanPtr Plan;
			Result.bSuccess = Query.EnsureHydrologyPlan(Dependency.HydrologyKey,
				Plan, Result.Error, &Cancel);
			break;
		}
		case EVoxelGenerationPlanKind::Cave:
		{
			FVoxelCavePlanPtr Plan;
			Result.bSuccess = Query.EnsureCavePlan(Dependency.TileKey,
				Plan, Result.Error, &Cancel);
			break;
		}
		case EVoxelGenerationPlanKind::Structure:
		{
			FVoxelStructurePlanPtr Plan;
			Result.bSuccess = Query.EnsureStructurePlan(Dependency.TileKey,
				Plan, Result.Error, &Cancel);
			break;
		}
		case EVoxelGenerationPlanKind::Feature:
		{
			FVoxelFeaturePlanPtr Plan;
			Result.bSuccess = Query.EnsureFeaturePlan(Dependency.TileKey,
				Plan, Result.Error, &Cancel);
			break;
		}
		case EVoxelGenerationPlanKind::Ecology:
		{
			FVoxelEcologyPlanPtr Plan;
			Result.bSuccess = Query.EnsureEcologyPlan(Dependency.EcologyKey,
				Plan, Result.Error, &Cancel);
			break;
		}
		}
		Result.bCanceled = Cancel.Load();
		return Result;
	};
	Request.Apply = [this, Dependency = InDependency](FVoxelTaskResult&& Result)
	{
		FState* State = States.Find(Dependency);
		if (!State) return;
		if (Result.bCanceled || Result.Error.StartsWith(TEXT("DependencyNotReady")))
		{
			State->State = EVoxelPlanDependencyState::Missing;
			return;
		}
		if (Result.bSuccess)
		{
			// The immutable cache owns completed plans. Keep coordinator state only
			// while a key is queued or has a failure to report.
			States.Remove(Dependency);
			return;
		}
		State->State = EVoxelPlanDependencyState::Failed;
		State->Error = MoveTemp(Result.Error);
		UE_LOG(LogTemp, Error, TEXT("Voxel plan build failed: kind=%d key=%s error=%s"),
			static_cast<int32>(Dependency.Kind), *Dependency.Coordinate().ToString(),
			*State->Error);
	};
	if (!Scheduler.Enqueue(MoveTemp(Request))) return false;
	InOutState.State = EVoxelPlanDependencyState::Queued;
	return true;
}

FVoxelGenerationDependencyStatus FVoxelGenerationPlanCoordinator::Ensure(
	TConstArrayView<FVoxelGenerationPlanDependency> InDependencies,
	const EVoxelWorkClass InWorkClass,
	const int32 InSourcePriority,
	const double InDistanceScore,
	const double InForwardScore,
	const FIntVector InConsumer)
{
	FVoxelGenerationDependencyStatus Status;
	bool bHydrologyReady = true;
	bool bPriorityChanged = false;
	auto Process = [&](const FVoxelGenerationPlanDependency& Dependency)
	{
		if (IsCached(Dependency))
		{
			States.Remove(Dependency);
			return true;
		}
		FState& State = States.FindOrAdd(Dependency);
		State.Consumers.Add(InConsumer);
		if (State.State == EVoxelPlanDependencyState::Ready)
		{
			State.State = EVoxelPlanDependencyState::Missing;
		}
		if (State.State == EVoxelPlanDependencyState::Failed)
		{
			Status.bFailed = true;
			Status.Error = State.Error;
			return false;
		}
		const bool bWasQueued = State.State == EVoxelPlanDependencyState::Queued;
		if (bWasQueued && DonatePriority(State, InWorkClass,
			InSourcePriority, InDistanceScore, InForwardScore))
		{
			bPriorityChanged = true;
			Cache->RecordPlanDonation(Dependency.Kind);
		}
		if (!bWasQueued)
		{
			DonatePriority(State, InWorkClass,
				InSourcePriority, InDistanceScore, InForwardScore);
			Queue(Dependency, State);
		}
		return false;
	};
	for (const FVoxelGenerationPlanDependency& Dependency : InDependencies)
	{
		if (Dependency.Kind != EVoxelGenerationPlanKind::Hydrology) continue;
		bHydrologyReady &= Process(Dependency);
		if (Status.bFailed) break;
	}
	if (bHydrologyReady && !Status.bFailed)
	{
		Status.bReady = true;
		for (const FVoxelGenerationPlanDependency& Dependency : InDependencies)
		{
			if (Dependency.Kind == EVoxelGenerationPlanKind::Hydrology) continue;
			Status.bReady &= Process(Dependency);
			if (Status.bFailed) break;
		}
	}
	if (bPriorityChanged) RefreshQueuedPriorities();
	return Status;
}

void FVoxelGenerationPlanCoordinator::Reset()
{
	States.Reset();
}

void FVoxelGenerationPlanCoordinator::ReleaseConsumer(const FIntVector& InConsumer)
{
	for (auto& Pair : States) Pair.Value.Consumers.Remove(InConsumer);
}

uint64 FVoxelGenerationPlanCoordinator::GetWaitingConsumers(const EVoxelGenerationPlanKind InKind) const
{
	TSet<FIntVector> Consumers;
	for (const auto& Pair : States)
	{
		if (Pair.Key.Kind == InKind && Pair.Value.State != EVoxelPlanDependencyState::Failed)
			Consumers.Append(Pair.Value.Consumers);
	}
	return Consumers.Num();
}
