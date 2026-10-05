#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Algo/Compare.h"

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

FVoxelViewPublisher::FPendingGroup* FVoxelViewPublisher::FindPendingGroup(const FGuid& InGroup)
{
	if (auto* Group = PendingGroups.Find(InGroup)) return Group;
	if (!PendingPlan) return nullptr;
	const auto* Frozen = PendingPlan->Groups.Find(InGroup);
	if (!Frozen || Frozen->bCommitted || PendingGroupIds.Contains(Frozen->Key)) return nullptr;
	// An open handle remains writable across budget boundaries. Promote its latest
	// input to the next cohort, and block the frozen component as a whole.
	PendingGroupIds.Add(Frozen->Key, InGroup);
	return &PendingGroups.Add(InGroup, *Frozen);
}

FGuid FVoxelViewPublisher::BeginGroup(const FVoxelPublishGroupKey& InKey)
{
	if (const FGuid* Existing = PendingGroupIds.Find(InKey))
	{
		// 尚未开始发布的分组可合并最新输入；关联的新 owner 尚未提交时不能把组锁死。
		PendingGroups.FindChecked(*Existing).bCommitted = false;
		return *Existing;
	}
	if (PendingPlan)
		if (const FGuid* Frozen = PendingPlan->GroupIds.Find(InKey))
			if (auto* Open = FindPendingGroup(*Frozen)) { Open->bCommitted = false; return *Frozen; }
	const FGuid Group = FGuid::NewGuid();
	PendingGroups.Add(Group, {InKey, false, CurrentGroupCallback});
	PendingGroupIds.Add(InKey, Group);
	return Group;
}

bool FVoxelViewPublisher::SetTerrainStage(const FGuid& InGroup, const int32 InTerrainStage)
{
	FPendingGroup* Group = FindPendingGroup(InGroup);
	if (!Group) return false;
	Group->TerrainStage = FMath::Min(Group->TerrainStage, FMath::Clamp(InTerrainStage, 0, 3));
	return true;
}

bool FVoxelViewPublisher::Stage(const FGuid& InGroup, AActor*& InOutActor,
	const FVector& InLocation, const double InScale, FVoxelSectionMeshResult&& InMesh,
	const int32 InTerrainStage, const bool bInTransition)
{
	if (!InMesh.bValidated && !InMesh.ValidateAndCache()) return false;
	return StageImmutable(InGroup, InOutActor, InLocation, InScale,
		MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>(MoveTemp(InMesh)), InTerrainStage, bInTransition);
}

bool FVoxelViewPublisher::StageImmutable(
	const FGuid& InGroup,
	AActor*& InOutActor,
	const FVector& InLocation,
	const double InScale,
	TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> InMesh,
	const int32 InTerrainStage,
	const bool bInTransition)
{
	const FPendingGroup* Group = FindPendingGroup(InGroup);
	if (!InMesh || !InMesh->bValidated || !Group || Group->bCommitted || !Module.GetWorld() || !Module.GetMaterialSet() ||
		!FMath::IsFinite(InScale) || InScale <= 0.0)
	{
		return false;
	}
	for (const FVoxelRenderBatch& Batch : InMesh->Batches)
	{
		if (!Module.GetMaterialSet()->FindMaterial(Batch.Group, Batch.Bank, Batch.MaterialVariant)) return false;
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
	OwnerActors.FindOrAdd(Group->Key).Add(TWeakObjectPtr<AActor>(InOutActor));
	Entry.bTransition = bInTransition;
	Entry.bDirty |= Entry.Source != InMesh || Entry.Location != InLocation || Entry.Scale != InScale;
	Entry.Location = InLocation;
	Entry.Scale = InScale;
	Entry.Bounds = InMesh->LocalBounds;
	Entry.Source = MoveTemp(InMesh);
	return true;
}

bool FVoxelViewPublisher::SetCoverage(const FGuid& InGroup, AActor* InActor, TArray<FBox> InWorldCellBoxes,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes)
{
	bool bSuccess = false;
	auto Work = SetCoverageSteps(InGroup, InActor, MoveTemp(InWorldCellBoxes), MoveTemp(InCommonBoxes), bSuccess);
	while (!Work.IsComplete())
	{
		Work.Resume();
	}
	return bSuccess;
}

FVoxelGameThreadContinuation FVoxelViewPublisher::SetCoverageSteps(
	const FGuid& InGroup, AActor* InActor, TArray<FBox> InWorldCellBoxes, TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, bool& OutSuccess)
{
	OutSuccess = false;
	const FPendingGroup* Group = FindPendingGroup(InGroup);
	const TWeakObjectPtr<AActor> ActorKey(InActor);
	FEntry* Entry = Entries.Find(ActorKey);
	if (!Group || Group->bCommitted || !Entry || Entry->GroupKey != Group->Key)
	{
		co_return;
	}
	const FVoxelPublishGroupKey GroupKey = Group->Key;
	TArray<FBox> WorldExclusions;
	co_await VoxelGameThread::AppendSteps(WorldExclusions, InWorldCellBoxes);
	if (InCommonBoxes)
	{
		co_await VoxelGameThread::AppendSteps(WorldExclusions, *InCommonBoxes);
	}
	Entry = Entries.Find(ActorKey);
	if (!Entry)
	{
		co_return;
	}
	OwnerActors.FindOrAdd(GroupKey).Add(ActorKey);
	Entry->DesiredWorldExclusions = MoveTemp(WorldExclusions);
	Entry->bHasDesiredExclusions = true;
	auto EqualBoxes = [](TConstArrayView<FBox> Left, TConstArrayView<FBox> Right, bool& OutEqual) -> FVoxelGameThreadContinuation
	{
		OutEqual = Left.Num() == Right.Num();
		for (int32 Index = 0; OutEqual && Index < Left.Num(); ++Index)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			OutEqual = Left[Index] == Right[Index];
		}
	};
	{
		const FUpdate* Latest = nullptr;
		if (const int32* Index = PendingUpdateIndices.Find(ActorKey))
			Latest = &PendingUpdates[*Index];
		else if (PendingPlan)
		{
			if (const int32* FrozenIndex = PendingPlan->UpdateIndices.Find(ActorKey))
				if (PendingPlan->Updates[*FrozenIndex].Source) Latest = &PendingPlan->Updates[*FrozenIndex];
		}
		if (!Latest)
		{
			uint64 LatestSerial = 0;
			if (const auto* Generations = ActorUpdateGenerations.Find(ActorKey))
				for (const auto& Generation : *Generations)
				{
					co_await FVoxelGameThreadContinuation::Yield();
					const auto& Publication = Generation.Publication;
					const auto& Update = Publication->Updates[Generation.UpdateIndex];
					if (Publication->bCanceled || Publication->bFinished || !Update.Source || Publication->Serial <= LatestSerial) continue;
					LatestSerial = Publication->Serial;
					Latest = &Update;
				}
		}
		if (Latest)
		{
			bool bSameCommon = Latest->CommonBoxes == InCommonBoxes;
			if (!bSameCommon && Latest->CommonBoxes && InCommonBoxes)
			{
				co_await EqualBoxes(*Latest->CommonBoxes, *InCommonBoxes, bSameCommon);
			}
			bool bSameLocal = false;
			co_await EqualBoxes(Latest->LocalWorldBoxes, InWorldCellBoxes, bSameLocal);
			Entry = Entries.Find(ActorKey);
			if (!Entry)
			{
				co_return;
			}
			if (Latest->Source == Entry->Source && Latest->Location == Entry->Location && Latest->Scale == Entry->Scale && bSameLocal && bSameCommon)
			{
				OutSuccess = true;
				co_return;
			}
		}
		else if (!Entry->bDirty && Entry->Source == Entry->PresentedSource)
		{
			TArray<FBox> Desired;
			const FVector Origin = Entry->Location / Module.BlockSize();
			const double Step = Entry->Scale / Module.BlockSize();
			const FBox Bounds = Entry->Bounds;
			for (const FBox& WorldBox : Entry->DesiredWorldExclusions)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				const FBox Local((WorldBox.Min - Origin) / Step, (WorldBox.Max - Origin) / Step);
				if (Bounds.Intersect(Local))
					Desired.Add(Local);
			}
			co_await VoxelMeshClipper::NormalizeBoxesSteps(Desired);
			Entry = Entries.Find(ActorKey);
			if (!Entry)
			{
				co_return;
			}
			bool bSame = false;
			co_await EqualBoxes(Desired, Entry->Exclusions, bSame);
			if (bSame)
			{
				OutSuccess = true;
				co_return;
			}
		}
	}
	TArray<FBox> LocalWorldBoxes;
	TArray<FBox> PreviousExclusions;
	co_await VoxelGameThread::AppendSteps(LocalWorldBoxes, InWorldCellBoxes);
	Entry = Entries.Find(ActorKey);
	if (!Entry)
	{
		co_return;
	}
	co_await VoxelGameThread::AppendSteps(PreviousExclusions, Entry->Exclusions);
	Entry = Entries.Find(ActorKey);
	if (!Entry)
	{
		co_return;
	}
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
	Update.LocalWorldBoxes = MoveTemp(LocalWorldBoxes);
	Update.CommonBoxes = MoveTemp(InCommonBoxes);
	Update.Bounds = Entry->Bounds;
	Update.PreviousExclusions = MoveTemp(PreviousExclusions);
	Update.bSourceDirty = Entry->bDirty;
	OutSuccess = true;
}

bool FVoxelViewPublisher::SetSharedCoverage(const FGuid& InGroup, AActor* InActor,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes)
{
	bool bSuccess = false;
	auto Work = SetSharedCoverageSteps(InGroup, InActor, MoveTemp(InCommonBoxes), bSuccess);
	while (!Work.IsComplete())
	{
		Work.Resume();
	}
	return bSuccess;
}

FVoxelGameThreadContinuation FVoxelViewPublisher::SetSharedCoverageSteps(
	const FGuid& InGroup, AActor* InActor, TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, bool& OutSuccess)
{
	OutSuccess = false;
	const TWeakObjectPtr<AActor> ActorKey(InActor);
	const FEntry* Entry = Entries.Find(ActorKey);
	if (!Entry || !Entry->bPresented || !Entry->PresentedSource)
	{
		co_return;
	}
	// 远景拓扑等待期间保留已提交的层间裁剪，仅更新与 Fine 共用的覆盖。
	TArray<FBox> LocalBoxes;
	co_await VoxelGameThread::AppendSteps(LocalBoxes, Entry->LocalWorldBoxes);
	co_await SetCoverageSteps(InGroup, InActor, MoveTemp(LocalBoxes), MoveTemp(InCommonBoxes), OutSuccess);
	Entry = Entries.Find(ActorKey);
	if (!OutSuccess || !Entry)
	{
		OutSuccess = false;
		co_return;
	}
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
}

bool FVoxelViewPublisher::SetHidden(const FGuid& InGroup, AActor* InActor, const bool bInHidden)
{
	const FPendingGroup* Group = FindPendingGroup(InGroup);
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
	else if (InActor->IsHidden() == bInHidden && !ActorVisibilityGenerations.Contains(Key) &&
		(!PendingPlan || !PendingPlan->Visibility.Contains(Key)))
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

void FVoxelViewPublisher::SetValidation(const FGuid& InGroup, TFunction<bool()> InValidation)
{
	if (auto* Group = FindPendingGroup(InGroup)) Group->Validation = MakeShared<TFunction<bool()>>(MoveTemp(InValidation));
}

TArray<FVoxelPublishGroupKey> FVoxelViewPublisher::TakeRetryOwners()
{
	TArray<FVoxelPublishGroupKey> Result = MoveTemp(RetryOwners);
	RetryOwners.Reset();
	return Result;
}

bool FVoxelViewPublisher::DiscardUnchangedGroup(const FGuid& InGroup)
{
	const FPendingGroup* Group = PendingGroups.Find(InGroup);
	if (!Group || Group->bCommitted || Group->bOwnershipChanged ||
		OwnerSerialQueues.Contains(Group->Key) ||
		(PendingPlan && PendingPlan->GroupIds.Contains(Group->Key))) return false;
	if (const auto* Actors = OwnerActors.Find(Group->Key))
		for (const auto& Actor : *Actors)
			if (PendingUpdateIndices.Contains(Actor) || PendingVisibility.Contains(Actor)) return false;
	PendingGroupIds.Remove(Group->Key);
	PendingGroups.Remove(InGroup);
	return true;
}

bool FVoxelViewPublisher::CommitGroup(const FGuid& InGroup)
{
	FPendingGroup* Group = FindPendingGroup(InGroup);
	if (!Group || Group->bCommitted)
	{
		return false;
	}
	Group->bCommitted = true;
	return true;
}

bool FVoxelViewPublisher::SetOwnershipChanged(const FGuid& InGroup, const bool bInChanged)
{
	FPendingGroup* Group = FindPendingGroup(InGroup);
	if (!Group || Group->bCommitted) return false;
	Group->bOwnershipChanged = bInChanged;
	return true;
}

void FVoxelViewPublisher::SetObservers(const TConstArrayView<FVector> InObservers)
{
	if (Observers.Num() == InObservers.Num() && Algo::Compare(Observers, InObservers)) return;
	Observers = TArray<FVector>(InObservers);
	InvalidatePriorities();
}

void FVoxelViewPublisher::InvalidatePriorities()
{
	++PriorityRevision;
	bPrioritiesDirty = true;
	if (!PriorityPlan)
		for (const auto& Publication : Publications)
			for (FGroup& Group : Publication->Groups) Group.bPriorityReady = false;
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
		const FBox Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Key, Module.BlockSize(), Module.GetViewSettings().Lod);
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
	return !InGroup.bCommitted;
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
			const int32 Radius = InStage == 0 ? Source.View.FineRadiusCells
				: InStage == 1 ? Source.View.VoxelProxyRadiusCells
				: InStage == 2 ? Source.View.SurfaceRadiusCells : Source.View.MacroRadiusCells;
			if (Distance <= Radius) Priority = FMath::Min(Priority, Source.GetSchedulingPriority());
			Result.DistanceScore = FMath::Min(Result.DistanceScore, Distance);
			if (!Source.Has(EVoxelStreamingCapability::LocalRefinement) &&
				VoxelViewLod::MovementCriticalFineRadius(Source) > 0 &&
				Distance <= VoxelViewLod::MovementCriticalFineRadius(Source))
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
		const FBox Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Key, Module.BlockSize(), Module.GetViewSettings().Lod);
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
	++Publication->GroupRevision;
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
	if (PendingGroups.IsEmpty()) return;
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
			Footprint.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Footprint.Key, Module.BlockSize(), Module.GetViewSettings().Lod);
		}
		KeyIndices.Add(Footprint.Key, Footprints.Num() - 1);
	}
	for (const auto& Owner : KeyIndices)
	{
		const auto* Actors = OwnerActors.Find(Owner.Key);
		if (!Actors) continue;
		for (const auto& Actor : *Actors)
		{
		const auto* Found = Entries.Find(Actor);
		if (!Found) continue;
		const FEntry& Entry = *Found;
		const int32* Index = KeyIndices.Find(Entry.GroupKey);
		if (!Index) continue;
		const FBox Desired = Entry.Bounds.IsValid
			? FBox(Entry.Location + Entry.Bounds.Min * Entry.Scale,
				Entry.Location + Entry.Bounds.Max * Entry.Scale)
			: FBox(ForceInit);
		Footprints[*Index].Bounds += FVoxelPublishGroupPlanner::PublicationBounds(
			Entry.GroupKey, Module.BlockSize(), Entry.PresentedWorldBounds, Desired, Entry.bTransition, Module.GetViewSettings().Lod);
	}
	}
	TArray<int32> GroupIndices;
	FVoxelPublishGroupPlanner::Build(Footprints, GroupIndices);
	TSet<int32> BlockedComponents;
	for (const auto& Pair : PendingGroups)
	{
		if (!Pair.Value.bCommitted || (OwnerSerialQueues.Contains(Pair.Value.Key) && OwnerSerialQueues.FindChecked(Pair.Value.Key).Num() >= 2))
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
		Publication->Validations.Add(Key, It.Value().Validation);
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
			Module.GetViewSettings().Budget.MaxPublishGroupsPerFrame);
		NextGroupLogSeconds = GroupNowSeconds + 5.0;
	}
#endif
	Publication->Serial = ++BatchSerial;
	Publication->PreparedMeshes.SetNum(Publication->Updates.Num());
	Publication->bCoveragePrepared = Publication->Updates.IsEmpty();
	Publication->bGroupsPrepared = false;
	Publications.Add(Publication);
	for (const auto& Group : Publication->Groups)
		for (const auto& Key : Group.Keys) SetOwnerActive(Key, Publication->Serial, true);
	bFirstOwnersDirty = true;
	++PriorityRevision;
	bPrioritiesDirty = true;
	bActiveBusyKeysDirty = true;
}


void FVoxelViewPublisher::AdvancePendingPlan()
{
	if (!PendingPlan)
	{
		if (PendingGroups.IsEmpty()) return;
		PendingPlan = MakeUnique<FPendingPlan>();
		// Freeze this cohort by moving storage. New inputs can accumulate independently.
		PendingPlan->Groups = MoveTemp(PendingGroups);
		PendingPlan->GroupIds = MoveTemp(PendingGroupIds);
		PendingPlan->Updates = MoveTemp(PendingUpdates);
		PendingPlan->UpdateIndices = MoveTemp(PendingUpdateIndices);
		PendingPlan->Visibility = MoveTemp(PendingVisibility);
		PendingPlan->Publication->Serial = ++BatchSerial;
		return;
	}
	auto& Plan = *PendingPlan;
	const auto Next = [&Plan] { ++Plan.Phase; Plan.Cursor = 0; };
	if (Plan.Phase == 0)
	{
		if (Plan.Cursor >= Plan.Groups.GetMaxIndex()) { Next(); return; }
		const auto Id = FSetElementId::FromInteger(Plan.Cursor++);
		if (!Plan.Groups.IsValidId(Id)) return;
		const auto& Pair = Plan.Groups.Get(Id);
		FVoxelPublishFootprint Footprint;
		Footprint.Key = Pair.Value.Key;
		Footprint.bTransition = Footprint.Key.Representation == 4 || Footprint.Key.Representation == 5;
		if (!Footprint.bTransition)
			Footprint.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Footprint.Key, Module.BlockSize(), Module.GetViewSettings().Lod);
		if (const auto* Actors = OwnerActors.Find(Footprint.Key))
			for (const auto& Actor : *Actors)
			{
				Plan.ActorKeys.Add(Actor, Footprint.Key);
				const auto* Entry = Entries.Find(Actor);
				if (!Entry) continue;
				const int32* UpdateIndex = Plan.UpdateIndices.Find(Actor);
				const FUpdate* Update = UpdateIndex ? &Plan.Updates[*UpdateIndex] : nullptr;
				const FBox Bounds = Update ? Update->Bounds : Entry->Bounds;
				const FVector Location = Update ? Update->Location : Entry->Location;
				const double Scale = Update ? Update->Scale : Entry->Scale;
				const FBox Desired = Bounds.IsValid ? FBox(Location + Bounds.Min * Scale, Location + Bounds.Max * Scale) : FBox(ForceInit);
				Footprint.Bounds += FVoxelPublishGroupPlanner::PublicationBounds(Footprint.Key, Module.BlockSize(),
					Entry->PresentedWorldBounds, Desired, Entry->bTransition, Module.GetViewSettings().Lod);
			}
		Plan.KeyIndices.Add(Footprint.Key, Plan.Footprints.Num());
		Plan.Footprints.Add(Footprint);
		Plan.Handles.Add(Pair.Key);
		if (!Plan.Graph.Add(Footprint))
		{
			Plan.bCapacityFailure = true;
			UE_LOG(LogTemp, Warning, TEXT("Publication footprint capacity exceeded; retaining previous presentation"));
		}
		return;
	}
	if (Plan.Phase == 1)
	{
		if (Plan.bCapacityFailure || Plan.Graph.Advance(1)) Next();
		return;
	}
	if (Plan.Phase == 2)
	{
		if (!Plan.Handles.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const int32 Index = Plan.Cursor++;
		if (Plan.bCapacityFailure) return;
		const auto& Group = Plan.Groups.FindChecked(Plan.Handles[Index]);
		const auto* Generations = OwnerSerialQueues.Find(Group.Key);
		// A committed frozen generation retains its immutable inputs when a newer
		// generation arrives. Owner ordering keeps the latter behind this one.
		if (!Group.bCommitted || (Generations && Generations->Num() >= 2))
			Plan.BlockedComponents.Add(Plan.Graph.GetGroupIndices()[Index]);
		return;
	}
	if (Plan.Phase == 3)
	{
		if (!Plan.Handles.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const int32 Index = Plan.Cursor++;
		const FGuid Handle = Plan.Handles[Index];
		const auto& Group = Plan.Groups.FindChecked(Handle);
		if (Plan.bCapacityFailure || Plan.BlockedComponents.Contains(Plan.Graph.GetGroupIndices()[Index]))
		{
			// A newer pending input wins. The frozen cohort never overwrites it.
			if (!PendingGroupIds.Contains(Group.Key))
			{
				PendingGroupIds.Add(Group.Key, Handle);
				PendingGroups.Add(Handle, Group);
			}
			return;
		}
		const int32 Component = Plan.Graph.GetGroupIndices()[Index];
		int32* Selected = Plan.SelectedComponents.Find(Component);
		if (!Selected)
		{
			const int32 GroupIndex = Plan.Publication->Groups.AddDefaulted();
			Selected = &Plan.SelectedComponents.Add(Component, GroupIndex);
			Plan.Publication->GroupOrder.Add(GroupIndex);
		}
		FGroup& Target = Plan.Publication->Groups[*Selected];
		Target.Keys.Add(Group.Key);
		Target.Bounds += Plan.Footprints[Index].Bounds;
		if (Plan.Footprints[Index].Bounds.IsValid) Target.ChangedBounds.Add(Plan.Footprints[Index].Bounds);
		Target.bTransition |= Plan.Footprints[Index].bTransition;
		Target.TerrainStage = FMath::Min(Target.TerrainStage, Group.TerrainStage);
		Plan.SelectedKeys.Add(Group.Key, *Selected);
		Plan.Publication->Callbacks.Add(Group.Key, Group.Callback);
		Plan.Publication->Validations.Add(Group.Key, Group.Validation);
		Plan.Publication->TerrainStages.Add(Group.Key, Group.TerrainStage);
		if (Group.DeferredPriorityBounds) Plan.Publication->DeferredPriorityBounds.Add(Group.Key, Group.DeferredPriorityBounds);
		if (Group.bOwnershipChanged) Plan.Publication->OwnershipChanges.Add(Group.Key);
		return;
	}
	if (Plan.Phase == 4)
	{
		if (!Plan.Updates.IsValidIndex(Plan.Cursor)) { Next(); return; }
		FUpdate Update = MoveTemp(Plan.Updates[Plan.Cursor++]);
		const auto* Key = Plan.ActorKeys.Find(Update.Actor);
		const int32* GroupIndex = Key ? Plan.SelectedKeys.Find(*Key) : nullptr;
		if (GroupIndex)
		{
			Update.GroupIndex = *GroupIndex;
			const int32 Index = Plan.Publication->Updates.Add(MoveTemp(Update));
			Plan.Publication->PreparedMeshes.Add(nullptr);
			IndexPublicationUpdate(Plan.Publication, Index);
			Plan.Publication->Groups[*GroupIndex].UpdateIndices.Add(Index);
		}
		else if (Key && !PendingUpdateIndices.Contains(Update.Actor))
		{
			PendingUpdateIndices.Add(Update.Actor, PendingUpdates.Num());
			PendingUpdates.Add(MoveTemp(Update));
		}
		return;
	}
	if (Plan.Phase == 5)
	{
		if (Plan.Cursor >= Plan.Visibility.GetMaxIndex()) { Next(); return; }
		const auto Id = FSetElementId::FromInteger(Plan.Cursor++);
		if (!Plan.Visibility.IsValidId(Id)) return;
		const auto& Pair = Plan.Visibility.Get(Id);
		const auto* Key = Plan.ActorKeys.Find(Pair.Key);
		const int32* GroupIndex = Key ? Plan.SelectedKeys.Find(*Key) : nullptr;
		if (GroupIndex)
		{
			Plan.Publication->Visibility.Add(Pair);
			ActorVisibilityGenerations.FindOrAdd(Pair.Key).Add({Plan.Publication, Pair.Value});
			Plan.Publication->Groups[*GroupIndex].VisibilityActors.Add(Pair.Key);
		}
		else if (Key && !PendingVisibility.Contains(Pair.Key)) PendingVisibility.Add(Pair);
		return;
	}
	if (Plan.Phase == 6)
	{
		if (!Plan.Footprints.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const auto Key = Plan.Footprints[Plan.Cursor++].Key;
		if (Plan.SelectedKeys.Contains(Key)) SetOwnerActive(Key, Plan.Publication->Serial, true);
		return;
	}
	if (!Plan.Publication->Groups.IsEmpty())
	{
		Plan.Publication->bCoveragePrepared = Plan.Publication->Updates.IsEmpty();
		Plan.Publication->bGroupsPrepared = false;
		Publications.Add(Plan.Publication);
		++PriorityRevision;
		bFirstOwnersDirty = bPrioritiesDirty = bActiveBusyKeysDirty = true;
	}
	PendingPlan.Reset();
}

void FVoxelViewPublisher::AdvancePreparedPlan(const FPublicationRef& Publication)
{
	if (!Publication->Plan) { Publication->Plan = MakeUnique<FPreparedPlan>(); return; }
	auto& Plan = *Publication->Plan;
	auto& Footprints = Plan.Footprints;
	auto& ActorIndices = Plan.ActorIndices;
	auto& Updates = Plan.Updates;
	constexpr bool bInPreparedCoverage = true;
	const auto Next = [&Plan] { ++Plan.Phase; Plan.Cursor = 0; };
	if (Plan.Phase == 0)
	{
		if (!Publication->Updates.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const FUpdate& Update = Publication->Updates[Plan.Cursor++];
		Updates.Add(Update.Actor, &Update);
		return;
	}
	auto AddActor = [this, &Footprints, &ActorIndices, &Updates, &Publication, &Plan](
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
			if (const auto* Generations = ActorVisibilityGenerations.Find(Actor))
				for (const auto& Generation : *Generations)
				{
					const auto& Older = Generation.Publication;
					if (Older->bCanceled || Older->bFinished || Older->Serial >= Publication->Serial || Older->Serial <= PreviousVisibilitySerial) continue;
					PreviousHidden = Generation.bHidden;
					PreviousVisibilitySerial = Older->Serial;
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
					Entry->GroupKey, Module.BlockSize(), Entry->PresentedWorldBounds, Desired, Entry->bTransition, Module.GetViewSettings().Lod);
			else if (bLogicalChange && !Entry->bTransition)
				Footprint.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Entry->GroupKey, Module.BlockSize(), Module.GetViewSettings().Lod);
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
		Plan.ActorKeys.Add(Footprints[FirstFootprint].Key);
	};
	if (Plan.Phase == 1)
	{
		if (!Publication->Updates.IsValidIndex(Plan.Cursor)) { Next(); return; }
		AddActor(Publication->Updates[Plan.Cursor++].Actor);
		return;
	}
	if (Plan.Phase == 2)
	{
		if (Plan.Cursor >= Publication->Visibility.GetMaxIndex()) { Next(); return; }
		const auto Id = FSetElementId::FromInteger(Plan.Cursor++);
		if (Publication->Visibility.IsValidId(Id)) AddActor(Publication->Visibility.Get(Id).Key);
		return;
	}
	if (Plan.Phase == 3)
	{
		if (Plan.Cursor >= Publication->Callbacks.GetMaxIndex()) { Next(); return; }
		const auto Id = FSetElementId::FromInteger(Plan.Cursor++);
		if (!Publication->Callbacks.IsValidId(Id)) return;
		const auto& Key = Publication->Callbacks.Get(Id).Key;
		if (Plan.ActorKeys.Contains(Key)) return;
		auto& Footprint = Footprints.AddDefaulted_GetRef();
		Footprint.Key = Key;
		const bool bLogicalChange = Publication->OwnershipChanges.Contains(Key);
		Footprint.bTransition = bLogicalChange && (Key.Representation == 4 || Key.Representation == 5);
		if (bLogicalChange && !Footprint.bTransition)
			Footprint.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Key, Module.BlockSize(), Module.GetViewSettings().Lod);
		return;
	}
	if (Plan.Phase == 4)
	{
		if (!Footprints.IsValidIndex(Plan.Cursor)) { Next(); return; }
		if (!Plan.Graph.Add(Footprints[Plan.Cursor++]))
		{
			DiscardPublication(Publication);
			bRetry = true;
			UE_LOG(LogTemp, Warning, TEXT("Prepared publication footprint capacity exceeded; retaining previous presentation"));
		}
		return;
	}
	if (Plan.Phase == 5) { if (Plan.Graph.Advance(1)) Next(); return; }
	if (Plan.Phase == 6)
	{
		if (!Footprints.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const int32 Index = Plan.Cursor++;
		const int32 GroupIndex = Plan.Graph.GetGroupIndices()[Index];
		// Ordinals are allocated at the first footprint of each connected component.
		if (GroupIndex == Plan.Groups.Num()) Plan.Groups.AddDefaulted();
		FGroup& Group = Plan.Groups[GroupIndex];
		const auto& Footprint = Footprints[Index];
		if (!Plan.GroupKeys.Contains(Footprint.Key))
		{
			Plan.GroupKeys.Add(Footprint.Key);
			Group.Keys.Add(Footprint.Key);
			if (const int32* Stage = Publication->TerrainStages.Find(Footprint.Key)) Group.TerrainStage = FMath::Min(Group.TerrainStage, *Stage);
		}
		Group.Bounds += Footprint.Bounds;
		if (Footprint.Bounds.IsValid) Group.ChangedBounds.Add(Footprint.Bounds);
		Group.bTransition |= Footprint.bTransition;
		return;
	}
	if (Plan.Phase == 7)
	{
		if (!Publication->Updates.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const int32 Index = Plan.Cursor++;
		FUpdate& Update = Publication->Updates[Index];
		Update.GroupIndex = Plan.Graph.GetGroupIndices()[ActorIndices.FindChecked(Update.Actor)];
		FGroup& Group = Plan.Groups[Update.GroupIndex];
		Group.UpdateIndices.Add(Index);
		if (Update.TerrainStage != INDEX_NONE) Group.TerrainStage = FMath::Min(Group.TerrainStage, Update.TerrainStage);
		return;
	}
	if (Plan.Phase == 8)
	{
		if (Plan.Cursor >= Publication->Visibility.GetMaxIndex()) { Next(); return; }
		const auto Id = FSetElementId::FromInteger(Plan.Cursor++);
		if (!Publication->Visibility.IsValidId(Id)) return;
		const auto Actor = Publication->Visibility.Get(Id).Key;
		Plan.Groups[Plan.Graph.GetGroupIndices()[ActorIndices.FindChecked(Actor)]].VisibilityActors.Add(Actor);
		return;
	}
	if (Plan.Phase == 9)
	{
		if (!Plan.Groups.IsValidIndex(Plan.Cursor))
		{
			++Publication->GroupRevision;
			Publication->Groups = MoveTemp(Plan.Groups);
			Publication->GroupOrder.Reset();
			Next();
			return;
		}
		FGroup& Group = Plan.Groups[Plan.Cursor++];
		Group.Priority = GroupPriority(Group);
		return;
	}
	if (Plan.Phase == 10)
	{
		if (!Publication->Groups.IsValidIndex(Plan.Cursor))
		{
			Publication->bGroupsPrepared = true;
			Publication->bBuildOrderInitialized = Publication->Updates.IsEmpty();
			Publication->bBuildOrderDirty = false;
			Next();
			return;
		}
		Publication->GroupOrder.Add(Plan.Cursor++);
		return;
	}
	if (Plan.Phase == 11)
	{
		if (Publication->Groups.IsValidIndex(Plan.Cursor)) { QueueReadyGroup(Publication, Plan.Cursor++); return; }
		Publication->Plan.Reset();
		++PriorityRevision;
		bPrioritiesDirty = true;
	}

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
			if (const auto* Generations = ActorVisibilityGenerations.Find(Actor))
				for (const auto& Generation : *Generations)
				{
					const auto& Older = Generation.Publication;
					if (Older->bCanceled || Older->bFinished || Older->Serial >= Publication->Serial || Older->Serial <= PreviousVisibilitySerial) continue;
					PreviousHidden = Generation.bHidden;
					PreviousVisibilitySerial = Older->Serial;
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
					Entry->GroupKey, Module.BlockSize(), Entry->PresentedWorldBounds, Desired, Entry->bTransition, Module.GetViewSettings().Lod);
			else if (bLogicalChange && !Entry->bTransition)
				Footprint.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Entry->GroupKey, Module.BlockSize(), Module.GetViewSettings().Lod);
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
			Footprint.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Pair.Key, Module.BlockSize(), Module.GetViewSettings().Lod);
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
	for (const auto& Group : Publication->Groups)
		for (const auto& Key : Group.Keys)
			if (const auto* Queue = OwnerSerialQueues.Find(Key))
				for (const uint64 Serial : *Queue) Generations.FindOrAdd(Key) += Serial < Publication->Serial ? 1 : 0;
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
			SetOwnerActive(Key, Publication->Serial, false);
			MergePriority(OutPriorities.FindOrAdd(Key), Priority);
		}
#if !UE_BUILD_SHIPPING
		static double NextSaturatedPointLog = 0.0;
		if (FPlatformTime::Seconds() >= NextSaturatedPointLog && Group.Keys.ContainsByPredicate([this](const FVoxelPublishGroupKey& Key)
		{
			if (Key.Representation != 0) return false;
			for (const FVector& Observer : Observers)
			{
				const FVector Cell = Observer / Module.BlockSize();
				if (FVoxelPublishGroupPlanner::OwnershipBounds(Key, Module.BlockSize(), Module.GetViewSettings().Lod).IsInside(Cell)) return true;
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
			if (!Pending.Validation) Pending.Validation = Publication->Validations.FindRef(Key);
			Publication->Validations.Remove(Key);
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

void FVoxelViewPublisher::AdvancePriorities()
{
	if (!PriorityPlan)
	{
		PriorityPlan = MakeUnique<FPriorityPlan>();
		PriorityPlan->Revision = PriorityRevision;
		return;
	}
	auto& Plan = *PriorityPlan;
	const auto Next = [&Plan] { ++Plan.Phase; Plan.Cursor = 0; Plan.InnerCursor = 0; Plan.PairCursor = 0; };
	const auto IncludeDeferred = [&Plan](const FPriorityBoundsPtr& Bounds)
	{
		if (Bounds && !Plan.SeenDeferred.Contains(Bounds.Get()))
		{
			Plan.SeenDeferred.Add(Bounds.Get());
			Plan.Deferred.Add(Bounds);
		}
	};
	if (Plan.Phase == 0)
	{
		if (!Publications.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const auto Publication = Publications[Plan.Cursor++];
		if (Publication->bCanceled || Publication->bFinished) return;
		int32 First = 0, Last = Plan.Publications.Num();
		while (First < Last)
		{
			const int32 Middle = (First + Last) / 2;
			if (Plan.Publications[Middle]->Serial > Publication->Serial) First = Middle + 1;
			else Last = Middle;
		}
		Plan.Publications.Insert(Publication, First);
		return;
	}
	if (Plan.Phase == 1)
	{
		if (Plan.Cursor >= PendingGroups.GetMaxIndex()) { Next(); return; }
		const auto Id = FSetElementId::FromInteger(Plan.Cursor++);
		if (PendingGroups.IsValidId(Id)) IncludeDeferred(PendingGroups.Get(Id).Value.DeferredPriorityBounds);
		return;
	}
	if (Plan.Phase == 2)
	{
		if (!Plan.Publications.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const auto& Publication = Plan.Publications[Plan.Cursor];
		if (Publication->bGroupsPrepared || Plan.InnerCursor >= Publication->DeferredPriorityBounds.GetMaxIndex())
		{
			++Plan.Cursor;
			Plan.InnerCursor = 0;
			return;
		}
		const auto Id = FSetElementId::FromInteger(Plan.InnerCursor++);
		if (Publication->DeferredPriorityBounds.IsValidId(Id)) IncludeDeferred(Publication->DeferredPriorityBounds.Get(Id).Value);
		return;
	}
	if (Plan.Phase == 3)
	{
		if (!Plan.Publications.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const auto& Publication = Plan.Publications[Plan.Cursor];
		if (!Publication->Groups.IsValidIndex(Plan.InnerCursor))
		{
			++Plan.Cursor;
			Plan.InnerCursor = 0;
			return;
		}
		const int32 Index = Plan.InnerCursor++;
		FGroup& Group = Publication->Groups[Index];
		if (Group.bCommitted) return;
		Group.Priority = GroupPriority(Group);
		Plan.SpatialIndex.Add(Plan.Groups.Num(), Group.Bounds);
		Group.bPriorityReady = false;
		Plan.Groups.Add({Publication, Index, Publication->GroupRevision});
		return;
	}
	if (Plan.Phase == 4)
	{
		if (!Plan.Deferred.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const auto& Deferred = Plan.Deferred[Plan.Cursor];
		if (!Plan.bSpatialQueryActive)
		{
			Plan.SpatialDemand = {};
			Plan.SpatialDemand.ChangedBounds = Deferred->Bounds;
			Plan.SpatialDemand.bTransition = Deferred->bTransition;
			for (const FBox& Bounds : Deferred->Bounds) if (Bounds.IsValid) Plan.SpatialDemand.Bounds += Bounds;
			Plan.SpatialIndex.BeginQuery(Plan.SpatialDemand.Bounds, Plan.SpatialQuery);
			Plan.bSpatialQueryActive = true;
			return;
		}
		int32 Candidate = INDEX_NONE;
		if (Plan.SpatialIndex.AdvanceQuery(Plan.SpatialQuery, Candidate))
		{ ++Plan.Cursor; Plan.bSpatialQueryActive = false; return; }
		if (Candidate == INDEX_NONE) return;
		const auto& Target = Plan.Groups[Candidate];
		if (!Target.IsCurrent()) return;
		if (Target.Key->Serial >= Deferred->Serial) return;
		FGroup& Group = Target.Key->Groups[Target.Value];
		if (GroupsInteract(Plan.SpatialDemand, Group)) MergePriority(Group.Priority, BoundsPriority(Deferred->Bounds, Deferred->TerrainStage));
		return;
	}

	if (Plan.Phase == 5)
	{
		if (!Plan.Groups.IsValidIndex(Plan.Cursor))
		{
			for (const auto& Resolved : Plan.Groups)
				if (Resolved.IsCurrent()) Resolved.Key->Groups[Resolved.Value].bPriorityReady = true;
			Plan.Phase = 8;
			Plan.Cursor = 0;
			return;
		}
		const auto& Source = Plan.Groups[Plan.Cursor];
		if (!Source.IsCurrent()) { ++Plan.Cursor; Plan.InnerCursor = 0; Plan.Phase = 5; Plan.bSpatialQueryActive = false; return; }
		FGroup& Group = Source.Key->Groups[Source.Value];
		if (!Group.Keys.IsValidIndex(Plan.InnerCursor)) { Plan.Phase = 6; Plan.InnerCursor = 0; return; }
		const auto& Key = Group.Keys[Plan.InnerCursor++];
		if (const auto* Priority = Plan.Priorities.Find(Key)) MergePriority(Group.Priority, *Priority);
		return;
	}
	if (Plan.Phase == 6)
	{
		const auto& Source = Plan.Groups[Plan.Cursor];
		if (!Source.IsCurrent()) { ++Plan.Cursor; Plan.InnerCursor = 0; Plan.Phase = 5; Plan.bSpatialQueryActive = false; return; }
		FGroup& Group = Source.Key->Groups[Source.Value];
		if (!Group.Keys.IsValidIndex(Plan.InnerCursor)) { Plan.Phase = 7; Plan.InnerCursor = 0; return; }
		if (Source.Key->bGroupsPrepared) MergePriority(Plan.Priorities.FindOrAdd(Group.Keys[Plan.InnerCursor]), Group.Priority);
		++Plan.InnerCursor;
		return;
	}
	if (Plan.Phase == 7)
	{
		const auto& Source = Plan.Groups[Plan.Cursor];
		if (!Source.IsCurrent()) { ++Plan.Cursor; Plan.InnerCursor = 0; Plan.Phase = 5; Plan.bSpatialQueryActive = false; return; }
		FGroup& Group = Source.Key->Groups[Source.Value];
		if (!Source.Key->bGroupsPrepared || Plan.Publications.Last()->Serial >= Source.Key->Serial)
		{ ++Plan.Cursor; Plan.InnerCursor = 0; Plan.Phase = 5; return; }
		if (!Plan.bSpatialQueryActive)
		{
			Plan.SpatialIndex.BeginQuery(Group.Bounds, Plan.SpatialQuery);
			Plan.bSpatialQueryActive = true;
			return;
		}
		int32 Candidate = INDEX_NONE;
		if (Plan.SpatialIndex.AdvanceQuery(Plan.SpatialQuery, Candidate))
		{
			++Plan.Cursor;
			Plan.InnerCursor = 0;
			Plan.Phase = 5;
			Plan.bSpatialQueryActive = false;
			return;
		}
		if (Candidate == INDEX_NONE || !Source.Key->bGroupsPrepared) return;
		const auto& Target = Plan.Groups[Candidate];
		if (!Target.IsCurrent()) return;
		if (Target.Key->Serial >= Source.Key->Serial) return;
		FGroup& Older = Target.Key->Groups[Target.Value];
		if (GroupsInteract(Group, Older)) MergePriority(Older.Priority, Group.Priority);
	}

	if (Plan.Phase == 8)
	{
		if (Plan.Publications.IsValidIndex(Plan.Cursor))
		{
			const auto& Publication = Plan.Publications[Plan.Cursor++];
			Publication->bBuildOrderDirty = true;
			Publication->BuildHeapifyIndex = INDEX_NONE;
			return;
		}
		// Scheduler has a fixed admission cap; preserve admitted tasks and their age.
		RefreshTaskPriorities();
		bPrioritiesDirty = Plan.Revision != PriorityRevision;
		PriorityPlan.Reset();
	}
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

void FVoxelViewPublisher::AdmitCoveragePreparation(const FPublicationRef& Publication, const double InDeadline)
{
	if (Publication->bCoveragePrepared || Publication->bCoveragePreparing)
	{
		return;
	}

	TMap<TWeakObjectPtr<AActor>, TPair<uint64, const FUpdate*>> Projected;
	TSet<TWeakObjectPtr<AActor>> Waiting;

	const int32 BatchLimit = FMath::Clamp(Module.GetViewSettings().Budget.CoveragePreparationBatchSize, 1, 128);

	TArray<FCoveragePreparationInput> Inputs;
	Inputs.Reserve(FMath::Min(BatchLimit, Publication->Updates.Num() - Publication->CoverageIndex));

	uint64 InputBytes = 0;
	for (int32 Index = Publication->CoverageIndex; Index < Publication->Updates.Num() && Inputs.Num() < BatchLimit; ++Index)
	{
		if (FPlatformTime::Seconds() >= InDeadline)
		{
			break;
		}

		FUpdate& Update = Publication->Updates[Index];
		if (const auto* Generations = ActorUpdateGenerations.Find(Update.Actor))
		{
			for (const auto& Generation : *Generations)
			{
				const auto& Older = Generation.Publication;
				if (Older->bCanceled || Older->bFinished || Older->Serial >= Publication->Serial)
				{
					continue;
				}

				if (!Older->bGroupsPrepared || !Older->bCoveragePrepared)
				{
					Waiting.Add(Update.Actor);
					continue;
				}

				const FUpdate& PriorUpdate = Older->Updates[Generation.UpdateIndex];
				if (!Older->Groups.IsValidIndex(PriorUpdate.GroupIndex) || Older->Groups[PriorUpdate.GroupIndex].bCommitted)
				{
					continue;
				}

				auto* Existing = Projected.Find(Update.Actor);
				if (!Existing || Existing->Key < Older->Serial)
				{
					Projected.Add(Update.Actor, {Older->Serial, &PriorUpdate});
				}
			}
		}

		if (Waiting.Contains(Update.Actor))
		{
			break;
		}

		if (!Inputs.IsEmpty() && InputBytes + Update.Exclusions.GetAllocatedSize() + Update.PreviousExclusions.GetAllocatedSize() > 4ull * 1024ull * 1024ull)
		{
			break;
		}

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
				Update.Bounds = Entry->PresentedWorldBounds.IsValid ? FBox((Entry->PresentedWorldBounds.Min - Update.Location) / Update.Scale,
																		  (Entry->PresentedWorldBounds.Max - Update.Location) / Update.Scale)
																	: FBox(ForceInit);
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
			Update.bSourceDirty =
				Previous->Value->Source != Update.Source || Previous->Value->Location != Update.Location || Previous->Value->Scale != Update.Scale;
		}
		else
		{
			Input.Previous = Entry ? Entry->Exclusions : Update.PreviousExclusions;
			if (Entry)
			{
				Update.bSourceDirty |= Entry->PresentedSource != Update.Source;
			}
		}

		Input.Common = Update.CommonBoxes;
		Input.bDirty = Update.bSourceDirty;
		InputBytes += Input.Boxes.GetAllocatedSize() + Input.Previous.GetAllocatedSize();
	}

	if (Inputs.IsEmpty())
	{
		return;
	}

	const bool bInitialUncovered =
		Inputs.ContainsByPredicate(
			[](const FCoveragePreparationInput& Input)
			{
				return !Input.bDirty || !Input.Previous.IsEmpty() || !Input.Boxes.IsEmpty() || (Input.Common && !Input.Common->IsEmpty());
			}) == false;

	if (bInitialUncovered)
	{
		const int32 End = Publication->CoverageIndex + Inputs.Num();
		for (int32 Index = Publication->CoverageIndex; Index < End; ++Index)
		{
			Publication->Updates[Index].PreviousExclusions.Reset();
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
	{
		PriorityGroups.Add(Publication->Updates[Index].GroupIndex);
	}
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
			if (Cancel.Load())
			{
				return Result;
			}

			const FCoveragePreparationInput& Input = Inputs[Index];
			TArray<FBox>& Boxes = Payload->Boxes[Index];

			const auto Include = [&Input, &Boxes](FBox Box)
			{
				Box.Min = (Box.Min - Input.Origin) / Input.Step;
				Box.Max = (Box.Max - Input.Origin) / Input.Step;
				if (Input.Bounds.Intersect(Box))
				{
					Boxes.Add(Box);
				}
			};

			for (const FBox& Box : Input.Boxes)
			{
				Include(Box);
			}
			if (Input.Common)
			{
				for (const FBox& Box : *Input.Common)
				{
					Include(Box);
				}
			}

			VoxelMeshClipper::NormalizeBoxes(Boxes);
			Payload->Unchanged[Index] = !Input.bDirty && Input.Previous == Boxes;

			if (!Input.bDirty && !Payload->Unchanged[Index])
			{
				VoxelMeshClipper::FindChangedCoverageBounds(Input.Previous, Boxes, Input.Bounds, Payload->ChangedBounds[Index], &Cancel);
			}

			if (!Input.bDirty && Payload->ChangedBounds[Index].IsEmpty())
			{
				Payload->Unchanged[Index] = true;
			}

			if (Cancel.Load())
			{
				return Result;
			}
		}

		Result.CustomPayload = Payload;
		Result.bSuccess = true;
		return Result;
	};

	const uint64 Serial = Publication->Serial;
	const int32 First = Publication->CoverageIndex;

	Request.Apply = [this, Publication, Serial, First](FVoxelTaskResult&& Result)
	{
		if (Publication->bCanceled || Serial != Publication->Serial)
		{
			return;
		}

		Publication->bCoveragePreparing = false;

		if (Result.bCanceled && Result.Stamp.WorldEpoch == WorldEpoch)
		{
			return;
		}

		if (!Result.bSuccess || Result.bCanceled || Result.Stamp.WorldEpoch != WorldEpoch)
		{
			DiscardPublication(Publication);
			bRetry = true;
			return;
		}

		const auto Payload = StaticCastSharedPtr<const FCoveragePreparationResult>(Result.CustomPayload);
		if (!Payload)
		{
			DiscardPublication(Publication);
			bRetry = true;
			return;
		}

		int32 Changed = 0;
		for (int32 Offset = 0; Offset < Payload->Boxes.Num(); ++Offset)
		{
			const int32 Index = First + Offset;
			if (!Publication->Updates.IsValidIndex(Index))
			{
				DiscardPublication(Publication);
				bRetry = true;
				return;
			}

			FUpdate& Update = Publication->Updates[Index];
			Update.Exclusions = Payload->Boxes[Offset];
			Update.bUnchanged = Payload->Unchanged[Offset] != 0;
			Update.ChangedBounds = Payload->ChangedBounds[Offset];

			for (FBox& Bounds : Update.ChangedBounds)
			{
				Bounds = FBox(Update.Location + Bounds.Min * Update.Scale, Update.Location + Bounds.Max * Update.Scale);
			}

			Update.PreviousExclusions.Reset();
			Changed += Update.bUnchanged ? 0 : 1;
		}

		Publication->CoverageIndex = First + Payload->Boxes.Num();
		Publication->bCoveragePrepared = Publication->CoverageIndex == Publication->Updates.Num();

		UE_LOG(LogTemp, Display, TEXT("Voxel coverage batch: checked=%d changed=%d compareMs=%.2f"), Payload->Boxes.Num(), Changed, Result.ExecuteMilliseconds);
	};

	Publication->bCoveragePreparing = Scheduler.Enqueue(MoveTemp(Request));
}

bool FVoxelViewPublisher::HigherBuildPriority(const FPublication& Publication, const int32 First, const int32 Second) const
{
	const int32 FirstGroup = Publication.Updates[First].GroupIndex;
	const int32 SecondGroup = Publication.Updates[Second].GroupIndex;
	const auto& FirstPriority = Publication.Groups[FirstGroup].Priority;
	const auto& SecondPriority = Publication.Groups[SecondGroup].Priority;
	if (HigherGroupPriority(FirstPriority, SecondPriority)) return true;
	if (HigherGroupPriority(SecondPriority, FirstPriority)) return false;
	return FirstGroup == SecondGroup ? First < Second : FirstGroup < SecondGroup;
}

void FVoxelViewPublisher::AdmitBuilds(const FPublicationRef& Publication)
{
	if (!Publication->bCoveragePrepared || !Publication->bGroupsPrepared || Publication->CoverageIndex == 0)
	{
		return;
	}

	constexpr int32 MaximumPendingBuilds = 4;
	const auto Before = [this, &Publication](const int32 First, const int32 Second)
	{
		return HigherBuildPriority(*Publication, First, Second);
	};

	if (!Publication->bBuildOrderInitialized)
	{
		if (Publication->Updates.IsValidIndex(Publication->BuildInitializationIndex))
		{
			Publication->BuildOrder.HeapPush(Publication->BuildInitializationIndex++, Before);
			return;
		}

		Publication->bBuildOrderInitialized = true;
		Publication->bBuildOrderDirty = false;
	}

	if (Publication->bBuildOrderDirty)
	{
		if (Publication->BuildHeapifyIndex == INDEX_NONE)
		{
			Publication->BuildHeapifyIndex = Publication->BuildOrder.Num() / 2;
		}

		if (Publication->BuildHeapifyIndex >= 0)
		{
			int32 Parent = Publication->BuildHeapifyIndex--;
			while (2 * Parent + 1 < Publication->BuildOrder.Num())
			{
				int32 Child = 2 * Parent + 1;
				if (Child + 1 < Publication->BuildOrder.Num() && Before(Publication->BuildOrder[Child + 1], Publication->BuildOrder[Child]))
				{
					++Child;
				}

				if (!Before(Publication->BuildOrder[Child], Publication->BuildOrder[Parent]))
				{
					break;
				}

				Publication->BuildOrder.Swap(Child, Parent);
				Parent = Child;
			}

			if (Publication->BuildHeapifyIndex < 0)
			{
				Publication->bBuildOrderDirty = false;
			}
			return;
		}
	}

	const int32 Limit = FMath::Min(Publication->BuildOrder.Num(), FMath::Clamp(Module.GetViewSettings().Budget.PublishAdmissionBatchSize, 1, 32));

	TArray<int32> RetryIndices;
	int32 Checked = 0;

	while (Checked < Limit && Publication->PendingBuildIndices.Num() < MaximumPendingBuilds)
	{
		int32 BuildIndex = INDEX_NONE;
		Publication->BuildOrder.HeapPop(BuildIndex, Before, EAllowShrinking::No);
		++Checked;

		if (Publication->Groups.IsValidIndex(Publication->Updates[BuildIndex].GroupIndex) &&
			Publication->Groups[Publication->Updates[BuildIndex].GroupIndex].bCommitted)
		{
			continue;
		}

		if (Publication->PreparedMeshes[BuildIndex])
		{
			QueueReadyGroup(Publication, Publication->Updates[BuildIndex].GroupIndex);
		}

		if (Publication->PreparedMeshes[BuildIndex] || Publication->PendingBuildIndices.Contains(BuildIndex))
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
			QueueReadyGroup(Publication, Update.GroupIndex);
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
			if (Cancel.Load())
			{
				return TaskResult;
			}

			const auto Payload = MakeShared<FVoxelViewCoverageResult, ESPMode::ThreadSafe>();
			auto Mesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();

			VoxelMeshClipper::Subtract(*Source, Exclusions, *Mesh, &Cancel);
			if (!Mesh->ValidateAndCache())
			{
				return TaskResult;
			}

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
			if (Publication->Groups[Publication->Updates[Index].GroupIndex].bCommitted)
			{
				return;
			}

			if (TaskResult.bCanceled && TaskResult.Stamp.WorldEpoch == WorldEpoch)
			{
				Publication->BuildOrder.HeapPush(Index,
					[this, &Publication](const int32 First, const int32 Second)
					{
						return HigherBuildPriority(*Publication, First, Second);
					});
				return;
			}

			if (!TaskResult.bSuccess || TaskResult.bCanceled || TaskResult.Stamp.WorldEpoch != WorldEpoch)
			{
				if (!TaskResult.Error.IsEmpty())
				{
					const FUpdate& Failed = Publication->Updates[Index];
					UE_LOG(LogTemp, Warning, TEXT("Voxel coverage publication failed: sourceBytes=%llu exclusions=%d error=%s"),
						Failed.Source ? Failed.Source->Bytes() : 0, Failed.Exclusions.Num(), *TaskResult.Error);
				}

				DiscardPublication(Publication);
				bRetry = true;
				return;
			}

			const auto Payload = StaticCastSharedPtr<const FVoxelViewCoverageResult>(TaskResult.CustomPayload);
			if (!Payload || Payload->Meshes.IsEmpty())
			{
				DiscardPublication(Publication);
				bRetry = true;
				return;
			}

			Publication->PreparedMeshes[Index] = Payload->Meshes[0];
			QueueReadyGroup(Publication, Publication->Updates[Index].GroupIndex);
		};

		if (!Scheduler.Enqueue(MoveTemp(Request)))
		{
			RetryIndices.Add(BuildIndex);
			continue;
		}

		Publication->PendingBuildIndices.Add(BuildIndex);
	}

	for (const int32 Index : RetryIndices)
	{
		Publication->BuildOrder.HeapPush(Index, Before);
	}
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
	while (Update.PreparedBatchIndex < Mesh.Batches.Num() && FPlatformTime::Seconds() < InDeadline)
	{
		if (InOutPreparedComponents >= Module.GetViewSettings().Budget.MaxPublishComponentsPerFrame)
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
		bOutComplete = Update.PreparedBatchIndex == Mesh.Batches.Num();
		break;
	}
	bOutComplete = Update.PreparedBatchIndex == Mesh.Batches.Num();
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
	FVoxelPublishBudget Budget;
	Budget.Begin(FPlatformTime::Seconds(), Module.GetViewSettings().Budget.TerrainPublishMilliseconds, 8192);
	Tick(Budget);
}

void FVoxelViewPublisher::CommitReadyGroups(const FPublicationRef& Publication, int32& CommittedThisFrame, const double Deadline,
	const TMap<FVoxelPublishGroupKey, uint64>& FirstOwners, const int32 InGroupIndex)
{
	if (!Publication->bGroupsPrepared) return;
	const int32 Limit = FMath::Max(1, Module.GetViewSettings().Budget.MaxPublishGroupsPerFrame);
	const TConstArrayView<int32> Candidates = InGroupIndex == INDEX_NONE ? MakeArrayView(Publication->GroupOrder) : MakeArrayView(&InGroupIndex, 1);
	for (const int32 GroupIndex : Candidates)
	{
		if (InGroupIndex != INDEX_NONE && GroupIndex != InGroupIndex) continue;
		FGroup& Group = Publication->Groups[GroupIndex];
		if (Group.bCommitted || !CanPublishGroup(Group)) continue;
		const bool bCurrent = !Group.Keys.ContainsByPredicate([&Publication](const auto& Key)
		{
			const auto Validation = Publication->Validations.FindRef(Key);
			return Validation && *Validation && !(*Validation)();
		});
		if (!bCurrent)
		{
			for (const auto& Key : Group.Keys) { SetOwnerActive(Key, Publication->Serial, false); RetryOwners.AddUnique(Key); }
			for (const int32 Index : Group.UpdateIndices)
			{
				ReleaseActorGeneration(Publication->Updates[Index].Actor, Publication->Serial);
				for (const auto& Component : Publication->Updates[Index].Components) if (Component) Component->DestroyComponent();
				Publication->Updates[Index].Components.Reset();
				if (Publication->PreparedMeshes.IsValidIndex(Index)) Publication->PreparedMeshes[Index].Reset();
			}
			for (const auto Actor : Group.VisibilityActors) ReleaseActorGeneration(Actor, Publication->Serial);
			Group.bCommitted = true;
			for (const auto& Key : Group.Keys) SetOwnerActive(Key, Publication->Serial, false);
			bFirstOwnersDirty = true;
			bActiveBusyKeysDirty = true;
			continue;
		}

		if (Group.PreparedUpdates != Group.UpdateIndices.Num()) continue;
		if (Group.Keys.ContainsByPredicate([&FirstOwners, &Publication](const FVoxelPublishGroupKey& Key)
		{
			const uint64* First = FirstOwners.Find(Key);
			return First && *First < Publication->Serial;
		})) continue;
		if (HasOlderDependency(Publication, Group)) continue;
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
	if (InGroupIndex != INDEX_NONE && Publication->Groups[InGroupIndex].bCommitted) Publication->GroupOrder.Remove(InGroupIndex);
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
		ReleaseActorGeneration(Update.Actor, Publication->Serial);
		AActor* Actor = Update.Actor.Get();
		FEntry* Entry = Entries.Find(Update.Actor);
		if (!Actor || !Entry) continue;
		Entry->LocalWorldBoxes = MoveTemp(Update.LocalWorldBoxes);
		Update.CommonBoxes.Reset();
		if (Update.bUnchanged)
		{
			Entry->Exclusions = MoveTemp(Update.Exclusions);
			Entry->bDirty = Entry->Source != Update.Source || Entry->Location != Update.Location || Entry->Scale != Update.Scale;
			continue;
		}
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
		Entry->PresentedVertices = Entry->PresentedTriangles = Entry->PresentedBytes = 0;
		if (const FMeshPtr Mesh = Publication->PreparedMeshes.IsValidIndex(Index) ? Publication->PreparedMeshes[Index] : FMeshPtr())
		{
			Entry->PresentedBytes = Mesh->Bytes();
			for (const auto& Batch : Mesh->Batches)
			{
				Entry->PresentedVertices += Batch.Mesh.Vertices.Num();
				Entry->PresentedTriangles += Batch.Mesh.Triangles.Num() / 3;
			}
		}
		Entry->bDirty = Entry->Source != Update.Source || Entry->Location != Update.Location || Entry->Scale != Update.Scale;
		Entry->bPresented = true;
		Entry->PresentedWorldBounds = Update.Bounds.IsValid
			? FBox(Update.Location + Update.Bounds.Min * Update.Scale,
				Update.Location + Update.Bounds.Max * Update.Scale)
			: FBox(ForceInit);
	}
	for (const TWeakObjectPtr<AActor> ActorKey : InGroup.VisibilityActors)
	{
		ReleaseActorGeneration(ActorKey, Publication->Serial);
		const bool* bHidden = Publication->Visibility.Find(ActorKey);
		if (AActor* Actor = ActorKey.Get();
			Actor && bHidden && Actor->IsHidden() != *bHidden)
		{
			Actor->SetActorHiddenInGame(*bHidden);
		}
		Publication->Visibility.Remove(ActorKey);
	}
	InGroup.bCommitted = true;
	for (const auto& Key : InGroup.Keys) SetOwnerActive(Key, Publication->Serial, false);
	bFirstOwnersDirty = true;
	bActiveBusyKeysDirty = true;
	for (const int32 Index : InGroup.UpdateIndices)
	{
		if (Publication->PreparedMeshes.IsValidIndex(Index))
		{
			Publication->PreparedMeshes[Index].Reset();
		}
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
	if (PendingPlan || !Publications.IsEmpty() || !PendingGroups.IsEmpty()) return;
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
	ActorUpdateGenerations.Reset();
	ActorVisibilityGenerations.Reset();
	ReadyPrepareGroups.Reset();
	ReadyCommitGroups.Reset();
	PrepareSelection.Reset();
	CommitSelection.Reset();
	OwnerFirstSerial.Reset();
	OwnerSerialQueues.Reset();
	bFirstOwnersDirty = true;
	++PriorityRevision;
	bPrioritiesDirty = true;
	PendingPlan.Reset();
	PriorityPlan.Reset();
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
	for (const auto& Group : Publication->Groups) if (!Group.bCommitted)
		for (const auto& Key : Group.Keys)
		{
			SetOwnerActive(Key, Publication->Serial, false);
			RetryOwners.AddUnique(Key);
		}
	bFirstOwnersDirty = true;
	++PriorityRevision;
	bPrioritiesDirty = true;
	bActiveBusyKeysDirty = true;
	for (FUpdate& Update : Publication->Updates)
	{
		ReleaseActorGeneration(Update.Actor, Publication->Serial);
		if (!Update.Actor.IsValid()) continue;
		for (UVoxelMeshComponent* Component : Update.Components)
		{
			if (Component) RetiredComponents.Add(Component);
		}
		Update.Components.Reset();
	}
	for (const auto& Pair : Publication->Visibility) ReleaseActorGeneration(Pair.Key, Publication->Serial);
}

void FVoxelViewPublisher::Forget(AActor* InActor)
{
	const TWeakObjectPtr<AActor> Key(InActor);
	PendingVisibility.Remove(Key);
	if (const int32* Index = PendingUpdateIndices.Find(Key)) TakePendingUpdate(*Index);
	if (const auto* Entry = Entries.Find(Key))
		if (auto* Actors = OwnerActors.Find(Entry->GroupKey)) { Actors->Remove(Key); if (Actors->IsEmpty()) OwnerActors.Remove(Entry->GroupKey); }
	Entries.Remove(Key);
	ActorUpdateGenerations.Remove(Key);
	ActorVisibilityGenerations.Remove(Key);
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
	OwnerActors.Reset();
	RetryOwners.Reset();
	ActiveBusyKeys.Reset();
	bActiveBusyKeysDirty = true;
	bRetry = false;
}

bool FVoxelViewPublisher::IsBusy() const
{
	if (PendingPlan || !Publications.IsEmpty()) return true;
	for (const auto& Pair : PendingGroups)
	{
		if (Pair.Value.bCommitted) return true;
	}
	return false;
}

bool FVoxelViewPublisher::IsGroupBusy(const FVoxelPublishGroupKey& InKey) const
{
	return PendingGroupIds.Contains(InKey) || (PendingPlan && PendingPlan->GroupIds.Contains(InKey)) || OwnerSerialQueues.Contains(InKey);
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

void FVoxelViewPublisher::IndexPublicationUpdate(const FPublicationRef& Publication, const int32 InIndex)
{
	const auto Actor = Publication->Updates[InIndex].Actor;
	if (!Actor.IsValid()) return;
	auto& Queue = ActorUpdateGenerations.FindOrAdd(Actor);
	if (!Queue.ContainsByPredicate([&Publication](const auto& Generation) { return Generation.Publication->Serial == Publication->Serial; }))
		Queue.Add({Publication, InIndex});
}

void FVoxelViewPublisher::ReleaseActorGeneration(const TWeakObjectPtr<AActor> InActor, const uint64 InSerial)
{
	if (auto* Queue = ActorUpdateGenerations.Find(InActor))
	{
		Queue->RemoveAll([InSerial](const auto& Generation) { return Generation.Publication->Serial == InSerial; });
		if (Queue->IsEmpty()) ActorUpdateGenerations.Remove(InActor);
	}
	if (auto* Queue = ActorVisibilityGenerations.Find(InActor))
	{
		Queue->RemoveAll([InSerial](const auto& Generation) { return Generation.Publication->Serial == InSerial; });
		if (Queue->IsEmpty()) ActorVisibilityGenerations.Remove(InActor);
	}
}

void FVoxelViewPublisher::QueueReadyGroup(const FPublicationRef& InPublication, const int32 InGroupIndex)
{
	if (!InPublication->bGroupsPrepared || !InPublication->Groups.IsValidIndex(InGroupIndex)) return;
	FGroup& Group = InPublication->Groups[InGroupIndex];
	if (Group.bCommitted || InPublication->bCanceled) return;
	if (Group.QueueSequence == 0)
	{
		Group.QueueSequence = ++ReadySequence;
		Group.FirstQueuedAt = FPlatformTime::Seconds();
	}
	if (Group.PreparedUpdates == Group.UpdateIndices.Num())
	{
		if (!Group.bCommitQueued)
		{
			Group.bCommitQueued = true;
			ReadyCommitGroups.Add({InPublication, InGroupIndex});
		}
	}
	else if (!Group.bPrepareQueued && InPublication->PreparedMeshes[Group.UpdateIndices[Group.PreparedUpdates]])
	{
		Group.bPrepareQueued = true;
		ReadyPrepareGroups.Add({InPublication, InGroupIndex});
	}
}

bool FVoxelViewPublisher::HasOlderDependency(const FPublicationRef& InPublication, const FGroup& InGroup) const
{
	int32 Operations = 0;
	for (const auto& Older : Publications)
	{
		if (Older->Serial >= InPublication->Serial || Older->bFinished || Older->bCanceled || Older->Serial < InGroup.DependencyNextSerial)
			continue;
		if (Older->DependencyIndexBuildRevision != Older->GroupRevision)
		{
			Older->DependencyIndex = {};
			Older->DependencyIndexBuildCursor = 0;
			Older->DependencyIndexBuildRevision = Older->GroupRevision;
			Older->IndexedGroupRevision = 0;
		}
		while (Older->DependencyIndexBuildCursor < Older->Groups.Num())
		{
			if (++Operations > 64)
				return true;
			const int32 Index = Older->DependencyIndexBuildCursor++;
			Older->DependencyIndex.Add(Index, Older->Groups[Index].Bounds);
		}
		Older->IndexedGroupRevision = Older->GroupRevision;
		if (!InGroup.bDependencyQueryActive || InGroup.DependencyQuerySerial != Older->Serial || InGroup.DependencyQueryRevision != Older->GroupRevision)
		{
			Older->DependencyIndex.BeginQuery(InGroup.Bounds, InGroup.DependencyQuery);
			InGroup.DependencyQuerySerial = Older->Serial;
			InGroup.DependencyQueryRevision = Older->GroupRevision;
			InGroup.bDependencyQueryActive = true;
		}
		while (true)
		{
			if (++Operations > 64)
				return true;
			int32 Index = INDEX_NONE;
			if (Older->DependencyIndex.AdvanceQuery(InGroup.DependencyQuery, Index))
				break;
			if (Index == INDEX_NONE)
				continue;
			const FGroup& Dependency = Older->Groups[Index];
			if (!Dependency.bCommitted && GroupsInteract(InGroup, Dependency))
			{
				// 被阻塞的候选在下一次重读，旧组提交后无需从头遍历索引。
				InGroup.DependencyQuery.Seen.Remove(Index);
				InGroup.bDependencyQueryActive = false;
				return true;
			}
		}
		InGroup.bDependencyQueryActive = false;
		InGroup.DependencyNextSerial = Older->Serial + 1;
	}
	return false;
}

int32 FVoxelViewPublisher::SelectReadyGroup(TConstArrayView<FReadyGroup> InReady, FVoxelTaskFairSelection& InSelection, const bool bInCommit)
{
	TArray<FVoxelTaskServiceCandidate> Candidates;
	Candidates.Reserve(InReady.Num());
	for (int32 Index = 0; Index < InReady.Num(); ++Index)
	{
		const FReadyGroup& Ready = InReady[Index];
		if (Ready.Publication->bCanceled || !Ready.Publication->Groups.IsValidIndex(Ready.GroupIndex)) return Index;
		const FGroup& Group = Ready.Publication->Groups[Ready.GroupIndex];
		if (Group.bCommitted) return Index;
		if (!Group.bPriorityReady) continue;
		if (bInCommit)
		{
			const bool bCurrent = !Group.Keys.ContainsByPredicate([&Ready](const auto& Key)
			{
				const auto Validation = Ready.Publication->Validations.FindRef(Key);
				return Validation && *Validation && !(*Validation)();
			});
			if (bCurrent && (Group.Keys.ContainsByPredicate([this, &Ready](const auto& Key)
			{
				const uint64* First = OwnerFirstSerial.Find(Key);
				return First && *First < Ready.Publication->Serial;
			}) || HasOlderDependency(Ready.Publication, Group))) continue;
		}
		FVoxelTaskServiceCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.PendingIndex = Index;
		Candidate.Lane = Group.Priority.WorkClass == EVoxelWorkClass::Critical ?
			EVoxelTaskServiceLane::Critical : EVoxelTaskServiceLane::Coverage;
		Candidate.SourcePriority = Group.Priority.SourcePriority;
		Candidate.Distance = Group.Priority.ObserverDistance;
		Candidate.EnqueuedAt = Group.FirstQueuedAt;
		Candidate.Sequence = Group.QueueSequence;
	}
	return InSelection.Select(Candidates, FPlatformTime::Seconds());
}

void FVoxelViewPublisher::SetOwnerActive(const FVoxelPublishGroupKey& InKey, const uint64 InSerial, const bool bInActive)
{
	if (bInActive)
	{
		auto& Queue = OwnerSerialQueues.FindOrAdd(InKey);
		Queue.AddUnique(InSerial);
		Queue.Sort();
		OwnerFirstSerial.Add(InKey, Queue[0]);
	}
	else if (auto* Queue = OwnerSerialQueues.Find(InKey))
	{
		Queue->Remove(InSerial);
		if (Queue->IsEmpty()) { OwnerSerialQueues.Remove(InKey); OwnerFirstSerial.Remove(InKey); }
		else OwnerFirstSerial.Add(InKey, (*Queue)[0]);
	}
}

void FVoxelViewPublisher::RebuildFirstOwners()
{
	// Begin, commit, defer and cancel maintain each owner's generation queue.
	bFirstOwnersDirty = false;
}

void FVoxelViewPublisher::Tick(FVoxelPublishBudget& InBudget, bool bInAllowCommit)
{
#if !UE_BUILD_SHIPPING
	if (FPlatformTime::Seconds() >= NextPublisherDiagnostic)
	{
		UE_LOG(LogTemp, Display, TEXT("Publisher continuation diagnostic: pending=%d frozen=%d phase=%d cursor=%d selected=%d blocked=%d publications=%d priorityPhase=%d readyPrepare=%d readyCommit=%d"),
			PendingGroups.Num(), PendingPlan ? PendingPlan->Groups.Num() : 0, PendingPlan ? PendingPlan->Phase : -1,
			PendingPlan ? PendingPlan->Cursor : -1, PendingPlan ? PendingPlan->SelectedKeys.Num() : 0,
			PendingPlan ? PendingPlan->BlockedComponents.Num() : 0, Publications.Num(), PriorityPlan ? PriorityPlan->Phase : -1,
			ReadyPrepareGroups.Num(), ReadyCommitGroups.Num());
		int32 LoggedGroups = 0;
		for (const auto& Publication : Publications)
		{
			if (LoggedGroups >= 6)
			{
				break;
			}
			if (Publication->bFinished || Publication->bCanceled) continue;
			for (const int32 Index : Publication->GroupOrder)
			{
				if (LoggedGroups >= 6)
				{
					break;
				}
				const FGroup& Group = Publication->Groups[Index];
				if (Group.bCommitted)
					continue;
				uint64 FirstSerial = Publication->Serial;
				for (const auto& Key : Group.Keys)
					if (const uint64* First = OwnerFirstSerial.Find(Key)) FirstSerial = FMath::Min(FirstSerial, *First);
				UE_LOG(LogTemp, Display, TEXT("Publisher group diagnostic: serial=%llu index=%d keys=%d prepared=%d/%d priorityReady=%d class=%d source=%d first=%llu queued=%d/%d coverage=%d/%d"),
					Publication->Serial, Index, Group.Keys.Num(), Group.PreparedUpdates, Group.UpdateIndices.Num(),
					Group.bPriorityReady, static_cast<int32>(Group.Priority.WorkClass), Group.Priority.SourcePriority, FirstSerial,
					Group.bPrepareQueued, Group.bCommitQueued, Publication->CoverageIndex, Publication->Updates.Num());
				++LoggedGroups;
			}
		}
		NextPublisherDiagnostic = FPlatformTime::Seconds() + 5.0;
	}
#endif
	FrameTimings = {};
	int32 PreparedComponents = 0;
	int32 CommittedGroups = 0;
	int32 IdlePhases = 0;
	bool bAttemptedPendingPlan = false;
	const auto Run = [this, &InBudget](EVoxelPublishPhase Phase, auto&& Operation)
	{
		return RunBudgetedPhase(InBudget, Phase, Operation);
	};
	// 一次推进一个可恢复操作，轮换起点防止昂贵 Plan 持续饿死 Prepare/Commit。
	while (InBudget.RemainingMilliseconds(FPlatformTime::Seconds()) > 0.0 && IdlePhases < 6)
	{
		const int32 Phase = NextPhase;
		NextPhase = (NextPhase + 1) % 6;
		bool bWorked = false;
		if (Phase == 0)
		{
			for (const auto& Publication : Publications)
			{
				if (Publication->bCanceled || !Publication->bCoveragePrepared || (Publication->bGroupsPrepared && !Publication->Plan)) continue;
				bWorked = Run(EVoxelPublishPhase::Plan, [this, Publication]
				{
					AdvancePreparedPlan(Publication);
				});
				break;
			}
			if (!bWorked && (PendingPlan || !PendingGroups.IsEmpty()) && !bAttemptedPendingPlan)
			{
				bWorked = Run(EVoxelPublishPhase::Plan, [this] { AdvancePendingPlan(); });
				bAttemptedPendingPlan = !PendingPlan;
			}
		}
		else if (Phase == 1 && bPrioritiesDirty)
		{
			bWorked = Run(EVoxelPublishPhase::Priority, [this]
			{
				AdvancePriorities();
			});
		}
		else if (Phase == 2 && !Publications.IsEmpty())
		{
			AdmissionCursor %= Publications.Num();
			const FPublicationRef Publication = Publications[AdmissionCursor++];
			if (!Publication->bCanceled && !Publication->bFinished &&
				((!Publication->bCoveragePrepared && !Publication->bCoveragePreparing) ||
				(Publication->bGroupsPrepared && !Publication->BuildOrder.IsEmpty()) || !Publication->bBuildOrderInitialized))
				bWorked = Run(EVoxelPublishPhase::Admission, [this, Publication, &InBudget]
				{
					const double Now = FPlatformTime::Seconds();
					AdmitCoveragePreparation(Publication, Now + InBudget.RemainingMilliseconds(Now) / 1000.0);
					AdmitBuilds(Publication);
				});
		}
		else if (Phase == 3 && !ReadyPrepareGroups.IsEmpty() &&
			PreparedComponents < Module.GetViewSettings().Budget.MaxPublishComponentsPerFrame)
		{
			bWorked = Run(EVoxelPublishPhase::Prepare, [&]
			{
				const int32 ReadyIndex = SelectReadyGroup(ReadyPrepareGroups, PrepareSelection);
				if (ReadyIndex == INDEX_NONE) return;
				const FReadyGroup Ready = ReadyPrepareGroups[ReadyIndex];
				ReadyPrepareGroups.RemoveAt(ReadyIndex, 1, EAllowShrinking::No);
				const FPublicationRef Publication = Ready.Publication;
				if (Publication->bCanceled || !Publication->Groups.IsValidIndex(Ready.GroupIndex)) return;
				FGroup& Group = Publication->Groups[Ready.GroupIndex];
				Group.bPrepareQueued = false;
				if (Group.bCommitted || Group.PreparedUpdates == Group.UpdateIndices.Num()) return;
				const int32 Index = Group.UpdateIndices[Group.PreparedUpdates];
				if (!Publication->PreparedMeshes[Index]) return;
				bool bComplete = false;
				const double Deadline = FPlatformTime::Seconds() + InBudget.RemainingMilliseconds(FPlatformTime::Seconds()) / 1000.0;
				if (!PrepareUpdate(Publication, Index, Deadline, PreparedComponents, bComplete))
				{
					DiscardPublication(Publication);
					bRetry = true;
					return;
				}
				if (bComplete) ++Group.PreparedUpdates;
				QueueReadyGroup(Publication, Ready.GroupIndex);
			});
		}
		else if (Phase == 4 && bInAllowCommit && !ReadyCommitGroups.IsEmpty() && CommittedGroups < Module.GetViewSettings().Budget.MaxPublishGroupsPerFrame)
		{
			bWorked = Run(EVoxelPublishPhase::Commit, [&]
			{
				if (bFirstOwnersDirty) RebuildFirstOwners();
				const int32 ReadyIndex = SelectReadyGroup(ReadyCommitGroups, CommitSelection, true);
				if (ReadyIndex == INDEX_NONE) return;
				const FReadyGroup Ready = ReadyCommitGroups[ReadyIndex];
				ReadyCommitGroups.RemoveAt(ReadyIndex, 1, EAllowShrinking::No);
				if (Ready.Publication->bCanceled || !Ready.Publication->Groups.IsValidIndex(Ready.GroupIndex)) return;
				Ready.Publication->Groups[Ready.GroupIndex].bCommitQueued = false;
				CommitReadyGroups(Ready.Publication, CommittedGroups, FPlatformTime::Seconds() +
					InBudget.RemainingMilliseconds(FPlatformTime::Seconds()) / 1000.0, OwnerFirstSerial, Ready.GroupIndex);
				QueueReadyGroup(Ready.Publication, Ready.GroupIndex);
			});
		}
		else if (Phase == 5 && !RetiredComponents.IsEmpty())
			bWorked = Run(EVoxelPublishPhase::Retire, [this]
			{
				if (UVoxelMeshComponent* Component = RetiredComponents.Pop().Get()) Component->DestroyComponent();
			});
		IdlePhases = bWorked ? 0 : IdlePhases + 1;
	}
	if (!PriorityPlan) Publications.RemoveAll([](const FPublicationRef& Publication) { return Publication->bFinished || Publication->bCanceled; });

	if (bInAllowCommit)
	{
		FinishGroups();
	}
	FrameTimings.PublishPrepareMilliseconds = InBudget.GetTiming(EVoxelPublishPhase::Prepare).Milliseconds;
	FrameTimings.PublishCommitMilliseconds = InBudget.GetTiming(EVoxelPublishPhase::Commit).Milliseconds;
	FrameTimings.RetireMilliseconds = InBudget.GetTiming(EVoxelPublishPhase::Retire).Milliseconds;
	FrameTimings.PublishPlanMilliseconds = InBudget.GetTiming(EVoxelPublishPhase::Plan).Milliseconds;
	FrameTimings.PublishPriorityMilliseconds = InBudget.GetTiming(EVoxelPublishPhase::Priority).Milliseconds;
	FrameTimings.PublishAdmissionMilliseconds = InBudget.GetTiming(EVoxelPublishPhase::Admission).Milliseconds;
	FrameTimings.PublishMaximumOperationMilliseconds = 0.0;
	for (int32 Phase = 1; Phase < static_cast<int32>(EVoxelPublishPhase::Count); ++Phase)
		FrameTimings.PublishMaximumOperationMilliseconds = FMath::Max(FrameTimings.PublishMaximumOperationMilliseconds,
			InBudget.GetTiming(static_cast<EVoxelPublishPhase>(Phase)).MaximumOperationMilliseconds);
	FrameTimings.PublishTotalMilliseconds = InBudget.TotalMilliseconds(FPlatformTime::Seconds());
	FrameTimings.bPublishBudgetOverrun = InBudget.HasOverrun(FPlatformTime::Seconds());
}

bool FVoxelViewPublisher::SetScopedCoverage(const FGuid& InGroup, AActor* InActor, TArray<FBox> InWorldCellBoxes,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes,
	TConstArrayView<FBox> InAffectedWorldCellBoxes, const bool bInRetainPresented)
{
	bool bSuccess = false;
	auto Work =
		SetScopedCoverageSteps(InGroup, InActor, MoveTemp(InWorldCellBoxes), MoveTemp(InCommonBoxes), InAffectedWorldCellBoxes, bSuccess, bInRetainPresented);
	while (!Work.IsComplete())
	{
		Work.Resume();
	}
	return bSuccess;
}

FVoxelGameThreadContinuation FVoxelViewPublisher::SetScopedCoverageSteps(const FGuid& InGroup, AActor* InActor, TArray<FBox> InWorldCellBoxes,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, TConstArrayView<FBox> InAffectedWorldCellBoxes, bool& OutSuccess,
	const bool bInRetainPresented)
{
	OutSuccess = false;
	FEntry* Entry = Entries.Find(TWeakObjectPtr<AActor>(InActor));
	if (!Entry)
	{
		co_return;
	}
	if (bInRetainPresented)
	{
		co_await VoxelGameThread::AppendSteps(InWorldCellBoxes, Entry->LocalWorldBoxes);
	}
	TArray<FBox> Previous;
	Entry = Entries.Find(InActor);
	if (!Entry)
	{
		co_return;
	}
	const FVector Origin = Entry->Location / Module.BlockSize();
	const double Step = Entry->Scale / Module.BlockSize();
	if (!Entry->bHasDesiredExclusions)
	{
		for (const FBox& Box : Entry->Exclusions)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			Previous.Add(FBox(Origin + Box.Min * Step, Origin + Box.Max * Step));
		}
	}
	else
	{
		co_await VoxelGameThread::AppendSteps(Previous, Entry->DesiredWorldExclusions);
	}
	// 局部更新只替换 scope 内遮罩，旧区域外的实际贡献域继续保留。
	for (const FBox& Scope : InAffectedWorldCellBoxes)
	{
		TArray<FBox> Remaining;
		for (const FBox& Box : Previous)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const FBox Cut = Box.Overlap(Scope);
			if (!Cut.IsValid || Cut.GetVolume() <= 0.0)
			{
				Remaining.Add(Box);
				continue;
			}
			FBox Center = Box;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				if (Center.Min[Axis] < Cut.Min[Axis])
				{
					FBox Piece = Center;
					Piece.Max[Axis] = Cut.Min[Axis];
					Remaining.Add(Piece);
					Center.Min[Axis] = Cut.Min[Axis];
				}
				if (Center.Max[Axis] > Cut.Max[Axis])
				{
					FBox Piece = Center;
					Piece.Min[Axis] = Cut.Max[Axis];
					Remaining.Add(Piece);
					Center.Max[Axis] = Cut.Max[Axis];
				}
			}
		}
		Previous = MoveTemp(Remaining);
	}
	if (InCommonBoxes)
	{
		co_await VoxelGameThread::AppendSteps(InWorldCellBoxes, *InCommonBoxes);
	}
	for (const FBox& Desired : InWorldCellBoxes)
	{
		for (const FBox& Scope : InAffectedWorldCellBoxes)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const FBox Cut = Desired.Overlap(Scope);
			if (Cut.IsValid && Cut.GetVolume() > 0.0)
			{
				Previous.Add(Cut);
			}
		}
	}
	co_await VoxelMeshClipper::NormalizeBoxesSteps(Previous);
	co_await SetCoverageSteps(InGroup, InActor, MoveTemp(Previous), nullptr, OutSuccess);
	Entry = Entries.Find(InActor);
	if (!OutSuccess || !Entry)
	{
		OutSuccess = false;
		co_return;
	}
	if (bInRetainPresented)
	{
		const int32* Index = PendingUpdateIndices.Find(TWeakObjectPtr<AActor>(InActor));
		if (!Entry->PresentedSource)
		{
			OutSuccess = false;
			co_return;
		}
		if (!Index)
		{
			co_return;
		}
		FUpdate& Update = PendingUpdates[*Index];
		Update.Source = Entry->PresentedSource;
		Update.bSourceDirty = false;
		Update.bRetainSource = true;
		Update.Bounds = Entry->PresentedWorldBounds.IsValid ? FBox(
			(Entry->PresentedWorldBounds.Min - Entry->Location) / Entry->Scale,
			(Entry->PresentedWorldBounds.Max - Entry->Location) / Entry->Scale) : FBox(ForceInit);
	}
}

bool FVoxelViewPublisher::RunBudgetedPhase(FVoxelPublishBudget& InBudget, const EVoxelPublishPhase InPhase, TFunctionRef<void()> InOperation)
{
	const uint8 Index = static_cast<uint8>(InPhase);
	check(Index > 0 && Index < static_cast<uint8>(EVoxelPublishPhase::Count));

	const auto& Policy = Module.GetViewSettings().Budget;
	const double FloorMs = FMath::Max(0.01, static_cast<double>(Policy.PublishOperationEstimateFloorMilliseconds));
	const double CeilingMs = FMath::Min(
		double(Policy.TerrainPublishMilliseconds) * 0.5, FMath::Max(FloorMs, static_cast<double>(Policy.PublishOperationEstimateCeilingMilliseconds)));

	double& Estimate = PhaseOperationEstimateMilliseconds[Index];
	if (Estimate <= 0.0)
	{
		Estimate = FloorMs;
	}
	Estimate = FMath::Clamp(Estimate, FloorMs, CeilingMs);

	const double Started = FPlatformTime::Seconds();
	if (!InBudget.TryStart(InPhase, Started, Estimate))
	{
		const uint64 Deferrals = ++PhaseConsecutiveDeferrals[Index];
		if (Deferrals == 60 || Deferrals % 600 == 0)
			UE_LOG(LogTemp, Warning, TEXT("Voxel publish deferral: phase=%d consecutive=%llu estimateMs=%.3f remainingMs=%.3f"), Index, Deferrals, Estimate,
				InBudget.RemainingMilliseconds(Started));
		return false;
	}
	PhaseConsecutiveDeferrals[Index] = 0;

	InOperation();

	const double Finished = FPlatformTime::Seconds();
	const double ElapsedMs = (Finished - Started) * 1000.0;

	// EWMA 只用于“是否应该在剩余预算中启动下一次操作”，
	// 不代替硬 wall-clock deadline。
	Estimate = FMath::Clamp(Estimate * 0.8 + ElapsedMs * 0.2, FloorMs, CeilingMs);

	InBudget.Finish(InPhase, Started, Finished);
	return true;
}

bool FVoxelViewPublisher::GetPresentedMeshCost(AActor* InActor, uint64& OutVertices, uint64& OutTriangles, uint64& OutBytes) const
{
	const FEntry* Entry = Entries.Find(InActor);
	if (!Entry || !Entry->bPresented || !InActor || InActor->IsHidden())
		return false;
	OutVertices = Entry->PresentedVertices;
	OutTriangles = Entry->PresentedTriangles;
	OutBytes = Entry->PresentedBytes;
	return true;
}
