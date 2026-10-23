#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"

#include "Misc/ScopeExit.h"
#include "HAL/PlatformTime.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	constexpr int32 MaximumHydrologyDependencies = 4096;
	struct FPlanReadDependencies final : FVoxelTaskCustomPayload
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TArray<FVoxelGenerationPlanDependency> Missing;
		bool bOverflow = false;
		TAtomic<bool> bWaitingCapacity{false};
		TAtomic<uint64> CapacityVersion{0};
		virtual uint64 GetAllocatedBytes() const override
		{
			return VoxelBoundedAllocation::WorkerSharedBytes<FPlanReadDependencies>() + VoxelGenerationAllocation::AllocatedBytes(Missing);
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

	const auto CacheResources = Cache->GetResources();
	if (CacheResources && (CacheResources.Get() != &Resources.Get() ||
		Cache->GetControlConsumer().WorldEpoch != WorldEpoch))
	{
		OutError = TEXT("Invalid original generation cache resource or world identity");
		return false;
	}
	if (!Cache->BindHydrologyRecipe(Config->Recipe->RecipeHash, Config->Recipe->AlgorithmVersion, OutError)) return false;
	if (Storage) return true;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	FVoxelGenerationRequiredSourceScope RootSources(&Resources.Get(), this,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Owner = *static_cast<const FVoxelGenerationPlanCoordinator*>(Context);
			uint64 CacheBytes;
			int32 CacheLeaseCount;
			Owner.Cache->GetRequiredInputCapacity(CacheBytes, CacheLeaseCount);
			if (CacheLeaseCount) Visit({&Owner.Cache.Get(), 0, EVoxelTerrainResourcePool::Input, CacheBytes, CacheLeaseCount});
			for (const auto* Original : {Owner.Config->InputLease.Get(), Owner.Config->RecipeInputLease.Get()})
				if (Original && Original->IsOwnedBy(Owner.Resources.Get())) FVoxelGenerationRequiredSourceScope::VisitLease(Original, Visit);
		});
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(Resources.Get(), nullptr, Required, OutError)) return false;
	const auto Admission = Resources->TryAcquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FStorage>(), Lease, 0,
		Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: generation coordinator storage") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: generation coordinator storage and original roots") :
			TEXT("InvalidRequest: generation coordinator storage");
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
	FVoxelGenerationRequiredSourceScope RootSources(&Resources.Get(), this,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Owner = *static_cast<const FVoxelGenerationPlanCoordinator*>(Context);
			uint64 CacheBytes;
			int32 CacheLeaseCount;
			Owner.Cache->GetRequiredInputCapacity(CacheBytes, CacheLeaseCount);
			if (CacheLeaseCount) Visit({&Owner.Cache.Get(), 0, EVoxelTerrainResourcePool::Input, CacheBytes, CacheLeaseCount});
			for (const auto* Original : {Owner.Config->InputLease.Get(), Owner.Config->RecipeInputLease.Get(),
				Owner.Storage->ResourceLease.Get(), Owner.Storage->StatesLease.Get(), Owner.Storage->ConsumersLease.Get(),
				Owner.Storage->FactsLease.Get(), Owner.Storage->EnvironmentLease.Get()})
				if (Original && Original->IsOwnedBy(Owner.Resources.Get())) FVoxelGenerationRequiredSourceScope::VisitLease(Original, Visit);
		});
	// 原map仍占有旧backing；新租约与它同时存在，来源按原serial去重。
	FVoxelGenerationRequiredSourceScope PreviousMapSource(&Resources.Get(), InLease.Get());
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(Resources.Get(), nullptr, Required, OutError)) return false;
	const auto Admission = Resources->TryAcquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<KeyType, ValueType>>(InMap, Capacity), Lease, 0,
		Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: generation coordinator control map") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: generation coordinator control map and retained inputs") :
			TEXT("InvalidRequest: generation coordinator control map");
		return false;
	}
	InMap.Reserve(Capacity);
	check(VoxelGenerationAllocation::AllocatedBytes(InMap) <= Lease->Bytes());
	InLease = MoveTemp(Lease);
	InCapacity = Capacity;
	return true;
}

bool FVoxelGenerationPlanCoordinator::PreparePrerequisites(FState& InState, const int32 InRequired, FString& OutError)
{
	if (InRequired < 0 || InRequired > MaximumHydrologyDependencies)
	{
		OutError = TEXT("生成事实的累计采样闭包超过上限");
		return false;
	}
	if (InRequired <= InState.PrerequisitesCapacity) return true;
	const int32 Capacity = FMath::Min(MaximumHydrologyDependencies,
		FMath::Max(InRequired, FMath::Max(4, InState.PrerequisitesCapacity * 2)));
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	FVoxelGenerationRequiredSourceScope RootSources(&Resources.Get(), this,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Owner = *static_cast<const FVoxelGenerationPlanCoordinator*>(Context);
			uint64 CacheBytes;
			int32 CacheLeaseCount;
			Owner.Cache->GetRequiredInputCapacity(CacheBytes, CacheLeaseCount);
			if (CacheLeaseCount) Visit({&Owner.Cache.Get(), 0, EVoxelTerrainResourcePool::Input, CacheBytes, CacheLeaseCount});
			for (const auto* Original : {Owner.Config->InputLease.Get(), Owner.Config->RecipeInputLease.Get(),
				Owner.Storage->ResourceLease.Get(), Owner.Storage->StatesLease.Get(), Owner.Storage->ConsumersLease.Get(),
				Owner.Storage->FactsLease.Get(), Owner.Storage->EnvironmentLease.Get()})
				if (Original && Original->IsOwnedBy(Owner.Resources.Get())) FVoxelGenerationRequiredSourceScope::VisitLease(Original, Visit);
		});
	FVoxelGenerationRequiredSourceScope StateSources(&Resources.Get(), &InState,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& State = *static_cast<const FState*>(Context);
			for (const auto* Original : {State.PrerequisitesLease.Get(), State.DonationsLease.Get()})
				if (Original) FVoxelGenerationRequiredSourceScope::VisitLease(Original, Visit);
			if (State.PrerequisiteRetention)
				FVoxelGenerationRequiredSourceScope::VisitLease(&State.PrerequisiteRetention->GetResourceLease().Get(), Visit);
		});
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(Resources.Get(), nullptr, Required, OutError)) return false;
	const auto Admission = Resources->TryAcquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelGenerationPlanDependency, FDefaultAllocator>(Capacity), Lease, 0,
		Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: generation prerequisite array") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: generation prerequisite array and retained inputs") :
			TEXT("InvalidRequest: generation prerequisite array");
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
	const auto MinRegion = FVoxelGenerationQuery::HydrologyKeyForVoxel(InBounds.Min.X, InBounds.Min.Y, Settings, Config->Recipe->AlgorithmVersion).Coordinate;
	const auto MaxRegion = FVoxelGenerationQuery::HydrologyKeyForVoxel(InBounds.Max.X - 1, InBounds.Max.Y - 1, Settings, Config->Recipe->AlgorithmVersion).Coordinate;
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
			const uint64 Bytes = VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelGenerationPlanDependency>(Closure, NewCapacity);
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
			check(VoxelGenerationAllocation::AllocatedBytes(Closure) <= NewLease->Bytes());
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

bool FVoxelGenerationPlanCoordinator::GatherEnvironmentSamples(const FIntPoint& InOrigin,
	const int32 InWidth, const int32 InHeight, const int32 InStep, const FVoxelGenerationConsumerKey& InConsumer,
	FVoxelGenerationPlanDependencyCapture& OutDependencies, FString& OutError) const
{
	check(IsInGameThread());
	if (!Config->Recipe || !InConsumer.IsValid() || InConsumer.WorldEpoch != WorldEpoch ||
		InWidth <= 0 || InHeight <= 0 || InStep <= 0 || InWidth > MaximumHydrologyDependencies ||
		InHeight > MaximumHydrologyDependencies / InWidth)
	{
		OutError = TEXT("InvalidRequest: environment sample grid or consumer");
		return false;
	}
	const int64 LastX = int64(InOrigin.X) + (int64(InWidth) - 1) * InStep;
	const int64 LastY = int64(InOrigin.Y) + (int64(InHeight) - 1) * InStep;
	if (InOrigin.X < VoxelGeneration::WorldCellMin || InOrigin.Y < VoxelGeneration::WorldCellMin ||
		LastX >= VoxelGeneration::WorldCellMaxExclusive || LastY >= VoxelGeneration::WorldCellMaxExclusive)
	{
		OutError = TEXT("InvalidRequest: environment sample grid outside world cell domain");
		return false;
	}
	const auto& Settings = Config->Recipe->Settings;
	int32 Width = 0;
	int32 Height = 0;
	int32 PreviousX = 0;
	int32 PreviousY = 0;
	for (int32 Index = 0; Index < InWidth; ++Index)
	{
		const int32 CellX = int32(int64(InOrigin.X) + int64(Index) * InStep);
		const int32 RegionX = FVoxelGenerationQuery::HydrologyKeyForVoxel(CellX, InOrigin.Y, Settings, Config->Recipe->AlgorithmVersion).Coordinate.X;
		if (Index == 0 || PreviousX != RegionX)
		{
			++Width;
			PreviousX = RegionX;
		}
	}
	for (int32 Index = 0; Index < InHeight; ++Index)
	{
		const int32 CellY = int32(int64(InOrigin.Y) + int64(Index) * InStep);
		const int32 RegionY = FVoxelGenerationQuery::HydrologyKeyForVoxel(InOrigin.X, CellY, Settings, Config->Recipe->AlgorithmVersion).Coordinate.Y;
		if (Index == 0 || PreviousY != RegionY)
		{
			++Height;
			PreviousY = RegionY;
		}
	}
	const int32 Count = Width * Height;
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FVoxelGenerationPlanDependency>(Count);
	const bool bPreviousOwned = OutDependencies.Lease && OutDependencies.Lease->IsOwnedBy(Resources.Get());
	FVoxelGenerationPlanDependencyCapture Candidate;
	const auto Admission = Resources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input, Bytes, Candidate.Lease,
		0, bPreviousOwned ? OutDependencies.Lease->Bytes() : 0, bPreviousOwned ? 1 : 0);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked
			? TEXT("CapacityBlocked: environment sample dependency capture")
			: Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable
			? TEXT("CapacityUnachievable: environment sample dependency capture")
			: TEXT("InvalidRequest: environment sample dependency capture");
		return false;
	}
	Candidate.Values.Reserve(Count);
	check(Candidate.Values.GetAllocatedSize() <= Candidate.Lease->Bytes());
	for (int32 Row = 0; Row < InHeight; ++Row)
	{
		const int32 CellY = int32(int64(InOrigin.Y) + int64(Row) * InStep);
		const int32 RegionY = FVoxelGenerationQuery::HydrologyKeyForVoxel(InOrigin.X, CellY, Settings, Config->Recipe->AlgorithmVersion).Coordinate.Y;
		if (Row > 0 && PreviousY == RegionY)
		{
			continue;
		}
		PreviousY = RegionY;
		for (int32 Column = 0; Column < InWidth; ++Column)
		{
			const int32 CellX = int32(int64(InOrigin.X) + int64(Column) * InStep);
			const int32 RegionX = FVoxelGenerationQuery::HydrologyKeyForVoxel(CellX, InOrigin.Y, Settings, Config->Recipe->AlgorithmVersion).Coordinate.X;
			if (Column > 0 && PreviousX == RegionX)
			{
				continue;
			}
			PreviousX = RegionX;
			Candidate.Values.Add(MakeDependency(EVoxelGenerationPlanKind::Hydrology, FIntVector(RegionX, RegionY, 0)));
		}
	}
	check(Candidate.Values.Num() == Count);
	OutDependencies = MoveTemp(Candidate);
	OutError.Reset();
	return true;


}

bool FVoxelGenerationPlanCoordinator::GatherEnvironmentGeometry(const FVoxelGenerationBounds& InBounds,
	const FIntPoint& InOrigin, const int32 InWidth, const int32 InHeight, const int32 InStep,
	const bool bIncludeTreeCandidates, const FVoxelGenerationConsumerKey& InConsumer,
	FVoxelGenerationPlanDependencyCapture& OutDependencies, FString& OutError) const
{
	constexpr int32 MaximumFacts = 1 << 20;
	if (!Config->Recipe || !InBounds.IsValid() || !InConsumer.IsValid() || InConsumer.WorldEpoch != WorldEpoch ||
		InWidth <= 0 || InHeight <= 0 || InStep <= 0 || InWidth > MaximumFacts || InHeight > MaximumFacts / InWidth)
	{ OutError = TEXT("Invalid or oversized sparse environment geometry"); return false; }
	const int64 LastX = int64(InOrigin.X) + (int64(InWidth) - 1) * InStep;
	const int64 LastY = int64(InOrigin.Y) + (int64(InHeight) - 1) * InStep;
	if (!VoxelGeneration::IsWorldCell(InOrigin.X) || !VoxelGeneration::IsWorldCell(InOrigin.Y) ||
		!VoxelGeneration::IsWorldCell(LastX) || !VoxelGeneration::IsWorldCell(LastY) ||
		InOrigin.X < InBounds.Min.X || InOrigin.Y < InBounds.Min.Y || LastX >= InBounds.Max.X || LastY >= InBounds.Max.Y)
	{ OutError = TEXT("Sparse environment samples outside declared bounds"); return false; }
	const auto& Settings = Config->Recipe->Settings;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ClosureLease;
	TSet<FVoxelGenerationPlanDependency> Closure;
	int32 Capacity = 0;
	const auto Append = [&](const FVoxelGenerationPlanDependency& Fact)
	{
		if (Closure.Contains(Fact)) return true;
		if (Closure.Num() == MaximumFacts) { OutError = TEXT("Oversized: sparse environment facts"); return false; }
		if (Closure.Num() == Capacity)
		{
			const int32 NewCapacity = FMath::Max(4, Capacity * 2);
			const uint64 Bytes = VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelGenerationPlanDependency>(Closure, NewCapacity);
			if (Bytes + (ClosureLease ? ClosureLease->Bytes() : 0) > Resources->Snapshot().Limits.WorkingBytes)
			{ OutError = TEXT("CapacityUnachievable: sparse environment capture backing"); return false; }
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NewLease;
			if (!Resources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working, Bytes, NewLease))
			{ OutError = TEXT("CapacityBlocked: sparse environment capture backing"); return false; }
			Closure.Reserve(NewCapacity);
			check(VoxelGenerationAllocation::AllocatedBytes(Closure) <= NewLease->Bytes());
			ClosureLease = MoveTemp(NewLease); Capacity = NewCapacity;
		}
		Closure.Add(Fact); return true;
	};
	const auto AppendCell = [&](const int32 X, const int32 Y)
	{
		const auto Key = FVoxelGenerationQuery::HydrologyKeyForVoxel(X, Y, Settings, Config->Recipe->AlgorithmVersion);
		return Append(MakeDependency(EVoxelGenerationPlanKind::Hydrology, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0)));
	};

		for (int32 Y = 0; Y < InHeight; ++Y) for (int32 X = 0; X < InWidth; ++X)
			if (!AppendCell(int32(int64(InOrigin.X) + int64(X) * InStep), int32(int64(InOrigin.Y) + int64(Y) * InStep))) return false;
		const auto& Trees = Settings.Ecology.Tree;
		if (bIncludeTreeCandidates && Trees.bEnabled && FMath::Clamp(int32((int64(Trees.ChancePermille) * Trees.DensityPermille) / 1000), 0, 1000) > 0)
		{
			const int32 Spacing = FMath::Max(4, Trees.Spacing);
			const int64 MinGX = VoxelGeneration::FloorDivideWide(InBounds.Min.X, Spacing);
			const int64 MaxGX = VoxelGeneration::FloorDivideWide(int64(InBounds.Max.X) - 1, Spacing);
			const int64 MinGY = VoxelGeneration::FloorDivideWide(InBounds.Min.Y, Spacing);
			const int64 MaxGY = VoxelGeneration::FloorDivideWide(int64(InBounds.Max.Y) - 1, Spacing);
			const int64 W = MaxGX - MinGX + 1, H = MaxGY - MinGY + 1;
			if (W > MaximumFacts || H > MaximumFacts / W) { OutError = TEXT("Oversized: sparse tree candidate facts"); return false; }
			for (int64 GY = MinGY; GY <= MaxGY; ++GY) for (int64 GX = MinGX; GX <= MaxGX; ++GX)
			{
				const uint64 Seed = VoxelGeneration::MakeSeed(Settings.Seed, FIntVector(int32(GX), int32(GY), 0), 0x62CEBFAD38B9C4D1ull);
				const int32 DX = VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0x91827211ABF49D33ull), 0, Spacing - 1);
				const int32 DY = VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0x42CF9A177A16C593ull), 0, Spacing - 1);
				FIntVector XY;
				if (!VoxelGeneration::TryWorldCell(FInt64Vector(GX * Spacing + DX, GY * Spacing + DY, 0), XY)) continue;
				if (XY.X >= InBounds.Min.X && XY.X < InBounds.Max.X && XY.Y >= InBounds.Min.Y && XY.Y < InBounds.Max.Y &&
					!AppendCell(XY.X, XY.Y)) return false;
			}
		}

	FVoxelGenerationPlanDependencyCapture Candidate;
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FVoxelGenerationPlanDependency>(Closure.Num());
	const uint64 Limit = Resources->Snapshot().Limits.InputBytes;
	const uint64 Previous = OutDependencies.Lease && OutDependencies.Lease->IsOwnedBy(Resources.Get()) ? OutDependencies.Lease->Bytes() : 0;
	if (Bytes > Limit || Previous > Limit - Bytes) { OutError = TEXT("CapacityUnachievable: sparse environment capture"); return false; }
	if (!Resources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, Bytes, Candidate.Lease))
	{ OutError = TEXT("CapacityBlocked: sparse environment capture"); return false; }
	Candidate.Values.Reserve(Closure.Num());
	check(Candidate.Values.GetAllocatedSize() <= Candidate.Lease->Bytes());
	for (const auto& Fact : Closure) Candidate.Values.Add(Fact);
	OutDependencies = MoveTemp(Candidate); OutError.Reset(); return true;
}

bool FVoxelGenerationPlanCoordinator::GetPrerequisiteRange(const FVoxelGenerationPlanDependency& InDependency,
	FIntPoint& OutMin, FIntPoint& OutMax, int32& OutCount, FString& OutError) const
{
	OutCount = 0;
	OutError.Reset();
	if (!Config->Recipe) { OutError = TEXT("生成计划缺少配方"); return false; }
	const auto& Settings = Config->Recipe->Settings;
	const auto Coordinate = InDependency.Coordinate();

	if (InDependency.Kind == EVoxelGenerationPlanKind::Hydrology)
	{
		const int64 Side = FVoxelGenerationQuery::HydrologyKeySide(Settings, Config->Recipe->AlgorithmVersion);
		const FInt64Vector2 Core(int64(Coordinate.X) * Side, int64(Coordinate.Y) * Side);
		if (Coordinate.Z != 0 || Core.X >= VoxelGeneration::WorldCellMaxExclusive || Core.Y >= VoxelGeneration::WorldCellMaxExclusive ||
			Core.X + Side <= VoxelGeneration::WorldCellMin || Core.Y + Side <= VoxelGeneration::WorldCellMin)
		{
			OutError = TEXT("Invalid hydrology dependency outside world cell domain");
			return false;
		}
		return true;
	}
	const bool bEcology = InDependency.Kind == EVoxelGenerationPlanKind::Ecology;
	const int64 Side = bEcology ? FVoxelGenerationQuery::EcologyTileSide : FVoxelGenerationQuery::GenerationPlanTileSide;
	const FInt64Vector CoreMin(int64(Coordinate.X) * Side, int64(Coordinate.Y) * Side,
		bEcology ? Settings.MinZ : int64(Coordinate.Z) * Side);
	FVoxelGenerationBounds CoreBounds;
	if ((bEcology && Coordinate.Z != 0) || !VoxelGeneration::ClipWorldBounds(CoreMin,
		FInt64Vector(CoreMin.X + Side, CoreMin.Y + Side, bEcology ? Settings.MaxZ : CoreMin.Z + Side), CoreBounds))
	{
		OutError = TEXT("Invalid generation dependency outside world cell domain");
		return false;
	}
	int64 Margin = 0;
	if (InDependency.Kind == EVoxelGenerationPlanKind::Cave) Margin = FVoxelCaveGenerator::GetMaximumReach(Settings);
	else if (bEcology) Margin = FMath::Max(Settings.Ecology.Grass.PatchRadius, Settings.Ecology.Flower.PatchRadius);
	else if (InDependency.Kind == EVoxelGenerationPlanKind::Feature)
	{
		for (const auto& Feature : Config->Recipe->Features) Margin = FMath::Max<int64>(Margin, Feature.Placement.Spacing - 1);
	}
	const auto At = [this, &Settings](int64 X, int64 Y)
	{
		return FVoxelGenerationQuery::HydrologyKeyForVoxel(int32(FMath::Clamp<int64>(X, MIN_int32, MAX_int32 - 1)),
			int32(FMath::Clamp<int64>(Y, MIN_int32, MAX_int32 - 1)), Settings, Config->Recipe->AlgorithmVersion).Coordinate;
	};
	OutMin = At(int64(CoreBounds.Min.X) - Margin, int64(CoreBounds.Min.Y) - Margin);
	OutMax = At(int64(CoreBounds.Max.X) - 1 + Margin, int64(CoreBounds.Max.Y) - 1 + Margin);
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
	const int64 HydrologySide = FVoxelGenerationQuery::HydrologyKeySide(Settings, Config->Recipe->AlgorithmVersion);
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
		InCell.X, InCell.Y, Config->Recipe->Settings, Config->Recipe->AlgorithmVersion);
	const FVoxelGenerationPlanDependency Dependency = MakeDependency(
		EVoxelGenerationPlanKind::Hydrology,
		FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0));
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
		InBounds.Min.X, InBounds.Min.Y, Settings, Config->Recipe->AlgorithmVersion).Coordinate;
	const FIntPoint Max = FVoxelGenerationQuery::HydrologyKeyForVoxel(
		InBounds.Max.X - 1, InBounds.Max.Y - 1, Settings, Config->Recipe->AlgorithmVersion).Coordinate;
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

void FVoxelGenerationPlanCoordinator::LinkPendingAdmission(FState& InState)
{
	if (InState.bAdmissionLinked)
	{
		return;
	}
	InState.bAdmissionLinked = true;
	InState.PreviousAdmission = Storage->AdmissionTail;
	InState.NextAdmission.Invalidate();
	if (Storage->AdmissionTail.IsValid())
	{
		Storage->States.FindChecked(Storage->PriorityFacts.FindChecked(Storage->AdmissionTail)).NextAdmission = InState.PriorityNode;
	}
	else
	{
		Storage->AdmissionHead = InState.PriorityNode;
	}
	Storage->AdmissionTail = InState.PriorityNode;
	++Storage->AdmissionCount;
}

void FVoxelGenerationPlanCoordinator::UnlinkPendingAdmission(FState& InState)
{
	if (!InState.bAdmissionLinked)
	{
		return;
	}
	if (InState.PreviousAdmission.IsValid())
	{
		Storage->States.FindChecked(Storage->PriorityFacts.FindChecked(InState.PreviousAdmission)).NextAdmission = InState.NextAdmission;
	}
	else
	{
		Storage->AdmissionHead = InState.NextAdmission;
	}
	if (InState.NextAdmission.IsValid())
	{
		Storage->States.FindChecked(Storage->PriorityFacts.FindChecked(InState.NextAdmission)).PreviousAdmission = InState.PreviousAdmission;
	}
	else
	{
		Storage->AdmissionTail = InState.PreviousAdmission;
	}
	InState.PreviousAdmission.Invalidate();
	InState.NextAdmission.Invalidate();
	InState.bAdmissionLinked = false;
	check(Storage->AdmissionCount > 0);
	--Storage->AdmissionCount;
}

void FVoxelGenerationPlanCoordinator::ClearPendingRequest(FState& InState)
{
	UnlinkPendingAdmission(InState);
	// 原 Request 的 callable 先销毁，最后一个读取正文持有者再释放原 Worker 租约。
	InState.PendingRequest.Reset();
	InState.PendingReadDependencies.Reset();
	InState.bAttemptPendingAdmission = false;
	InState.bWaitingTaskCapacity = false;
	InState.bWaitingCapacity = false;
}

void FVoxelGenerationPlanCoordinator::FailAdmission(const FVoxelGenerationPlanDependency& InDependency, FState& InState,
	const FString& InError, const EVoxelTaskBlockReason InReason)
{
	if (InState.PendingRequest.IsSet())
	{
		const auto& Request = InState.PendingRequest.GetValue();
		Scheduler.ObserveDemand(Request.Kind, Request.Stamp, InReason, Request.InputBytes, Request.ReservedBytes);
	}
	ClearPendingRequest(InState);
	InState.State = EVoxelPlanDependencyState::Failed;
	InState.Error = InError;
	WakeConsumers(InDependency);
}

void FVoxelGenerationPlanCoordinator::WaitForTaskAdmission(FState& InState, const uint64 InVersion,
	const EVoxelTaskBlockReason InReason)
{
	InState.bWaitingTaskCapacity = true;
	InState.TaskCapacityVersion = InVersion;
	LinkPendingAdmission(InState);
	const auto& Request = InState.PendingRequest.GetValue();
	Scheduler.ObserveDemand(Request.Kind, Request.Stamp, InReason, Request.InputBytes, Request.ReservedBytes);
}

void FVoxelGenerationPlanCoordinator::RejectTaskBinding(const FVoxelGenerationPlanDependency& InDependency, FState& InState,
	const FString& InError, const uint64 InVersion)
{
	if (InError.StartsWith(TEXT("CapacityBlocked")))
	{
		WaitForTaskAdmission(InState, InVersion, EVoxelTaskBlockReason::InputMemory);
	}
	else
	{
		FailAdmission(InDependency, InState, InError,
			InError.StartsWith(TEXT("Oversized")) || InError.StartsWith(TEXT("CapacityUnachievable")) ?
			EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::TerminalFailure);
	}
}

int32 FVoxelGenerationPlanCoordinator::AdvancePendingAdmissions(const double InDeadline, const int32 InMaximumFacts)
{
	if (!Storage)
	{
		return 0;
	}
	FVoxelTaskScheduler::FEnqueueBatch EnqueueBatch(Scheduler);
	const int32 Maximum = FMath::Min(FMath::Max(0, InMaximumFacts), Storage->AdmissionCount);
	int32 Processed = 0;
	while (Storage && Storage->AdmissionHead.IsValid() && Processed < Maximum && FPlatformTime::Seconds() <= InDeadline)
	{
		const auto Dependency = Storage->PriorityFacts.FindChecked(Storage->AdmissionHead);
		auto& State = Storage->States.FindChecked(Dependency);
		UnlinkPendingAdmission(State);
		++Processed;
		if (State.Donations.IsEmpty() && !PriorityGraph.HasConsumers(State.PriorityNode))
		{
			ForgetFact(Dependency);
			continue;
		}
		if (IsCached(Dependency))
		{
			ClearPendingRequest(State);
			State.State = EVoxelPlanDependencyState::Ready;
			State.Error.Reset();
			WakeConsumers(Dependency);
			continue;
		}
		if (ArePrerequisitesReady(State))
		{
			const auto CapturedStorage = Storage;
			const FGuid CapturedNode = State.PriorityNode;
			Queue(Dependency, State);
			// Enqueue 可以触发外部回调，旧槽引用不能跨越它继续使用。
			if (Storage != CapturedStorage || !Storage || Storage->Owner != this)
			{
				break;
			}
			const auto* Current = Storage->States.Find(Dependency);
			if (Current && Current->PriorityNode == CapturedNode && Current->PendingRequest.IsSet())
			{
				LinkPendingAdmission(Storage->States.FindChecked(Dependency));
			}
		}
		else if (State.PendingRequest.IsSet())
		{
			LinkPendingAdmission(State);
		}
	}
	return Processed;
}

FVoxelTaskStamp FVoxelGenerationPlanCoordinator::MakeTaskStamp(
	const FVoxelGenerationPlanDependency& InDependency, const uint64 InAttempt) const
{
	FVoxelTaskStamp Stamp;
	Stamp.WorldEpoch = WorldEpoch;
	Stamp.Token = InAttempt;
	Stamp.OwnerId = EnvironmentOwnerId;
	Stamp.OwnerIncarnation = EnvironmentOwnerIncarnation;
	Stamp.Revision = static_cast<uint8>(InDependency.Kind);
	Stamp.Section = InDependency.Coordinate();
	return Stamp;
}

bool FVoxelGenerationPlanCoordinator::Queue(
	const FVoxelGenerationPlanDependency& InDependency,
	FState& InOutState)
{
	if (!InOutState.PendingRequest.IsSet())
	{
		InOutState.PendingRequest.Emplace();
		auto& NewRequest = InOutState.PendingRequest.GetValue();
		NewRequest.Kind = EVoxelTaskKind::BuildGenerationPlan;
		NewRequest.TerrainStage = 0;
		if (!InOutState.bAttemptPendingAdmission)
		{
			checkf(NextAttemptId != MAX_uint64, TEXT("Generation attempt serial exhausted"));
			InOutState.AttemptId = ++NextAttemptId;
			InOutState.bAttemptPendingAdmission = true;
		}
		NewRequest.Stamp = MakeTaskStamp(InDependency, InOutState.AttemptId);
		NewRequest.GenerationConsumer = {EVoxelGenerationConsumerKind::WorldFact, WorldEpoch,
			EnvironmentOwnerId, EnvironmentOwnerIncarnation, EVoxelGenerationSpatialKind::Section,
			NewRequest.Stamp.Section, NewRequest.Stamp.Section, NewRequest.Stamp.Token};
		// 等待绑定或队列容量不能重置原事实首次可运行年龄。
		NewRequest.FirstRunnableSeconds = FPlatformTime::Seconds();
		NewRequest.ReservedBytes = 0;
	}
	auto& Request = InOutState.PendingRequest.GetValue();
	Request.WorkClass = InOutState.WorkClass;
	Request.SourcePriority = InOutState.SourcePriority;
	Request.DistanceScore = InOutState.DistanceScore;
	Request.ForwardScore = InOutState.ForwardScore;
	Request.ServiceBand = InOutState.ServicePriority.Band;
	Request.SourceStableKey = InOutState.ServicePriority.SourceStableKey;
	// 正文由原 Cache 租约承担，任务只返回已经授额的读取闭包。
	const auto CaptureConsumer = Request.GenerationConsumer;
	if (InOutState.bWaitingTaskCapacity && Scheduler.GetTerrainResources()->ReleaseVersionExcludingOwner(CaptureConsumer) <=
		InOutState.TaskCapacityVersion)
	{
		return false;
	}
	InOutState.bWaitingTaskCapacity = false;
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
			if (InOutState.bWaitingCapacity)
			{
				LinkPendingAdmission(InOutState);
			}
			else
			{
				FailAdmission(InDependency, InOutState, Error,
					Error.StartsWith(TEXT("Oversized")) || Error.StartsWith(TEXT("CapacityUnachievable")) ?
					EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::TerminalFailure);
			}
			return false;
		}
		InOutState.bPrerequisiteRetentionPending = false;
	}
	if (!InOutState.PendingReadDependencies)
	{
		// 这些原 Input 授额在本次读取构建期间不可释放；不能把永久峰值不足记成外部容量等待。
		uint64 RequiredInputBytes = Cache->GetRootInputBytes();
		int32 RequiredLeaseCount = RequiredInputBytes ? 1 : 0;
		uint64 Serials[7] = {};
		int32 SerialCount = 0;
		const FVoxelTerrainResourceLease* RequiredLeases[] =
		{
			Storage->ResourceLease.Get(), Storage->StatesLease.Get(), Storage->ConsumersLease.Get(),
			Storage->FactsLease.Get(), Storage->EnvironmentLease.Get(),
			InOutState.PrerequisitesLease.Get(), InOutState.DonationsLease.Get()
		};
		for (const auto* Lease : RequiredLeases)
		{
			if (!Lease || !Lease->IsOwnedBy(Resources.Get()) || Lease->GetPool() != EVoxelTerrainResourcePool::Input)
			{
				continue;
			}
			const uint64 Serial = Lease->Serial();
			bool bKnown = false;
			for (int32 Index = 0; Index < SerialCount; ++Index)
			{
				bKnown |= Serials[Index] == Serial;
			}
			if (bKnown)
			{
				continue;
			}
			const uint64 Bytes = Lease->Bytes();
			if (Bytes > MAX_uint64 - RequiredInputBytes)
			{
				FailAdmission(InDependency, InOutState, TEXT("Oversized: generation read input floor overflow"),
					EVoxelTaskBlockReason::Oversized);
				return false;
			}
			Serials[SerialCount++] = Serial;
			RequiredInputBytes += Bytes;
			++RequiredLeaseCount;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ReadLease;
		const uint64 BeforeReadAdmission = Resources->ReleaseVersionExcludingOwner(CaptureConsumer);
		const auto Admission = Resources->TryAcquire(CaptureConsumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<FPlanReadDependencies>() +
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelGenerationPlanDependency, FDefaultAllocator>(MaximumHydrologyDependencies),
			ReadLease, 0, RequiredInputBytes, RequiredLeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
			{
				InOutState.bWaitingCapacity = true;
				InOutState.CapacityVersion = BeforeReadAdmission;
				LinkPendingAdmission(InOutState);
			}
			else
			{
				FailAdmission(InDependency, InOutState,
					Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
					TEXT("Oversized: generation plan read capture is unreachable") : TEXT("InvalidResource: generation plan read capture"),
					Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
					EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::TerminalFailure);
			}
			return false;
		}
		auto ReadDependencies = VoxelBoundedAllocation::MakeWorkerShared<FPlanReadDependencies>(ReadLease);
		ReadDependencies->Lease = MoveTemp(ReadLease);
		ReadDependencies->Missing.Reserve(MaximumHydrologyDependencies);
		check(ReadDependencies->GetAllocatedBytes() <= ReadDependencies->Lease->Bytes());
		InOutState.PendingReadDependencies = MoveTemp(ReadDependencies);
	}
	const auto ReadDependencies = StaticCastSharedPtr<FPlanReadDependencies>(InOutState.PendingReadDependencies);
	Request.InputBytes = ReadDependencies->Lease->Bytes();
	Request.bResumable = true;
	const uint64 BeforeTaskAdmission = Scheduler.GetTerrainResources()->ReleaseVersionExcludingOwner(CaptureConsumer);
	if (!Request.bBlockBound)
	{
		FString CallbackBindingError;
		if (!Request.BindGetBlockReason(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[ReadDependencies, CapturedResources = Resources, CaptureConsumer]
			{
				if (!ReadDependencies->bWaitingCapacity.Load()) return EVoxelTaskBlockReason::None;
				uint64 Version = 0;
				return CapturedResources->TryGetReleaseVersionExcludingOwner(CaptureConsumer, Version) &&
					Version > ReadDependencies->CapacityVersion.Load() ? EVoxelTaskBlockReason::None : EVoxelTaskBlockReason::Memory;
			}, CallbackBindingError))
		{
			RejectTaskBinding(InDependency, InOutState, CallbackBindingError, BeforeTaskAdmission);
			return false;
		}
	}
	if (!Request.bExecuteBound)
	{
		FString CallbackBindingError;
		if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[Dependency = InDependency, CapturedConfig = Config,
			CapturedCache = Cache, PrerequisiteRetention = InOutState.PrerequisiteRetention, ReadDependencies,
			CapturedResources = Resources, CapturedStamp = Request.Stamp, CaptureConsumer](const TAtomic<bool>& Cancel)
			{
				FVoxelTaskResult Result;
				if (Cancel.Load())
				{
					Result.bCanceled = true;
					return Result;
				}
				ReadDependencies->bWaitingCapacity.Store(false);
				ReadDependencies->Missing.Reset();
				ReadDependencies->bOverflow = false;
				const uint64 BeforeBuild = CapturedResources->ReleaseVersionExcludingOwner(CaptureConsumer);
				FVoxelGenerationQuery Query;
				if (!FVoxelGenerationQuery::Create(CapturedConfig, CapturedCache,
					Query, Result.Error, false, true, false, &CaptureConsumer, ReadDependencies->Lease.Get()))
				{
					Result.bCanceled = Cancel.Load();
					if (!Result.bCanceled && Result.Error.StartsWith(TEXT("CapacityBlocked")))
					{
						ReadDependencies->CapacityVersion.Store(BeforeBuild);
						ReadDependencies->bWaitingCapacity.Store(true);
						Result.bSuccess = true;
						Result.bContinue = true;
						Result.Error.Reset();
					}
					return Result;
				}
				// 原生查询保留构建权限；随后绑定原任务身份，不把它变成项目只读 preflight。
				auto ObserveMissing = [ReadDependencies](EVoxelGenerationPlanKind Kind, const FIntVector& Coordinate)
				{
					const auto Missing = MakeDependency(Kind, Coordinate);
					if (ReadDependencies->Missing.Contains(Missing)) return;
					if (ReadDependencies->Missing.Num() == MaximumHydrologyDependencies) ReadDependencies->bOverflow = true;
					else ReadDependencies->Missing.Add(Missing);
				};
				const FVoxelGenerationPlanningReadScope::FObserver Observer(ObserveMissing);
				FVoxelGenerationPlanningReadScope NativeReads(&Observer, &CapturedStamp, &Cancel, &CaptureConsumer);

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
				if (!Result.bCanceled && !ReadDependencies->bOverflow && Result.Error.StartsWith(TEXT("Yielded")))
				{
					Result.bSuccess = true;
					Result.bContinue = true;
					// 保留生产者的即时续跑证明，复用调度器原有 8ms 时间片，不把每个小片退回 GT 重排。
					Result.bContinueImmediately = Result.Error.StartsWith(TEXT("YieldedImmediate:"));
					Result.Error.Reset();
				}

				if (!Result.bCanceled && !ReadDependencies->bOverflow && Result.Error.StartsWith(TEXT("CapacityBlocked")))
				{
					ReadDependencies->CapacityVersion.Store(BeforeBuild);
					ReadDependencies->bWaitingCapacity.Store(true);
					Result.bSuccess = true;
					Result.bContinue = true;
					Result.Error.Reset();
				}
				return Result;
			}, CallbackBindingError))
		{
			RejectTaskBinding(InDependency, InOutState, CallbackBindingError, BeforeTaskAdmission);
			return false;
		}
	}
	if (!Request.bApplyBound)
	{
		FString CallbackBindingError;
		if (!Request.BindApply(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[this, WeakStorage = TWeakPtr<FStorage, ESPMode::ThreadSafe>(Storage), Dependency = InDependency](FVoxelTaskResult&& Result)
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
				if (Result.bCanceled)
				{
					const uint64 Count = ++Storage->CanceledResults;
					if (Count <= 8 || Count % 1024 == 0)
					{
						UE_LOG(LogTemp, Display, TEXT("Voxel plan canceled result: count=%llu kind=%d key=%s attempt=%llu cached=%d executeMs=%.3f subscriptions=%d parents=%d"),
							Count, int32(Dependency.Kind), *Dependency.Coordinate().ToString(), Result.Stamp.Token,
							IsCached(Dependency), Result.ExecuteMilliseconds, State->Donations.Num(), PriorityGraph.HasConsumers(State->PriorityNode));
					}
				}

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
					if (Current && Current->Donations.IsEmpty() && Current->State != EVoxelPlanDependencyState::Queued &&
						!PriorityGraph.HasConsumers(Current->PriorityNode))
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
			}, CallbackBindingError))
		{
			RejectTaskBinding(InDependency, InOutState, CallbackBindingError, BeforeTaskAdmission);
			return false;
		}
	}
	// Scheduler 的成功入队会 Pump 并通知外部；Request 与槽引用必须在调用前分离。
	const auto AdmissionStorage = Storage;
	const FGuid AdmissionNode = InOutState.PriorityNode;
	const FVoxelTaskStamp AdmissionStamp = Request.Stamp;
	FVoxelTaskRequest Submission = MoveTemp(Request);
	UnlinkPendingAdmission(InOutState);
	InOutState.PendingRequest.Reset();
	InOutState.PendingReadDependencies.Reset();
	InOutState.bAttemptPendingAdmission = false;
	InOutState.bWaitingTaskCapacity = false;
	InOutState.bWaitingCapacity = false;
	InOutState.State = EVoxelPlanDependencyState::Queued;
	InOutState.bInFlight = true;
	if (Scheduler.Enqueue(MoveTemp(Submission)))
	{
		return true;
	}
	if (Storage != AdmissionStorage || !Storage || Storage->Owner != this ||
		AdmissionStamp.OwnerIncarnation != EnvironmentOwnerIncarnation)
	{
		return false;
	}
	auto* Current = Storage->States.Find(InDependency);
	if (!Current || Current->PriorityNode != AdmissionNode || Current->AttemptId != AdmissionStamp.Token ||
		Current->PendingRequest.IsSet() || !Current->bInFlight || Current->State != EVoxelPlanDependencyState::Queued ||
		Submission.GenerationConsumer != CaptureConsumer)
	{
		return false;
	}
	Current->bInFlight = false;
	Current->State = EVoxelPlanDependencyState::Missing;
	Current->bAttemptPendingAdmission = true;
	Current->PendingReadDependencies = ReadDependencies;
	Current->PendingRequest.Emplace(MoveTemp(Submission));
	const auto Reason = Current->PendingRequest.GetValue().Blocked;
	{
		const uint16 Reasons = static_cast<uint16>(Reason);
		if (Reasons & (static_cast<uint16>(EVoxelTaskBlockReason::Oversized) | static_cast<uint16>(EVoxelTaskBlockReason::TerminalFailure)))
		{
			FailAdmission(InDependency, *Current, Reasons & static_cast<uint16>(EVoxelTaskBlockReason::Oversized) ?
				TEXT("Oversized: generation task admission is unreachable") : TEXT("InvalidRequest: generation task admission rejected"), Reason);
		}
		else
		{
			WaitForTaskAdmission(*Current, BeforeTaskAdmission, Reason);
		}
	}
	return false;
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
	if (!PrepareStorage(Error)) return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
	const auto CapturedStorage = Storage;
	FVoxelGenerationDependencyStatus Status;
	{
		// 先销毁依赖登记临时链，再统一启动任务，避免调度回调重入旧闭包。
		FVoxelTaskScheduler::FEnqueueBatch EnqueueBatch(Scheduler);
		FVoxelGenerationRequiredSourceScope RootSources(&Resources.Get(), this,
			[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
			{
				const auto& Owner = *static_cast<const FVoxelGenerationPlanCoordinator*>(Context);
				uint64 CacheBytes;
				int32 CacheLeaseCount;
				Owner.Cache->GetRequiredInputCapacity(CacheBytes, CacheLeaseCount);
				if (CacheLeaseCount) Visit({&Owner.Cache.Get(), 0, EVoxelTerrainResourcePool::Input, CacheBytes, CacheLeaseCount});
				for (const auto* Original : {Owner.Config->InputLease.Get(), Owner.Config->RecipeInputLease.Get(),
					Owner.Storage->ResourceLease.Get(), Owner.Storage->StatesLease.Get(), Owner.Storage->ConsumersLease.Get(),
					Owner.Storage->FactsLease.Get(), Owner.Storage->EnvironmentLease.Get()})
					if (Original && Original->IsOwnedBy(Owner.Resources.Get())) FVoxelGenerationRequiredSourceScope::VisitLease(Original, Visit);
			});
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
			FVoxelGenerationRequiredSourceScope PreviousDependenciesSource(&Resources.Get(), Consumer->DependenciesLease.Get());
			FVoxelGenerationRequiredCapacity RequiredScratch;
			if (!FVoxelGenerationRequiredSourceScope::Collect(Resources.Get(), nullptr, RequiredScratch, Error))
				return {false, true, MoveTemp(Error)};
			FVoxelGenerationWorkspace Scratch(Resources, ControlConsumer,
				RequiredScratch.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], RequiredScratch.LeaseCount);
			TSet<FGuid> DependencyNodes;
			TArray<FDonationGrowth> Growth;
			ON_SCOPE_EXIT
			{
				Scratch.FreeArray(Growth);
				Scratch.FreeSet(DependencyNodes);
			};
			if (!Scratch.ReserveSet(DependencyNodes, Desired.Count, Error) ||
				!Scratch.ReserveArray(Growth, GrowthCount, Error))
				return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
			// 新租约在本段只由Growth强持，提交前未向任何其它来源暴露，也不会调整池/字节。
			// 因此可精确累加实际Input字节和原租约数，避免第N次授额重放前N-1个来源。
			FVoxelGenerationRequiredCapacity GrowthCapacity;
			FVoxelGenerationRequiredSourceScope GrowthSources(&Resources.Get(), &GrowthCapacity,
				[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
				{
					const auto& Capacity = *static_cast<const FVoxelGenerationRequiredCapacity*>(Context);
					if (Capacity.LeaseCount) Visit({Context, 0, EVoxelTerrainResourcePool::Input,
						Capacity.Bytes[int32(EVoxelTerrainResourcePool::Input)], Capacity.LeaseCount});
				});
			const double DonationStarted = FPlatformTime::Seconds();
			for (FGuid Node = Desired.Head; Node.IsValid();)
			{
				const auto& State = Storage->States.FindChecked(Storage->PriorityFacts.FindChecked(Node));
				DependencyNodes.Add(Node);
				if (!State.Donations.Contains(InConsumer) && State.Donations.Num() + 1 > State.DonationsCapacity)
				{
					FDonationGrowth Plan;
					Plan.Node = Node;
					Plan.Capacity = FMath::Max(State.Donations.Num() + 1, FMath::Max(4, State.DonationsCapacity * 2));
					FVoxelGenerationRequiredSourceScope PreviousDonationSource(&Resources.Get(), State.DonationsLease.Get());
					FVoxelGenerationRequiredCapacity Required;
					if (!FVoxelGenerationRequiredSourceScope::Collect(Resources.Get(), nullptr, Required, Error))
						return {false, true, MoveTemp(Error)};
					const auto Admission = Resources->TryAcquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
						VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FVoxelGenerationConsumerKey, FDonation>>(State.Donations, Plan.Capacity),
						Plan.Lease, 0, Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
					if (Admission != EVoxelTerrainResourceAdmission::Granted)
					{
						return {false, Admission != EVoxelTerrainResourceAdmission::CapacityBlocked,
							Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: generation donations") :
							Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: generation donations and retained inputs") :
							TEXT("InvalidRequest: generation donations")};
					}
					EVoxelTerrainResourcePool GrantedPool;
					uint64 GrantedBytes = 0;
					uint64& PrefixBytes = GrowthCapacity.Bytes[int32(EVoxelTerrainResourcePool::Input)];
					if (!Plan.Lease->ReadCapacity(GrantedPool, GrantedBytes) || GrantedPool != EVoxelTerrainResourcePool::Input ||
						GrantedBytes > MAX_uint64 - PrefixBytes || GrowthCapacity.LeaseCount == MAX_int32)
						return {false, true, TEXT("CapacityUnachievable: invalid owned donation prefix")};
					PrefixBytes += GrantedBytes;
					++GrowthCapacity.LeaseCount;
					Growth.Add(MoveTemp(Plan));
				}
				Node = State.NextDesired;
			}
			if (Growth.Num() >= 1024)
				UE_LOG(LogTemp, Display, TEXT("Voxel donation preparation: dependencies=%d growth=%d inputBytes=%llu leases=%d ms=%.3f"),
					Desired.Count, Growth.Num(), GrowthCapacity.Bytes[int32(EVoxelTerrainResourcePool::Input)],
					GrowthCapacity.LeaseCount, (FPlatformTime::Seconds() - DonationStarted) * 1000.0);
			if (!PrepareMap<FVoxelGenerationPlanDependency, FSubscription>(Consumer->Dependencies, Desired.Count,
				Consumer->Capacity, Consumer->DependenciesLease, Error)) return {false, !Error.StartsWith(TEXT("CapacityBlocked")), MoveTemp(Error)};
			for (auto& Plan : Growth)
			{
				EVoxelTerrainResourcePool GrantedPool;
				uint64 GrantedBytes = 0;
				uint64& PrefixBytes = GrowthCapacity.Bytes[int32(EVoxelTerrainResourcePool::Input)];
				if (!Plan.Lease->ReadCapacity(GrantedPool, GrantedBytes) || GrantedPool != EVoxelTerrainResourcePool::Input ||
					GrantedBytes > PrefixBytes || GrowthCapacity.LeaseCount <= 0)
					return {false, true, TEXT("CapacityUnachievable: invalid donation ownership transfer")};
				auto& State = Storage->States.FindChecked(Storage->PriorityFacts.FindChecked(Plan.Node));
				State.Donations.Reserve(Plan.Capacity);
				check(VoxelGenerationAllocation::AllocatedBytes(State.Donations) <= Plan.Lease->Bytes());
				PrefixBytes -= GrantedBytes;
				--GrowthCapacity.LeaseCount;
				State.DonationsLease = MoveTemp(Plan.Lease);
				State.DonationsCapacity = Plan.Capacity;
			}
			check(GrowthCapacity.LeaseCount == 0 && GrowthCapacity.Bytes[int32(EVoxelTerrainResourcePool::Input)] == 0);
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
				if (State.PendingRequest.IsSet())
				{
					ClearPendingRequest(State);
					WakeConsumers(Dependency);
				}
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
			const FGuid Next = State.NextDesired;
			if (State.State == EVoxelPlanDependencyState::Missing && !State.bInFlight && ArePrerequisitesReady(State)) Queue(Dependency, State);
			if (Storage != Desired.CapturedStorage || !Storage || Storage->Owner != this || !Storage->Consumers.Contains(InConsumer))
			{
				Status.bReady = false;
				return Status;
			}
			const auto* Current = Storage->States.Find(Dependency);
			if (Current && Current->State == EVoxelPlanDependencyState::Failed)
			{
				Status.bReady = false;
				Status.bFailed = true;
				Status.Error = Current->Error;
			}
			Node = Next;
		}
		RefreshQueuedPriorities();
	}
	if (Storage != CapturedStorage || !Storage || Storage->Owner != this || !Storage->Consumers.Contains(InConsumer))
	{
		Status.bReady = false;
	}
	return Status;
}

void FVoxelGenerationPlanCoordinator::Reset()
{
	check(IsInGameThread());
	check(EnvironmentOwnerIncarnation != MAX_uint64 && ControlConsumer.OwnerIncarnation != MAX_uint64);
	const uint64 PreviousIncarnation = EnvironmentOwnerIncarnation++;
	if (Storage)
	{
		// 先解除本地未入队捕获，再撤销 Scheduler 中的旧代。
		Storage->Owner = nullptr;
		for (auto& Pair : Storage->States)
		{
			ClearPendingRequest(Pair.Value);
			if (Pair.Key.Kind == EVoxelGenerationPlanKind::Hydrology && Pair.Value.AttemptId)
			{
				Cache->CancelHydrologyBuild(Pair.Key.HydrologyKey, WorldEpoch, EnvironmentOwnerId,
					PreviousIncarnation, Pair.Value.AttemptId);
			}

		}
	}
	// 在任何 Scheduler 通知之前撤掉旧存储，重入的新代不能被本次 Reset 再删除。
	auto RetiringStorage = MoveTemp(Storage);
	++ControlConsumer.OwnerIncarnation;
	PriorityGraph.Reset();
	Scheduler.CancelMatching([Epoch = WorldEpoch, Owner = EnvironmentOwnerId, PreviousIncarnation](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildGenerationPlan && Stamp.WorldEpoch == Epoch &&
			Stamp.OwnerId == Owner && Stamp.OwnerIncarnation == PreviousIncarnation;
	});
}

void FVoxelGenerationPlanCoordinator::ReleaseConsumer(const FVoxelGenerationConsumerKey& InConsumer)
{
	check(IsInGameThread());
	if (!Storage) return;
	const auto CapturedStorage = Storage;
	if (auto* Consumer = Storage->Consumers.Find(InConsumer))
	{
		const FGuid Node = Consumer->PriorityNode;
		for (auto& Edge : Consumer->Dependencies)
		{
			UnlinkSubscriptionWake(*Consumer, Edge.Value);
		}
		UnlinkConsumerWake(*Consumer);
		// 先脱离可重入查找表，再逐个撤销；旧依赖正文与授额在局部一起持有。
		FConsumerState Releasing = MoveTemp(*Consumer);
		Storage->Consumers.Remove(InConsumer);
		PriorityGraph.Remove(Node);
		for (const auto& Edge : Releasing.Dependencies)
		{
			RemoveSubscription(Edge.Key, InConsumer, Edge.Value.Id);
			if (Storage != CapturedStorage || !Storage || Storage->Owner != this)
			{
				return;
			}
		}
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
	if (!State->Donations.IsEmpty() || PriorityGraph.HasConsumers(State->PriorityNode))
	{
		return;
	}
	if (State->bInFlight)
	{
		const uint64 Attempt = State->AttemptId;
		const FVoxelTaskStamp Stamp = MakeTaskStamp(InDependency, Attempt);
		const uint64 Count = ++Storage->SubscriptionCancellations;
		if (Count <= 8 || Count % 1024 == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("Voxel plan subscription cancellation: count=%llu kind=%d key=%s attempt=%llu consumerKind=%d cached=%d prerequisites=%d"),
				Count, int32(InDependency.Kind), *InDependency.Coordinate().ToString(), Attempt, int32(InConsumer.Kind),
				IsCached(InDependency), State->Prerequisites.Num());
		}
		if (InDependency.Kind == EVoxelGenerationPlanKind::Hydrology)
		{
			Cache->CancelHydrologyBuild(InDependency.HydrologyKey, WorldEpoch, EnvironmentOwnerId,
				EnvironmentOwnerIncarnation, Attempt);
		}

		Scheduler.CancelTask(EVoxelTaskKind::BuildGenerationPlan, Stamp);
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
	const int32 Maximum = FMath::Max(0, InMaximumEdges);
	const int32 AdmissionBudget = Storage && Storage->WakeHead.IsValid() ? Maximum / 2 : Maximum;
	const int32 AdmissionSteps = AdvancePendingAdmissions(InDeadline, AdmissionBudget);
	int32 Processed = 0;
	while (Storage && Storage->WakeHead.IsValid() && Processed < Maximum - AdmissionSteps && FPlatformTime::Seconds() <= InDeadline)
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
	, CapturedStorage(InOwner.Storage)
{
}

FVoxelGenerationPlanCoordinator::FClosureRegistration::~FClosureRegistration()
{
	if (Owner.Storage != CapturedStorage)
	{
		return;
	}
	while (Head.IsValid() && Owner.Storage == CapturedStorage)
	{
		const auto Dependency = Owner.Storage->PriorityFacts.FindChecked(Head);
		auto& State = Owner.Storage->States.FindChecked(Dependency);
		Head = State.NextRegistration;
		State.NextRegistration.Invalidate();
		State.bRegistrationPending = false;
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
	InState.bRegistrationPending = true;
	Head = InState.PriorityNode;
}

void FVoxelGenerationPlanCoordinator::FClosureRegistration::Commit()
{
	bCommitted = true;
	while (Head.IsValid())
	{
		auto& State = CapturedStorage->States.FindChecked(CapturedStorage->PriorityFacts.FindChecked(Head));
		Head = State.NextRegistration;
		State.NextRegistration.Invalidate();
		State.bRegistrationPending = false;
	}
}

FVoxelGenerationPlanCoordinator::FClosureDemand::FClosureDemand(FVoxelGenerationPlanCoordinator& InOwner)
	: Owner(InOwner)
	, CapturedStorage(InOwner.Storage)
{
}

FVoxelGenerationPlanCoordinator::FClosureDemand::~FClosureDemand()
{
	if (Owner.Storage != CapturedStorage)
	{
		return;
	}
	while (Head.IsValid() && Owner.Storage == CapturedStorage)
	{
		const auto Dependency = CapturedStorage->PriorityFacts.FindChecked(Head);
		auto& State = CapturedStorage->States.FindChecked(Dependency);
		Head = State.NextDesired;
		State.NextDesired.Invalidate();
		State.bDesired = false;
		if (State.Donations.IsEmpty() && !State.bInFlight && !Owner.PriorityGraph.HasConsumers(State.PriorityNode))
		{
			Owner.ForgetFact(Dependency);
		}
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
		// 同步 Enqueue 通知可释放消费者；原 Ensure 闭包结束后再移除其正在遍历的槽。
		if (State->bDesired || State->bRegistrationPending)
		{
			return;
		}
		const auto Node = State->PriorityNode;
		const uint64 Attempt = State->AttemptId;
		const FVoxelTaskStamp Stamp = MakeTaskStamp(InDependency, Attempt);
		if (State->bInFlight)
		{
			const uint64 Count = ++Storage->ForgottenInFlight;
			if (Count <= 8 || Count % 1024 == 0)
			{
				UE_LOG(LogTemp, Display, TEXT("Voxel plan forgotten in-flight: count=%llu kind=%d key=%s attempt=%llu cached=%d subscriptions=%d parents=%d"),
					Count, int32(InDependency.Kind), *InDependency.Coordinate().ToString(), Attempt, IsCached(InDependency),
					State->Donations.Num(), PriorityGraph.HasConsumers(Node));
			}
		}
		if (InDependency.Kind == EVoxelGenerationPlanKind::Hydrology && Attempt)
		{
			Cache->CancelHydrologyBuild(InDependency.HydrologyKey, WorldEpoch, EnvironmentOwnerId,
				EnvironmentOwnerIncarnation, Attempt);
		}

		const auto CapturedStorage = Storage;
		ClearPendingRequest(*State);
		auto PrerequisitesLease = MoveTemp(State->PrerequisitesLease);
		auto Prerequisites = MoveTemp(State->Prerequisites);
		PriorityGraph.Remove(Node);
		Storage->PriorityFacts.Remove(Node);
		Storage->States.Remove(InDependency);
		if (Attempt)
		{
			Scheduler.CancelTask(EVoxelTaskKind::BuildGenerationPlan, Stamp);
		}
		if (Storage != CapturedStorage || !Storage || Storage->Owner != this)
		{
			return;
		}
		if (bReleasePrerequisites)
		{
			for (const auto& Prerequisite : Prerequisites)
			{
				if (Storage != CapturedStorage || !Storage || Storage->Owner != this)
				{
					break;
				}
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
	const auto CapturedStorage = Storage;
	const FIntVector Cell(InCell.X, InCell.Y, 0);
	const FVoxelGenerationConsumerKey Space{EVoxelGenerationConsumerKind::Environment, WorldEpoch, EnvironmentOwnerId, EnvironmentOwnerIncarnation,
		EVoxelGenerationSpatialKind::Column, Cell, Cell, 1};
	if (const auto* Current = Storage->EnvironmentAttempts.Find(Space))
	{
		const auto Consumer = *Current;
		ReleaseConsumer(Consumer);
		if (Storage == CapturedStorage && Storage && Storage->Owner == this)
		{
			Storage->EnvironmentAttempts.Remove(Space);
		}
	}
}
