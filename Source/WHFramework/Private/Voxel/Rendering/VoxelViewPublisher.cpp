#include "Voxel/Rendering/VoxelViewPublisher.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Rendering/VoxelMeshClipper.h"
#include "Voxel/VoxelModule.h"

namespace
{
	struct FCoveragePreparationInput
	{
		FVector Origin;
		double Step = 1.0;
		FBox Bounds;
		TArray<FBox> Boxes;
		TArray<FBox> Previous;
		TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> Common;
		bool bDirty = false;
	};

	struct FCoveragePreparationResult final : FVoxelTaskCustomPayload
	{
		TArray<TArray<FBox>> Boxes;
		TArray<TArray<FBox>> ChangedBounds;
		TArray<uint8> Unchanged;
		virtual uint64 GetAllocatedBytes() const override
		{
			uint64 Bytes = Boxes.GetAllocatedSize() + ChangedBounds.GetAllocatedSize() + Unchanged.GetAllocatedSize();
			for (const auto& Entry : Boxes) Bytes += Entry.GetAllocatedSize();
			for (const auto& Entry : ChangedBounds) Bytes += Entry.GetAllocatedSize();
			return Bytes;
		}
	};
}

FVoxelViewPublisher::FVoxelViewPublisher(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler, const uint64 InWorldEpoch)
	: Module(InModule), Scheduler(InScheduler), WorldEpoch(InWorldEpoch)
{
}

FVoxelViewPublisher::~FVoxelViewPublisher()
{
	Reset();
}

FGuid FVoxelViewPublisher::BeginGroup(const FVoxelPublishGroupKey& InKey)
{
	if (const FGuid* Existing = PendingGroupIds.Find(InKey))
	{
		// 尚未开始发布的分组可合并最新输入；关联的新 owner 尚未提交时不能把组锁死。
		PendingGroups.FindChecked(*Existing).bCommitted = false;
		return *Existing;
	}
	const FGuid Group = FGuid::NewGuid();
	PendingGroups.Add(Group, {InKey, false, CurrentGroupCallback});
	PendingGroupIds.Add(InKey, Group);
	return Group;
}

bool FVoxelViewPublisher::SetTerrainStage(const FGuid& InGroup, const int32 InTerrainStage)
{
	FPendingGroup* Group = PendingGroups.Find(InGroup);
	if (!Group) return false;
	Group->TerrainStage = FMath::Min(Group->TerrainStage, FMath::Clamp(InTerrainStage, 0, 3));
	return true;
}

bool FVoxelViewPublisher::Stage(
	const FGuid& InGroup,
	AActor*& InOutActor,
	const FVector& InLocation,
	const double InScale,
	FVoxelSectionMeshResult&& InMesh,
	const int32 InTerrainStage,
	const bool bInTransition)
{
	const FPendingGroup* Group = PendingGroups.Find(InGroup);
	if (!Group || Group->bCommitted || !Module.GetWorld() || !Module.GetMaterialSet() ||
		!FMath::IsFinite(InScale) || InScale <= 0.0)
	{
		return false;
	}
	for (const FVoxelRenderBatch& Batch : InMesh.Batches)
	{
		if (!Module.GetMaterialSet()->FindMaterial(Batch.Group, Batch.Bank, Batch.MaterialVariant) || !Batch.Mesh.Validate()) return false;
	}
	if (!InOutActor)
	{
		FActorSpawnParameters Parameters;
		Parameters.ObjectFlags |= RF_Transient;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		InOutActor = Module.GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
			FTransform(InLocation), Parameters);
		if (!InOutActor) return false;
		InOutActor->SetReplicates(false);
		InOutActor->SetActorEnableCollision(false);
		USceneComponent* Root = NewObject<USceneComponent>(InOutActor);
		InOutActor->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	FEntry& Entry = Entries.FindOrAdd(TWeakObjectPtr<AActor>(InOutActor));
	Entry.TerrainStage = InTerrainStage;
	Entry.GroupKey = Group->Key;
	Entry.bTransition = bInTransition;
	Entry.Location = InLocation;
	Entry.Scale = InScale;
	Entry.Bounds = FBox(ForceInit);
	for (const FVoxelRenderBatch& Batch : InMesh.Batches)
	{
		for (const FVector& Vertex : Batch.Mesh.Vertices) Entry.Bounds += Vertex;
	}
	Entry.Source = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>(MoveTemp(InMesh));
	Entry.bDirty = true;
	return true;
}

bool FVoxelViewPublisher::SetCoverage(const FGuid& InGroup, AActor* InActor, TArray<FBox> InWorldCellBoxes,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes)
{
	const FPendingGroup* Group = PendingGroups.Find(InGroup);
	const TWeakObjectPtr<AActor> ActorKey(InActor);
	const FEntry* Entry = Entries.Find(ActorKey);
	if (!Group || Group->bCommitted || !Entry || Entry->GroupKey != Group->Key)
	{
		return false;
	}
	// 已提交 Fine 的空裁剪无须每帧再排一次 worker，也不能覆盖更新的待提交输入。
	if (!Entry->bDirty && InWorldCellBoxes.IsEmpty() &&
		(!InCommonBoxes || InCommonBoxes->IsEmpty()) && Entry->Exclusions.IsEmpty() &&
		!PendingUpdateIndices.Contains(ActorKey))
		return true;
	const int32* Existing = PendingUpdateIndices.Find(ActorKey);
	const int32 Index = Existing ? *Existing : PendingUpdates.AddDefaulted();
	if (!Existing) PendingUpdateIndices.Add(ActorKey, Index);
	FUpdate& Update = PendingUpdates[Index];
	Update = FUpdate();
	Update.Actor = InActor;
	Update.Source = Entry->Source;
	Update.TerrainStage = Entry->TerrainStage;
	Update.Location = Entry->Location;
	Update.Scale = Entry->Scale;
	Update.Exclusions = MoveTemp(InWorldCellBoxes);
	Update.LocalWorldBoxes = Update.Exclusions;
	Update.CommonBoxes = MoveTemp(InCommonBoxes);
	Update.Bounds = Entry->Bounds;
	Update.PreviousExclusions = Entry->Exclusions;
	Update.bSourceDirty = Entry->bDirty;
	return true;
}

bool FVoxelViewPublisher::SetSharedCoverage(const FGuid& InGroup, AActor* InActor,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes)
{
	const TWeakObjectPtr<AActor> ActorKey(InActor);
	const FEntry* Entry = Entries.Find(ActorKey);
	if (!Entry || !Entry->bPresented || !Entry->PresentedSource) return false;
	// 远景拓扑等待期间保留已提交的层间裁剪，仅更新与 Fine 共用的覆盖。
	if (!SetCoverage(InGroup, InActor, Entry->LocalWorldBoxes, MoveTemp(InCommonBoxes))) return false;
	if (const int32* Index = PendingUpdateIndices.Find(ActorKey))
	{
		FUpdate* Update = &PendingUpdates[*Index];
		Update->Source = Entry->PresentedSource;
		Update->Bounds = Entry->PresentedWorldBounds.IsValid
			? FBox((Entry->PresentedWorldBounds.Min - Entry->Location) / Entry->Scale,
				(Entry->PresentedWorldBounds.Max - Entry->Location) / Entry->Scale)
			: FBox(ForceInit);
		Update->bSourceDirty = false;
		Update->bRetainSource = true;
	}
	return true;
}

bool FVoxelViewPublisher::SetHidden(const FGuid& InGroup, AActor* InActor, const bool bInHidden)
{
	const FPendingGroup* Group = PendingGroups.Find(InGroup);
	const FEntry* Entry = Entries.Find(TWeakObjectPtr<AActor>(InActor));
	if (!Group || Group->bCommitted || !Entry || Entry->GroupKey != Group->Key)
	{
		return false;
	}

	const TWeakObjectPtr<AActor> Key(InActor);
	if (const bool* Pending = PendingVisibility.Find(Key))
	{
		if (*Pending == bInHidden)
		{
			return true;
		}
	}
	else if (InActor->IsHidden() == bInHidden && !Publications.ContainsByPredicate([&Key](const FPublicationRef& Publication)
	{
		return !Publication->bCanceled && !Publication->bFinished && Publication->Visibility.Contains(Key);
	}))
	{
		return true;
	}

	PendingVisibility.Add(Key, bInHidden);
	return true;
}

uint64 FVoxelViewCoverageResult::GetAllocatedBytes() const
{
	return Meshes.GetAllocatedSize() + OwnedMeshBytes;
}

void FVoxelViewPublisher::SetCommitCallbacks(TFunction<void()> InOnCommitted,
	TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)> InOnGroupCommitted)
{
	OnCommitted = MoveTemp(InOnCommitted);
	CurrentGroupCallback = MakeShared<FGroupCallback>(MoveTemp(InOnGroupCommitted));
	for (auto& Pair : PendingGroups)
		if (!Pair.Value.bCommitted) Pair.Value.Callback = CurrentGroupCallback;
	bRetry = false;
}

bool FVoxelViewPublisher::CommitGroup(const FGuid& InGroup)
{
	FPendingGroup* Group = PendingGroups.Find(InGroup);
	if (!Group || Group->bCommitted)
	{
		return false;
	}
	Group->bCommitted = true;
	return true;
}

bool FVoxelViewPublisher::SetOwnershipChanged(const FGuid& InGroup, const bool bInChanged)
{
	FPendingGroup* Group = PendingGroups.Find(InGroup);
	if (!Group || Group->bCommitted) return false;
	Group->bOwnershipChanged = bInChanged;
	return true;
}

void FVoxelViewPublisher::SetObservers(const TConstArrayView<FVector> InObservers)
{
	Observers = TArray<FVector>(InObservers);
}

double FVoxelViewPublisher::BoundsDistance(const TConstArrayView<FBox> InBounds) const
{
	double Distance = TNumericLimits<double>::Max();
	for (const FBox& Bounds : InBounds)
	{
		if (!Bounds.IsValid) continue;
		for (const FVector& Observer : Observers)
		{
			const double X = Observer.X - FMath::Clamp(Observer.X, Bounds.Min.X, Bounds.Max.X);
			const double Y = Observer.Y - FMath::Clamp(Observer.Y, Bounds.Min.Y, Bounds.Max.Y);
			Distance = FMath::Min(Distance, X * X + Y * Y);
		}
	}
	return Observers.IsEmpty() ? 0.0 : Distance;
}

double FVoxelViewPublisher::GroupDistance(const FGroup& InGroup) const
{
	if (!InGroup.ChangedBounds.IsEmpty())
	{
		return BoundsDistance(InGroup.ChangedBounds);
	}
	else if (InGroup.Bounds.IsValid)
	{
		return BoundsDistance(MakeArrayView(&InGroup.Bounds, 1));
	}
	double Distance = MAX_dbl;
	for (const auto& Key : InGroup.Keys)
	{
		const FBox Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Key, Module.BlockSize());
		Distance = FMath::Min(Distance, BoundsDistance(MakeArrayView(&Bounds, 1)));
	}
	return Observers.IsEmpty() ? 0.0 : Distance;
}

int32 FVoxelViewPublisher::GroupStage(const FGroup& InGroup)
{
	int32 Stage = InGroup.TerrainStage;
	for (const auto& Key : InGroup.Keys)
		Stage = FMath::Min(Stage, Key.Representation < 4 ? FMath::Min<int32>(Key.Representation, 3)
			: Key.Representation == 4 ? 2 : 3);
	return Stage;
}

bool FVoxelViewPublisher::CanPublishGroup(const FGroup& InGroup) const
{
	return Scheduler.CanBuildTerrainStage(FMath::Min(GroupStage(InGroup), InGroup.Priority.TerrainStage));
}

void FVoxelViewPublisher::MergePriority(FGroupPriority& InOutPriority, const FGroupPriority& InPriority)
{
	InOutPriority.ObserverDistance = FMath::Min(InOutPriority.ObserverDistance, InPriority.ObserverDistance);
	InOutPriority.DistanceScore = FMath::Min(InOutPriority.DistanceScore, InPriority.DistanceScore);
	InOutPriority.SourcePriority = FMath::Min(InOutPriority.SourcePriority, InPriority.SourcePriority);
	InOutPriority.TerrainStage = FMath::Min(InOutPriority.TerrainStage, InPriority.TerrainStage);
	InOutPriority.WorkClass = FMath::Min(InOutPriority.WorkClass, InPriority.WorkClass);
}

bool FVoxelViewPublisher::HigherGroupPriority(const FGroupPriority& InFirst, const FGroupPriority& InSecond)
{
	if (InFirst.SourcePriority != InSecond.SourcePriority) return InFirst.SourcePriority < InSecond.SourcePriority;
	if (InFirst.WorkClass != InSecond.WorkClass) return InFirst.WorkClass < InSecond.WorkClass;
	return InFirst.ObserverDistance < InSecond.ObserverDistance;
}

FVoxelViewPublisher::FGroupPriority FVoxelViewPublisher::BoundsPriority(const TConstArrayView<FBox> InBounds,
	const int32 InStage) const
{
	FGroupPriority Result;
	Result.ObserverDistance = BoundsDistance(InBounds);
	Result.TerrainStage = InStage;
	for (const FBox& WorldBounds : InBounds)
	{
		if (!WorldBounds.IsValid) continue;
		const FBox Bounds(WorldBounds.Min / Module.BlockSize(), WorldBounds.Max / Module.BlockSize());
		int32 Priority = MAX_int32;
		int32 NearestPriority = MAX_int32;
		double NearestDistance = MAX_dbl;
		for (const auto& Pair : Module.GetCurrentInterest().Sources)
		{
			const auto& Source = Pair.Value.Source;
			if (!Source.Has(EVoxelStreamingCapability::FineVisual) ||
				(InStage > 0 && !Source.Has(EVoxelStreamingCapability::WorldVisual))) continue;
			const double X = Source.Center.X - FMath::Clamp<double>(Source.Center.X, Bounds.Min.X, Bounds.Max.X);
			const double Y = Source.Center.Y - FMath::Clamp<double>(Source.Center.Y, Bounds.Min.Y, Bounds.Max.Y);
			const double Distance = FMath::Sqrt(X * X + Y * Y);
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				NearestPriority = Source.GetSchedulingPriority();
			}
			const int32 Radius = InStage == 0 ? Source.View.FineRadiusCells + Source.View.FinePreloadCells
				: InStage == 1 ? Source.View.VoxelProxyRadiusCells
				: InStage == 2 ? Source.View.SurfaceRadiusCells : Module.GetViewSettings().MacroRadiusCells;
			if (Distance <= Radius) Priority = FMath::Min(Priority, Source.GetSchedulingPriority());
			Result.DistanceScore = FMath::Min(Result.DistanceScore, Distance);
			if (!Source.Has(EVoxelStreamingCapability::LocalRefinement) &&
				Source.View.MovementCriticalFineRadiusCells > 0 &&
				Distance <= Source.View.MovementCriticalFineRadiusCells)
				Result.WorkClass = EVoxelWorkClass::Critical;
		}
		Result.SourcePriority = FMath::Min(Result.SourcePriority,
			Priority != MAX_int32 ? Priority : NearestPriority);
	}
	return Result;
}

FVoxelViewPublisher::FGroupPriority FVoxelViewPublisher::GroupPriority(const FGroup& InGroup) const
{
	const int32 Stage = GroupStage(InGroup);
	if (!InGroup.ChangedBounds.IsEmpty()) return BoundsPriority(InGroup.ChangedBounds, Stage);
	if (InGroup.Bounds.IsValid) return BoundsPriority(MakeArrayView(&InGroup.Bounds, 1), Stage);
	FGroupPriority Result;
	for (const auto& Key : InGroup.Keys)
	{
		const FBox Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Key, Module.BlockSize());
		MergePriority(Result, BoundsPriority(MakeArrayView(&Bounds, 1), Stage));
	}
	return Result;
}

void FVoxelViewPublisher::SetPublicationPriority(FVoxelTaskRequest& InOutRequest,
	const FPublication& InPublication, const TConstArrayView<int32> InGroupIndices) const
{
	FGroupPriority Priority;
	for (const int32 Index : InGroupIndices)
	{
		const FGroup& Group = InPublication.Groups[Index];
		MergePriority(Priority, GroupPriority(Group));
		MergePriority(Priority, Group.Priority);
	}
	InOutRequest.DistanceScore = Priority.DistanceScore;
	InOutRequest.SourcePriority = Priority.SourcePriority;
	InOutRequest.TerrainStage = Priority.TerrainStage;
	InOutRequest.WorkClass = InOutRequest.WorkClass == EVoxelWorkClass::None ? Priority.WorkClass
		: FMath::Min(InOutRequest.WorkClass, Priority.WorkClass);
}

void FVoxelViewPublisher::SortGroups(const FPublicationRef& Publication)
{
	TArray<int32> Order;
	TArray<double> Distances;
	for (int32 Index = 0; Index < Publication->Groups.Num(); ++Index)
	{
		Order.Add(Index);
		Distances.Add(GroupDistance(Publication->Groups[Index]));
	}
	Order.StableSort([&Distances](const int32 A, const int32 B) { return Distances[A] < Distances[B]; });
	TArray<FGroup> Sorted;
	TArray<int32> Remap;
	Remap.SetNumUninitialized(Publication->Groups.Num());
	for (const int32 Index : Order)
	{
		Remap[Index] = Sorted.Num();
		Sorted.Add(MoveTemp(Publication->Groups[Index]));
	}
	Publication->Groups = MoveTemp(Sorted);
	for (FUpdate& Update : Publication->Updates) Update.GroupIndex = Remap[Update.GroupIndex];
	Publication->Updates.StableSort([](const FUpdate& A, const FUpdate& B)
	{
		return A.GroupIndex == B.GroupIndex ? A.TerrainStage < B.TerrainStage : A.GroupIndex < B.GroupIndex;
	});
	for (FGroup& Group : Publication->Groups) Group.UpdateIndices.Reset();
	for (int32 Index = 0; Index < Publication->Updates.Num(); ++Index) Publication->Groups[Publication->Updates[Index].GroupIndex].UpdateIndices.Add(Index);
	Publication->GroupOrder.Reset();
	RefreshGroupOrder(Publication);
}

void FVoxelViewPublisher::RefreshGroupOrder(const FPublicationRef& Publication)
{
	if (Publication->GroupOrder.IsEmpty())
	{
		for (int32 Index = 0; Index < Publication->Groups.Num(); ++Index)
			if (!Publication->Groups[Index].bCommitted) Publication->GroupOrder.Add(Index);
	}
	else
	{
		Publication->GroupOrder.RemoveAll([&Publication](const int32 Index)
		{
			return Publication->Groups[Index].bCommitted;
		});
	}
	TMap<int32, double> Distances;
	for (const int32 Index : Publication->GroupOrder) Distances.Add(Index, GroupDistance(Publication->Groups[Index]));
	Publication->GroupOrder.StableSort([&Distances](const int32 First, const int32 Second)
	{
		return Distances.FindChecked(First) < Distances.FindChecked(Second);
	});
}

FVoxelViewPublisher::FUpdate FVoxelViewPublisher::TakePendingUpdate(const int32 InIndex)
{
	FUpdate Update = MoveTemp(PendingUpdates[InIndex]);
	PendingUpdateIndices.Remove(Update.Actor);
	PendingUpdates.RemoveAtSwap(InIndex, 1, EAllowShrinking::No);
	if (PendingUpdates.IsValidIndex(InIndex)) PendingUpdateIndices.FindChecked(PendingUpdates[InIndex].Actor) = InIndex;
	return Update;
}

void FVoxelViewPublisher::StartCommittedGroups()
{
	// 每个 owner 最多两代在途；远处旧组不占据独立新组的全局位置。
	// 同时只比较一份新快照，覆盖比较后再按真实连接组检查 owner 容量。
	if (PendingGroups.IsEmpty() || Publications.ContainsByPredicate([this](const FPublicationRef& Active)
	{
		return !Active->bCanceled && !Active->bFinished && !Active->bGroupsPrepared &&
			Active->Groups.ContainsByPredicate([this](const FGroup& Group) { return CanPublishGroup(Group); });
	})) return;
	// Include uncommitted owners so a parent/child or seam handoff remains atomic.
	// Independent committed components can start without the rest of the world.
	TArray<FVoxelPublishFootprint> Footprints;
	TMap<FVoxelPublishGroupKey, int32> KeyIndices;
	for (const auto& Pair : PendingGroups)
	{
		FVoxelPublishFootprint& Footprint = Footprints.AddDefaulted_GetRef();
		Footprint.Key = Pair.Value.Key;
		Footprint.bTransition = Footprint.Key.Representation == 4 || Footprint.Key.Representation == 5;
		if (!Footprint.bTransition)
		{
			Footprint.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Footprint.Key, Module.BlockSize());
		}
		KeyIndices.Add(Footprint.Key, Footprints.Num() - 1);
	}
	for (const auto& Pair : Entries)
	{
		const FEntry& Entry = Pair.Value;
		const int32* Index = KeyIndices.Find(Entry.GroupKey);
		if (!Index) continue;
		const FBox Desired = Entry.Bounds.IsValid
			? FBox(Entry.Location + Entry.Bounds.Min * Entry.Scale,
				Entry.Location + Entry.Bounds.Max * Entry.Scale)
			: FBox(ForceInit);
		Footprints[*Index].Bounds += FVoxelPublishGroupPlanner::PublicationBounds(
			Entry.GroupKey, Module.BlockSize(), Entry.PresentedWorldBounds, Desired, Entry.bTransition);
	}
	TArray<int32> GroupIndices;
	FVoxelPublishGroupPlanner::Build(Footprints, GroupIndices);
	TSet<int32> BlockedComponents;
	for (const auto& Pair : PendingGroups)
	{
		if (!Pair.Value.bCommitted)
		{
			BlockedComponents.Add(GroupIndices[KeyIndices.FindChecked(Pair.Value.Key)]);
		}
	}
	const FPublicationRef Publication = MakeShared<FPublication>();
	TSet<FVoxelPublishGroupKey> ReadyKeys;
	for (auto It = PendingGroups.CreateIterator(); It; ++It)
	{
		const FVoxelPublishGroupKey Key = It.Value().Key;
		if (BlockedComponents.Contains(GroupIndices[KeyIndices.FindChecked(Key)])) continue;
		ReadyKeys.Add(Key);
		Publication->Callbacks.Add(Key, It.Value().Callback);
		Publication->TerrainStages.Add(Key, It.Value().TerrainStage);
		if (It.Value().DeferredPriorityBounds) Publication->DeferredPriorityBounds.Add(Key, It.Value().DeferredPriorityBounds);
		if (It.Value().bOwnershipChanged) Publication->OwnershipChanges.Add(Key);
		PendingGroupIds.Remove(Key);
		It.RemoveCurrent();
	}
	if (ReadyKeys.IsEmpty()) return;
	for (int32 Index = PendingUpdates.Num() - 1; Index >= 0; --Index)
	{
		const FEntry* Entry = Entries.Find(PendingUpdates[Index].Actor);
		if (Entry && !ReadyKeys.Contains(Entry->GroupKey)) continue;
		Publication->Updates.Add(TakePendingUpdate(Index));
	}
	for (auto It = PendingVisibility.CreateIterator(); It; ++It)
	{
		const FEntry* Entry = Entries.Find(It.Key());
		if (Entry && !ReadyKeys.Contains(Entry->GroupKey)) continue;
		Publication->Visibility.Add(It.Key(), It.Value());
		It.RemoveCurrent();
	}
	Publication->Updates.StableSort([](const FUpdate& A, const FUpdate& B)
	{
		return A.TerrainStage < B.TerrainStage;
	});
	BuildGroups(Publication);
	for (const FVoxelPublishGroupKey& Key : ReadyKeys)
	{
		if (!Publication->Groups.ContainsByPredicate([&Key](const FGroup& Group) { return Group.Keys.Contains(Key); }))
		{
			Publication->Groups.AddDefaulted_GetRef().Keys.Add(Key);
		}
	}
	SortGroups(Publication);
#if !UE_BUILD_SHIPPING
	static double NextGroupLogSeconds = 0.0;
	const double GroupNowSeconds = FPlatformTime::Seconds();
	if (Publication->Groups.Num() > 1 && GroupNowSeconds >= NextGroupLogSeconds)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Voxel local publish plan: groups=%d meshUpdates=%d visibilityChanges=%d maxGroupsPerFrame=%d"),
			Publication->Groups.Num(), Publication->Updates.Num(), Publication->Visibility.Num(),
			Module.GetViewSettings().MaxPublishGroupsPerFrame);
		NextGroupLogSeconds = GroupNowSeconds + 5.0;
	}
#endif
	Publication->Serial = ++BatchSerial;
	Publication->PreparedMeshes.SetNum(Publication->Updates.Num());
	Publication->bCoveragePrepared = Publication->Updates.IsEmpty();
	Publication->bGroupsPrepared = false;
	Publications.Add(Publication);
	bActiveBusyKeysDirty = true;
}

void FVoxelViewPublisher::BuildGroups(const FPublicationRef& Publication, const bool bInPreparedCoverage)
{
	TArray<FVoxelPublishFootprint> Footprints;
	TMap<TWeakObjectPtr<AActor>, int32> ActorIndices;
	TMap<TWeakObjectPtr<AActor>, const FUpdate*> Updates;
	for (const FUpdate& Update : Publication->Updates) Updates.Add(Update.Actor, &Update);
	auto AddActor = [this, &Footprints, &ActorIndices, &Updates, &Publication, bInPreparedCoverage](
		const TWeakObjectPtr<AActor> Actor)
	{
		if (ActorIndices.Contains(Actor)) return;
		const FEntry* Entry = Entries.Find(Actor);
		const int32 FirstFootprint = Footprints.Num();
		FVoxelPublishFootprint& Footprint = Footprints.AddDefaulted_GetRef();
		if (Entry)
		{
			Footprint.Key = Entry->GroupKey;
			const FUpdate* const* Update = Updates.Find(Actor);
			const bool* Hidden = Publication->Visibility.Find(Actor);
			bool PreviousHidden = Actor.IsValid() && Actor->IsHidden();
			uint64 PreviousVisibilitySerial = 0;
			for (const auto& Older : Publications)
			{
				if (Older->bCanceled || Older->bFinished || Older->Serial >= Publication->Serial ||
					Older->Serial <= PreviousVisibilitySerial) continue;
				if (const bool* ProjectedHidden = Older->Visibility.Find(Actor))
				{
					PreviousHidden = *ProjectedHidden;
					PreviousVisibilitySerial = Older->Serial;
				}
			}
			const bool bVisibilityChange = Hidden && Actor.IsValid() && PreviousHidden != *Hidden;
			const bool bMeshChange = Update && !(**Update).bUnchanged;
			const bool bClippingChange = bInPreparedCoverage && bMeshChange && !(**Update).bSourceDirty;
			const bool bPhysicalChange = !bInPreparedCoverage || bVisibilityChange ||
				(bMeshChange && (**Update).bSourceDirty);
			const bool bLogicalChange = !bInPreparedCoverage || Publication->OwnershipChanges.Contains(Entry->GroupKey);
			Footprint.bTransition = Entry->bTransition && (bPhysicalChange || bLogicalChange || bClippingChange);
			const FBox Bounds = Update ? (**Update).Bounds : Entry->Bounds;
			const FVector Location = Update ? (**Update).Location : Entry->Location;
			const double Scale = Update ? (**Update).Scale : Entry->Scale;
			const FBox Desired = Bounds.IsValid
				? FBox(Location + Bounds.Min * Scale, Location + Bounds.Max * Scale)
				: FBox(ForceInit);
			if (bPhysicalChange)
				Footprint.Bounds = FVoxelPublishGroupPlanner::PublicationBounds(
					Entry->GroupKey, Module.BlockSize(), Entry->PresentedWorldBounds, Desired, Entry->bTransition);
			else if (bLogicalChange && !Entry->bTransition)
				Footprint.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Entry->GroupKey, Module.BlockSize());
			if (bClippingChange && !bPhysicalChange)
			{
				FVoxelPublishFootprint Changed = Footprint;
				bool bHasFootprint = Footprint.Bounds.IsValid != 0;
				for (const FBox& ChangeBounds : (**Update).ChangedBounds)
				{
					if (!bHasFootprint) { Footprints[FirstFootprint].Bounds = ChangeBounds; bHasFootprint = true; }
					else { Changed.Bounds = ChangeBounds; Footprints.Add(Changed); }
				}
			}
		}
		ActorIndices.Add(Actor, FirstFootprint);
	};
	for (const FUpdate& Update : Publication->Updates) AddActor(Update.Actor);
	for (const auto& Pair : Publication->Visibility) AddActor(Pair.Key);
	// 空网格仍承担空间所有权，必须参与父子组并集；独立追加会提前交出旧覆盖。
	TSet<FVoxelPublishGroupKey> ActorKeys;
	for (const auto& Footprint : Footprints) ActorKeys.Add(Footprint.Key);
	for (const auto& Pair : Publication->Callbacks)
	{
		if (ActorKeys.Contains(Pair.Key)) continue;
		auto& Footprint = Footprints.AddDefaulted_GetRef();
		Footprint.Key = Pair.Key;
		const bool bLogicalChange = !bInPreparedCoverage || Publication->OwnershipChanges.Contains(Pair.Key);
		Footprint.bTransition = bLogicalChange && (Pair.Key.Representation == 4 || Pair.Key.Representation == 5);
		if (bLogicalChange && !Footprint.bTransition)
			Footprint.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Pair.Key, Module.BlockSize());
	}

	TArray<int32> GroupIndices;
	FVoxelPublishGroupPlanner::Build(Footprints, GroupIndices);
	int32 GroupCount = 0;
	for (const int32 Index : GroupIndices)
	{
		GroupCount = FMath::Max(GroupCount, Index + 1);
	}
	Publication->Groups.Reset();
	Publication->GroupOrder.Reset();
	Publication->Groups.SetNum(GroupCount);
	TSet<FVoxelPublishGroupKey> GroupKeys;
	for (int32 Index = 0; Index < Footprints.Num(); ++Index)
	{
		FGroup& Group = Publication->Groups[GroupIndices[Index]];
		if (!GroupKeys.Contains(Footprints[Index].Key))
		{
			GroupKeys.Add(Footprints[Index].Key);
			Group.Keys.Add(Footprints[Index].Key);
		}
		Group.Bounds += Footprints[Index].Bounds;
		if (Footprints[Index].Bounds.IsValid) Group.ChangedBounds.Add(Footprints[Index].Bounds);
		Group.bTransition |= Footprints[Index].bTransition;
	}
	for (FGroup& Group : Publication->Groups)
	{
		VoxelMeshClipper::NormalizeBoxes(Group.ChangedBounds);
		for (const auto& Key : Group.Keys)
			if (const int32* Stage = Publication->TerrainStages.Find(Key))
				Group.TerrainStage = FMath::Min(Group.TerrainStage, *Stage);
	}
	for (FUpdate& Update : Publication->Updates)
	{
		Update.GroupIndex = GroupIndices[ActorIndices.FindChecked(Update.Actor)];
	}
	Publication->Updates.StableSort([](const FUpdate& A, const FUpdate& B)
	{
		return A.GroupIndex == B.GroupIndex
			? A.TerrainStage < B.TerrainStage
			: A.GroupIndex < B.GroupIndex;
	});
	for (int32 Index = 0; Index < Publication->Updates.Num(); ++Index)
	{
		const FUpdate& Update = Publication->Updates[Index];
		FGroup& Group = Publication->Groups[Update.GroupIndex];
		Group.UpdateIndices.Add(Index);
		if (Update.TerrainStage != INDEX_NONE) Group.TerrainStage = FMath::Min(Group.TerrainStage, Update.TerrainStage);
	}
	for (const auto& Pair : Publication->Visibility)
	{
		Publication->Groups[GroupIndices[ActorIndices.FindChecked(Pair.Key)]].VisibilityActors.Add(Pair.Key);
	}
}

void FVoxelViewPublisher::DeferSaturatedGroups(const FPublicationRef& Publication, TMap<FVoxelPublishGroupKey, FGroupPriority>& OutPriorities)
{
	TMap<FVoxelPublishGroupKey, int32> Generations;
	for (const auto& Older : Publications)
	{
		if (Older->bCanceled || Older->bFinished || Older->Serial >= Publication->Serial) continue;
		for (const int32 Index : Older->GroupOrder)
		{
			const FGroup& Group = Older->Groups[Index];
			if (!Group.bCommitted) for (const auto& Key : Group.Keys) ++Generations.FindOrAdd(Key);
		}
	}
	if (!Publication->Groups.ContainsByPredicate([&Generations](const FGroup& Group)
	{
		return Group.Keys.ContainsByPredicate([&Generations](const FVoxelPublishGroupKey& Key)
		{
			return Generations.FindRef(Key) >= 2;
		});
	})) return;
	TArray<int32> Remap;
	Remap.Init(INDEX_NONE, Publication->Groups.Num());
	TArray<FGroup> Accepted;
	TSet<TWeakObjectPtr<AActor>> DeferredActors;
	for (int32 Index = 0; Index < Publication->Groups.Num(); ++Index)
	{
		FGroup& Group = Publication->Groups[Index];
		if (!Group.Keys.ContainsByPredicate([&Generations](const FVoxelPublishGroupKey& Key)
		{
			return Generations.FindRef(Key) >= 2;
		}))
		{
			Remap[Index] = Accepted.Num();
			Accepted.Add(MoveTemp(Group));
			continue;
		}
		// 整个真实连接组退回待提交区，保留更新的输入；不能拆开父子或接缝交接。
		const FGroupPriority Priority = GroupPriority(Group);
		const auto PriorityBounds = MakeShared<FDeferredPriorityDemand, ESPMode::ThreadSafe>();
		PriorityBounds->Bounds = Group.ChangedBounds.IsEmpty() ? TArray<FBox>{Group.Bounds} : Group.ChangedBounds;
		PriorityBounds->Serial = Publication->Serial;
		PriorityBounds->TerrainStage = GroupStage(Group);
		PriorityBounds->bTransition = Group.bTransition;
		for (const auto& Key : Group.Keys)
		{
			MergePriority(OutPriorities.FindOrAdd(Key), Priority);
		}
#if !UE_BUILD_SHIPPING
		static double NextSaturatedPointLog = 0.0;
		if (FPlatformTime::Seconds() >= NextSaturatedPointLog && Group.Keys.ContainsByPredicate([this](const FVoxelPublishGroupKey& Key)
		{
			if (Key.Representation != 0) return false;
			for (const FVector& Observer : Observers)
			{
				const FIntPoint Cell(FMath::FloorToInt(Observer.X / Module.BlockSize()), FMath::FloorToInt(Observer.Y / Module.BlockSize()));
				if (Key.Coordinate == FIntPoint(VoxelGeneration::FloorDivide(Cell.X, 16), VoxelGeneration::FloorDivide(Cell.Y, 16))) return true;
			}
			return false;
		}))
		{
			for (const auto& Key : Group.Keys)
			{
				if (Generations.FindRef(Key) < 2) continue;
				UE_LOG(LogTemp, Display, TEXT("Voxel point deferred: serial=%llu owners=%d updates=%d saturatedRepresentation=%d key=%s level=%d bounds=%s"),
					Publication->Serial, Group.Keys.Num(), Group.UpdateIndices.Num(), Key.Representation,
					*Key.Coordinate.ToString(), Key.Level, *Group.Bounds.ToString());
				break;
			}
			NextSaturatedPointLog = FPlatformTime::Seconds() + 1.0;
		}
#endif
		for (const auto& Key : Group.Keys)
		{
			if (!PendingGroupIds.Contains(Key))
			{
				const FGuid Handle = FGuid::NewGuid();
				PendingGroupIds.Add(Key, Handle);
				PendingGroups.Add(Handle, {Key, true, Publication->Callbacks.FindRef(Key),
					Publication->OwnershipChanges.Contains(Key)});
			}
			else if (Publication->OwnershipChanges.Contains(Key))
				PendingGroups.FindChecked(PendingGroupIds.FindChecked(Key)).bOwnershipChanged = true;
			PendingGroups.FindChecked(PendingGroupIds.FindChecked(Key)).DeferredPriorityBounds = PriorityBounds;
			FPendingGroup& Pending = PendingGroups.FindChecked(PendingGroupIds.FindChecked(Key));
			Pending.TerrainStage = FMath::Min(Pending.TerrainStage, GroupStage(Group));
			Publication->Callbacks.Remove(Key);
			Publication->TerrainStages.Remove(Key);
			Publication->OwnershipChanges.Remove(Key);
		}
		for (const int32 UpdateIndex : Group.UpdateIndices) DeferredActors.Add(Publication->Updates[UpdateIndex].Actor);
		for (const auto Actor : Group.VisibilityActors)
		{
			if (!PendingVisibility.Contains(Actor)) PendingVisibility.Add(Actor, Publication->Visibility.FindChecked(Actor));
			Publication->Visibility.Remove(Actor);
		}
	}
	TArray<FUpdate> Updates;
	for (FUpdate& Update : Publication->Updates)
	{
		if (!DeferredActors.Contains(Update.Actor))
		{
			Update.GroupIndex = Remap[Update.GroupIndex];
			Updates.Add(MoveTemp(Update));
			continue;
		}
		if (!PendingUpdateIndices.Contains(Update.Actor))
		{
			// Exclusions 已转换为网格局部坐标；重试从原始世界格输入重新比较最新前代。
			Update.Exclusions = Update.LocalWorldBoxes;
			Update.PreviousExclusions.Reset();
			Update.ChangedBounds.Reset();
			Update.bUnchanged = false;
			Update.GroupIndex = INDEX_NONE;
			PendingUpdateIndices.Add(Update.Actor, PendingUpdates.Num());
			PendingUpdates.Add(MoveTemp(Update));
		}
	}
	Publication->Groups = MoveTemp(Accepted);
	Publication->GroupOrder.Reset();
	Publication->Updates = MoveTemp(Updates);
	for (FGroup& Group : Publication->Groups) Group.UpdateIndices.Reset();
	for (int32 Index = 0; Index < Publication->Updates.Num(); ++Index)
		Publication->Groups[Publication->Updates[Index].GroupIndex].UpdateIndices.Add(Index);
	Publication->PreparedMeshes.Reset();
	Publication->PreparedMeshes.SetNum(Publication->Updates.Num());
	Publication->CoverageIndex = Publication->Updates.Num();
	Publication->BuildOrder.Reset();
	Publication->bBuildOrderInitialized = false;
	Publication->bFinished = Publication->Groups.IsEmpty();
	bActiveBusyKeysDirty = true;
}

void FVoxelViewPublisher::UpdateGroupPriorities(TMap<FVoxelPublishGroupKey, FGroupPriority> InPriorities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewGroupPriority);
	TSet<const FDeferredPriorityDemand*> SeenDeferred;
	TArray<FPriorityBoundsPtr> DeferredDemands;
	const auto IncludeDeferred = [this, &InPriorities, &SeenDeferred, &DeferredDemands](
		const FVoxelPublishGroupKey& Key, const FPriorityBoundsPtr& Bounds)
	{
		if (!Bounds) return;
		MergePriority(InPriorities.FindOrAdd(Key), BoundsPriority(Bounds->Bounds, Bounds->TerrainStage));
		if (!SeenDeferred.Contains(Bounds.Get()))
		{
			SeenDeferred.Add(Bounds.Get());
			DeferredDemands.Add(Bounds);
		}
	};
	for (const auto& Pair : PendingGroups) IncludeDeferred(Pair.Value.Key, Pair.Value.DeferredPriorityBounds);
	for (const auto& Publication : Publications)
		if (!Publication->bCanceled && !Publication->bGroupsPrepared)
			for (const auto& Pair : Publication->DeferredPriorityBounds) IncludeDeferred(Pair.Key, Pair.Value);
	TArray<FPublicationRef> Ordered = Publications;
	Ordered.Sort([](const FPublicationRef& First, const FPublicationRef& Second) { return First->Serial > Second->Serial; });
	TMap<const FPublication*, FGroupPriority> Priorities;
	TMap<const FGroup*, FGroupPriority> SpatialPriorities;
	TArray<TPair<uint64, FGroup*>> SpatialGroups;
	for (const auto& Publication : Ordered)
	{
		if (Publication->bCanceled || Publication->bFinished) continue;
		for (const int32 Index : Publication->GroupOrder)
		{
			FGroup& Group = Publication->Groups[Index];
			if (!Group.bCommitted && Group.Bounds.IsValid) SpatialGroups.Emplace(Publication->Serial, &Group);
		}
	}
	// 饱和后退回待提交区的 Fine 仍依赖旧空间覆盖，必须把阶段需求传给这些前代。
	for (const FPriorityBoundsPtr& Deferred : DeferredDemands)
	{
		FGroup Demand;
		Demand.Bounds = FBox(ForceInit);
		Demand.ChangedBounds = Deferred->Bounds;
		Demand.bTransition = Deferred->bTransition;
		for (const FBox& Bounds : Deferred->Bounds)
			if (Bounds.IsValid) Demand.Bounds += Bounds;
		const FGroupPriority Priority = BoundsPriority(Deferred->Bounds, Deferred->TerrainStage);
		for (const auto& Candidate : SpatialGroups)
		{
			if (Candidate.Key >= Deferred->Serial || !GroupsInteract(Demand, *Candidate.Value)) continue;
			MergePriority(SpatialPriorities.FindOrAdd(Candidate.Value), Priority);
		}
	}
	for (const auto& Publication : Ordered)
	{
		FGroupPriority PublicationPriority;
		if (Publication->bCanceled || Publication->bFinished)
		{
			Priorities.Add(&Publication.Get(), PublicationPriority);
			continue;
		}
		for (const int32 Index : Publication->GroupOrder)
		{
			FGroup& Group = Publication->Groups[Index];
			Group.Priority = GroupPriority(Group);
			if (const FGroupPriority* Priority = SpatialPriorities.Find(&Group)) MergePriority(Group.Priority, *Priority);
			if (Publication->bGroupsPrepared)
			{
				for (const auto& Key : Group.Keys)
					if (const FGroupPriority* Priority = InPriorities.Find(Key)) MergePriority(Group.Priority, *Priority);
				for (const auto& Key : Group.Keys)
				{
					MergePriority(InPriorities.FindOrAdd(Key), Group.Priority);
				}
				if (Group.Bounds.IsValid)
				{
					for (const auto& Candidate : SpatialGroups)
					{
						if (Candidate.Key >= Publication->Serial || !GroupsInteract(Group, *Candidate.Value)) continue;
						MergePriority(SpatialPriorities.FindOrAdd(Candidate.Value), Group.Priority);
					}
				}
			}
			if (HigherGroupPriority(Group.Priority, PublicationPriority)) PublicationPriority = Group.Priority;
		}
		Publication->GroupOrder.StableSort([&Publication](const int32 First, const int32 Second)
		{
			return HigherGroupPriority(Publication->Groups[First].Priority, Publication->Groups[Second].Priority);
		});
		Priorities.Add(&Publication.Get(), PublicationPriority);
	}
	Publications.StableSort([&Priorities](const FPublicationRef& First, const FPublicationRef& Second)
	{
		const FGroupPriority& FirstPriority = Priorities.FindChecked(&First.Get());
		const FGroupPriority& SecondPriority = Priorities.FindChecked(&Second.Get());
		if (HigherGroupPriority(FirstPriority, SecondPriority)) return true;
		if (HigherGroupPriority(SecondPriority, FirstPriority)) return false;
		return First->Serial < Second->Serial;
	});
}

bool FVoxelViewPublisher::GroupsInteract(const FGroup& InFirst, const FGroup& InSecond)
{
	if (!InFirst.Bounds.IsValid || !InSecond.Bounds.IsValid) return false;
	const bool bTransition = InFirst.bTransition || InSecond.bTransition;
	const auto Interacts = [bTransition](const FBox& A, const FBox& B)
	{
		const double X = FMath::Min(A.Max.X, B.Max.X) - FMath::Max(A.Min.X, B.Min.X);
		const double Y = FMath::Min(A.Max.Y, B.Max.Y) - FMath::Max(A.Min.Y, B.Min.Y);
		return bTransition ? (X >= 0.0 && Y > 0.0) || (Y >= 0.0 && X > 0.0) : X > 0.0 && Y > 0.0;
	};
	if (!Interacts(InFirst.Bounds, InSecond.Bounds)) return false;
	const TConstArrayView<FBox> First = InFirst.ChangedBounds.IsEmpty()
		? MakeArrayView(&InFirst.Bounds, 1) : MakeArrayView(InFirst.ChangedBounds);
	const TConstArrayView<FBox> Second = InSecond.ChangedBounds.IsEmpty()
		? MakeArrayView(&InSecond.Bounds, 1) : MakeArrayView(InSecond.ChangedBounds);
	for (const FBox& A : First)
		for (const FBox& B : Second) if (Interacts(A, B)) return true;
	return false;
}

void FVoxelViewPublisher::RefreshTaskPriorities()
{
	TMap<uint64, const FPublication*> Active;
	for (const auto& Publication : Publications)
		if (!Publication->bCanceled && !Publication->bFinished) Active.Add(Publication->Serial, &Publication.Get());
	Scheduler.UpdatePriorities([this, &Active](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, int32& SourcePriority, double& DistanceScore, double& ForwardScore)
	{
		if (Kind != EVoxelTaskKind::BuildViewCoverage || Stamp.WorldEpoch != WorldEpoch ||
			Stamp.Section.X != MIN_int32 || (Stamp.Section.Y != 2 && Stamp.Section.Y != 3)) return;
		const FPublication* const* Found = Active.Find(Stamp.Token);
		if (!Found) return;
		const FPublication& Publication = **Found;
		TSet<int32> GroupIndices;
		if (Stamp.Section.Y == 3)
		{
			if (!Publication.Updates.IsValidIndex(Stamp.Section.Z)) return;
			GroupIndices.Add(Publication.Updates[Stamp.Section.Z].GroupIndex);
		}
		else
		{
			for (int32 Index = Publication.CoverageIndex;
				Index < FMath::Min(Publication.Updates.Num(), Publication.CoverageIndex + 128); ++Index)
				GroupIndices.Add(Publication.Updates[Index].GroupIndex);
		}
		TArray<int32> PriorityGroups = GroupIndices.Array();
		FVoxelTaskRequest Priority;
		Priority.WorkClass = EVoxelWorkClass::Visible;
		SetPublicationPriority(Priority, Publication, PriorityGroups);
		WorkClass = Priority.WorkClass;
		SourcePriority = Priority.SourcePriority;
		DistanceScore = Priority.DistanceScore;
		ForwardScore = Priority.ForwardScore;
	});
	Scheduler.UpdateTerrainStages([this, &Active](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp, int32& TerrainStage)
	{
		if (Kind != EVoxelTaskKind::BuildViewCoverage || Stamp.WorldEpoch != WorldEpoch ||
			Stamp.Section.X != MIN_int32 || (Stamp.Section.Y != 2 && Stamp.Section.Y != 3)) return;
		const FPublication* const* Found = Active.Find(Stamp.Token);
		if (!Found) return;
		const FPublication& Publication = **Found;
		FGroupPriority Priority;
		if (Stamp.Section.Y == 3)
		{
			if (!Publication.Updates.IsValidIndex(Stamp.Section.Z)) return;
			MergePriority(Priority, Publication.Groups[Publication.Updates[Stamp.Section.Z].GroupIndex].Priority);
		}
		else
		{
			for (int32 Index = Publication.CoverageIndex;
				Index < FMath::Min(Publication.Updates.Num(), Publication.CoverageIndex + 128); ++Index)
				MergePriority(Priority, Publication.Groups[Publication.Updates[Index].GroupIndex].Priority);
		}
		TerrainStage = Priority.TerrainStage;
	});
}

void FVoxelViewPublisher::AdmitCoveragePreparation(const FPublicationRef& Publication)
{
	if (Publication->bCoveragePrepared || Publication->bCoveragePreparing) return;
	TMap<TWeakObjectPtr<AActor>, TPair<uint64, const FUpdate*>> Projected;
	TSet<TWeakObjectPtr<AActor>> Waiting;
	for (const auto& Older : Publications)
	{
		if (Older->bCanceled || Older->bFinished || Older->Serial >= Publication->Serial) continue;
		for (const FUpdate& Update : Older->Updates)
		{
			if (!Update.Actor.IsValid() || !Older->Groups.IsValidIndex(Update.GroupIndex)) continue;
			if (Older->Groups[Update.GroupIndex].bCommitted) continue;
			if (!Older->bCoveragePrepared) { Waiting.Add(Update.Actor); continue; }
			auto* Existing = Projected.Find(Update.Actor);
			if (!Existing || Existing->Key < Older->Serial) Projected.Add(Update.Actor, {Older->Serial, &Update});
		}
	}
	TArray<FCoveragePreparationInput> Inputs;
	Inputs.Reserve(FMath::Min(128, Publication->Updates.Num() - Publication->CoverageIndex));
	uint64 InputBytes = 0;
	for (int32 Index = Publication->CoverageIndex; Index < Publication->Updates.Num() && Inputs.Num() < 128; ++Index)
	{
		FUpdate& Update = Publication->Updates[Index];
		if (Waiting.Contains(Update.Actor)) return;
		if (!Inputs.IsEmpty() && InputBytes + Update.Exclusions.GetAllocatedSize() + Update.PreviousExclusions.GetAllocatedSize() > 4ull * 1024ull * 1024ull) break;
		const auto* Previous = Projected.Find(Update.Actor);
		const FEntry* Entry = Entries.Find(Update.Actor);
		if (Update.bRetainSource)
		{
			if (Previous)
			{
				Update.Source = Previous->Value->Source;
				Update.Bounds = Previous->Value->Bounds;
				Update.Location = Previous->Value->Location;
				Update.Scale = Previous->Value->Scale;
				Update.LocalWorldBoxes = Previous->Value->LocalWorldBoxes;
			}
			else if (Entry && Entry->PresentedSource)
			{
				Update.Source = Entry->PresentedSource;
				Update.Bounds = Entry->PresentedWorldBounds.IsValid
					? FBox((Entry->PresentedWorldBounds.Min - Update.Location) / Update.Scale,
						(Entry->PresentedWorldBounds.Max - Update.Location) / Update.Scale) : FBox(ForceInit);
				Update.LocalWorldBoxes = Entry->LocalWorldBoxes;
			}
			Update.Exclusions = Update.LocalWorldBoxes;
		}
		FCoveragePreparationInput& Input = Inputs.AddDefaulted_GetRef();
		Input.Origin = Update.Location / Module.BlockSize();
		Input.Step = Update.Scale / Module.BlockSize();
		Input.Bounds = Update.Bounds;
		Input.Boxes = Update.Exclusions;
		if (Previous)
		{
			Input.Previous = Previous->Value->Exclusions;
			Update.bSourceDirty = Previous->Value->Source != Update.Source ||
				Previous->Value->Location != Update.Location || Previous->Value->Scale != Update.Scale;
		}
		else
		{
			Input.Previous = Entry ? Entry->Exclusions : Update.PreviousExclusions;
			if (Entry) Update.bSourceDirty = Entry->PresentedSource != Update.Source;
		}
		Input.Common = Update.CommonBoxes;
		Input.bDirty = Update.bSourceDirty;
		InputBytes += Input.Boxes.GetAllocatedSize() + Input.Previous.GetAllocatedSize();
	}
	const bool bInitialUncovered = !Inputs.IsEmpty() && Inputs.ContainsByPredicate([](const FCoveragePreparationInput& Input)
	{
		return !Input.bDirty || !Input.Previous.IsEmpty() || !Input.Boxes.IsEmpty() ||
			(Input.Common && !Input.Common->IsEmpty());
	}) == false;
	if (bInitialUncovered)
	{
		const int32 End = Publication->CoverageIndex + Inputs.Num();
		for (int32 Index = Publication->CoverageIndex; Index < End; ++Index)
		{
			Publication->Updates[Index].PreviousExclusions.Reset();
			// 覆盖比较后还会重排 Updates；直接网格在分组固定后按最终索引准备。
		}
		Publication->CoverageIndex = End;
		Publication->bCoveragePrepared = End == Publication->Updates.Num();
		return;
	}
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildViewCoverage;
	Request.bPublicationContinuation = true;
	Request.bTerrainDataOnly = true;
	Request.WorkClass = EVoxelWorkClass::Visible;
	TSet<int32> PriorityGroups;
	for (int32 Index = Publication->CoverageIndex; Index < Publication->CoverageIndex + Inputs.Num(); ++Index)
		PriorityGroups.Add(Publication->Updates[Index].GroupIndex);
	SetPublicationPriority(Request, *Publication, PriorityGroups.Array());
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.Section = FIntVector(MIN_int32, 2, 0);
	Request.Stamp.Token = Publication->Serial;
	Request.ReservedBytes = 32ull * 1024ull * 1024ull;
	Request.InputBytes = InputBytes + Inputs.GetAllocatedSize();
	Request.Execute = [Inputs = MoveTemp(Inputs)](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		auto Payload = MakeShared<FCoveragePreparationResult, ESPMode::ThreadSafe>();
		Payload->Boxes.SetNum(Inputs.Num());
		Payload->ChangedBounds.SetNum(Inputs.Num());
		Payload->Unchanged.SetNumZeroed(Inputs.Num());
		for (int32 Index = 0; Index < Inputs.Num(); ++Index)
		{
			if (Cancel.Load()) return Result;
			const auto& Input = Inputs[Index];
			auto& Boxes = Payload->Boxes[Index];
			auto Include = [&](FBox Box)
			{
				Box.Min = (Box.Min - Input.Origin) / Input.Step;
				Box.Max = (Box.Max - Input.Origin) / Input.Step;
				if (Input.Bounds.Intersect(Box)) Boxes.Add(Box);
			};
			for (const FBox& Box : Input.Boxes) Include(Box);
			if (Input.Common) for (const FBox& Box : *Input.Common) Include(Box);
			VoxelMeshClipper::NormalizeBoxes(Boxes);
			Payload->Unchanged[Index] = !Input.bDirty && Input.Previous == Boxes;
			if (!Input.bDirty && !Payload->Unchanged[Index])
			VoxelMeshClipper::FindChangedCoverageBounds(Input.Previous, Boxes,
					Input.Bounds, Payload->ChangedBounds[Index], &Cancel);
			if (!Input.bDirty && Payload->ChangedBounds[Index].IsEmpty()) Payload->Unchanged[Index] = true;
			if (Cancel.Load()) return Result;
		}
		Result.CustomPayload = Payload;
		Result.bSuccess = true;
		return Result;
	};
	const uint64 Serial = Publication->Serial;
	const int32 First = Publication->CoverageIndex;
	Request.Apply = [this, Publication, Serial, First](FVoxelTaskResult&& Result)
	{
		if (Publication->bCanceled || Serial != Publication->Serial) return;
		Publication->bCoveragePreparing = false;
		// 队列让位仅重试这一份不可变输入，保留已完成的裁剪与组件。
		if (Result.bCanceled && Result.Stamp.WorldEpoch == WorldEpoch) return;
		if (!Result.bSuccess || Result.bCanceled || Result.Stamp.WorldEpoch != WorldEpoch)
		{
			DiscardPublication(Publication);
			bRetry = true;
			return;
		}
		const auto Payload = StaticCastSharedPtr<const FCoveragePreparationResult>(Result.CustomPayload);
		int32 Changed = 0;
		for (int32 Offset = 0; Offset < Payload->Boxes.Num(); ++Offset)
		{
			const int32 Index = First + Offset;
			Publication->Updates[Index].Exclusions = Payload->Boxes[Offset];
			Publication->Updates[Index].bUnchanged = Payload->Unchanged[Offset] != 0;
			Publication->Updates[Index].ChangedBounds = Payload->ChangedBounds[Offset];
			for (FBox& Bounds : Publication->Updates[Index].ChangedBounds)
			{
				const FUpdate& Update = Publication->Updates[Index];
				Bounds = FBox(Update.Location + Bounds.Min * Update.Scale,
					Update.Location + Bounds.Max * Update.Scale);
			}
			Publication->Updates[Index].PreviousExclusions.Reset();
			Changed += Publication->Updates[Index].bUnchanged ? 0 : 1;
		}
		Publication->CoverageIndex = First + Payload->Boxes.Num();
		Publication->bCoveragePrepared = Publication->CoverageIndex == Publication->Updates.Num();
		UE_LOG(LogTemp, Display, TEXT("Voxel coverage batch: checked=%d changed=%d compareMs=%.2f"),
			Payload->Boxes.Num(), Changed, Result.ExecuteMilliseconds);
	};
	Publication->bCoveragePreparing = Scheduler.Enqueue(MoveTemp(Request));
}

void FVoxelViewPublisher::AdmitBuilds(const FPublicationRef& Publication)
{
	if (!Publication->bCoveragePrepared || !Publication->bGroupsPrepared || Publication->CoverageIndex == 0) return;
	constexpr int32 MaximumPendingBuilds = 4;
	if (!Publication->bBuildOrderInitialized)
	{
		for (const int32 GroupIndex : Publication->GroupOrder)
			Publication->BuildOrder.Append(Publication->Groups[GroupIndex].UpdateIndices);
		Publication->bBuildOrderInitialized = true;
	}
	// 只重排派生的待裁剪索引；已接纳任务仍引用不可变 Updates 的物理索引。
	Publication->BuildOrder.StableSort([&Publication](const int32 First, const int32 Second)
	{
		const int32 FirstGroup = Publication->Updates[First].GroupIndex;
		const int32 SecondGroup = Publication->Updates[Second].GroupIndex;
		const FGroupPriority& FirstPriority = Publication->Groups[FirstGroup].Priority;
		const FGroupPriority& SecondPriority = Publication->Groups[SecondGroup].Priority;
		if (HigherGroupPriority(FirstPriority, SecondPriority)) return true;
		if (HigherGroupPriority(SecondPriority, FirstPriority)) return false;
		return FirstGroup == SecondGroup ? First < Second : FirstGroup < SecondGroup;
	});
	const int32 Limit = FMath::Min(Publication->BuildOrder.Num(), 128);
	TArray<int32> RetryIndices;
	int32 Checked = 0;
	while (Checked < Limit &&
		Publication->PendingBuildIndices.Num() < MaximumPendingBuilds)
	{
		const int32 BuildIndex = Publication->BuildOrder[Checked++];
		if (Publication->Groups.IsValidIndex(Publication->Updates[BuildIndex].GroupIndex) &&
			Publication->Groups[Publication->Updates[BuildIndex].GroupIndex].bCommitted) continue;
		if (Publication->PreparedMeshes[BuildIndex] ||
			Publication->PendingBuildIndices.Contains(BuildIndex))
		{
			continue;
		}
		FUpdate& Update = Publication->Updates[BuildIndex];
		if (!CanPublishGroup(Publication->Groups[Update.GroupIndex]))
		{
			RetryIndices.Add(BuildIndex);
			continue;
		}
		if (Update.bUnchanged || Update.Exclusions.IsEmpty())
		{
			Publication->PreparedMeshes[BuildIndex] = Update.Source;
			continue;
		}
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildViewCoverage;
		Request.bPublicationContinuation = true;
		Request.TerrainStage = Update.TerrainStage;
		Request.WorkClass = EVoxelWorkClass::Visible;
		SetPublicationPriority(Request, *Publication, MakeArrayView(&Update.GroupIndex, 1));
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.Section = FIntVector(MIN_int32, 3, BuildIndex);
		Request.Stamp.Token = Publication->Serial;
		Request.ReservedBytes = 48ull * 1024ull * 1024ull;
		Request.Execute = [Source = Update.Source, Exclusions = Update.Exclusions](const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult TaskResult;
			if (Cancel.Load()) return TaskResult;
			const auto Payload = MakeShared<FVoxelViewCoverageResult, ESPMode::ThreadSafe>();
			auto Mesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
			VoxelMeshClipper::Subtract(*Source, Exclusions, *Mesh, &Cancel);
			Payload->OwnedMeshBytes = Mesh->Bytes();
			Payload->Meshes.Add(MoveTemp(Mesh));
			TaskResult.CustomPayload = Payload;
			TaskResult.bSuccess = true;
			return TaskResult;
		};
		const uint64 Serial = Publication->Serial;
		const int32 Index = BuildIndex;
		Request.Apply = [this, Publication, Serial, Index](FVoxelTaskResult&& TaskResult)
		{
			if (Publication->bCanceled || Serial != Publication->Serial)
			{
				return;
			}
			Publication->PendingBuildIndices.Remove(Index);
			if (TaskResult.bCanceled && TaskResult.Stamp.WorldEpoch == WorldEpoch)
			{
				Publication->BuildOrder.Add(Index);
				return;
			}
			if (!TaskResult.bSuccess || TaskResult.bCanceled || TaskResult.Stamp.WorldEpoch != WorldEpoch)
			{
				if (!TaskResult.Error.IsEmpty())
				{
					UE_LOG(LogTemp, Warning, TEXT("Voxel coverage publication failed: %s"), *TaskResult.Error);
				}
				DiscardPublication(Publication);
				bRetry = true;
				return;
			}
			const auto Payload = StaticCastSharedPtr<const FVoxelViewCoverageResult>(TaskResult.CustomPayload);
			Publication->PreparedMeshes[Index] = Payload->Meshes[0];
		};
		if (!Scheduler.Enqueue(MoveTemp(Request)))
		{
			RetryIndices.Add(BuildIndex);
			continue;
		}
		Publication->PendingBuildIndices.Add(BuildIndex);
	}
	Publication->BuildOrder.RemoveAt(0, Checked, EAllowShrinking::No);
	Publication->BuildOrder.Append(RetryIndices);
}
bool FVoxelViewPublisher::PrepareUpdate(const FPublicationRef& Publication, const int32 InIndex, const double InDeadline,
	int32& InOutPreparedComponents, bool& bOutComplete)
{
	bOutComplete = true;
	const double PrepareStart = FPlatformTime::Seconds();
	FUpdate& Update = Publication->Updates[InIndex];
	if (Update.bUnchanged) return true;
	AActor* Actor = Update.Actor.Get();
	if (!Actor) return true;
	const FVoxelSectionMeshResult& Mesh = *Publication->PreparedMeshes[InIndex];
	while (Update.PreparedBatchIndex < Mesh.Batches.Num())
	{
		if (InOutPreparedComponents >= Module.GetViewSettings().MaxPublishComponentsPerFrame)
		{
			bOutComplete = false;
			break;
		}
		const FVoxelRenderBatch& Batch = Mesh.Batches[Update.PreparedBatchIndex++];
		if (Batch.Mesh.Triangles.IsEmpty()) continue;
		UMaterialInterface* Material = Module.GetMaterialSet()->FindMaterial(Batch.Group, Batch.Bank, Batch.MaterialVariant);
		if (!Material) return false;
		UVoxelMeshComponent* Component = NewObject<UVoxelMeshComponent>(Actor);
		Component->SetVisibility(false);
		Component->SetupAttachment(Actor->GetRootComponent());
		Component->RegisterComponent();
		Update.Components.Add(Component);
		if (!Component->Apply(Batch.Mesh, Update.Scale, Material)) return false;
		++InOutPreparedComponents;
		if (FPlatformTime::Seconds() >= InDeadline)
		{
			bOutComplete = Update.PreparedBatchIndex == Mesh.Batches.Num();
			break;
		}
	}
#if !UE_BUILD_SHIPPING
	const double PrepareMs = (FPlatformTime::Seconds() - PrepareStart) * 1000.0;
	if (PrepareMs > 8.0)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel publish slow prepare: ms=%.2f stage=%d batches=%d bytes=%llu location=%s"),
			PrepareMs, Update.TerrainStage, Mesh.Batches.Num(), Mesh.Bytes(), *Update.Location.ToString());
	}
#endif
	return true;
}

void FVoxelViewPublisher::Tick()
{
	bActiveBusyKeysDirty = true;
	FrameTimings = {};
	const double RetireStart = FPlatformTime::Seconds();
	const double RetireDeadline = FPlatformTime::Seconds() +
		Module.GetViewSettings().PublishRetireMilliseconds / 1000.0;
	while (!RetiredComponents.IsEmpty())
	{
		if (UVoxelMeshComponent* Component = RetiredComponents.Pop().Get()) Component->DestroyComponent();
		if (FPlatformTime::Seconds() >= RetireDeadline) break;
	}
	FrameTimings.RetireMilliseconds = (FPlatformTime::Seconds() - RetireStart) * 1000.0;
	// 活跃的独立分组不能阻止新分组开始；同一 owner 与关联接缝仍串行交接。
	for (const auto& Publication : Publications)
	{
		RefreshGroupOrder(Publication);
		for (const int32 Index : Publication->GroupOrder) Publication->Groups[Index].Priority = {};
	}
	StartCommittedGroups();
	TMap<const FPublication*, double> Distances;
	for (const auto& Publication : Publications)
	{
		double Distance = MAX_dbl;
		for (const int32 Index : Publication->GroupOrder)
			Distance = FMath::Min(Distance, GroupDistance(Publication->Groups[Index]));
		Distances.Add(&Publication.Get(), Distance);
	}
	Publications.StableSort([&Distances](const FPublicationRef& A, const FPublicationRef& B)
	{
		return Distances.FindChecked(&A.Get()) < Distances.FindChecked(&B.Get());
	});
	TMap<FVoxelPublishGroupKey, FGroupPriority> PriorityDonations;
	for (const auto& Publication : Publications)
	{
		AdmitCoveragePreparation(Publication);
		if (Publication->bCoveragePrepared && !Publication->bGroupsPrepared)
		{
#if !UE_BUILD_SHIPPING
			const int32 Before = Publication->Groups.Num();
#endif
			BuildGroups(Publication, true);
			SortGroups(Publication);
			DeferSaturatedGroups(Publication, PriorityDonations);
			Publication->DeferredPriorityBounds.Reset();
			RefreshGroupOrder(Publication);
			Publication->bGroupsPrepared = true;
#if !UE_BUILD_SHIPPING
			static double NextRefinedGroupLog = 0.0;
			if (FPlatformTime::Seconds() >= NextRefinedGroupLog)
			{
				int32 Changed = 0;
				for (const FUpdate& Update : Publication->Updates) Changed += !Update.bUnchanged;
				UE_LOG(LogTemp, Display, TEXT("Voxel prepared publish plan: groups=%d/%d updates=%d changed=%d"),
					Before, Publication->Groups.Num(), Publication->Updates.Num(), Changed);
				NextRefinedGroupLog = FPlatformTime::Seconds() + 5.0;
			}
#endif
		}
	}
	UpdateGroupPriorities(MoveTemp(PriorityDonations));
	RefreshTaskPriorities();
	for (const auto& Publication : Publications)
		if (!Publication->bCanceled && !Publication->bFinished) AdmitBuilds(Publication);
	const double PrepareStart = FPlatformTime::Seconds();
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewPrepareGT);
	const double Deadline = FPlatformTime::Seconds() +
		Module.GetViewSettings().PublishPrepareMilliseconds / 1000.0;
	int32 PreparedComponents = 0;
	for (const auto& Publication : Publications)
	{
		if (Publication->bCanceled || !Publication->bGroupsPrepared) continue;
		for (const int32 GroupIndex : Publication->GroupOrder)
		{
			FGroup& Group = Publication->Groups[GroupIndex];
			if (Group.bCommitted || !CanPublishGroup(Group)) continue;
			while (Group.PreparedUpdates < Group.UpdateIndices.Num())
			{
				const int32 Index = Group.UpdateIndices[Group.PreparedUpdates];
				if (!Publication->PreparedMeshes[Index]) break;
				bool bComplete = false;
				if (!PrepareUpdate(Publication, Index, Deadline, PreparedComponents, bComplete))
				{
					DiscardPublication(Publication);
					bRetry = true;
					break;
				}
				if (bComplete) ++Group.PreparedUpdates;
				if (!bComplete) break;
			}
			if (Publication->bCanceled || PreparedComponents >= Module.GetViewSettings().MaxPublishComponentsPerFrame ||
				FPlatformTime::Seconds() >= Deadline) break;
		}
		if (PreparedComponents >= Module.GetViewSettings().MaxPublishComponentsPerFrame ||
			FPlatformTime::Seconds() >= Deadline)
		{
			break;
		}
	}
	FrameTimings.PublishPrepareMilliseconds = (FPlatformTime::Seconds() - PrepareStart) * 1000.0;
	const double CommitStart = FPlatformTime::Seconds();
	const double CommitDeadline = CommitStart + Module.GetViewSettings().PublishPrepareMilliseconds / 1000.0;
	int32 Committed = 0;
	TMap<FVoxelPublishGroupKey, uint64> FirstOwners;
	for (const auto& Publication : Publications)
	{
		if (Publication->bCanceled || Publication->bFinished) continue;
		for (const int32 GroupIndex : Publication->GroupOrder)
		{
			const FGroup& Group = Publication->Groups[GroupIndex];
			if (Group.bCommitted) continue;
			for (const auto& Key : Group.Keys)
			{
				uint64* Existing = FirstOwners.Find(Key);
				if (!Existing) FirstOwners.Add(Key, Publication->Serial);
				else *Existing = FMath::Min(*Existing, Publication->Serial);
			}
		}
	}
	for (const auto& Publication : Publications)
	{
		if (!Publication->bCanceled) CommitReadyGroups(Publication, Committed, CommitDeadline, FirstOwners);
		if (FPlatformTime::Seconds() >= CommitDeadline) break;
	}
	FrameTimings.PublishCommitMilliseconds = (FPlatformTime::Seconds() - CommitStart) * 1000.0;
	Publications.RemoveAll([](const FPublicationRef& Publication) { return Publication->bFinished || Publication->bCanceled; });
	FinishGroups();
#if !UE_BUILD_SHIPPING
	static double NextPublicationStateLog = 0.0;
	if (!Publications.IsEmpty() && FPlatformTime::Seconds() >= NextPublicationStateLog)
	{
		for (const auto& Publication : Publications)
		{
			int32 CommittedGroups = 0, PreparedGroups = 0, ReadyMeshes = 0, NullSources = 0, ActiveBuilds = 0;
			for (const FGroup& Group : Publication->Groups)
			{
				CommittedGroups += Group.bCommitted;
				PreparedGroups += Group.PreparedUpdates == Group.UpdateIndices.Num();
			}
			for (int32 Index = 0; Index < Publication->Updates.Num(); ++Index)
			{
				const FUpdate& Update = Publication->Updates[Index];
				if (Publication->Groups.IsValidIndex(Update.GroupIndex) && Publication->Groups[Update.GroupIndex].bCommitted) continue;
				ReadyMeshes += Publication->PreparedMeshes[Index].IsValid();
				NullSources += !Update.Source;
			}
			for (const int32 Index : Publication->PendingBuildIndices)
			{
				FVoxelTaskStamp Stamp;
				Stamp.WorldEpoch = WorldEpoch;
				Stamp.Section = FIntVector(MIN_int32, 3, Index);
				Stamp.Token = Publication->Serial;
				ActiveBuilds += Scheduler.Has(Stamp, EVoxelTaskKind::BuildViewCoverage);
			}
			UE_LOG(LogTemp, Display,
				TEXT("Voxel publication state: serial=%llu coverage=%d/%d preparing=%d refined=%d groups=%d prepared=%d committed=%d buildRemaining=%d meshes=%d nullSources=%d pending=%d active=%d"),
				Publication->Serial, Publication->CoverageIndex, Publication->Updates.Num(), Publication->bCoveragePreparing,
				Publication->bGroupsPrepared, Publication->Groups.Num(), PreparedGroups, CommittedGroups,
				Publication->BuildOrder.Num(), ReadyMeshes, NullSources, Publication->PendingBuildIndices.Num(), ActiveBuilds);
			for (const auto& Pair : Module.GetCurrentInterest().Sources)
			{
				const auto& Source = Pair.Value.Source;
				if (!Source.bLocalView || !Source.Has(EVoxelStreamingCapability::FineVisual) ||
					Source.Has(EVoxelStreamingCapability::LocalRefinement)) continue;
				const FVoxelPublishGroupKey Key{0, FIntPoint(
					VoxelGeneration::FloorDivide(Source.Center.X, 16), VoxelGeneration::FloorDivide(Source.Center.Y, 16)), 0};
				if (const FGuid* PendingId = PendingGroupIds.Find(Key))
				{
					const FPendingGroup& Pending = PendingGroups.FindChecked(*PendingId);
					UE_LOG(LogTemp, Display, TEXT("Voxel point pending owner: key=%s committed=%d ownershipChanged=%d"),
						*Key.Coordinate.ToString(), Pending.bCommitted, Pending.bOwnershipChanged);
				}
				for (const FGroup& Group : Publication->Groups)
				{
					if (Group.bCommitted || !Group.Keys.Contains(Key)) continue;
					int32 Meshes = 0;
					for (const int32 Index : Group.UpdateIndices) Meshes += Publication->PreparedMeshes[Index].IsValid();
					const uint64* First = FirstOwners.Find(Key);
					UE_LOG(LogTemp, Display,
						TEXT("Voxel point publish group: serial=%llu first=%llu key=%s owners=%d updates=%d meshes=%d prepared=%d bounds=%s"),
						Publication->Serial, First ? *First : 0, *Key.Coordinate.ToString(), Group.Keys.Num(),
						Group.UpdateIndices.Num(), Meshes, Group.PreparedUpdates, *Group.Bounds.ToString());
				}
				break;
			}
			if (Publication->bGroupsPrepared)
			{
				for (int32 Index = 0; Index < Publication->Updates.Num(); ++Index)
				{
					const FUpdate& Update = Publication->Updates[Index];
					if (Publication->PreparedMeshes[Index] || !Publication->Groups.IsValidIndex(Update.GroupIndex) ||
						Publication->Groups[Update.GroupIndex].bCommitted) continue;
					FVoxelTaskRequest Priority;
					Priority.WorkClass = EVoxelWorkClass::Visible;
					SetPublicationPriority(Priority, *Publication, MakeArrayView(&Update.GroupIndex, 1));
					const auto Tasks = Scheduler.GetDiagnostics();
					UE_LOG(LogTemp, Display,
						TEXT("Voxel publication missing mesh: serial=%llu index=%d stage=%d effectiveStage=%d gate=%d class=%d priority=%d distance=%.1f exclusions=%d sourceMiB=%.2f scheduler=%d/%d reservedMiB=%.2f"),
						Publication->Serial, Index, Update.TerrainStage, Priority.TerrainStage, Scheduler.GetTerrainBuildStage(), static_cast<int32>(Priority.WorkClass),
						Priority.SourcePriority, Priority.DistanceScore, Update.Exclusions.Num(),
						Update.Source ? Update.Source->Bytes() / 1048576.0 : 0.0, Tasks.Pending, Tasks.Running,
						Tasks.ReservedBytes / 1048576.0);
					break;
				}
			}
		}
		NextPublicationStateLog = FPlatformTime::Seconds() + 5.0;
	}
#endif
}

void FVoxelViewPublisher::CommitReadyGroups(const FPublicationRef& Publication, int32& CommittedThisFrame, const double Deadline,
	const TMap<FVoxelPublishGroupKey, uint64>& FirstOwners)
{
	if (!Publication->bGroupsPrepared) return;
	const int32 Limit = FMath::Max(1, Module.GetViewSettings().MaxPublishGroupsPerFrame);
	for (const int32 GroupIndex : Publication->GroupOrder)
	{
		FGroup& Group = Publication->Groups[GroupIndex];
		if (Group.bCommitted || !CanPublishGroup(Group)) continue;
		if (Group.PreparedUpdates != Group.UpdateIndices.Num()) continue;
		if (Group.Keys.ContainsByPredicate([&FirstOwners, &Publication](const FVoxelPublishGroupKey& Key)
		{
			const uint64* First = FirstOwners.Find(Key);
			return First && *First < Publication->Serial;
		})) continue;
		bool bOlderDependency = false;
		for (const auto& Older : Publications)
		{
			if (!Group.Bounds.IsValid) break;
			if (Older->bCanceled || Older->bFinished || Older->Serial >= Publication->Serial) continue;
			for (const int32 OlderIndex : Older->GroupOrder)
			{
				const FGroup& Dependency = Older->Groups[OlderIndex];
				bOlderDependency = !Dependency.bCommitted && GroupsInteract(Group, Dependency);
				if (bOlderDependency) break;
			}
			if (bOlderDependency) break;
		}
		if (bOlderDependency) continue;
		const bool bHasChanges = Group.UpdateIndices.ContainsByPredicate([&Publication](const int32 Index)
		{
			return !Publication->Updates[Index].bUnchanged;
		}) || Group.VisibilityActors.ContainsByPredicate([&Publication](const TWeakObjectPtr<AActor>& Key)
		{
			const AActor* Actor = Key.Get();
			const bool* Hidden = Publication->Visibility.Find(Key);
			return Actor && Hidden && Actor->IsHidden() != *Hidden;
		});
		if (bHasChanges && CommittedThisFrame >= Limit) continue;
		CommitReadyGroup(Publication, Group);
		CommittedThisFrame += bHasChanges ? 1 : 0;
		if (FPlatformTime::Seconds() >= Deadline) break;
	}
	if (!Publication->GroupOrder.ContainsByPredicate([&Publication](const int32 Index) { return !Publication->Groups[Index].bCommitted; }))
	{
		Publication->bFinished = true;
	}
}

void FVoxelViewPublisher::CommitReadyGroup(const FPublicationRef& Publication, FGroup& InGroup)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewCommit);
	const double CommitStart = FPlatformTime::Seconds();
	for (const int32 Index : InGroup.UpdateIndices)
	{
		FUpdate& Update = Publication->Updates[Index];
		AActor* Actor = Update.Actor.Get();
		FEntry* Entry = Entries.Find(Update.Actor);
		if (!Actor || !Entry) continue;
		Entry->LocalWorldBoxes = MoveTemp(Update.LocalWorldBoxes);
		Update.CommonBoxes.Reset();
		if (Update.bUnchanged) { Entry->Exclusions = MoveTemp(Update.Exclusions); continue; }
		for (UVoxelMeshComponent* Component : Entry->Components)
		{
			if (Component)
			{
				Component->SetVisibility(false);
				RetiredComponents.Add(Component);
			}
		}
		Actor->SetActorLocation(Update.Location);
		Entry->Components = MoveTemp(Update.Components);
		for (UVoxelMeshComponent* Component : Entry->Components) Component->SetVisibility(true);
		Entry->Exclusions = MoveTemp(Update.Exclusions);
		Entry->PresentedSource = Update.Source;
		Entry->bDirty = Entry->Source != Update.Source;
		Entry->bPresented = true;
		Entry->PresentedWorldBounds = Update.Bounds.IsValid
			? FBox(Update.Location + Update.Bounds.Min * Update.Scale,
				Update.Location + Update.Bounds.Max * Update.Scale)
			: FBox(ForceInit);
	}
	for (const TWeakObjectPtr<AActor> ActorKey : InGroup.VisibilityActors)
	{
		const bool* bHidden = Publication->Visibility.Find(ActorKey);
		if (AActor* Actor = ActorKey.Get();
			Actor && bHidden && Actor->IsHidden() != *bHidden)
		{
			Actor->SetActorHiddenInGame(*bHidden);
		}
		Publication->Visibility.Remove(ActorKey);
	}
	InGroup.bCommitted = true;
	bActiveBusyKeysDirty = true;
	for (const int32 Index : InGroup.UpdateIndices)
	{
		Publication->PreparedMeshes[Index].Reset();
		Publication->Updates[Index].Source.Reset();
	}
	TMap<FGroupCallback*, TArray<FVoxelPublishGroupKey>> CallbackKeys;
	for (const auto& Key : InGroup.Keys)
	{
		if (const auto* Callback = Publication->Callbacks.Find(Key); Callback && Callback->IsValid() && **Callback)
			CallbackKeys.FindOrAdd(Callback->Get()).Add(Key);
	}
	for (const auto& Pair : CallbackKeys) (*Pair.Key)(Pair.Value);
#if !UE_BUILD_SHIPPING
	const double CommitMs = (FPlatformTime::Seconds() - CommitStart) * 1000.0;
	if (CommitMs > 8.0)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel publish slow local commit: ms=%.2f updates=%d"),
			CommitMs, InGroup.UpdateIndices.Num());
	}
#endif
}

void FVoxelViewPublisher::FinishGroups()
{
	if (!Publications.IsEmpty() || !PendingGroups.IsEmpty()) return;
	CurrentGroupCallback.Reset();
	if (OnCommitted)
	{
		TFunction<void()> Completed = MoveTemp(OnCommitted);
		Completed();
	}
}

void FVoxelViewPublisher::DiscardGroups()
{
	for (const auto& Publication : Publications) DiscardPublication(Publication);
	Publications.Reset();
	PendingGroups.Reset();
	PendingGroupIds.Reset();
	PendingUpdates.Reset();
	PendingUpdateIndices.Reset();
	PendingVisibility.Reset();
	OnCommitted = {};
	CurrentGroupCallback.Reset();
}

void FVoxelViewPublisher::DiscardPublication(const FPublicationRef& Publication)
{
	Publication->bCanceled = true;
	bActiveBusyKeysDirty = true;
	for (FUpdate& Update : Publication->Updates)
	{
		if (!Update.Actor.IsValid()) continue;
		for (UVoxelMeshComponent* Component : Update.Components)
		{
			if (Component) Component->DestroyComponent();
		}
		Update.Components.Reset();
	}
}

void FVoxelViewPublisher::Forget(AActor* InActor)
{
	const TWeakObjectPtr<AActor> Key(InActor);
	PendingVisibility.Remove(Key);
	if (const int32* Index = PendingUpdateIndices.Find(Key)) TakePendingUpdate(*Index);
	Entries.Remove(Key);
}

void FVoxelViewPublisher::Reset()
{
	++BatchSerial;
	DiscardGroups();
	PendingGroups.Reset();
	PendingGroupIds.Reset();
	for (const auto& Retired : RetiredComponents)
	{
		if (UVoxelMeshComponent* Component = Retired.Get()) Component->DestroyComponent();
	}
	RetiredComponents.Reset();
	Entries.Reset();
	ActiveBusyKeys.Reset();
	bActiveBusyKeysDirty = true;
	bRetry = false;
}

bool FVoxelViewPublisher::IsBusy() const
{
	if (!Publications.IsEmpty()) return true;
	for (const auto& Pair : PendingGroups)
	{
		if (Pair.Value.bCommitted) return true;
	}
	return false;
}

bool FVoxelViewPublisher::IsGroupBusy(const FVoxelPublishGroupKey& InKey) const
{
	if (PendingGroupIds.Contains(InKey)) return true;
	if (bActiveBusyKeysDirty)
	{
		ActiveBusyKeys.Reset();
		for (const auto& Publication : Publications)
		{
			if (Publication->bCanceled || Publication->bFinished) continue;
			for (const FGroup& Group : Publication->Groups)
				if (!Group.bCommitted) for (const auto& Key : Group.Keys) ActiveBusyKeys.Add(Key);
		}
		bActiveBusyKeysDirty = false;
	}
	return ActiveBusyKeys.Contains(InKey);
}

bool FVoxelViewPublisher::NeedsUpdate() const
{
	return bRetry;
}

bool FVoxelViewPublisher::IsPresented(AActor* InActor) const
{
	const FEntry* Entry = Entries.Find(TWeakObjectPtr<AActor>(InActor));
	return Entry && Entry->bPresented && !Entry->Components.IsEmpty();
}

bool FVoxelViewPublisher::IsCommitted(AActor* InActor) const
{
	const FEntry* Entry = Entries.Find(TWeakObjectPtr<AActor>(InActor));
	return Entry && Entry->bPresented && !Entry->bDirty;
}

bool FVoxelViewPublisher::HasPresentation(AActor* InActor) const
{
	const FEntry* Entry = Entries.Find(TWeakObjectPtr<AActor>(InActor));
	return Entry && Entry->bPresented;
}
