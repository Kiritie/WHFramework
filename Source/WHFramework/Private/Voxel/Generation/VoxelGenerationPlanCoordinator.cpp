#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"

#include "Misc/ScopeExit.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	constexpr int32 MaximumHydrologyDependencies = 4096;
	struct FPlanReadDependencies final : FVoxelTaskCustomPayload
	{
		TArray<FVoxelGenerationPlanDependency> Missing;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		bool bOverflow = false;
		virtual uint64 GetAllocatedBytes() const override
		{
			return VoxelBoundedAllocation::SharedBytes<FPlanReadDependencies>() + Missing.GetAllocatedSize();
		}
		virtual uint64 GetResourceAccountedBytes() const override { return GetAllocatedBytes(); }
	};

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
	TSet<FVoxelGenerationPlanDependency> Closure;
	const auto AppendFact = [this, &Closure, &OutError](const FVoxelGenerationPlanDependency& Fact)
	{
		Closure.Add(Fact);
		TArray<FVoxelGenerationPlanDependency> Prerequisites;
		if (!GatherPrerequisites(Fact, Prerequisites, OutError)) return false;
		for (const auto& Prerequisite : Prerequisites) Closure.Add(Prerequisite);
		return true;
	};
	for (const FVoxelGenerationTileKey& Tile : Keys.Tiles)
	{
		for (const auto Kind : {EVoxelGenerationPlanKind::Cave, EVoxelGenerationPlanKind::Structure, EVoxelGenerationPlanKind::Feature})
			if (!AppendFact(MakeDependency(Kind, Tile.Coordinate))) return false;
	}
	for (const FVoxelEcologyTileKey& Tile : Keys.EcologyTiles)
	{
		if (!AppendFact(MakeDependency(EVoxelGenerationPlanKind::Ecology,
			FIntVector(Tile.Coordinate.X, Tile.Coordinate.Y, 0)))) return false;
	}
	const auto MinRegion = FVoxelGenerationQuery::HydrologyKeyForVoxel(InBounds.Min.X, InBounds.Min.Y, Settings).Coordinate;
	const auto MaxRegion = FVoxelGenerationQuery::HydrologyKeyForVoxel(InBounds.Max.X - 1, InBounds.Max.Y - 1, Settings).Coordinate;
	const int64 Count = (int64(MaxRegion.X) - MinRegion.X + 1) * (int64(MaxRegion.Y) - MinRegion.Y + 1);
	if (Count > MaximumHydrologyDependencies)
	{
		OutError = TEXT("生成采样范围超过水文闭包上限");
		return false;
	}
	for (int64 Y = MinRegion.Y; Y <= MaxRegion.Y; ++Y)
		for (int64 X = MinRegion.X; X <= MaxRegion.X; ++X)
			Closure.Add(MakeDependency(EVoxelGenerationPlanKind::Hydrology, FIntVector(int32(X), int32(Y), 0)));
	OutDependencies.Reserve(Closure.Num());
	for (const auto& Fact : Closure) OutDependencies.Add(Fact);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPlanCoordinator::GatherPrerequisites(const FVoxelGenerationPlanDependency& InDependency,
	TArray<FVoxelGenerationPlanDependency>& OutDependencies, FString& OutError) const
{
	OutDependencies.Reset();
	OutError.Reset();
	if (InDependency.Kind == EVoxelGenerationPlanKind::Hydrology) return true;
	if (!Config->Recipe) { OutError = TEXT("生成计划缺少配方"); return false; }
	const auto& Settings = Config->Recipe->Settings;
	const bool bEcology = InDependency.Kind == EVoxelGenerationPlanKind::Ecology;
	const int64 Side = bEcology ? FVoxelGenerationQuery::EcologyTileSide : FVoxelGenerationQuery::GenerationPlanTileSide;
	int64 Margin = 0;
	if (InDependency.Kind == EVoxelGenerationPlanKind::Cave) Margin = FVoxelCaveGenerator::GetMaximumReach(Settings);
	else if (bEcology) Margin = FMath::Max(Settings.Ecology.Grass.PatchRadius, Settings.Ecology.Flower.PatchRadius);
	else if (InDependency.Kind == EVoxelGenerationPlanKind::Feature)
	{
		// 锚点来自与瓦片相交的格；实际算法的其他采样键由 Query 读依赖回执补齐。
		for (const auto& Feature : Config->Recipe->Features) Margin = FMath::Max<int64>(Margin, Feature.Placement.Spacing - 1);
	}
	const auto Coordinate = InDependency.Coordinate();
	const auto At = [&Settings](int64 X, int64 Y)
	{
		return FVoxelGenerationQuery::HydrologyKeyForVoxel(int32(FMath::Clamp<int64>(X, MIN_int32, MAX_int32)),
			int32(FMath::Clamp<int64>(Y, MIN_int32, MAX_int32)), Settings).Coordinate;
	};
	const auto Min = At(int64(Coordinate.X) * Side - Margin, int64(Coordinate.Y) * Side - Margin);
	const auto Max = At(int64(Coordinate.X) * Side + Side - 1 + Margin, int64(Coordinate.Y) * Side + Side - 1 + Margin);
	const int64 Count = (int64(Max.X) - Min.X + 1) * (int64(Max.Y) - Min.Y + 1);
	if (Count > MaximumHydrologyDependencies) { OutError = TEXT("单个生成事实的水文闭包超过上限"); return false; }
	OutDependencies.Reserve(int32(Count));
	for (int64 Y = Min.Y; Y <= Max.Y; ++Y)
		for (int64 X = Min.X; X <= Max.X; ++X)
			OutDependencies.Add(MakeDependency(EVoxelGenerationPlanKind::Hydrology, FIntVector(int32(X), int32(Y), 0)));
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
		EnvironmentConsumer({FIntVector(InCell.X, InCell.Y, 0), FIntVector(InCell.X, InCell.Y, 0)}, true));
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
	const FVoxelGenerationConsumerKey Consumer = EnvironmentConsumer(InBounds, false);
	const FVoxelGenerationDependencyStatus Status = Ensure(Dependencies, InWorkClass,
		InSourcePriority, InDistanceScore, InForwardScore, Consumer);
	if (Status.bReady || Status.bFailed)
	{
		ReleaseConsumer(Consumer);
	}
	return Status;
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

void FVoxelGenerationPlanCoordinator::RefreshQueuedPriorities()
{
	Scheduler.UpdateServicePriorities([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, FVoxelTerrainServicePriority& Priority)
	{
		if (Kind != EVoxelTaskKind::BuildGenerationPlan || Stamp.WorldEpoch != WorldEpoch) return;
		const uint64 KindValue = Stamp.Revision;
		if (KindValue > static_cast<uint64>(EVoxelGenerationPlanKind::Ecology)) return;
		const FVoxelGenerationPlanDependency Dependency = MakeDependency(
			static_cast<EVoxelGenerationPlanKind>(KindValue), Stamp.Section);
		if (const FState* State = States.Find(Dependency))
		{
			if (State->AttemptId != Stamp.Token)
			{
				return;
			}
			WorkClass = State->WorkClass;
			Priority = State->ServicePriority;
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
	Request.ServiceBand = InOutState.ServicePriority.Band;
	Request.SourceStableKey = InOutState.ServicePriority.SourceStableKey;
	Request.Stamp.WorldEpoch = WorldEpoch;
	checkf(NextAttemptId != MAX_uint64, TEXT("Generation attempt serial exhausted"));
	InOutState.AttemptId = ++NextAttemptId;
	Request.Stamp.Token = InOutState.AttemptId;
	Request.Stamp.OwnerId = EnvironmentOwnerId;
	Request.Stamp.OwnerIncarnation = EnvironmentOwnerIncarnation;
	Request.Stamp.Revision = static_cast<uint8>(InDependency.Kind);
	Request.Stamp.Section = InDependency.Coordinate();
	Request.ReservedBytes = InDependency.Kind == EVoxelGenerationPlanKind::Hydrology
		? 8ull * 1024ull * 1024ull : 4ull * 1024ull * 1024ull;
	const FVoxelGenerationConsumerKey CaptureConsumer{EVoxelGenerationConsumerKind::WorldFact, WorldEpoch,
		EnvironmentOwnerId, 1, EVoxelGenerationSpatialKind::Section, Request.Stamp.Section,
		Request.Stamp.Section, Request.Stamp.Token};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ReadLease;
	if (!Scheduler.GetRuntimeResources()->Acquire(CaptureConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::SharedBytes<FPlanReadDependencies>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelGenerationPlanDependency>(MaximumHydrologyDependencies), ReadLease)) return false;
	auto ReadDependencies = MakeShared<FPlanReadDependencies, ESPMode::ThreadSafe>();
	ReadDependencies->Lease = MoveTemp(ReadLease);
	ReadDependencies->Missing.Reserve(MaximumHydrologyDependencies);
	Request.InputBytes = ReadDependencies->Lease->Bytes();
	Request.Execute = [Dependency = InDependency, CapturedConfig = Config,
		CapturedCache = Cache, ReadDependencies](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		FVoxelGenerationQuery Query;
		if (!FVoxelGenerationQuery::Create(CapturedConfig, CapturedCache,
			Query, Result.Error, false, true)) return Result;
		Query.SetMissingHydrologyObserver([ReadDependencies](const FVoxelHydrologyRegionKey& Key)
		{
			const auto Missing = MakeDependency(EVoxelGenerationPlanKind::Hydrology, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0));
			if (ReadDependencies->Missing.Contains(Missing)) return;
			if (ReadDependencies->Missing.Num() == MaximumHydrologyDependencies) ReadDependencies->bOverflow = true;
			else ReadDependencies->Missing.Add(Missing);
		});
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
		if (ReadDependencies->bOverflow)
		{
			Result.bSuccess = false;
			Result.Error = TEXT("生成事实的实际采样闭包超过上限");
		}
		Result.CustomPayload = ReadDependencies;
		return Result;
	};
	Request.Apply = [this, Dependency = InDependency](FVoxelTaskResult&& Result)
	{
		FState* State = States.Find(Dependency);
		if (!State || Result.Stamp.WorldEpoch != WorldEpoch || Result.Stamp.OwnerId != EnvironmentOwnerId ||
			Result.Stamp.OwnerIncarnation != EnvironmentOwnerIncarnation || Result.Stamp.Token != State->AttemptId)
		{
			return;
		}
		State->bInFlight = false;
		if (!Result.bCanceled && Result.Error.StartsWith(TEXT("DependencyNotReady")))
		{
			const auto Reads = StaticCastSharedPtr<const FPlanReadDependencies>(Result.CustomPayload);
			if (Reads)
			{
				for (const auto& Missing : Reads->Missing)
				{
					if (!State->Prerequisites.Contains(Missing))
					{
						if (State->Prerequisites.Num() == MaximumHydrologyDependencies)
						{
							Result.Error = TEXT("生成事实的累计采样闭包超过上限");
							break;
						}
						State->Prerequisites.Add(Missing);
					}
				}
				// 注册事实状态先于通知父消费者；下一次 Ensure 原子替换完整订阅闭包。
				for (const auto& Missing : Reads->Missing)
				{
					State = States.Find(Dependency);
					if (!State->Prerequisites.Contains(Missing)) continue;
					FString Error;
					if (!RegisterClosure(Missing, Error))
					{
						Result.Error = MoveTemp(Error);
						break;
					}
				}
				State = States.Find(Dependency);
				const int64 Side = Dependency.Kind == EVoxelGenerationPlanKind::Ecology ?
					FVoxelGenerationQuery::EcologyTileSide : FVoxelGenerationQuery::GenerationPlanTileSide;
				State->PrerequisiteRetention = RetainDependencies(State->Prerequisites,
					FIntPoint(int32(FMath::Clamp<int64>(int64(Dependency.Coordinate().X) * Side, MIN_int32, MAX_int32)),
						int32(FMath::Clamp<int64>(int64(Dependency.Coordinate().Y) * Side, MIN_int32, MAX_int32))));
				TSet<FGuid> Edges;
				for (const auto& Prerequisite : State->Prerequisites)
				{
					if (const FState* PrerequisiteState = States.Find(Prerequisite))
						Edges.Add(PrerequisiteState->PriorityNode);
					else
						Result.Error = TEXT("生成事实的实际采样闭包注册失败");
				}
				FString GraphError;
				if (!PriorityGraph.SetDependencies(State->PriorityNode, Edges, GraphError))
				{
					Result.Error = MoveTemp(GraphError);
				}
			}
		}
		ON_SCOPE_EXIT
		{
			WakeConsumers(Dependency);
			const FState* Current = States.Find(Dependency);
			if (Current && Current->Donations.IsEmpty() && Current->State != EVoxelPlanDependencyState::Queued)
			{
				ForgetFact(Dependency);
			}
		};
		if (Result.bCanceled || Result.Error.StartsWith(TEXT("DependencyNotReady")))
		{
			State->State = EVoxelPlanDependencyState::Missing;
			return;
		}
		if (Result.bSuccess)
		{
			Result.MarkWorkUsed();
			State->State = EVoxelPlanDependencyState::Ready;
			State->Error.Reset();
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
	InOutState.bInFlight = true;
	return true;
}

FVoxelGenerationDependencyStatus FVoxelGenerationPlanCoordinator::Ensure(
	TConstArrayView<FVoxelGenerationPlanDependency> InDependencies,
	const EVoxelWorkClass InWorkClass,
	const int32 InSourcePriority,
	const double InDistanceScore,
	const double InForwardScore,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	check(IsInGameThread());
	if (!InConsumer.IsValid() || InConsumer.WorldEpoch != WorldEpoch ||
		!FMath::IsFinite(InDistanceScore) || InDistanceScore < 0.0 || !FMath::IsFinite(InForwardScore))
	{
		return {false, true, TEXT("Invalid generation consumer or priority")};
	}
	FVoxelTaskRequest PriorityRequest;
	PriorityRequest.Kind = EVoxelTaskKind::GenerateExactBase;
	PriorityRequest.WorkClass = InWorkClass;
	PriorityRequest.SourcePriority = InSourcePriority;
	PriorityRequest.DistanceScore = InDistanceScore;
	PriorityRequest.ForwardScore = InForwardScore;
	PriorityRequest.SourceStableKey = InConsumer.OwnerId;
	const FVoxelTerrainServicePriority Priority = FVoxelTaskScheduler::ResolveServicePriority(PriorityRequest);
	return Ensure(InDependencies, InWorkClass, Priority, InConsumer);
}

FVoxelGenerationDependencyStatus FVoxelGenerationPlanCoordinator::Ensure(
	TConstArrayView<FVoxelGenerationPlanDependency> InDependencies, const EVoxelWorkClass InWorkClass,
	const FVoxelTerrainServicePriority& Priority, const FVoxelGenerationConsumerKey& InConsumer)
{
	check(IsInGameThread());
	if (!InConsumer.IsValid() || InConsumer.WorldEpoch != WorldEpoch || !Priority.SourceStableKey.IsValid() ||
		!FMath::IsFinite(Priority.DistanceCells) || Priority.DistanceCells < 0.0 || !FMath::IsFinite(Priority.ForwardScore) ||
		Priority.Band < EVoxelTerrainServiceBand::P0 || Priority.Band > EVoxelTerrainServiceBand::P6)
	{
		return {false, true, TEXT("生成事实需要完整Source优先级与Consumer身份")};
	}

	// 完整事实及读依赖状态先注册，再观察 Ready，最后才允许排队。
	TSet<FVoxelGenerationPlanDependency> Desired;
	for (const FVoxelGenerationPlanDependency& Dependency : InDependencies)
	{
		FString Error;
		if (!RegisterClosure(Dependency, Error)) return {false, true, MoveTemp(Error)};
		Desired.Add(Dependency);
		for (const auto& Prerequisite : States.FindChecked(Dependency).Prerequisites) Desired.Add(Prerequisite);
	}
	if (const auto* Previous = ConsumerDependencies.Find(InConsumer))
	{
		const auto PreviousEdges = *Previous;
		for (const auto& Edge : PreviousEdges)
		{
			if (!Desired.Contains(Edge.Key))
			{
				RemoveSubscription(Edge.Key, InConsumer, Edge.Value);
				ConsumerDependencies.FindChecked(InConsumer).Remove(Edge.Key);
			}
		}
	}
	FGuid* ConsumerNode = ConsumerPriorityNodes.Find(InConsumer);
	if (!ConsumerNode)
	{
		ConsumerNode = &ConsumerPriorityNodes.Add(InConsumer, FGuid::NewGuid());
		PriorityGraph.Register(*ConsumerNode, Priority);
	}
	else PriorityGraph.SetPriority(*ConsumerNode, Priority);
	TSet<FGuid> DependencyNodes;
	for (const auto& Dependency : Desired) DependencyNodes.Add(States.FindChecked(Dependency).PriorityNode);
	FString GraphError;
	if (!PriorityGraph.SetDependencies(*ConsumerNode, DependencyNodes, GraphError)) return {false, true, MoveTemp(GraphError)};
	FVoxelGenerationDependencyStatus Status;
	Status.bReady = true;
	for (const FVoxelGenerationPlanDependency& Dependency : Desired)
	{
		FState& State = States.FindOrAdd(Dependency);
		auto& Edges = ConsumerDependencies.FindOrAdd(InConsumer);
		uint64* Subscription = Edges.Find(Dependency);
		if (!Subscription)
		{
			checkf(NextSubscriptionId != MAX_uint64, TEXT("Generation subscription serial exhausted"));
			Subscription = &Edges.Add(Dependency, ++NextSubscriptionId);
		}
		const FVoxelTerrainServicePriority Previous = State.ServicePriority;
		State.Donations.Add(InConsumer, {InWorkClass, Priority, *Subscription});
		ResolveDonations(State);
		PriorityGraph.SetPriority(State.PriorityNode, State.ServicePriority);
		if (State.bInFlight && Previous != State.ServicePriority)
		{
			Cache->RecordPlanDonation(Dependency.Kind);
		}
		const bool bCached = IsCached(Dependency);
		if (bCached)
		{
			State.State = EVoxelPlanDependencyState::Ready;
			State.Error.Reset();
		}
		else if (State.State == EVoxelPlanDependencyState::Ready)
		{
			State.State = State.bInFlight ? EVoxelPlanDependencyState::Queued : EVoxelPlanDependencyState::Missing;
		}
		if (State.State == EVoxelPlanDependencyState::Failed)
		{
			Status.bFailed = true;
			Status.Error = State.Error;
		}
		Status.bReady &= bCached;
	}
	AdvancePriorityDonations();
	for (const FVoxelGenerationPlanDependency& Dependency : Desired)
	{
		FState& State = States.FindChecked(Dependency);
		if (State.State == EVoxelPlanDependencyState::Missing && !State.bInFlight && ArePrerequisitesReady(State))
		{
			Queue(Dependency, State);
		}
	}
	RefreshQueuedPriorities();
	return Status;
}

void FVoxelGenerationPlanCoordinator::Reset()
{
	check(IsInGameThread());
	check(EnvironmentOwnerIncarnation != MAX_uint64);
	const uint64 PreviousIncarnation = EnvironmentOwnerIncarnation++;
	Scheduler.CancelMatching([Epoch = WorldEpoch, Owner = EnvironmentOwnerId, PreviousIncarnation](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildGenerationPlan && Stamp.WorldEpoch == Epoch &&
			Stamp.OwnerId == Owner && Stamp.OwnerIncarnation == PreviousIncarnation;
	});
	States.Reset();
	ConsumerDependencies.Reset();
	PriorityFacts.Reset();
	ConsumerPriorityNodes.Reset();
	PriorityGraph.Reset();
	EnvironmentAttempts.Reset();
}

void FVoxelGenerationPlanCoordinator::ReleaseConsumer(const FVoxelGenerationConsumerKey& InConsumer)
{
	check(IsInGameThread());
	TMap<FVoxelGenerationPlanDependency, uint64> Dependencies;
	if (!ConsumerDependencies.RemoveAndCopyValue(InConsumer, Dependencies))
	{
		return;
	}
	FGuid ConsumerNode;
	if (ConsumerPriorityNodes.RemoveAndCopyValue(InConsumer, ConsumerNode)) PriorityGraph.Remove(ConsumerNode);
	if (InConsumer.Kind == EVoxelGenerationConsumerKind::Environment)
	{
		auto Space = InConsumer;
		Space.RequestSerial = 1;
		if (const auto* Current = EnvironmentAttempts.Find(Space); Current && *Current == InConsumer) EnvironmentAttempts.Remove(Space);
	}
	for (const auto& Edge : Dependencies)
	{
		RemoveSubscription(Edge.Key, InConsumer, Edge.Value);
	}
	AdvancePriorityDonations();
	RefreshQueuedPriorities();
}

void FVoxelGenerationPlanCoordinator::RemoveSubscription(const FVoxelGenerationPlanDependency& InDependency,
	const FVoxelGenerationConsumerKey& InConsumer, const uint64 InSubscriptionId)
{
	FState* State = States.Find(InDependency);
	const FDonation* Donation = State ? State->Donations.Find(InConsumer) : nullptr;
	if (!Donation || Donation->SubscriptionId != InSubscriptionId)
	{
		return;
	}
	State->Donations.Remove(InConsumer);
	ResolveDonations(*State);
	PriorityGraph.SetPriority(State->PriorityNode, State->ServicePriority);
	if (!State->Donations.IsEmpty())
	{
		return;
	}
	if (State->bInFlight)
	{
		const uint64 Attempt = State->AttemptId;
		Scheduler.CancelMatching([Attempt, Epoch = WorldEpoch](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildGenerationPlan && Stamp.WorldEpoch == Epoch && Stamp.Token == Attempt;
		});
	}
	else
	{
		ForgetFact(InDependency);
	}
}

uint64 FVoxelGenerationPlanCoordinator::GetWaitingConsumers(const EVoxelGenerationPlanKind InKind) const
{
	TSet<FVoxelGenerationConsumerKey> Consumers;
	for (const auto& Pair : States)
	{
		if (Pair.Key.Kind == InKind && Pair.Value.State != EVoxelPlanDependencyState::Ready &&
			Pair.Value.State != EVoxelPlanDependencyState::Failed)
		{
			for (const auto& Donation : Pair.Value.Donations)
			{
				Consumers.Add(Donation.Key);
			}
		}
	}
	return Consumers.Num();
}

void FVoxelGenerationPlanCoordinator::SetConsumerWakeCallback(TFunction<void(const FVoxelGenerationConsumerKey&)> InCallback)
{
	ConsumerWakeCallback = MoveTemp(InCallback);
}

void FVoxelGenerationPlanCoordinator::ResolveDonations(FState& InOutState)
{
	InOutState.WorkClass = EVoxelWorkClass::Background;
	InOutState.ServicePriority = {};
	const FDonation* Best = nullptr;
	for (const auto& Donation : InOutState.Donations)
	{
		if (!Best || FVoxelTerrainWorkSelection::HigherPriority(Donation.Value.Priority, Best->Priority) ||
			(Donation.Value.Priority == Best->Priority && Donation.Value.WorkClass < Best->WorkClass))
		{
			Best = &Donation.Value;
		}
	}
	if (Best)
	{
		InOutState.ServicePriority = Best->Priority;
		InOutState.WorkClass = Best->WorkClass;
	}
	InOutState.SourcePriority = InOutState.ServicePriority.SourcePriority;
	InOutState.DistanceScore = InOutState.ServicePriority.DistanceCells;
	InOutState.ForwardScore = InOutState.ServicePriority.ForwardScore;
}

void FVoxelGenerationPlanCoordinator::WakeConsumers(const FVoxelGenerationPlanDependency& InDependency)
{
	const FState* State = States.Find(InDependency);
	if (!State || !ConsumerWakeCallback)
	{
		return;
	}
	const auto Subscriptions = State->Donations;
	for (const auto& Subscription : Subscriptions)
	{
		const auto* Edges = ConsumerDependencies.Find(Subscription.Key);
		const uint64* CurrentId = Edges ? Edges->Find(InDependency) : nullptr;
		if (CurrentId && *CurrentId == Subscription.Value.SubscriptionId && Subscription.Key.WorldEpoch == WorldEpoch)
		{
			ConsumerWakeCallback(Subscription.Key);
		}
	}
}

FVoxelGenerationConsumerKey FVoxelGenerationPlanCoordinator::EnvironmentConsumer(
	const FVoxelGenerationBounds& InBounds, const bool bInColumn) const
{
	const FVoxelGenerationConsumerKey Space{EVoxelGenerationConsumerKind::Environment, WorldEpoch, EnvironmentOwnerId, EnvironmentOwnerIncarnation,
		bInColumn ? EVoxelGenerationSpatialKind::Column : EVoxelGenerationSpatialKind::Bounds,
		InBounds.Min, InBounds.Max, 1};
	if (const auto* Existing = EnvironmentAttempts.Find(Space)) return *Existing;
	checkf(NextEnvironmentAttempt != MAX_uint64, TEXT("环境请求代耗尽"));
	auto Consumer = Space;
	Consumer.RequestSerial = ++NextEnvironmentAttempt;
	EnvironmentAttempts.Add(Space, Consumer);
	return Consumer;
}

bool FVoxelGenerationPlanCoordinator::RegisterClosure(const FVoxelGenerationPlanDependency& InDependency, FString& OutError)
{
	if (const auto* Existing = States.Find(InDependency); Existing && Existing->PriorityNode.IsValid())
	{
		const int32 Count = Existing->Prerequisites.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			// 递归注册可能移动 States，逐项重新取父状态，避免保留悬空引用。
			const auto Prerequisite = States.FindChecked(InDependency).Prerequisites[Index];
			if (!RegisterClosure(Prerequisite, OutError)) return false;
		}
		const auto& State = States.FindChecked(InDependency);
		TSet<FGuid> Edges;
		for (const auto& Prerequisite : State.Prerequisites) Edges.Add(States.FindChecked(Prerequisite).PriorityNode);
		return PriorityGraph.SetDependencies(State.PriorityNode, Edges, OutError);
	}
	TArray<FVoxelGenerationPlanDependency> Prerequisites;
	if (!GatherPrerequisites(InDependency, Prerequisites, OutError)) return false;
	for (const auto& Prerequisite : Prerequisites)
	{
		if (!RegisterClosure(Prerequisite, OutError)) return false;
	}
	auto& State = States.FindOrAdd(InDependency);
	State.PriorityNode = FGuid::NewGuid();
	State.Prerequisites = MoveTemp(Prerequisites);
	PriorityFacts.Add(State.PriorityNode, InDependency);
	PriorityGraph.Register(State.PriorityNode, State.ServicePriority);
	TSet<FGuid> Edges;
	for (const auto& Prerequisite : State.Prerequisites) Edges.Add(States.FindChecked(Prerequisite).PriorityNode);
	return PriorityGraph.SetDependencies(State.PriorityNode, Edges, OutError);
}

bool FVoxelGenerationPlanCoordinator::ArePrerequisitesReady(const FState& InState) const
{
	for (const auto& Dependency : InState.Prerequisites) if (!IsCached(Dependency)) return false;
	return true;
}

void FVoxelGenerationPlanCoordinator::AdvancePriorityDonations()
{
	PriorityGraph.Advance(128, [this](const FGuid& Node, const FVoxelTerrainServicePriority& Priority)
	{
		const auto* Fact = PriorityFacts.Find(Node);
		auto* State = Fact ? States.Find(*Fact) : nullptr;
		if (!State) return;
		if (State->bInFlight && State->ServicePriority != Priority) Cache->RecordPlanDonation(Fact->Kind);
		State->ServicePriority = Priority;
		State->SourcePriority = Priority.SourcePriority;
		State->DistanceScore = Priority.DistanceCells;
		State->ForwardScore = Priority.ForwardScore;
	});
}

void FVoxelGenerationPlanCoordinator::ForgetFact(const FVoxelGenerationPlanDependency& InDependency)
{
	if (const auto* State = States.Find(InDependency))
	{
		const auto Node = State->PriorityNode;
		PriorityGraph.Remove(Node);
		PriorityFacts.Remove(Node);
		States.Remove(InDependency);
	}
}

void FVoxelGenerationPlanCoordinator::ReleaseEnvironment(const FIntPoint& InCell)
{
	const FIntVector Cell(InCell.X, InCell.Y, 0);
	const FVoxelGenerationConsumerKey Space{EVoxelGenerationConsumerKind::Environment, WorldEpoch, EnvironmentOwnerId, EnvironmentOwnerIncarnation,
		EVoxelGenerationSpatialKind::Column, Cell, Cell, 1};
	if (const auto* Current = EnvironmentAttempts.Find(Space))
	{
		const auto Consumer = *Current;
		ReleaseConsumer(Consumer);
		EnvironmentAttempts.Remove(Space);
	}
}
