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
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TArray<FVoxelGenerationPlanDependency> Missing;
		bool bOverflow = false;
		virtual uint64 GetAllocatedBytes() const override
		{
			return VoxelBoundedAllocation::WorkerSharedBytes<FPlanReadDependencies>() + Missing.GetAllocatedSize();
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
	, Resources(InScheduler.GetRuntimeResources())
	, ControlConsumer{EVoxelGenerationConsumerKind::WorldFact, InWorldEpoch, FGuid::NewGuid(), 1,
		EVoxelGenerationSpatialKind::Column, FIntVector::ZeroValue, FIntVector::ZeroValue, 1}
	, PriorityGraph(Resources, ControlConsumer)
{
}

FVoxelGenerationPlanCoordinator::~FVoxelGenerationPlanCoordinator()
{
	Reset();
}

bool FVoxelGenerationPlanCoordinator::PrepareStorage(FString& OutError)
{
	check(IsInGameThread());
	if (Storage) return true;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Resources->Acquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FStorage>(), Lease))
	{
		OutError = TEXT("CapacityBlocked: generation coordinator storage");
		return false;
	}
	Storage = VoxelBoundedAllocation::MakeWorkerShared<FStorage>(Lease);
	Storage->Owner = this;
	Storage->ResourceLease = MoveTemp(Lease);
	return true;
}

template<typename KeyType, typename ValueType>
bool FVoxelGenerationPlanCoordinator::PrepareMap(TMap<KeyType, ValueType>& InMap, const int32 InRequired,
	int32& InCapacity, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease, FString& OutError)
{
	if (InRequired < 0 || InRequired > (1 << 20))
	{
		OutError = TEXT("Oversized: generation coordinator control entries");
		return false;
	}
	if (InRequired <= InCapacity) return true;
	const int32 Capacity = FMath::Max(InRequired, FMath::Max(4, InCapacity * 2));
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Resources->Acquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::MapBytes<KeyType, ValueType>(Capacity), Lease))
	{
		OutError = TEXT("CapacityBlocked: generation coordinator control map");
		return false;
	}
	InMap.Reserve(Capacity);
	check(InMap.GetAllocatedSize() <= Lease->Bytes());
	InLease = MoveTemp(Lease);
	InCapacity = Capacity;
	return true;
}

bool FVoxelGenerationPlanCoordinator::PreparePrerequisites(FState& InState, const int32 InRequired, FString& OutError)
{
	if (InRequired > MaximumHydrologyDependencies)
	{
		OutError = TEXT("生成事实的累计采样闭包超过上限");
		return false;
	}
	if (InRequired <= InState.PrerequisitesCapacity) return true;
	const int32 Capacity = FMath::Min(MaximumHydrologyDependencies,
		FMath::Max(InRequired, FMath::Max(4, InState.PrerequisitesCapacity * 2)));
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Resources->Acquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::ArrayBytes<FVoxelGenerationPlanDependency>(Capacity), Lease))
	{
		OutError = TEXT("CapacityBlocked: generation prerequisite array");
		return false;
	}
	InState.Prerequisites.Reserve(Capacity);
	check(InState.Prerequisites.GetAllocatedSize() <= Lease->Bytes());
	InState.PrerequisitesLease = MoveTemp(Lease);
	InState.PrerequisitesCapacity = Capacity;
	return true;
}

bool FVoxelGenerationPlanCoordinator::GatherForBounds(
	const FVoxelGenerationBounds& InBounds,
	const FVoxelGenerationConsumerKey& InConsumer,
	FVoxelGenerationPlanDependencyCapture& OutDependencies,
	FString& OutError) const
{
	if (!InBounds.IsValid() || !Config->Recipe || !InConsumer.IsValid() || InConsumer.WorldEpoch != WorldEpoch)
	{
		OutError = TEXT("Voxel plan preflight received invalid bounds, recipe or consumer");
		return false;
	}
	const FVoxelGenerationSettings& Settings = Config->Recipe->Settings;
	FVoxelGenerationPlanKeyRanges Ranges;
	if (!FVoxelGenerationQuery::GetPlanKeyRanges(InBounds, Settings, Ranges, OutError)) return false;
	constexpr int32 MaximumFacts = 1 << 20;
	if (int64(Ranges.TileCount) * 3 + Ranges.EcologyCount > MaximumFacts)
	{
		OutError = TEXT("Oversized: generation preflight root facts");
		return false;
	}
	const auto MinRegion = FVoxelGenerationQuery::HydrologyKeyForVoxel(InBounds.Min.X, InBounds.Min.Y, Settings).Coordinate;
	const auto MaxRegion = FVoxelGenerationQuery::HydrologyKeyForVoxel(InBounds.Max.X - 1, InBounds.Max.Y - 1, Settings).Coordinate;
	const int64 Width = int64(MaxRegion.X) - MinRegion.X + 1;
	const int64 Height = int64(MaxRegion.Y) - MinRegion.Y + 1;
	if (Width <= 0 || Height <= 0 || Width > MaximumHydrologyDependencies ||
		Height > MaximumHydrologyDependencies || Width > MaximumHydrologyDependencies / Height)
	{
		OutError = TEXT("生成采样范围超过水文闭包上限");
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ClosureLease;
	TSet<FVoxelGenerationPlanDependency> Closure;
	int32 Capacity = 0;
	const auto Append = [&](const FVoxelGenerationPlanDependency& Fact)
	{
		if (Closure.Contains(Fact)) return true;
		if (Closure.Num() == MaximumFacts)
		{
			OutError = TEXT("Oversized: generation preflight closure");
			return false;
		}
		if (Closure.Num() == Capacity)
		{
			const int32 NewCapacity = FMath::Max(4, Capacity * 2);
			const uint64 Bytes = VoxelBoundedAllocation::SetBytes<FVoxelGenerationPlanDependency>(NewCapacity);
			if (Bytes + (ClosureLease ? ClosureLease->Bytes() : 0) > Resources->Snapshot().Limits.WorkingBytes)
			{
				OutError = TEXT("CapacityUnachievable: generation preflight closure backing");
				return false;
			}
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NewLease;
			if (!Resources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working,
				Bytes, NewLease))
			{
				OutError = TEXT("CapacityBlocked: generation preflight closure backing");
				return false;
			}
			Closure.Reserve(NewCapacity);
			check(Closure.GetAllocatedSize() <= NewLease->Bytes());
			ClosureLease = MoveTemp(NewLease);
			Capacity = NewCapacity;
		}
		Closure.Add(Fact);
		return true;
	};
	const auto AppendFact = [&](const FVoxelGenerationPlanDependency& Fact)
	{
		if (!Append(Fact)) return false;
		FIntPoint Min;
		FIntPoint Max;
		int32 Reads = 0;
		if (!GetPrerequisiteRange(Fact, Min, Max, Reads, OutError)) return false;
		if (Reads)
			for (int64 Y = Min.Y; Y <= Max.Y; ++Y)
				for (int64 X = Min.X; X <= Max.X; ++X)
					if (!Append(MakeDependency(EVoxelGenerationPlanKind::Hydrology, FIntVector(int32(X), int32(Y), 0)))) return false;
		return true;
	};
	for (int64 Y = Ranges.TileMin.Y; Y <= Ranges.TileMax.Y; ++Y)
	{
		for (int64 X = Ranges.TileMin.X; X <= Ranges.TileMax.X; ++X)
			for (const auto Kind : {EVoxelGenerationPlanKind::Cave, EVoxelGenerationPlanKind::Structure, EVoxelGenerationPlanKind::Feature})
				if (!AppendFact(MakeDependency(Kind, FIntVector(int32(X), int32(Y), 0)))) return false;
	}
	for (int64 Y = Ranges.EcologyMin.Y; Y <= Ranges.EcologyMax.Y; ++Y)
	{
		for (int64 X = Ranges.EcologyMin.X; X <= Ranges.EcologyMax.X; ++X)
			if (!AppendFact(MakeDependency(EVoxelGenerationPlanKind::Ecology, FIntVector(int32(X), int32(Y), 0)))) return false;
	}
	for (int64 Y = MinRegion.Y; Y <= MaxRegion.Y; ++Y)
		for (int64 X = MinRegion.X; X <= MaxRegion.X; ++X)
			if (!Append(MakeDependency(EVoxelGenerationPlanKind::Hydrology, FIntVector(int32(X), int32(Y), 0)))) return false;
	FVoxelGenerationPlanDependencyCapture Candidate;
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FVoxelGenerationPlanDependency>(Closure.Num());
	const uint64 Limit = Resources->Snapshot().Limits.InputBytes;
	const uint64 Previous = OutDependencies.Lease && OutDependencies.Lease->IsOwnedBy(Resources.Get()) ? OutDependencies.Lease->Bytes() : 0;
	if (Bytes > Limit || Previous > Limit - Bytes)
	{
		OutError = TEXT("CapacityUnachievable: generation preflight capture");
		return false;
	}
	if (!Resources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input,
		Bytes, Candidate.Lease))
	{
		OutError = TEXT("CapacityBlocked: generation preflight capture");
		return false;
	}
	Candidate.Values.Reserve(Closure.Num());
	check(Candidate.Values.GetAllocatedSize() <= Candidate.Lease->Bytes());
	for (const auto& Fact : Closure) Candidate.Values.Add(Fact);
	OutDependencies = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPlanCoordinator::GetPrerequisiteRange(const FVoxelGenerationPlanDependency& InDependency,
	FIntPoint& OutMin, FIntPoint& OutMax, int32& OutCount, FString& OutError) const
{
	OutCount = 0;
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
		for (const auto& Feature : Config->Recipe->Features) Margin = FMath::Max<int64>(Margin, Feature.Placement.Spacing - 1);
	}
	const auto Coordinate = InDependency.Coordinate();
	const auto At = [&Settings](int64 X, int64 Y)
	{
		return FVoxelGenerationQuery::HydrologyKeyForVoxel(int32(FMath::Clamp<int64>(X, MIN_int32, MAX_int32)),
			int32(FMath::Clamp<int64>(Y, MIN_int32, MAX_int32)), Settings).Coordinate;
	};
	OutMin = At(int64(Coordinate.X) * Side - Margin, int64(Coordinate.Y) * Side - Margin);
	OutMax = At(int64(Coordinate.X) * Side + Side - 1 + Margin, int64(Coordinate.Y) * Side + Side - 1 + Margin);
	const int64 Width = int64(OutMax.X) - OutMin.X + 1;
	const int64 Height = int64(OutMax.Y) - OutMin.Y + 1;
	if (Width <= 0 || Height <= 0 || Width > MaximumHydrologyDependencies ||
		Height > MaximumHydrologyDependencies || Width > MaximumHydrologyDependencies / Height)
	{
		OutError = TEXT("单个生成事实的水文闭包超过上限");
		return false;
	}
	OutCount = int32(Width * Height);
	return true;
}

bool FVoxelGenerationPlanCoordinator::GatherPrerequisites(const FVoxelGenerationPlanDependency& InDependency,
	FVoxelGenerationPlanDependencyCapture& OutDependencies, FString& OutError) const
{
	FIntPoint Min;
	FIntPoint Max;
	int32 Count = 0;
	if (!GetPrerequisiteRange(InDependency, Min, Max, Count, OutError)) return false;
	FVoxelGenerationPlanDependencyCapture Candidate;
	if (Count)
	{
		const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FVoxelGenerationPlanDependency>(Count);
		const uint64 Limit = Resources->Snapshot().Limits.InputBytes;
		const uint64 Previous = OutDependencies.Lease && OutDependencies.Lease->IsOwnedBy(Resources.Get()) ? OutDependencies.Lease->Bytes() : 0;
		if (Bytes > Limit || Previous > Limit - Bytes)
		{
			OutError = TEXT("CapacityUnachievable: generation prerequisite capture");
			return false;
		}
		if (!Resources->Acquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
			Bytes, Candidate.Lease))
		{
			OutError = TEXT("CapacityBlocked: generation prerequisite capture");
			return false;
		}
		Candidate.Values.Reserve(Count);
		check(Candidate.Values.GetAllocatedSize() <= Candidate.Lease->Bytes());
		for (int64 Y = Min.Y; Y <= Max.Y; ++Y)
			for (int64 X = Min.X; X <= Max.X; ++X)
				Candidate.Values.Add(MakeDependency(EVoxelGenerationPlanKind::Hydrology, FIntVector(int32(X), int32(Y), 0)));
	}
	OutDependencies = MoveTemp(Candidate);
	return true;
}

bool FVoxelGenerationPlanCoordinator::RetainDependencies(
	TConstArrayView<FVoxelGenerationPlanDependency> InDependencies,
	const FIntPoint& InCenter, const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>& OutRetention, FString& OutError) const
{
	if (!Config->Recipe || !InConsumer.IsValid() || InConsumer.WorldEpoch != WorldEpoch)
	{
		OutError = TEXT("InvalidConsumer: generation cache retention");
		return false;
	}
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
	return Cache->RetainForTask(Point, Resources, InConsumer, OutRetention, OutError);
}

FVoxelGenerationDependencyStatus FVoxelGenerationPlanCoordinator::EnsureEnvironment(const FIntPoint& InCell)
{
	const FVoxelHydrologyRegionKey Key = FVoxelGenerationQuery::HydrologyKeyForVoxel(
		InCell.X, InCell.Y, Config->Recipe->Settings);
	const FVoxelGenerationPlanDependency Dependency = MakeDependency(
		EVoxelGenerationPlanKind::Hydrology, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0));
	const auto Consumer = EnvironmentConsumer({FIntVector(InCell.X, InCell.Y, 0), FIntVector(InCell.X, InCell.Y, 0)}, true);
	if (!Consumer.IsValid()) return {false, false, TEXT("CapacityBlocked: generation environment attempt")};
	return Ensure(MakeArrayView(&Dependency, 1), EVoxelWorkClass::Interactive, 0, 0.0, 0.0, Consumer);
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
	const int64 Width = int64(Max.X) - Min.X + 1;
	const int64 Height = int64(Max.Y) - Min.Y + 1;
	if (Width <= 0 || Height <= 0 || Width > MaximumHydrologyDependencies ||
		Height > MaximumHydrologyDependencies || Width > MaximumHydrologyDependencies / Height)
		return { false, true, TEXT("Environment preflight exceeds hydrology dependency limit") };
	const int32 Count = int32(Width * Height);
	const FVoxelGenerationConsumerKey Consumer = EnvironmentConsumer(InBounds, false);
	if (!Consumer.IsValid()) return {false, false, TEXT("CapacityBlocked: generation environment attempt")};
	FVoxelGenerationPlanDependencyCapture Dependencies;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::ArrayBytes<FVoxelGenerationPlanDependency>(int32(Count)), Dependencies.Lease))
		return {false, false, TEXT("CapacityBlocked: generation environment capture")};
	Dependencies.Values.Reserve(static_cast<int32>(Count));
	check(Dependencies.Values.GetAllocatedSize() <= Dependencies.Lease->Bytes());
	for (int64 Y = Min.Y; Y <= Max.Y; ++Y)
		for (int64 X = Min.X; X <= Max.X; ++X)
			Dependencies.Values.Add(MakeDependency(EVoxelGenerationPlanKind::Hydrology,
				FIntVector(static_cast<int32>(X), static_cast<int32>(Y), 0)));
	const FVoxelGenerationDependencyStatus Status = Ensure(Dependencies.Values, InWorkClass,
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
	if (!Storage) return;
	Scheduler.UpdateServicePriorities([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, FVoxelTerrainServicePriority& Priority)
	{
		if (Kind != EVoxelTaskKind::BuildGenerationPlan || Stamp.WorldEpoch != WorldEpoch) return;
		const uint64 KindValue = Stamp.Revision;
		if (KindValue > static_cast<uint64>(EVoxelGenerationPlanKind::Ecology)) return;
		const FVoxelGenerationPlanDependency Dependency = MakeDependency(
			static_cast<EVoxelGenerationPlanKind>(KindValue), Stamp.Section);
		if (const FState* State = Storage->States.Find(Dependency))
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
	if (!InOutState.bAttemptPendingAdmission)
	{
		checkf(NextAttemptId != MAX_uint64, TEXT("Generation attempt serial exhausted"));
		InOutState.AttemptId = ++NextAttemptId;
		InOutState.bAttemptPendingAdmission = true;
	}
	Request.Stamp.Token = InOutState.AttemptId;
	Request.Stamp.OwnerId = EnvironmentOwnerId;
	Request.Stamp.OwnerIncarnation = EnvironmentOwnerIncarnation;
	Request.Stamp.Revision = static_cast<uint8>(InDependency.Kind);
	Request.Stamp.Section = InDependency.Coordinate();
	Request.ReservedBytes = InDependency.Kind == EVoxelGenerationPlanKind::Hydrology
		? 8ull * 1024ull * 1024ull : 4ull * 1024ull * 1024ull;
	const FVoxelGenerationConsumerKey CaptureConsumer{EVoxelGenerationConsumerKind::WorldFact, WorldEpoch,
		EnvironmentOwnerId, Request.Stamp.OwnerIncarnation, EVoxelGenerationSpatialKind::Section, Request.Stamp.Section,
		Request.Stamp.Section, Request.Stamp.Token};
	if (InOutState.bWaitingCapacity && Resources->ReleaseVersionExcludingOwner(CaptureConsumer) <= InOutState.CapacityVersion) return false;
	InOutState.bWaitingCapacity = false;
	const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(CaptureConsumer);
	if ((!InOutState.PrerequisiteRetention || InOutState.bPrerequisiteRetentionPending) && !InOutState.Prerequisites.IsEmpty())
	{
		const int64 Side = InDependency.Kind == EVoxelGenerationPlanKind::Ecology ?
			FVoxelGenerationQuery::EcologyTileSide : FVoxelGenerationQuery::GenerationPlanTileSide;
		const FIntPoint Center(int32(FMath::Clamp<int64>(int64(InDependency.Coordinate().X) * Side, MIN_int32, MAX_int32)),
			int32(FMath::Clamp<int64>(int64(InDependency.Coordinate().Y) * Side, MIN_int32, MAX_int32)));
		FString Error;
		if (!RetainDependencies(InOutState.Prerequisites, Center, CaptureConsumer, InOutState.PrerequisiteRetention, Error))
		{
			InOutState.bWaitingCapacity = Error.StartsWith(TEXT("CapacityBlocked"));
			InOutState.CapacityVersion = BeforeAdmission;
			if (!InOutState.bWaitingCapacity) { InOutState.State = EVoxelPlanDependencyState::Failed; InOutState.Error = MoveTemp(Error); }
			return false;
		}
		InOutState.bPrerequisiteRetentionPending = false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ReadLease;
	const uint64 BeforeReadAdmission = Resources->ReleaseVersionExcludingOwner(CaptureConsumer);
	if (!Scheduler.GetRuntimeResources()->Acquire(CaptureConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FPlanReadDependencies>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelGenerationPlanDependency>(MaximumHydrologyDependencies), ReadLease))
	{
		InOutState.bWaitingCapacity = true;
		InOutState.CapacityVersion = BeforeReadAdmission;
		return false;
	}
	auto ReadDependencies = VoxelBoundedAllocation::MakeWorkerShared<FPlanReadDependencies>(ReadLease);
	ReadDependencies->Lease = MoveTemp(ReadLease);
	ReadDependencies->Missing.Reserve(MaximumHydrologyDependencies);
	check(ReadDependencies->GetAllocatedBytes() <= ReadDependencies->Lease->Bytes());
	Request.InputBytes = ReadDependencies->Lease->Bytes();
	Request.Execute = [Dependency = InDependency, CapturedConfig = Config,
		CapturedCache = Cache, PrerequisiteRetention = InOutState.PrerequisiteRetention, ReadDependencies](const TAtomic<bool>& Cancel)
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
	Request.Apply = [this, WeakStorage = TWeakPtr<FStorage, ESPMode::ThreadSafe>(Storage), Dependency = InDependency](FVoxelTaskResult&& Result)
	{
		const auto CapturedStorage = WeakStorage.Pin();
		if (!CapturedStorage || CapturedStorage->Owner != this) return;
		FState* State = Storage->States.Find(Dependency);
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
				FClosureRegistration Registration(*this);
				int32 Required = State->Prerequisites.Num();
				for (const auto& Missing : Reads->Missing) if (!State->Prerequisites.Contains(Missing)) ++Required;
				bool bRegistered = PreparePrerequisites(*State, Required, Result.Error);
				for (const auto& Missing : Reads->Missing)
				{
					if (!bRegistered) break;
					if (!State->Prerequisites.Contains(Missing))
					{
						if (State->Prerequisites.Num() == MaximumHydrologyDependencies)
						{
							Result.Error = TEXT("生成事实的累计采样闭包超过上限");
							bRegistered = false;
							break;
						}
						State->Prerequisites.Add(Missing);
					}
				}
				// 注册事实状态先于通知父消费者；下一次 Ensure 原子替换完整订阅闭包。
				for (const auto& Missing : Reads->Missing)
				{
					if (!bRegistered) break;
					State = Storage->States.Find(Dependency);
					if (!State->Prerequisites.Contains(Missing)) continue;
					FString Error;
					if (!RegisterClosure(Missing, Registration, Error))
					{
						Result.Error = MoveTemp(Error);
						bRegistered = false;
						break;
					}
				}
				State = Storage->States.Find(Dependency);
				const int64 Side = Dependency.Kind == EVoxelGenerationPlanKind::Ecology ?
					FVoxelGenerationQuery::EcologyTileSide : FVoxelGenerationQuery::GenerationPlanTileSide;
				const FVoxelGenerationConsumerKey RetentionConsumer{EVoxelGenerationConsumerKind::WorldFact, Result.Stamp.WorldEpoch,
					Result.Stamp.OwnerId, Result.Stamp.OwnerIncarnation, EVoxelGenerationSpatialKind::Section,
					Result.Stamp.Section, Result.Stamp.Section, Result.Stamp.Token};
				FString RetentionError;
				State->bPrerequisiteRetentionPending = true;
				if (!RetainDependencies(State->Prerequisites,
					FIntPoint(int32(FMath::Clamp<int64>(int64(Dependency.Coordinate().X) * Side, MIN_int32, MAX_int32)),
						int32(FMath::Clamp<int64>(int64(Dependency.Coordinate().Y) * Side, MIN_int32, MAX_int32))),
					RetentionConsumer, State->PrerequisiteRetention, RetentionError))
				{
					Result.Error = MoveTemp(RetentionError);
					bRegistered = false;
				}
				else State->bPrerequisiteRetentionPending = false;
				if (bRegistered)
				{
					FString GraphError;
					if (SetFactDependencies(Dependency, GraphError)) Registration.Commit();
					else Result.Error = MoveTemp(GraphError);
				}
			}
		}
		State = Storage->States.Find(Dependency);
		check(State);
		ON_SCOPE_EXIT
		{
			WakeConsumers(Dependency);
			const FState* Current = Storage->States.Find(Dependency);
			if (Current && Current->Donations.IsEmpty() && Current->State != EVoxelPlanDependencyState::Queued)
			{
				ForgetFact(Dependency);
			}
		};
		if (Result.bCanceled || Result.Error.StartsWith(TEXT("DependencyNotReady")) ||
			Result.Error.StartsWith(TEXT("CapacityBlocked")))
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
	InOutState.bAttemptPendingAdmission = false;
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

	FString Error;
	if (!PrepareStorage(Error)) return {false, false, MoveTemp(Error)};
	FClosureRegistration Registration(*this);
	FClosureDemand Desired(*this);
	for (const auto& Dependency : InDependencies)
	{
		if (!RegisterClosure(Dependency, Registration, Error))
			return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
		Desired.Add(Dependency);
	}
	FConsumerState* Consumer = Storage->Consumers.Find(InConsumer);
	if (!Consumer)
	{
		if (!PrepareMap<FVoxelGenerationConsumerKey, FConsumerState>(Storage->Consumers, Storage->Consumers.Num() + 1,
			Storage->ConsumersCapacity, Storage->ConsumersLease, Error)) return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
		const FGuid Node = FGuid::NewGuid();
		if (!PriorityGraph.Register(Node, Priority, Error)) return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
		Consumer = &Storage->Consumers.Add(InConsumer);
		Consumer->PriorityNode = Node;
	}
	bool bSame = Consumer->Dependencies.Num() == Desired.Count;
	for (FGuid Node = Desired.Head; bSame && Node.IsValid();)
	{
		const auto& Dependency = Storage->PriorityFacts.FindChecked(Node);
		bSame = Consumer->Dependencies.Contains(Dependency);
		Node = Storage->States.FindChecked(Dependency).NextDesired;
	}
	if (!bSame)
	{
		int32 GrowthCount = 0;
		for (FGuid Node = Desired.Head; Node.IsValid();)
		{
			const auto& State = Storage->States.FindChecked(Storage->PriorityFacts.FindChecked(Node));
			if (!State.Donations.Contains(InConsumer) && State.Donations.Num() + 1 > State.DonationsCapacity) ++GrowthCount;
			Node = State.NextDesired;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
		const uint64 ScratchBytes = VoxelBoundedAllocation::SetBytes<FGuid>(Desired.Count) +
			VoxelBoundedAllocation::ArrayBytes<FDonationGrowth>(GrowthCount);
		if (ScratchBytes && !Resources->Acquire(ControlConsumer, EVoxelTerrainResourcePool::Working, ScratchBytes, ScratchLease))
			return {false, false, TEXT("CapacityBlocked: generation subscription plan")};
		TSet<FGuid> DependencyNodes;
		TArray<FDonationGrowth> Growth;
		DependencyNodes.Reserve(Desired.Count);
		Growth.Reserve(GrowthCount);
		for (FGuid Node = Desired.Head; Node.IsValid();)
		{
			const auto& State = Storage->States.FindChecked(Storage->PriorityFacts.FindChecked(Node));
			DependencyNodes.Add(Node);
			if (!State.Donations.Contains(InConsumer) && State.Donations.Num() + 1 > State.DonationsCapacity)
			{
				FDonationGrowth Plan;
				Plan.Node = Node;
				Plan.Capacity = FMath::Max(State.Donations.Num() + 1, FMath::Max(4, State.DonationsCapacity * 2));
				if (!Resources->Acquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
					VoxelBoundedAllocation::MapBytes<FVoxelGenerationConsumerKey, FDonation>(Plan.Capacity), Plan.Lease))
					return {false, false, TEXT("CapacityBlocked: generation donations")};
				Growth.Add(MoveTemp(Plan));
			}
			Node = State.NextDesired;
		}
		if (!PrepareMap<FVoxelGenerationPlanDependency, FSubscription>(Consumer->Dependencies, Desired.Count,
			Consumer->Capacity, Consumer->DependenciesLease, Error)) return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
		for (auto& Plan : Growth)
		{
			auto& State = Storage->States.FindChecked(Storage->PriorityFacts.FindChecked(Plan.Node));
			State.Donations.Reserve(Plan.Capacity);
			check(State.Donations.GetAllocatedSize() <= Plan.Lease->Bytes());
			State.DonationsLease = MoveTemp(Plan.Lease);
			State.DonationsCapacity = Plan.Capacity;
		}
		check(!ScratchLease || DependencyNodes.GetAllocatedSize() + Growth.GetAllocatedSize() <= ScratchLease->Bytes());
		if (!PriorityGraph.SetDependencies(Consumer->PriorityNode, DependencyNodes, Error))
			return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
	}
	PriorityGraph.SetPriority(Consumer->PriorityNode, Priority);
	Registration.Commit();
	for (auto Edge = Consumer->Dependencies.CreateIterator(); Edge; ++Edge)
	{
		if (!Desired.Contains(Edge.Key()))
		{
			UnlinkSubscriptionWake(*Consumer, Edge.Value());
			RemoveSubscription(Edge.Key(), InConsumer, Edge.Value().Id);
			Edge.RemoveCurrent();
		}
	}
	FVoxelGenerationDependencyStatus Status;
	Status.bReady = true;
	for (FGuid Node = Desired.Head; Node.IsValid();)
	{
		const auto Dependency = Storage->PriorityFacts.FindChecked(Node);
		auto& State = Storage->States.FindChecked(Dependency);
		FSubscription* Subscription = Consumer->Dependencies.Find(Dependency);
		if (!Subscription)
		{
			checkf(NextSubscriptionId != MAX_uint64, TEXT("Generation subscription serial exhausted"));
			Subscription = &Consumer->Dependencies.Add(Dependency);
			Subscription->Id = ++NextSubscriptionId;
		}
		const auto Previous = State.ServicePriority;
		State.Donations.Add(InConsumer, {InWorkClass, Priority, Subscription->Id});
		ResolveDonations(State);
		PriorityGraph.SetPriority(Node, State.ServicePriority);
		if (State.bInFlight && Previous != State.ServicePriority) Cache->RecordPlanDonation(Dependency.Kind);
		const bool bCached = IsCached(Dependency);
		if (bCached)
		{
			State.State = EVoxelPlanDependencyState::Ready;
			State.Error.Reset();
		}
		else if (State.State == EVoxelPlanDependencyState::Ready)
			State.State = State.bInFlight ? EVoxelPlanDependencyState::Queued : EVoxelPlanDependencyState::Missing;
		if (State.State == EVoxelPlanDependencyState::Failed)
		{
			Status.bFailed = true;
			Status.Error = State.Error;
		}
		Status.bReady &= bCached;
		Node = State.NextDesired;
	}
	AdvancePriorityDonations();
	for (FGuid Node = Desired.Head; Node.IsValid();)
	{
		const auto Dependency = Storage->PriorityFacts.FindChecked(Node);
		auto& State = Storage->States.FindChecked(Dependency);
		if (State.State == EVoxelPlanDependencyState::Missing && !State.bInFlight && ArePrerequisitesReady(State)) Queue(Dependency, State);
		Node = State.NextDesired;
	}
	RefreshQueuedPriorities();
	return Status;
}

void FVoxelGenerationPlanCoordinator::Reset()
{
	check(IsInGameThread());
	check(EnvironmentOwnerIncarnation != MAX_uint64 && ControlConsumer.OwnerIncarnation != MAX_uint64);
	const uint64 PreviousIncarnation = EnvironmentOwnerIncarnation++;
	Scheduler.CancelMatching([Epoch = WorldEpoch, Owner = EnvironmentOwnerId, PreviousIncarnation](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildGenerationPlan && Stamp.WorldEpoch == Epoch &&
			Stamp.OwnerId == Owner && Stamp.OwnerIncarnation == PreviousIncarnation;
	});
	if (Storage) Storage->Owner = nullptr;
	Storage.Reset();
	++ControlConsumer.OwnerIncarnation;
	PriorityGraph.Reset();
}

void FVoxelGenerationPlanCoordinator::ReleaseConsumer(const FVoxelGenerationConsumerKey& InConsumer)
{
	check(IsInGameThread());
	if (!Storage) return;
	if (auto* Consumer = Storage->Consumers.Find(InConsumer))
	{
		const FGuid Node = Consumer->PriorityNode;
		for (auto Edge = Consumer->Dependencies.CreateIterator(); Edge; ++Edge)
		{
			UnlinkSubscriptionWake(*Consumer, Edge.Value());
			RemoveSubscription(Edge.Key(), InConsumer, Edge.Value().Id);
			Edge.RemoveCurrent();
		}
		UnlinkConsumerWake(*Consumer);
		PriorityGraph.Remove(Node);
		Storage->Consumers.Remove(InConsumer);
	}
	if (InConsumer.Kind == EVoxelGenerationConsumerKind::Environment)
	{
		auto Space = InConsumer;
		Space.RequestSerial = 1;
		if (const auto* Current = Storage->EnvironmentAttempts.Find(Space); Current && *Current == InConsumer)
			Storage->EnvironmentAttempts.Remove(Space);
	}
	AdvancePriorityDonations();
	RefreshQueuedPriorities();
}

void FVoxelGenerationPlanCoordinator::RemoveSubscription(const FVoxelGenerationPlanDependency& InDependency,
	const FVoxelGenerationConsumerKey& InConsumer, const uint64 InSubscriptionId)
{
	FState* State = Storage->States.Find(InDependency);
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
	if (!Storage) return 0;
	uint64 Count = 0;
	for (const auto& Consumer : Storage->Consumers)
	{
		for (const auto& Edge : Consumer.Value.Dependencies)
		{
			const auto* State = Storage->States.Find(Edge.Key);
			if (Edge.Key.Kind == InKind && State && State->State != EVoxelPlanDependencyState::Ready &&
				State->State != EVoxelPlanDependencyState::Failed)
			{
				++Count;
				break;
			}
		}
	}
	return Count;
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

void FVoxelGenerationPlanCoordinator::LinkConsumerWake(const FVoxelGenerationConsumerKey& InConsumer, FConsumerState& InState)
{
	if (InState.bWakeQueued) return;
	InState.bWakeQueued = true;
	InState.PreviousWake = Storage->WakeTail;
	InState.NextWake = {};
	if (Storage->WakeTail.IsValid()) Storage->Consumers.FindChecked(Storage->WakeTail).NextWake = InConsumer;
	else Storage->WakeHead = InConsumer;
	Storage->WakeTail = InConsumer;
}

void FVoxelGenerationPlanCoordinator::UnlinkConsumerWake(FConsumerState& InState)
{
	if (!InState.bWakeQueued) return;
	if (InState.PreviousWake.IsValid()) Storage->Consumers.FindChecked(InState.PreviousWake).NextWake = InState.NextWake;
	else Storage->WakeHead = InState.NextWake;
	if (InState.NextWake.IsValid()) Storage->Consumers.FindChecked(InState.NextWake).PreviousWake = InState.PreviousWake;
	else Storage->WakeTail = InState.PreviousWake;
	InState.PreviousWake = {};
	InState.NextWake = {};
	InState.bWakeQueued = false;
}

void FVoxelGenerationPlanCoordinator::UnlinkSubscriptionWake(FConsumerState& InConsumer, FSubscription& InSubscription)
{
	if (!InSubscription.bWakePending) return;
	if (InSubscription.PreviousWake.IsValid())
		InConsumer.Dependencies.FindChecked(Storage->PriorityFacts.FindChecked(InSubscription.PreviousWake)).NextWake = InSubscription.NextWake;
	else InConsumer.WakeHead = InSubscription.NextWake;
	if (InSubscription.NextWake.IsValid())
		InConsumer.Dependencies.FindChecked(Storage->PriorityFacts.FindChecked(InSubscription.NextWake)).PreviousWake = InSubscription.PreviousWake;
	else InConsumer.WakeTail = InSubscription.PreviousWake;
	InSubscription.PreviousWake.Invalidate();
	InSubscription.NextWake.Invalidate();
	InSubscription.bWakePending = false;
	if (!InConsumer.WakeHead.IsValid()) UnlinkConsumerWake(InConsumer);
}

void FVoxelGenerationPlanCoordinator::WakeConsumers(const FVoxelGenerationPlanDependency& InDependency)
{
	const auto* State = Storage ? Storage->States.Find(InDependency) : nullptr;
	if (!State || !ConsumerWakeCallback) return;
	for (const auto& Donation : State->Donations)
	{
		auto* Consumer = Storage->Consumers.Find(Donation.Key);
		auto* Edge = Consumer ? Consumer->Dependencies.Find(InDependency) : nullptr;
		if (!Edge || Edge->Id != Donation.Value.SubscriptionId || Donation.Key.WorldEpoch != WorldEpoch || Edge->bWakePending) continue;
		Edge->bWakePending = true;
		Edge->PreviousWake = Consumer->WakeTail;
		Edge->NextWake.Invalidate();
		if (Consumer->WakeTail.IsValid())
			Consumer->Dependencies.FindChecked(Storage->PriorityFacts.FindChecked(Consumer->WakeTail)).NextWake = State->PriorityNode;
		else Consumer->WakeHead = State->PriorityNode;
		Consumer->WakeTail = State->PriorityNode;
		LinkConsumerWake(Donation.Key, *Consumer);
	}
}

int32 FVoxelGenerationPlanCoordinator::AdvanceConsumerWakes(const double InDeadline, const int32 InMaximumEdges)
{
	check(IsInGameThread());
	int32 Processed = 0;
	while (Storage && Storage->WakeHead.IsValid() && Processed < FMath::Max(0, InMaximumEdges) && FPlatformTime::Seconds() <= InDeadline)
	{
		const auto ConsumerKey = Storage->WakeHead;
		auto& Consumer = Storage->Consumers.FindChecked(ConsumerKey);
		const auto Dependency = Storage->PriorityFacts.FindChecked(Consumer.WakeHead);
		auto& Edge = Consumer.Dependencies.FindChecked(Dependency);
		const uint64 Id = Edge.Id;
		UnlinkSubscriptionWake(Consumer, Edge);
		if (Consumer.bWakeQueued)
		{
			UnlinkConsumerWake(Consumer);
			LinkConsumerWake(ConsumerKey, Consumer);
		}
		++Processed;
		const auto* State = Storage->States.Find(Dependency);
		const auto* Donation = State ? State->Donations.Find(ConsumerKey) : nullptr;
		if (Donation && Donation->SubscriptionId == Id && ConsumerKey.WorldEpoch == WorldEpoch && ConsumerWakeCallback)
			ConsumerWakeCallback(ConsumerKey);
	}
	return Processed;
}

FVoxelGenerationConsumerKey FVoxelGenerationPlanCoordinator::EnvironmentConsumer(
	const FVoxelGenerationBounds& InBounds, const bool bInColumn)
{
	FString Error;
	if (!PrepareStorage(Error)) return {};
	const FVoxelGenerationConsumerKey Space{EVoxelGenerationConsumerKind::Environment, WorldEpoch, EnvironmentOwnerId, EnvironmentOwnerIncarnation,
		bInColumn ? EVoxelGenerationSpatialKind::Column : EVoxelGenerationSpatialKind::Bounds,
		InBounds.Min, InBounds.Max, 1};
	if (const auto* Existing = Storage->EnvironmentAttempts.Find(Space)) return *Existing;
	if (!PrepareMap<FVoxelGenerationConsumerKey, FVoxelGenerationConsumerKey>(Storage->EnvironmentAttempts,
		Storage->EnvironmentAttempts.Num() + 1, Storage->EnvironmentCapacity, Storage->EnvironmentLease, Error)) return {};
	checkf(NextEnvironmentAttempt != MAX_uint64, TEXT("环境请求代耗尽"));
	auto Consumer = Space;
	Consumer.RequestSerial = ++NextEnvironmentAttempt;
	Storage->EnvironmentAttempts.Add(Space, Consumer);
	return Consumer;
}

FVoxelGenerationPlanCoordinator::FClosureRegistration::FClosureRegistration(FVoxelGenerationPlanCoordinator& InOwner)
	: Owner(InOwner)
{
}

FVoxelGenerationPlanCoordinator::FClosureRegistration::~FClosureRegistration()
{
	while (Head.IsValid())
	{
		const auto Dependency = Owner.Storage->PriorityFacts.FindChecked(Head);
		auto& State = Owner.Storage->States.FindChecked(Dependency);
		Head = State.NextRegistration;
		State.NextRegistration.Invalidate();
		if (!bCommitted)
		{
			check(State.Donations.IsEmpty() && !State.bInFlight);
			Owner.ForgetFact(Dependency, false);
		}
	}
}

void FVoxelGenerationPlanCoordinator::FClosureRegistration::Add(FState& InState)
{
	InState.NextRegistration = Head;
	Head = InState.PriorityNode;
}

void FVoxelGenerationPlanCoordinator::FClosureRegistration::Commit()
{
	bCommitted = true;
}

FVoxelGenerationPlanCoordinator::FClosureDemand::FClosureDemand(FVoxelGenerationPlanCoordinator& InOwner)
	: Owner(InOwner)
{
}

FVoxelGenerationPlanCoordinator::FClosureDemand::~FClosureDemand()
{
	while (Head.IsValid())
	{
		auto& State = Owner.Storage->States.FindChecked(Owner.Storage->PriorityFacts.FindChecked(Head));
		Head = State.NextDesired;
		State.NextDesired.Invalidate();
		State.bDesired = false;
	}
}

void FVoxelGenerationPlanCoordinator::FClosureDemand::Add(const FVoxelGenerationPlanDependency& InDependency)
{
	auto& State = Owner.Storage->States.FindChecked(InDependency);
	if (State.bDesired) return;
	State.bDesired = true;
	State.NextDesired = Head;
	Head = State.PriorityNode;
	++Count;
	for (const auto& Prerequisite : State.Prerequisites) Add(Prerequisite);
}

bool FVoxelGenerationPlanCoordinator::FClosureDemand::Contains(const FVoxelGenerationPlanDependency& InDependency) const
{
	const auto* State = Owner.Storage->States.Find(InDependency);
	return State && State->bDesired;
}

bool FVoxelGenerationPlanCoordinator::SetFactDependencies(const FVoxelGenerationPlanDependency& InDependency, FString& OutError)
{
	const auto& State = Storage->States.FindChecked(InDependency);
	bool bSame = PriorityGraph.GetDependencyCount(State.PriorityNode) == State.Prerequisites.Num();
	for (const auto& Prerequisite : State.Prerequisites)
	{
		const auto* Child = Storage->States.Find(Prerequisite);
		if (!Child)
		{
			OutError = TEXT("CapacityBlocked: generation read closure registration incomplete");
			return false;
		}
		bSame &= PriorityGraph.HasDependency(State.PriorityNode, Child->PriorityNode);
	}
	if (bSame)
	{
		OutError.Reset();
		return true;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::SetBytes<FGuid>(State.Prerequisites.Num());
	if (Bytes && !Resources->Acquire(ControlConsumer, EVoxelTerrainResourcePool::Working, Bytes, Lease))
	{
		OutError = TEXT("CapacityBlocked: generation dependency edges");
		return false;
	}
	TSet<FGuid> Edges;
	Edges.Reserve(State.Prerequisites.Num());
	for (const auto& Prerequisite : State.Prerequisites)
	{
		const auto* Child = Storage->States.Find(Prerequisite);
		if (!Child)
		{
			OutError = TEXT("CapacityBlocked: generation read closure registration incomplete");
			return false;
		}
		Edges.Add(Child->PriorityNode);
	}
	return PriorityGraph.SetDependencies(State.PriorityNode, Edges, OutError);
}

bool FVoxelGenerationPlanCoordinator::RegisterClosure(const FVoxelGenerationPlanDependency& InDependency,
	FClosureRegistration& InRegistration, FString& OutError)
{
	if (auto* Existing = Storage->States.Find(InDependency); Existing && Existing->PriorityNode.IsValid())
	{
		if (Existing->bRegistering)
		{
			OutError = TEXT("生成事实的采样依赖形成循环");
			return false;
		}
		Existing->bRegistering = true;
		ON_SCOPE_EXIT
		{
			Storage->States.FindChecked(InDependency).bRegistering = false;
		};
		const int32 Count = Existing->Prerequisites.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const auto Prerequisite = Storage->States.FindChecked(InDependency).Prerequisites[Index];
			if (!RegisterClosure(Prerequisite, InRegistration, OutError)) return false;
		}
		return SetFactDependencies(InDependency, OutError);
	}
	FIntPoint Min;
	FIntPoint Max;
	int32 Count = 0;
	if (!GetPrerequisiteRange(InDependency, Min, Max, Count, OutError)) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PrerequisitesLease;
	if (Count && !Resources->Acquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::ArrayBytes<FVoxelGenerationPlanDependency>(Count), PrerequisitesLease))
	{
		OutError = TEXT("CapacityBlocked: generation prerequisite capture");
		return false;
	}
	TArray<FVoxelGenerationPlanDependency> Prerequisites;
	Prerequisites.Reserve(Count);
	if (Count)
	{
		for (int64 Y = Min.Y; Y <= Max.Y; ++Y)
			for (int64 X = Min.X; X <= Max.X; ++X)
				Prerequisites.Add(MakeDependency(EVoxelGenerationPlanKind::Hydrology, FIntVector(int32(X), int32(Y), 0)));
	}
	for (const auto& Prerequisite : Prerequisites)
		if (!RegisterClosure(Prerequisite, InRegistration, OutError)) return false;
	if (!PrepareMap<FVoxelGenerationPlanDependency, FState>(Storage->States, Storage->States.Num() + 1,
		Storage->StatesCapacity, Storage->StatesLease, OutError) ||
		!PrepareMap<FGuid, FVoxelGenerationPlanDependency>(Storage->PriorityFacts, Storage->PriorityFacts.Num() + 1,
			Storage->FactsCapacity, Storage->FactsLease, OutError)) return false;
	const FGuid Node = FGuid::NewGuid();
	if (!PriorityGraph.Register(Node, {}, OutError)) return false;
	auto& State = Storage->States.Add(InDependency);
	State.PriorityNode = Node;
	State.Prerequisites = MoveTemp(Prerequisites);
	State.PrerequisitesLease = MoveTemp(PrerequisitesLease);
	State.PrerequisitesCapacity = Count;
	Storage->PriorityFacts.Add(Node, InDependency);
	InRegistration.Add(State);
	return SetFactDependencies(InDependency, OutError);
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
		const auto* Fact = Storage->PriorityFacts.Find(Node);
		auto* State = Fact ? Storage->States.Find(*Fact) : nullptr;
		if (!State) return;
		if (State->bInFlight && State->ServicePriority != Priority) Cache->RecordPlanDonation(Fact->Kind);
		State->ServicePriority = Priority;
		State->SourcePriority = Priority.SourcePriority;
		State->DistanceScore = Priority.DistanceCells;
		State->ForwardScore = Priority.ForwardScore;
	});
}

void FVoxelGenerationPlanCoordinator::ForgetFact(const FVoxelGenerationPlanDependency& InDependency, const bool bReleasePrerequisites)
{
	if (auto* State = Storage->States.Find(InDependency))
	{
		const auto Node = State->PriorityNode;
		auto PrerequisitesLease = MoveTemp(State->PrerequisitesLease);
		auto Prerequisites = MoveTemp(State->Prerequisites);
		PriorityGraph.Remove(Node);
		Storage->PriorityFacts.Remove(Node);
		Storage->States.Remove(InDependency);
		if (bReleasePrerequisites)
		{
			for (const auto& Prerequisite : Prerequisites)
			{
				const auto* Child = Storage->States.Find(Prerequisite);
				if (Child && Child->Donations.IsEmpty() && !Child->bInFlight && !PriorityGraph.HasConsumers(Child->PriorityNode))
					ForgetFact(Prerequisite);
			}
		}
	}
}

void FVoxelGenerationPlanCoordinator::ReleaseEnvironment(const FIntPoint& InCell)
{
	if (!Storage) return;
	const FIntVector Cell(InCell.X, InCell.Y, 0);
	const FVoxelGenerationConsumerKey Space{EVoxelGenerationConsumerKind::Environment, WorldEpoch, EnvironmentOwnerId, EnvironmentOwnerIncarnation,
		EVoxelGenerationSpatialKind::Column, Cell, Cell, 1};
	if (const auto* Current = Storage->EnvironmentAttempts.Find(Space))
	{
		const auto Consumer = *Current;
		ReleaseConsumer(Consumer);
		Storage->EnvironmentAttempts.Remove(Space);
	}
}
