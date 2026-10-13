#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Rendering/VoxelTerrainCoarseMesh.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Algo/Compare.h"

#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Rendering/VoxelMeshClipper.h"
#include "Voxel/VoxelModule.h"

namespace
{
	FVoxelGameThreadContinuation FindCoverageChangesSteps(TConstArrayView<FBox> InPrevious,
		TConstArrayView<FBox> InCurrent, const FBox InBounds, TArray<FBox>& OutChanges)
	{
		OutChanges.Reset();
		if (!InBounds.IsValid) co_return;
		for (int32 Direction = 0; Direction < 2; ++Direction)
		{
			const auto From = Direction == 0 ? InPrevious : InCurrent;
			const auto Against = Direction == 0 ? InCurrent : InPrevious;
			for (const FBox& Box : From)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (!Box.Intersect(InBounds)) continue;
				TArray<FBox> Fragments{Box.Overlap(InBounds)};
				for (const FBox& Other : Against)
				{
					TArray<FBox> Remaining;
					for (FBox Fragment : Fragments)
					{
						co_await FVoxelGameThreadContinuation::Yield();
						if (!Fragment.Intersect(Other)) { Remaining.Add(Fragment); continue; }
						const FBox Intersection = Fragment.Overlap(Other);
						for (int32 Axis = 0; Axis < 3; ++Axis)
						{
							if (Fragment.Min[Axis] < Intersection.Min[Axis])
							{
								FBox Part = Fragment;
								Part.Max[Axis] = Intersection.Min[Axis];
								Remaining.Add(Part);
								Fragment.Min[Axis] = Intersection.Min[Axis];
							}
							if (Fragment.Max[Axis] > Intersection.Max[Axis])
							{
								FBox Part = Fragment;
								Part.Min[Axis] = Intersection.Max[Axis];
								Remaining.Add(Part);
								Fragment.Max[Axis] = Intersection.Max[Axis];
							}
						}
					}
					Fragments = MoveTemp(Remaining);
					if (Fragments.IsEmpty()) break;
				}
				co_await VoxelGameThread::AppendSteps(OutChanges, Fragments);
			}
		}
		co_await VoxelMeshClipper::NormalizeBoxesSteps(OutChanges);
	}
}

FVoxelViewPublisher::FVoxelViewPublisher(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler, const uint64 InWorldEpoch)
	: Module(InModule), Scheduler(InScheduler), WorldEpoch(InWorldEpoch)
{
	const auto SelectionConsumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, NativeOwnerId, 1, {}, 1);
	PrepareSelection.BindResources(Scheduler.GetTerrainResources(), SelectionConsumer);
	CommitSelection.BindResources(Scheduler.GetTerrainResources(), SelectionConsumer);
	TerrainRenderer.Emplace(Module, Scheduler, WorldEpoch);
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

bool FVoxelViewPublisher::Stage(const FGuid& InGroup, FVoxelViewSourceId& InOutSourceId,
	const FVector& InLocation, const double InScale, FVoxelSectionMeshResult&& InMesh,
	const int32 InTerrainStage, const bool bInTransition)
{
	if (!InMesh.bValidated && !InMesh.ValidateAndCache()) return false;
	return StageImmutable(InGroup, InOutSourceId, InLocation, InScale,
		MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>(MoveTemp(InMesh)), InTerrainStage, bInTransition);
}

bool FVoxelViewPublisher::StageImmutable(
	const FGuid& InGroup,
	FVoxelViewSourceId& InOutSourceId,
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
	if (!InMesh->Stamp.bHasGeometryIdentity)
	{
		const auto Representation = Group->Key.Representation == 1 || Group->Key.Representation == 6 ?
			EVoxelTerrainRepresentation::VoxelProxy : Group->Key.Representation == 2 || Group->Key.Representation == 4 ?
			EVoxelTerrainRepresentation::Surface : EVoxelTerrainRepresentation::Macro;
		if (!Module.GetCurrentInterest().Geometry.Contains({Representation, Group->Key.Coordinate, Group->Key.Level})) return false;
	}
	if (!Entries.Contains(InOutSourceId)) InOutSourceId = FVoxelViewSourceId::NewId();
	FEntry& Entry = Entries.FindOrAdd(InOutSourceId);
	if (Entry.Source != InMesh)
	{
		Entry.NativeStamp = InMesh->Stamp;
		if (!Entry.NativeStamp.bHasGeometryIdentity)
		{
			const auto Representation = Group->Key.Representation == 1 || Group->Key.Representation == 6 ?
				EVoxelTerrainRepresentation::VoxelProxy : Group->Key.Representation == 2 || Group->Key.Representation == 4 ?
				EVoxelTerrainRepresentation::Surface : EVoxelTerrainRepresentation::Macro;
			const FVoxelTerrainSpatialKey Key{Representation, Group->Key.Coordinate, Group->Key.Level};
			const auto* Geometry = Module.GetCurrentInterest().Geometry.Find(Key);
			if (!Geometry) return false;
			check(NativeSourceSerial < MAX_uint64);
			Entry.NativeStamp.OwnerId = NativeOwnerId;
			Entry.NativeStamp.OwnerIncarnation = 1;
			Entry.NativeStamp.WorldEpoch = WorldEpoch;
			Entry.NativeStamp.Token = ++NativeSourceSerial;
			Entry.NativeStamp.bHasGeometryIdentity = true;
			Entry.NativeStamp.GeometryIdentity = *Geometry;
			const auto& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
			Entry.NativeStamp.Revision = Representation == EVoxelTerrainRepresentation::VoxelProxy ?
				Hierarchy.GetVoxelProxyRevision({Key.Coordinate, Key.Level}) : Representation == EVoxelTerrainRepresentation::Surface ?
				Hierarchy.GetSurfaceRevision({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}) :
				Hierarchy.GetMacroRevision({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
		}
	}
	Entry.TerrainStage = InTerrainStage;
	Entry.bRetiring = false;
	Entry.GroupKey = Group->Key;
	OwnerSources.FindOrAdd(Group->Key).Add(FVoxelViewSourceId(InOutSourceId));
	Entry.bTransition = bInTransition;
	Entry.bDirty |= Entry.Source != InMesh || Entry.Location != InLocation || Entry.Scale != InScale;
	Entry.Location = InLocation;
	Entry.Scale = InScale;
	Entry.Bounds = InMesh->LocalBounds;
	Entry.Source = MoveTemp(InMesh);
	return true;
}

bool FVoxelViewPublisher::SetCoverage(const FGuid& InGroup, FVoxelViewSourceId InSourceId, TArray<FBox> InWorldCellBoxes,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes)
{
	bool bSuccess = false;
	auto Work = SetCoverageSteps(InGroup, InSourceId, MoveTemp(InWorldCellBoxes), MoveTemp(InCommonBoxes), bSuccess);
	while (!Work.IsComplete())
	{
		Work.Resume();
	}
	return bSuccess;
}

const FVoxelViewPublisher::FUpdate* FVoxelViewPublisher::FindProjectedUpdate(
	const FVoxelViewSourceId InSourceId, const uint64 InBeforeSerial, const FPublication** OutPublication) const
{
	if (OutPublication) *OutPublication = nullptr;
	if (InBeforeSerial == MAX_uint64)
	{
		if (const int32* Index = PendingUpdateIndices.Find(InSourceId)) return &PendingUpdates[*Index];
		if (PendingPlan)
			if (const int32* Index = PendingPlan->UpdateIndices.Find(InSourceId))
				if (PendingPlan->Updates[*Index].Source) return &PendingPlan->Updates[*Index];
	}
	const FUpdate* Latest = nullptr;
	uint64 Serial = 0;
	if (const auto* Generations = SourceUpdateGenerations.Find(InSourceId))
		for (const auto& Generation : *Generations)
		{
			const auto& Publication = Generation.Publication;
			const auto& Update = Publication->Updates[Generation.UpdateIndex];
			if (Publication->bCanceled || Publication->bFinished || !Update.Source ||
				Publication->Serial >= InBeforeSerial || Publication->Serial <= Serial) continue;
			Serial = Publication->Serial;
			Latest = &Update;
			if (OutPublication) *OutPublication = &Publication.Get();
		}
	return Latest;
}

bool FVoxelViewPublisher::CaptureCoverageSource(const FVoxelViewSourceId InSourceId,
	const bool bInRetainPresented, FUpdate& OutSource) const
{
	const FEntry* Entry = Entries.Find(InSourceId);
	if (!Entry) return false;
	const FUpdate* Projected = FindProjectedUpdate(InSourceId);
	OutSource.SourceId = InSourceId;
	OutSource.bRetainSource = bInRetainPresented;
	if (bInRetainPresented && Projected)
	{
		OutSource.Source = Projected->Source;
		OutSource.NativeStamp = Projected->NativeStamp;
		OutSource.NativeMesh = Projected->NativeMesh;
		OutSource.Location = Projected->Location;
		OutSource.Scale = Projected->Scale;
		OutSource.Bounds = Projected->Bounds;
		OutSource.LocalWorldBoxes = Projected->LocalWorldBoxes;
		OutSource.CommonBoxes = Projected->CommonBoxes;
		OutSource.TerrainStage = Projected->TerrainStage;
		OutSource.bTransition = Projected->bTransition;
	}
	else
	{
		OutSource.Source = bInRetainPresented ? Entry->PresentedSource : Entry->Source;
		OutSource.NativeStamp = bInRetainPresented ? Entry->PresentedNativeStamp : Entry->NativeStamp;
		OutSource.NativeMesh = bInRetainPresented || Entry->Source == Entry->PresentedSource ? Entry->NativeMesh : nullptr;
		OutSource.Location = Entry->Location;
		OutSource.Scale = Entry->Scale;
		OutSource.Bounds = Entry->Bounds;
		if (bInRetainPresented && Entry->NativeMesh)
		{
			OutSource.Location = Entry->NativeMesh->WorldCellOrigin * Module.BlockSize();
			OutSource.Scale = Entry->NativeMesh->CellScale * Module.BlockSize();
			OutSource.Bounds = Entry->NativeMesh->Source->LocalBounds;
		}
		OutSource.LocalWorldBoxes = Entry->LocalWorldBoxes;
		OutSource.TerrainStage = Entry->TerrainStage;
		OutSource.bTransition = Entry->bTransition;
	}
	OutSource.PreviousExclusions = Projected ? Projected->Exclusions : Entry->Exclusions;
	OutSource.bSourceDirty = !bInRetainPresented && Entry->bDirty;
	return OutSource.Source.IsValid() && OutSource.Scale > 0.0;
}

FVoxelGameThreadContinuation FVoxelViewPublisher::SetCoverageSteps(
	const FGuid& InGroup, FVoxelViewSourceId InSourceId, TArray<FBox> InWorldCellBoxes,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, bool& OutSuccess)
{
	FUpdate Source;
	OutSuccess = false;
	if (!CaptureCoverageSource(InSourceId, false, Source)) co_return;
	co_await SetCoverageSourceSteps(InGroup, InSourceId, MoveTemp(Source), MoveTemp(InWorldCellBoxes), MoveTemp(InCommonBoxes), OutSuccess);
}

FVoxelGameThreadContinuation FVoxelViewPublisher::SetCoverageSourceSteps(
	const FGuid& InGroup, FVoxelViewSourceId InSourceId, FUpdate InSource, TArray<FBox> InWorldCellBoxes,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, bool& OutSuccess)
{
	OutSuccess = false;
	const FVoxelViewSourceId SourceKey(InSourceId);
	const FPendingGroup* Group = FindPendingGroup(InGroup);
	const FEntry* Entry = Entries.Find(SourceKey);
	if (!Group || Group->bCommitted || !Entry || Entry->GroupKey != Group->Key) co_return;
	const FVoxelPublishGroupKey GroupKey = Group->Key;
	const bool bMatchesPresentedSource = InSource.Source == Entry->PresentedSource;
	// 先拥有不可变比较快照，再让协程跨预算边界；Entry 的待替换位姿不能改写保留源。
	FUpdate Previous;
	const FUpdate* Latest = FindProjectedUpdate(SourceKey);
	const bool bHasProjected = Latest != nullptr;
	if (Latest) Previous = *Latest;
	TArray<FBox> WorldExclusions;
	co_await VoxelGameThread::AppendSteps(WorldExclusions, InWorldCellBoxes);
	if (InCommonBoxes) co_await VoxelGameThread::AppendSteps(WorldExclusions, *InCommonBoxes);
	Entry = Entries.Find(SourceKey);
	if (!Entry) co_return;
	OwnerSources.FindOrAdd(GroupKey).Add(SourceKey);
	FEntry& Current = Entries.FindChecked(SourceKey);
	Current.DesiredWorldExclusions = WorldExclusions;
	Current.bHasDesiredExclusions = true;
	auto EqualBoxes = [](TConstArrayView<FBox> Left, TConstArrayView<FBox> Right, bool& OutEqual) -> FVoxelGameThreadContinuation
	{
		OutEqual = Left.Num() == Right.Num();
		for (int32 Index = 0; OutEqual && Index < Left.Num(); ++Index)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			OutEqual = Left[Index] == Right[Index];
		}
	};
	if (bHasProjected)
	{
		bool bSameCommon = Previous.CommonBoxes == InCommonBoxes;
		if (!bSameCommon && Previous.CommonBoxes && InCommonBoxes)
			co_await EqualBoxes(*Previous.CommonBoxes, *InCommonBoxes, bSameCommon);
		bool bSameLocal = false;
		co_await EqualBoxes(Previous.LocalWorldBoxes, InWorldCellBoxes, bSameLocal);
		if (Previous.Source == InSource.Source && Previous.NativeStamp == InSource.NativeStamp &&
			Previous.Location == InSource.Location && Previous.Scale == InSource.Scale && bSameLocal && bSameCommon)
		{
			OutSuccess = true;
			co_return;
		}
	}
	else if (!InSource.bSourceDirty && bMatchesPresentedSource)
	{
		TArray<FBox> Desired;
		const FVector Origin = InSource.Location / Module.BlockSize();
		const double Step = InSource.Scale / Module.BlockSize();
		for (const FBox& WorldBox : WorldExclusions)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const FBox Local((WorldBox.Min - Origin) / Step, (WorldBox.Max - Origin) / Step);
			if (InSource.Bounds.Intersect(Local)) Desired.Add(Local);
		}
		co_await VoxelMeshClipper::NormalizeBoxesSteps(Desired);
		bool bSame = false;
		co_await EqualBoxes(Desired, InSource.PreviousExclusions, bSame);
		if (bSame) { OutSuccess = true; co_return; }
	}
	if (!Entries.Contains(SourceKey)) co_return;
	const int32* Existing = PendingUpdateIndices.Find(SourceKey);
	const int32 Index = Existing ? *Existing : PendingUpdates.AddDefaulted();
	if (!Existing) PendingUpdateIndices.Add(SourceKey, Index);
	InSource.Exclusions = InWorldCellBoxes;
	InSource.LocalWorldBoxes = MoveTemp(InWorldCellBoxes);
	InSource.CommonBoxes = MoveTemp(InCommonBoxes);
	PendingUpdates[Index] = MoveTemp(InSource);
	OutSuccess = true;
}

bool FVoxelViewPublisher::SetSharedCoverage(const FGuid& InGroup, FVoxelViewSourceId InSourceId,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes)
{
	bool bSuccess = false;
	auto Work = SetSharedCoverageSteps(InGroup, InSourceId, MoveTemp(InCommonBoxes), bSuccess);
	while (!Work.IsComplete())
	{
		Work.Resume();
	}
	return bSuccess;
}

FVoxelGameThreadContinuation FVoxelViewPublisher::SetSharedCoverageSteps(
	const FGuid& InGroup, FVoxelViewSourceId InSourceId, TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, bool& OutSuccess)
{
	OutSuccess = false;
	FUpdate Source;
	if (!CaptureCoverageSource(InSourceId, true, Source)) co_return;
	TArray<FBox> LocalBoxes = Source.LocalWorldBoxes;
	co_await SetCoverageSourceSteps(InGroup, InSourceId, MoveTemp(Source), MoveTemp(LocalBoxes), MoveTemp(InCommonBoxes), OutSuccess);
}

bool FVoxelViewPublisher::SetHidden(const FGuid& InGroup, FVoxelViewSourceId InSourceId, const bool bInHidden)
{
	const FPendingGroup* Group = FindPendingGroup(InGroup);
	const FEntry* Entry = Entries.Find(FVoxelViewSourceId(InSourceId));
	if (!Group || Group->bCommitted || !Entry || Entry->GroupKey != Group->Key)
	{
		return false;
	}

	const bool bHidden = bInHidden || Entry->bRetiring;
	const FVoxelViewSourceId Key(InSourceId);
	if (const bool* Pending = PendingVisibility.Find(Key))
	{
		if (*Pending == bHidden)
		{
			return true;
		}
	}
	else if (Entry->bHidden == bHidden && !SourceVisibilityGenerations.Contains(Key) &&
		(!PendingPlan || !PendingPlan->Visibility.Contains(Key)))
	{
		return true;
	}

	if (!PendingUpdateIndices.Contains(Key))
	{
		FUpdate Source;
		if (CaptureCoverageSource(Key, true, Source))
		{
			// 可见性也进入同一原生粗层事务，记录标志不能独自代表呈现根中的移除。
			if (!FindProjectedUpdate(Key))
			{
				const FVector Origin = Source.Location / Module.BlockSize();
				const double Step = Source.Scale / Module.BlockSize();
				auto Combined = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>();
				Combined->Reserve(Source.PreviousExclusions.Num());
				for (const FBox& Box : Source.PreviousExclusions)
					Combined->Add(FBox(Origin + Box.Min * Step, Origin + Box.Max * Step));
				Source.CommonBoxes = MoveTemp(Combined);
			}
			Source.Exclusions = Source.PreviousExclusions;
			Source.bUnchanged = true;
			PendingUpdateIndices.Add(Key, PendingUpdates.Num());
			PendingUpdates.Add(MoveTemp(Source));
			OwnerSources.FindOrAdd(Group->Key).Add(Key);
		}
	}
	PendingVisibility.Add(Key, bHidden);
	return true;
}

uint64 FVoxelViewCoverageResult::GetAllocatedBytes() const
{
	return Meshes.GetAllocatedSize() + OwnedMeshBytes;
}

void FVoxelViewPublisher::SetCommitCallbacks(TFunction<void()> InOnCommitted,
	TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)> InOnGroupCommitted, TConstArrayView<FGuid> InGroups)
{
	// 每批拥有自己的回调和完成集合；下一局部 pass 不能改写尚未完成的上一批。
	struct FBatch
	{
		TSharedPtr<bool> Lifetime;
		TSet<FVoxelPublishGroupKey> Remaining;
		FGroupCallback Group;
		TFunction<void()> Finished;
		~FBatch()
		{
			if (Lifetime && *Lifetime) Finish();
		}
		void Finish()
		{
			Remaining.Empty(); Group = {};
			if (Finished) { auto Callback = MoveTemp(Finished); Callback(); }
		}
	};
	if (!CommitCallbackLifetime) CommitCallbackLifetime = MakeShared<bool>(true);
	const auto Batch = MakeShared<FBatch>();
	Batch->Lifetime = CommitCallbackLifetime;
	Batch->Remaining.Reserve(InGroups.IsEmpty() ? PendingGroups.Num() : InGroups.Num());
	Batch->Group = MoveTemp(InOnGroupCommitted);
	Batch->Finished = MoveTemp(InOnCommitted);
	const auto Callback = MakeShared<FGroupCallback>([Batch](TConstArrayView<FVoxelPublishGroupKey> Keys)
	{
		if (Batch->Group) Batch->Group(Keys);
		for (const auto& Key : Keys) Batch->Remaining.Remove(Key);
		if (Batch->Remaining.IsEmpty()) Batch->Finish();
	});
	if (InGroups.IsEmpty())
	{
		CurrentGroupCallback = Callback;
		for (auto& Pair : PendingGroups)
			if (!Pair.Value.bCommitted) { Pair.Value.Callback = Callback; Batch->Remaining.Add(Pair.Value.Key); }
	}
	else
	{
		CurrentGroupCallback.Reset();
		for (const auto& Handle : InGroups)
			if (auto* Group = FindPendingGroup(Handle)) { Group->Callback = Callback; Batch->Remaining.Add(Group->Key); }
	}
	if (InGroups.IsEmpty()) OnCommitted = [Batch] { Batch->Finish(); };
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
	if (const auto* Sources = OwnerSources.Find(Group->Key))
		for (const auto& SourceId : *Sources)
			if (PendingUpdateIndices.Contains(SourceId) || PendingVisibility.Contains(SourceId)) return false;
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
	InvalidateCommitReadiness();
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
	const int32 Stage = FMath::Min(InOutPriority.TerrainStage, InPriority.TerrainStage);
	if (FVoxelTerrainWorkSelection::HigherPriority(InPriority.Service, InOutPriority.Service))
	{
		InOutPriority = InPriority;
	}
	InOutPriority.TerrainStage = Stage;
}

bool FVoxelViewPublisher::HigherGroupPriority(const FGroupPriority& InFirst, const FGroupPriority& InSecond)
{
	return FVoxelTerrainWorkSelection::HigherPriority(InFirst.Service, InSecond.Service);
}

FVoxelViewPublisher::FGroupPriority FVoxelViewPublisher::BoundsPriority(const TConstArrayView<FBox> InBounds,
	const int32 InStage) const
{
	FGroupPriority Result;
	Result.TerrainStage = InStage;
	const double CellSize = Module.BlockSize();
	const auto& Policy = Module.GetViewSettings().Fine;
	for (const FBox& WorldBounds : InBounds)
	{
		if (!WorldBounds.IsValid) continue;
		const FBox Bounds(WorldBounds.Min / CellSize, WorldBounds.Max / CellSize);
		for (const auto& Pair : Module.GetCurrentInterest().Sources)
		{
			const auto& Source = Pair.Value.Source;
			if (!Source.Has(EVoxelStreamingCapability::FineVisual) ||
				(InStage > 0 && !Source.Has(EVoxelStreamingCapability::WorldVisual) &&
				 !Source.Has(EVoxelStreamingCapability::LocalRefinement))) continue;
			const FVector Position(Source.Center);
			FVector Nearest(FMath::Clamp(Position.X, Bounds.Min.X, Bounds.Max.X),
				FMath::Clamp(Position.Y, Bounds.Min.Y, Bounds.Max.Y), FMath::Clamp(Position.Z, Bounds.Min.Z, Bounds.Max.Z));
			if (InStage >= 2) Nearest.Z = Position.Z;
			const FVector Delta = Nearest - Position;
			FGroupPriority Candidate;
			Candidate.TerrainStage = InStage;
			Candidate.Service.DistanceCells = Delta.Size();
			Candidate.Service.ForwardScore = Candidate.Service.DistanceCells > UE_DOUBLE_SMALL_NUMBER ?
				FVector::DotProduct(Source.Direction.GetSafeNormal(), Delta / Candidate.Service.DistanceCells) : 1.0;
			Candidate.Service.SourcePriority = Source.GetSchedulingPriority();
			Candidate.Service.SourceStableKey = Source.Id;
			// 粗表示邻近观察源不构成 Core/Guard 依赖，不能因此抢占 Near 池。
			const bool bLocalRefinement = Source.Has(EVoxelStreamingCapability::LocalRefinement) &&
				!Source.Has(EVoxelStreamingCapability::WorldVisual);
			const bool bNear = !bLocalRefinement && InStage == 0 && Candidate.Service.DistanceCells <= Policy.CollisionGuardRadiusCells;
			Candidate.Service.Band = bNear ?
				(Candidate.Service.DistanceCells <= Policy.InteractiveRadiusCells ? EVoxelTerrainServiceBand::P0 : EVoxelTerrainServiceBand::P1) :
				(InStage == 0 ? EVoxelTerrainServiceBand::P2 : InStage == 1 ? EVoxelTerrainServiceBand::P3 :
				 InStage == 2 ? EVoxelTerrainServiceBand::P4 : EVoxelTerrainServiceBand::P5);
			Candidate.WorkClass = bNear ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
			Candidate.SourcePriority = Candidate.Service.SourcePriority;
			Candidate.DistanceScore = Candidate.Service.DistanceCells;
			Candidate.ObserverDistance = FMath::Square(Candidate.DistanceScore * CellSize);
			MergePriority(Result, Candidate);
		}
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
	InOutRequest.ForwardScore = Priority.Service.ForwardScore;
	InOutRequest.SourceStableKey = Priority.Service.SourceStableKey;
	InOutRequest.ServiceBand = Priority.Service.Band;
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
	PendingUpdateIndices.Remove(Update.SourceId);
	PendingUpdates.RemoveAtSwap(InIndex, 1, EAllowShrinking::No);
	if (PendingUpdates.IsValidIndex(InIndex)) PendingUpdateIndices.FindChecked(PendingUpdates[InIndex].SourceId) = InIndex;
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
		const auto* Sources = OwnerSources.Find(Owner.Key);
		if (!Sources) continue;
		for (const auto& SourceId : *Sources)
		{
		const auto* Found = Entries.Find(SourceId);
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
		const FEntry* Entry = Entries.Find(PendingUpdates[Index].SourceId);
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
		if (const auto* Sources = OwnerSources.Find(Footprint.Key))
			for (const auto& SourceId : *Sources)
			{
				Plan.SourceKeys.Add(SourceId, Footprint.Key);
				const auto* Entry = Entries.Find(SourceId);
				if (!Entry) continue;
				const int32* UpdateIndex = Plan.UpdateIndices.Find(SourceId);
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
		if (!Entries.Contains(Update.SourceId)) return;
		const auto* Key = Plan.SourceKeys.Find(Update.SourceId);
		const int32* GroupIndex = Key ? Plan.SelectedKeys.Find(*Key) : nullptr;
		if (GroupIndex)
		{
			Update.GroupIndex = *GroupIndex;
			const int32 Index = Plan.Publication->Updates.Add(MoveTemp(Update));
			Plan.Publication->PreparedMeshes.Add(nullptr);
			IndexPublicationUpdate(Plan.Publication, Index);
			Plan.Publication->Groups[*GroupIndex].UpdateIndices.Add(Index);
		}
		else if (Key && !PendingUpdateIndices.Contains(Update.SourceId))
		{
			PendingUpdateIndices.Add(Update.SourceId, PendingUpdates.Num());
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
		if (!Entries.Contains(Pair.Key)) return;
		const auto* Key = Plan.SourceKeys.Find(Pair.Key);
		const int32* GroupIndex = Key ? Plan.SelectedKeys.Find(*Key) : nullptr;
		if (GroupIndex)
		{
			Plan.Publication->Visibility.Add(Pair);
			SourceVisibilityGenerations.FindOrAdd(Pair.Key).Add({Plan.Publication, Pair.Value});
			Plan.Publication->Groups[*GroupIndex].VisibilitySources.Add(Pair.Key);
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
	auto& SourceIndices = Plan.SourceIndices;
	auto& Updates = Plan.Updates;
	constexpr bool bInPreparedCoverage = true;
	const auto Next = [&Plan] { ++Plan.Phase; Plan.Cursor = 0; };
	if (Plan.Phase == 0)
	{
		if (!Publication->Updates.IsValidIndex(Plan.Cursor)) { Next(); return; }
		const FUpdate& Update = Publication->Updates[Plan.Cursor++];
		Updates.Add(Update.SourceId, &Update);
		return;
	}
	auto AddSource = [this, &Footprints, &SourceIndices, &Updates, &Publication, &Plan](
		const FVoxelViewSourceId SourceId)
	{
		if (SourceIndices.Contains(SourceId)) return;
		const FEntry* Entry = Entries.Find(SourceId);
		const int32 FirstFootprint = Footprints.Num();
		FVoxelPublishFootprint& Footprint = Footprints.AddDefaulted_GetRef();
		if (Entry)
		{
			Footprint.Key = Entry->GroupKey;
			const FUpdate* const* Update = Updates.Find(SourceId);
			const bool* Hidden = Publication->Visibility.Find(SourceId);
			bool PreviousHidden = Entry->bHidden;
			uint64 PreviousVisibilitySerial = 0;
			if (const auto* Generations = SourceVisibilityGenerations.Find(SourceId))
				for (const auto& Generation : *Generations)
				{
					const auto& Older = Generation.Publication;
					if (Older->bCanceled || Older->bFinished || Older->Serial >= Publication->Serial || Older->Serial <= PreviousVisibilitySerial) continue;
					PreviousHidden = Generation.bHidden;
					PreviousVisibilitySerial = Older->Serial;
				}
			const bool bVisibilityChange = Hidden && Entry && PreviousHidden != *Hidden;
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
		SourceIndices.Add(SourceId, FirstFootprint);
		Plan.SourceKeys.Add(Footprints[FirstFootprint].Key);
	};
	if (Plan.Phase == 1)
	{
		if (!Publication->Updates.IsValidIndex(Plan.Cursor)) { Next(); return; }
		AddSource(Publication->Updates[Plan.Cursor++].SourceId);
		return;
	}
	if (Plan.Phase == 2)
	{
		if (Plan.Cursor >= Publication->Visibility.GetMaxIndex()) { Next(); return; }
		const auto Id = FSetElementId::FromInteger(Plan.Cursor++);
		if (Publication->Visibility.IsValidId(Id)) AddSource(Publication->Visibility.Get(Id).Key);
		return;
	}
	if (Plan.Phase == 3)
	{
		if (Plan.Cursor >= Publication->Callbacks.GetMaxIndex()) { Next(); return; }
		const auto Id = FSetElementId::FromInteger(Plan.Cursor++);
		if (!Publication->Callbacks.IsValidId(Id)) return;
		const auto& Key = Publication->Callbacks.Get(Id).Key;
		if (Plan.SourceKeys.Contains(Key)) return;
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
		Update.GroupIndex = Plan.Graph.GetGroupIndices()[SourceIndices.FindChecked(Update.SourceId)];
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
		const auto SourceId = Publication->Visibility.Get(Id).Key;
		Plan.Groups[Plan.Graph.GetGroupIndices()[SourceIndices.FindChecked(SourceId)]].VisibilitySources.Add(SourceId);
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
	TMap<FVoxelViewSourceId, int32> SourceIndices;
	TMap<FVoxelViewSourceId, const FUpdate*> Updates;
	for (const FUpdate& Update : Publication->Updates) Updates.Add(Update.SourceId, &Update);
	auto AddSource = [this, &Footprints, &SourceIndices, &Updates, &Publication, bInPreparedCoverage](
		const FVoxelViewSourceId SourceId)
	{
		if (SourceIndices.Contains(SourceId)) return;
		const FEntry* Entry = Entries.Find(SourceId);
		const int32 FirstFootprint = Footprints.Num();
		FVoxelPublishFootprint& Footprint = Footprints.AddDefaulted_GetRef();
		if (Entry)
		{
			Footprint.Key = Entry->GroupKey;
			const FUpdate* const* Update = Updates.Find(SourceId);
			const bool* Hidden = Publication->Visibility.Find(SourceId);
			bool PreviousHidden = Entry->bHidden;
			uint64 PreviousVisibilitySerial = 0;
			if (const auto* Generations = SourceVisibilityGenerations.Find(SourceId))
				for (const auto& Generation : *Generations)
				{
					const auto& Older = Generation.Publication;
					if (Older->bCanceled || Older->bFinished || Older->Serial >= Publication->Serial || Older->Serial <= PreviousVisibilitySerial) continue;
					PreviousHidden = Generation.bHidden;
					PreviousVisibilitySerial = Older->Serial;
				}
			const bool bVisibilityChange = Hidden && Entry && PreviousHidden != *Hidden;
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
		SourceIndices.Add(SourceId, FirstFootprint);
	};
	for (const FUpdate& Update : Publication->Updates) AddSource(Update.SourceId);
	for (const auto& Pair : Publication->Visibility) AddSource(Pair.Key);
	// 空网格仍承担空间所有权，必须参与父子组并集；独立追加会提前交出旧覆盖。
	TSet<FVoxelPublishGroupKey> SourceKeys;
	for (const auto& Footprint : Footprints) SourceKeys.Add(Footprint.Key);
	for (const auto& Pair : Publication->Callbacks)
	{
		if (SourceKeys.Contains(Pair.Key)) continue;
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
		Update.GroupIndex = GroupIndices[SourceIndices.FindChecked(Update.SourceId)];
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
		Publication->Groups[GroupIndices[SourceIndices.FindChecked(Pair.Key)]].VisibilitySources.Add(Pair.Key);
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
	TSet<FVoxelViewSourceId> DeferredSources;
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
		for (const int32 UpdateIndex : Group.UpdateIndices) DeferredSources.Add(Publication->Updates[UpdateIndex].SourceId);
		for (const auto SourceId : Group.VisibilitySources)
		{
			if (!PendingVisibility.Contains(SourceId)) PendingVisibility.Add(SourceId, Publication->Visibility.FindChecked(SourceId));
			Publication->Visibility.Remove(SourceId);
		}
	}
	TArray<FUpdate> Updates;
	for (FUpdate& Update : Publication->Updates)
	{
		if (!DeferredSources.Contains(Update.SourceId))
		{
			Update.GroupIndex = Remap[Update.GroupIndex];
			Updates.Add(MoveTemp(Update));
			continue;
		}
		if (!PendingUpdateIndices.Contains(Update.SourceId))
		{
			// Exclusions 已转换为网格局部坐标；重试从原始世界格输入重新比较最新前代。
			Update.Exclusions = Update.LocalWorldBoxes;
			Update.PreviousExclusions.Reset();
			Update.ChangedBounds.Reset();
			Update.bUnchanged = false;
			Update.GroupIndex = INDEX_NONE;
			PendingUpdateIndices.Add(Update.SourceId, PendingUpdates.Num());
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
			bCommitOrderDirty = true;
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
		Next();
		return;
	}
	if (Plan.Phase == 9)
	{
		if (ReadyCommitGroups.IsValidIndex(Plan.Cursor))
		{
			const FReadyGroup& Existing = ReadyCommitGroups[Plan.Cursor];
			if (Existing.PriorityRevision == Plan.Revision)
			{
				++Plan.Cursor;
				return;
			}
			FReadyGroup Ready = Existing;
			ReadyCommitGroups.RemoveAt(Plan.Cursor, 1, EAllowShrinking::No);
			if (Ready.Publication->bCanceled || !Ready.Publication->Groups.IsValidIndex(Ready.GroupIndex))
			{
				return;
			}
			Ready.Priority = Ready.Publication->Groups[Ready.GroupIndex].Priority.Service;
			Ready.PriorityRevision = Plan.Revision;
			InsertReadyCommit(MoveTemp(Ready));
			return;
		}
		bCommitOrderDirty = false;
		InvalidateCommitReadiness();
		Next();
		return;
	}
	if (Plan.Phase == 10)
	{
		if (ReadyPrepareGroups.IsValidIndex(Plan.Cursor))
		{
			FReadyGroup Ready = ReadyPrepareGroups[Plan.Cursor];
			if (Ready.PriorityRevision == Plan.Revision) { ++Plan.Cursor; return; }
			ReadyPrepareGroups.RemoveAt(Plan.Cursor, 1, EAllowShrinking::No);
			if (Ready.Publication->bCanceled || !Ready.Publication->Groups.IsValidIndex(Ready.GroupIndex)) return;
			Ready.Priority = Ready.Publication->Groups[Ready.GroupIndex].Priority.Service;
			Ready.PriorityRevision = Plan.Revision;
			InsertReadyPrepare(MoveTemp(Ready));
			return;
		}
		for (const auto& Resolved : Plan.Groups)
			if (Resolved.IsCurrent()) Resolved.Key->Groups[Resolved.Value].bPriorityReady = true;
		check(PrepareReadinessRevision < MAX_uint64);
		++PrepareReadinessRevision;
		ReadySelectionCursors[0] = 0;
		InvalidateCommitReadiness();
		Next();
		return;
	}
	if (Plan.Phase == 11)
	{
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
	const auto FindNativePublication = [this, &Active](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp) -> const FPublication*
	{
		if (Kind != EVoxelTaskKind::BuildViewCoverage || Stamp.WorldEpoch != WorldEpoch ||
			Stamp.OwnerId != NativeOwnerId || Stamp.Section.X != MIN_int32 || Stamp.Section.Y != 7) return nullptr;
		const FPublication* const* Found = Active.Find(Stamp.OwnerIncarnation);
		if (!Found || !(**Found).Updates.IsValidIndex(Stamp.Section.Z)) return nullptr;
		const auto& Update = (**Found).Updates[Stamp.Section.Z];
		if (!(**Found).Groups.IsValidIndex(Update.GroupIndex)) return nullptr;
		FVoxelTaskStamp Expected = Update.NativeStamp;
		Expected.OwnerId = NativeOwnerId;
		Expected.OwnerIncarnation = (**Found).Serial;
		Expected.Section = FIntVector(MIN_int32, 7, Stamp.Section.Z);
		return Expected == Stamp ? *Found : nullptr;
	};
	Scheduler.UpdateServicePriorities([this, &FindNativePublication](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, FVoxelTerrainServicePriority& ServicePriority)
	{
		const FPublication* Publication = FindNativePublication(Kind, Stamp);
		if (!Publication) return;
		const int32 GroupIndex = Publication->Updates[Stamp.Section.Z].GroupIndex;
		FVoxelTaskRequest Priority;
		Priority.WorkClass = EVoxelWorkClass::Visible;
		SetPublicationPriority(Priority, *Publication, MakeArrayView(&GroupIndex, 1));
		WorkClass = Priority.WorkClass;
		ServicePriority = FVoxelTaskScheduler::ResolveServicePriority(Priority);
	});
	Scheduler.UpdateTerrainStages([&FindNativePublication](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp, int32& TerrainStage)
	{
		const FPublication* Publication = FindNativePublication(Kind, Stamp);
		if (!Publication) return;
		TerrainStage = Publication->Groups[Publication->Updates[Stamp.Section.Z].GroupIndex].Priority.TerrainStage;
	});
}

FVoxelGameThreadContinuation FVoxelViewPublisher::PrepareCoverageSteps(FPublication& InPublication)
{
	for (; InPublication.CoverageIndex < InPublication.Updates.Num(); ++InPublication.CoverageIndex)
	{
		FUpdate& Update = InPublication.Updates[InPublication.CoverageIndex];
		FUpdate Previous;
		bool bHasPrevious = false;
		for (;;)
		{
			const FPublication* Older = nullptr;
			const FUpdate* Projected = FindProjectedUpdate(Update.SourceId, InPublication.Serial, &Older);
			if (Older && !Older->bCoveragePrepared)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				continue;
			}
			if (Projected) { Previous = *Projected; bHasPrevious = true; }
			else if (const FEntry* Entry = Entries.Find(Update.SourceId); Entry && Entry->PresentedSource)
			{
				Previous.Source = Entry->PresentedSource;
				Previous.NativeStamp = Entry->PresentedNativeStamp;
				Previous.Exclusions = Entry->Exclusions;
				Previous.Location = Entry->NativeMesh ? Entry->NativeMesh->WorldCellOrigin * Module.BlockSize() : Entry->Location;
				Previous.Scale = Entry->NativeMesh ? Entry->NativeMesh->CellScale * Module.BlockSize() : Entry->Scale;
				Previous.NativeMesh = Entry->NativeMesh;
				bHasPrevious = true;
			}
			break;
		}
		const FVector Origin = Update.Location / Module.BlockSize();
		const double Step = Update.Scale / Module.BlockSize();
		TArray<FBox> WorldBoxes;
		co_await VoxelGameThread::AppendSteps(WorldBoxes, Update.LocalWorldBoxes);
		if (Update.CommonBoxes) co_await VoxelGameThread::AppendSteps(WorldBoxes, *Update.CommonBoxes);
		Update.Exclusions.Reset();
		for (const FBox& Box : WorldBoxes)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const FBox Local((Box.Min - Origin) / Step, (Box.Max - Origin) / Step);
			if (Update.Bounds.Intersect(Local)) Update.Exclusions.Add(Local);
		}
		co_await VoxelMeshClipper::NormalizeBoxesSteps(Update.Exclusions);
		Update.bSourceDirty = !bHasPrevious || Previous.Source != Update.Source || Previous.NativeStamp != Update.NativeStamp ||
			Previous.Location != Update.Location || Previous.Scale != Update.Scale;
		Update.PreviousExclusions = MoveTemp(Previous.Exclusions);
		co_await FindCoverageChangesSteps(Update.PreviousExclusions, Update.Exclusions, Update.Bounds, Update.ChangedBounds);
		Update.bUnchanged = !Update.bSourceDirty && Update.ChangedBounds.IsEmpty();
		if (!Update.bSourceDirty && !Update.NativeMesh) Update.NativeMesh = Previous.NativeMesh;
		for (FBox& Box : Update.ChangedBounds)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			Box = FBox(Update.Location + Box.Min * Update.Scale, Update.Location + Box.Max * Update.Scale);
		}
	}
	InPublication.bCoveragePrepared = true;
	InPublication.bCoveragePreparing = false;
}

bool FVoxelViewPublisher::AdmitCoveragePreparation(const FPublicationRef& InPublication, const double InDeadline)
{
	if (InPublication->bCoveragePrepared || InPublication->bCanceled || FPlatformTime::Seconds() >= InDeadline) return false;
	bool bAdvanced = false;
	if (!InPublication->CoverageWork.IsValid())
	{
		InPublication->CoverageWork = PrepareCoverageSteps(InPublication.Get());
		InPublication->bCoveragePreparing = true;
		bAdvanced = true;
	}
	const int32 End = FMath::Min(InPublication->Updates.Num(), InPublication->CoverageIndex +
		FMath::Max(1, Module.GetViewSettings().Budget.CoveragePreparationBatchSize));
	// 正文仍由原生 Atom 根处理；此游标只恢复所有权规划所需的覆盖元数据。
	for (int32 Steps = 0; Steps < 64 && InPublication->CoverageIndex < End &&
		FPlatformTime::Seconds() < InDeadline && !InPublication->CoverageWork.IsComplete(); ++Steps)
	{
		InPublication->CoverageWork.Resume();
		bAdvanced = true;
	}
	if (InPublication->CoverageIndex == InPublication->Updates.Num() && !InPublication->CoverageWork.IsComplete() &&
		FPlatformTime::Seconds() < InDeadline)
	{
		InPublication->CoverageWork.Resume();
		bAdvanced = true;
	}
	if (InPublication->CoverageWork.IsComplete()) InPublication->CoverageWork.Reset();
	return bAdvanced;
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

bool FVoxelViewPublisher::AdmitBuilds(const FPublicationRef& InPublication, const double InDeadline)
{
	if (!InPublication->bCoveragePrepared || !InPublication->bGroupsPrepared ||
		FPlatformTime::Seconds() >= InDeadline) return false;
	bool bAdvanced = false;
	const auto Before = [this, &InPublication](const int32 InA, const int32 InB)
	{
		return HigherBuildPriority(*InPublication, InA, InB);
	};
	if (!InPublication->bBuildOrderInitialized)
	{
		if (InPublication->Updates.IsValidIndex(InPublication->BuildInitializationIndex))
		{
			InPublication->BuildOrder.HeapPush(InPublication->BuildInitializationIndex++, Before);
			return true;
		}
		InPublication->bBuildOrderInitialized = true;
		bAdvanced = true;
		InPublication->bBuildOrderDirty = false;
	}
	if (InPublication->bBuildOrderDirty)
	{
		if (InPublication->BuildHeapifyIndex == INDEX_NONE) InPublication->BuildHeapifyIndex = InPublication->BuildOrder.Num() / 2;
		if (InPublication->BuildHeapifyIndex >= 0)
		{
			int32 Parent = InPublication->BuildHeapifyIndex--;
			while (2 * Parent + 1 < InPublication->BuildOrder.Num())
			{
				int32 Child = 2 * Parent + 1;
				if (Child + 1 < InPublication->BuildOrder.Num() && Before(InPublication->BuildOrder[Child + 1], InPublication->BuildOrder[Child])) ++Child;
				if (!Before(InPublication->BuildOrder[Child], InPublication->BuildOrder[Parent])) break;
				InPublication->BuildOrder.Swap(Child, Parent);
				Parent = Child;
			}
		}
		if (InPublication->BuildHeapifyIndex < 0) InPublication->bBuildOrderDirty = false;
		return true;
	}
	const double Deadline = FMath::Min(InDeadline, Module.GetTerrainFrameContext().Deadline());
	for (int32 Count = 0; Count < 8 && !InPublication->BuildOrder.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Count)
	{
		int32 Index = INDEX_NONE;
		InPublication->BuildOrder.HeapPop(Index, Before, EAllowShrinking::No);
		bAdvanced = true;
		const auto& Update = InPublication->Updates[Index];
		if (InPublication->Groups[Update.GroupIndex].bCommitted) continue;
		// 所有权由原生 Atom 根裁定，粗网格不再按 Fine/Page 可见性重复重裁整块网格。
		InPublication->PreparedMeshes[Index] = Update.Source;
		QueueReadyGroup(InPublication, Update.GroupIndex);
	}
	return bAdvanced;
}
void FVoxelViewPublisher::Tick()
{
	Tick(Module.GetTerrainFrameContext().GetPublishBudget());
}

bool FVoxelViewPublisher::CommitReadyGroups(const FPublicationRef& Publication, int32& CommittedThisFrame, const double Deadline,
	const TMap<FVoxelPublishGroupKey, uint64>& FirstOwners, const int32 InGroupIndex)
{
	if (!Publication->bGroupsPrepared) return false;
	bool bAdvanced = false;
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
				ReleaseSourceGeneration(Publication->Updates[Index].SourceId, Publication->Serial);
				for (const auto& Component : Publication->Updates[Index].Components) if (Component) Component->DestroyComponent();
				Publication->Updates[Index].Components.Reset();
				Publication->Updates[Index].Source.Reset();
				Publication->Updates[Index].NativeMesh.Reset();
				if (Publication->PreparedMeshes.IsValidIndex(Index)) Publication->PreparedMeshes[Index].Reset();
			}
			for (const auto SourceId : Group.VisibilitySources) ReleaseSourceGeneration(SourceId, Publication->Serial);
			Group.bCommitted = true;
			ReleaseNativeGroup(Group);
			for (const auto& Key : Group.Keys) SetOwnerActive(Key, Publication->Serial, false);
			bFirstOwnersDirty = true;
			bActiveBusyKeysDirty = true;
			bAdvanced = true;
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
		}) || Group.VisibilitySources.ContainsByPredicate([this, &Publication](const FVoxelViewSourceId& Key)
		{
			const FEntry* Entry = Entries.Find(Key);
			const bool* Hidden = Publication->Visibility.Find(Key);
			return Entry && Hidden && Entry->bHidden != *Hidden;
		});
		if (bHasChanges && CommittedThisFrame >= Limit) continue;
		bAdvanced |= CommitReadyGroup(Publication, Group);
		if (Group.bCommitted) CommittedThisFrame += bHasChanges ? 1 : 0;
		if (FPlatformTime::Seconds() >= Deadline) break;
	}
	if (InGroupIndex != INDEX_NONE && Publication->Groups[InGroupIndex].bCommitted) Publication->GroupOrder.Remove(InGroupIndex);
	if (!Publication->GroupOrder.ContainsByPredicate([&Publication](const int32 Index) { return !Publication->Groups[Index].bCommitted; }))
	{
		Publication->bFinished = true;
	}
	return bAdvanced;
}

bool FVoxelViewPublisher::CommitReadyGroup(const FPublicationRef& Publication, FGroup& InGroup)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewCommit);
	bool bAdvanced = false;
	if (!AdvanceNativeGroup(Publication, InGroup, bAdvanced)) return bAdvanced;
	const double CommitStart = FPlatformTime::Seconds();
	for (const int32 Index : InGroup.UpdateIndices)
	{
		FUpdate& Update = Publication->Updates[Index];
		ReleaseSourceGeneration(Update.SourceId, Publication->Serial);
		FEntry* Entry = Entries.Find(Update.SourceId);
		if (!Entry) continue;
		Entry->LocalWorldBoxes = MoveTemp(Update.LocalWorldBoxes);
		Update.CommonBoxes.Reset();
		const bool bRetainedGeometry = !Update.bSourceDirty && Entry->NativeMesh &&
			Entry->PresentedSource == Update.Source && Entry->PresentedNativeStamp == Update.NativeStamp &&
			Entry->NativeMesh->WorldCellOrigin == Update.Location / Module.BlockSize() &&
			Entry->NativeMesh->CellScale == Update.Scale / Module.BlockSize();
		if (Update.bUnchanged || bRetainedGeometry)
		{
			Entry->Exclusions = MoveTemp(Update.Exclusions);
			Entry->bDirty = Entry->Source != Update.Source || Entry->Location != Update.Location || Entry->Scale != Update.Scale;
			continue;
		}
		Entry->NativeMesh = MoveTemp(Update.NativeMesh);
		Entry->PresentedNativeStamp = Update.NativeStamp;
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
	for (const FVoxelViewSourceId SourceKey : InGroup.VisibilitySources)
	{
		ReleaseSourceGeneration(SourceKey, Publication->Serial);
		const bool* bHidden = Publication->Visibility.Find(SourceKey);
		if (FEntry* Entry = Entries.Find(SourceKey); Entry && bHidden)
		{
			Entry->bHidden = *bHidden;
		}
		Publication->Visibility.Remove(SourceKey);
	}
	InGroup.bCommitted = true;
	ReleaseNativeGroup(InGroup);
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
		Publication->Updates[Index].NativeMesh.Reset();
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
	return true;
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
	check(ActiveNativeGroups == 0);
	Publications.Reset();
	SourceUpdateGenerations.Reset();
	SourceVisibilityGenerations.Reset();
	ReadyPrepareGroups.Reset();
	ReadyCommitGroups.Reset();
	ReadySelectionCursors[0] = 0;
	ReadySelectionCursors[1] = 0;
	PublisherTickSerial = 0;
	CommitResourceVersion = 0;
	CommitFrontendVersion = 0;
	bCommitOrderDirty = false;
	bCommitSelectionComplete = false;
	bCommitNeedsPolling = false;
	verify(PrepareSelection.Reset());
	verify(CommitSelection.Reset());
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

void FVoxelViewPublisher::DiscardPublication(const FPublicationRef& Publication, const bool bInAllowRetry)
{
	if (Publication->bCanceled) return;
	Publication->CoverageWork.Reset();
	Publication->bCanceled = true;
	// 阻塞中的索引 Worker 也持有 Publication，必须同时撤销，不能等容量释放才退出。
	if (Publication->Updates.ContainsByPredicate([](const FUpdate& Update) { return Update.bNativeMeshPending; }))
		Scheduler.CancelMatching([this, Serial = Publication->Serial](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildViewCoverage && Stamp.WorldEpoch == WorldEpoch &&
				Stamp.OwnerId == NativeOwnerId && Stamp.OwnerIncarnation == Serial;
		});
	for (auto& Group : Publication->Groups) ReleaseNativeGroup(Group);
	for (const auto& Group : Publication->Groups) if (!Group.bCommitted)
		for (const auto& Key : Group.Keys)
		{
			SetOwnerActive(Key, Publication->Serial, false);
			if (bInAllowRetry) RetryOwners.AddUnique(Key);
		}
	bFirstOwnersDirty = true;
	++PriorityRevision;
	bPrioritiesDirty = true;
	bActiveBusyKeysDirty = true;
	for (FUpdate& Update : Publication->Updates)
	{
		ReleaseSourceGeneration(Update.SourceId, Publication->Serial);
		Update.NativeMesh.Reset();
		Update.Source.Reset();
		if (!Entries.Contains(Update.SourceId)) continue;
		for (UVoxelMeshComponent* Component : Update.Components)
		{
			if (Component) RetiredComponents.Add(Component);
		}
		Update.Components.Reset();
	}
	for (const auto& Pair : Publication->Visibility) ReleaseSourceGeneration(Pair.Key, Publication->Serial);
	Publication->PreparedMeshes.Reset();
}

void FVoxelViewPublisher::ReleaseNativeGroup(FGroup& InGroup)
{
	if (InGroup.bNativeAdmitted)
	{
		check(ActiveNativeGroups > 0);
		--ActiveNativeGroups;
		InGroup.bNativeAdmitted = false;
		ReadySelectionCursors[0] = 0;
		check(PrepareReadinessRevision < MAX_uint64);
		++PrepareReadinessRevision;
		InvalidateCommitReadiness();
	}
	InGroup.NativeLastChunk.Reset();
	InGroup.NativeCoarse.Reset();
	InGroup.NativeChanges.Empty();
	InGroup.NativeGroupLease.Reset();
}

void FVoxelViewPublisher::Forget(FVoxelViewSourceId InSourceId)
{
	const FVoxelViewSourceId Key(InSourceId);
	// 值标识不会像弱 UObject 引用一样自动失效，撤销仍引用它的在途事务。
	for (const auto& Publication : Publications)
	{
		if (Publication->bCanceled || Publication->bFinished) continue;
		if (Publication->Updates.ContainsByPredicate([&](const FUpdate& Update)
		{
			return Update.SourceId == Key && Publication->Groups.IsValidIndex(Update.GroupIndex) &&
				!Publication->Groups[Update.GroupIndex].bCommitted;
		}) || Publication->Visibility.Contains(Key))
		{
			DiscardPublication(Publication);
		}
	}
	PendingVisibility.Remove(Key);
	if (const int32* Index = PendingUpdateIndices.Find(Key)) TakePendingUpdate(*Index);
	if (const auto* Entry = Entries.Find(Key))
		if (auto* Sources = OwnerSources.Find(Entry->GroupKey)) { Sources->Remove(Key); if (Sources->IsEmpty()) OwnerSources.Remove(Entry->GroupKey); }
	Entries.Remove(Key);
	SourceUpdateGenerations.Remove(Key);
	SourceVisibilityGenerations.Remove(Key);
}

bool FVoxelViewPublisher::Retire(FVoxelViewSourceId InSourceId)
{
	FEntry* Entry = Entries.Find(InSourceId);
	if (!Entry) return true;
	if (IsGroupBusy(Entry->GroupKey)) return false;
	if (!Entry->bPresented || Entry->bHidden)
	{
		Forget(InSourceId);
		return true;
	}
	// 先取得原生根移除的真实回执，再释放源记录及生成层的所有权。
	Entry->bRetiring = true;
	const FGuid Group = BeginGroup(Entry->GroupKey);
	SetTerrainStage(Group, Entry->TerrainStage);
	if (!SetHidden(Group, InSourceId, true)) return false;
	CommitGroup(Group);
	return false;
}

void FVoxelViewPublisher::Reset()
{
	if (CommitCallbackLifetime) *CommitCallbackLifetime = false;
	CommitCallbackLifetime.Reset();
	if (TerrainRenderer) TerrainRenderer->Reset();
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
	OwnerSources.Reset();
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

bool FVoxelViewPublisher::IsHidden(FVoxelViewSourceId InSourceId) const
{
	const FEntry* Entry = Entries.Find(InSourceId);
	return !Entry || Entry->bHidden;
}

bool FVoxelViewPublisher::IsPresented(FVoxelViewSourceId InSourceId) const
{
	const FEntry* Entry = Entries.Find(FVoxelViewSourceId(InSourceId));
	return Entry && Entry->bPresented && Entry->NativeMesh;
}

bool FVoxelViewPublisher::IsCommitted(FVoxelViewSourceId InSourceId) const
{
	const FEntry* Entry = Entries.Find(FVoxelViewSourceId(InSourceId));
	return Entry && Entry->bPresented && !Entry->bDirty;
}

bool FVoxelViewPublisher::HasPresentation(FVoxelViewSourceId InSourceId) const
{
	const FEntry* Entry = Entries.Find(FVoxelViewSourceId(InSourceId));
	return Entry && Entry->bPresented;
}

void FVoxelViewPublisher::IndexPublicationUpdate(const FPublicationRef& Publication, const int32 InIndex)
{
	const auto SourceId = Publication->Updates[InIndex].SourceId;
	if (!Entries.Contains(SourceId)) return;
	auto& Queue = SourceUpdateGenerations.FindOrAdd(SourceId);
	if (!Queue.ContainsByPredicate([&Publication](const auto& Generation) { return Generation.Publication->Serial == Publication->Serial; }))
		Queue.Add({Publication, InIndex});
}

void FVoxelViewPublisher::ReleaseSourceGeneration(const FVoxelViewSourceId InSourceId, const uint64 InSerial)
{
	if (auto* Queue = SourceUpdateGenerations.Find(InSourceId))
	{
		Queue->RemoveAll([InSerial](const auto& Generation) { return Generation.Publication->Serial == InSerial; });
		if (Queue->IsEmpty()) SourceUpdateGenerations.Remove(InSourceId);
	}
	if (auto* Queue = SourceVisibilityGenerations.Find(InSourceId))
	{
		Queue->RemoveAll([InSerial](const auto& Generation) { return Generation.Publication->Serial == InSerial; });
		if (Queue->IsEmpty()) SourceVisibilityGenerations.Remove(InSourceId);
	}
}

bool FVoxelViewPublisher::HigherReadyCommit(const FReadyGroup& InFirst, const FReadyGroup& InSecond)
{
	const auto& First = InFirst.Priority;
	const auto& Second = InSecond.Priority;
	const bool bFirstSafety = FVoxelTerrainWorkSelection::IsNear(First.Band);
	const bool bSecondSafety = FVoxelTerrainWorkSelection::IsNear(Second.Band);
	if (bFirstSafety != bSecondSafety) return bFirstSafety;
	if (bFirstSafety && First.Band != Second.Band) return uint8(First.Band) < uint8(Second.Band);
	if (First.DistanceCells != Second.DistanceCells) return First.DistanceCells < Second.DistanceCells;
	if (First.SourcePriority != Second.SourcePriority) return First.SourcePriority < Second.SourcePriority;
	if (First.ForwardScore != Second.ForwardScore) return First.ForwardScore > Second.ForwardScore;
	if (InFirst.Publication->Serial != InSecond.Publication->Serial)
		return InFirst.Publication->Serial < InSecond.Publication->Serial;
	return InFirst.GroupIndex < InSecond.GroupIndex;
}

void FVoxelViewPublisher::InvalidateCommitReadiness()
{
	check(CommitReadinessRevision < MAX_uint64);
	++CommitReadinessRevision;
	ReadySelectionCursors[1] = 0;
	bCommitSelectionComplete = false;
}

void FVoxelViewPublisher::InsertReadyCommit(FReadyGroup InReady)
{
	int32 First = 0;
	int32 Last = ReadyCommitGroups.Num();
	while (First < Last)
	{
		const int32 Middle = First + (Last - First) / 2;
		if (HigherReadyCommit(ReadyCommitGroups[Middle], InReady)) First = Middle + 1;
		else Last = Middle;
	}
	ReadyCommitGroups.Insert(MoveTemp(InReady), First);
	check(CommitReadinessRevision < MAX_uint64);
	++CommitReadinessRevision;
	ReadySelectionCursors[1] = FMath::Min(ReadySelectionCursors[1], First);
	bCommitSelectionComplete = false;
}

void FVoxelViewPublisher::InsertReadyPrepare(FReadyGroup InReady)
{
	int32 First = 0;
	int32 Last = ReadyPrepareGroups.Num();
	while (First < Last)
	{
		const int32 Middle = First + (Last - First) / 2;
		if (HigherReadyCommit(ReadyPrepareGroups[Middle], InReady)) First = Middle + 1;
		else Last = Middle;
	}
	ReadyPrepareGroups.Insert(MoveTemp(InReady), First);
	ReadySelectionCursors[0] = FMath::Min(ReadySelectionCursors[0], First);
	check(PrepareReadinessRevision < MAX_uint64);
	++PrepareReadinessRevision;
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
			InsertReadyCommit({InPublication, InGroupIndex, Group.Priority.Service, MAX_uint64});
		}
	}
	else if (!Group.bPrepareQueued && InPublication->PreparedMeshes[Group.UpdateIndices[Group.PreparedUpdates]] &&
		!InPublication->Updates[Group.UpdateIndices[Group.PreparedUpdates]].bNativeMeshPending)
	{
		Group.bPrepareQueued = true;
		InsertReadyPrepare({InPublication, InGroupIndex, Group.Priority.Service, MAX_uint64});
	}
}

bool FVoxelViewPublisher::HasOlderDependency(const FPublicationRef& InPublication, const FGroup& InGroup,
	bool* bOutNeedsContinuation) const
{
	if (bOutNeedsContinuation) *bOutNeedsContinuation = false;
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
			{
				if (bOutNeedsContinuation) *bOutNeedsContinuation = true;
				return true;
			}
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
			{
				if (bOutNeedsContinuation) *bOutNeedsContinuation = true;
				return true;
			}
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

int32 FVoxelViewPublisher::SelectReadyGroup(TConstArrayView<FReadyGroup> InReady, FVoxelTerrainWorkSelection& InSelection,
	FVoxelTerrainServiceCharge& OutCharge, const bool bInCommit, const double InDeadline, bool* bOutAdvanced)
{
	OutCharge = {};
	if (bOutAdvanced) *bOutAdvanced = false;
	FVoxelTerrainServiceCandidateSet Candidates;
	int32& Cursor = ReadySelectionCursors[bInCommit ? 1 : 0];
	if (bInCommit && (bCommitOrderDirty || bCommitSelectionComplete)) return INDEX_NONE;
	const int32 CandidateCount = FMath::Max(0, InReady.Num() - Cursor);
	int32 FirstRunnableIndex = INDEX_NONE;
	for (int32 Attempt = 0; Attempt < CandidateCount && FPlatformTime::Seconds() < InDeadline; ++Attempt)
	{
		Cursor %= InReady.Num();
		const int32 Index = Cursor++;
		const FReadyGroup& Ready = InReady[Index];
		if (Ready.Publication->bCanceled || !Ready.Publication->Groups.IsValidIndex(Ready.GroupIndex))
		{
			if (bInCommit) Cursor = 0;
			return Index;
		}
		const FGroup& Group = Ready.Publication->Groups[Ready.GroupIndex];
		if (Group.bCommitted)
		{
			if (bInCommit) Cursor = 0;
			return Index;
		}
		if (PublisherTickSerial != 0 &&
			(bInCommit ? Group.CommitWaitTick : Group.PrepareWaitTick) == PublisherTickSerial) continue;
		if (bInCommit && Group.NativeTransaction.IsValid() && TerrainRenderer->ConsumeTransactionFailure(Group.NativeTransaction))
		{
			DiscardPublication(Ready.Publication, false);
			Cursor = 0;
			return Index;
		}
		if (!Group.bPriorityReady) continue;
		if (!Group.bNativeAdmitted)
		{
			if (ActiveNativeGroups >= MaximumNativeGroups) continue;
			// 完成所需的旧代先准入，否则新代持有槽位和资源会反过来阻塞它的前置依赖。
			if (Group.Keys.ContainsByPredicate([this, &Ready](const auto& Key)
			{
				const uint64* First = OwnerFirstSerial.Find(Key);
				return First && *First < Ready.Publication->Serial;
			})) continue;
			bool bNeedsContinuation = false;
			if (HasOlderDependency(Ready.Publication, Group, &bNeedsContinuation))
			{
				if (bNeedsContinuation)
				{
					Cursor = Index;
					if (bOutAdvanced) *bOutAdvanced = true;
					return INDEX_NONE;
				}
				continue;
			}
		}
		if (bInCommit && Group.NativeTransaction.IsValid() &&
			TerrainRenderer->IsTransactionPending(Group.NativeTransaction)) continue;
		if (bInCommit && Group.bNativeControlWaiting && TerrainRenderer->GetCoarseAdmissionVersion() == Group.NativeFrontendVersion &&
			!(Group.bNativeBootstrapWaiting && TerrainRenderer->GetNativeStateAdmission() == EVoxelTerrainResourceAdmission::Granted))
		{
			uint64 Current = 0;
			if (!Scheduler.GetTerrainResources()->TryGetReleaseVersionExcludingOwner(Group.NativeControlConsumer, Current) ||
				Current <= Group.NativeControlVersion) continue;
		}
		if (!bInCommit && Group.UpdateIndices.IsValidIndex(Group.PreparedUpdates))
		{
			const int32 UpdateIndex = Group.UpdateIndices[Group.PreparedUpdates];
			const auto& Update = Ready.Publication->Updates[UpdateIndex];
			if (Update.bNativeMeshPending) continue;
			if (Update.bNativeAdmissionWaiting && IsNativeGeometryCurrent(Update.NativeStamp))
			{
				const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, NativeOwnerId,
					Ready.Publication->Serial, FIntVector(MIN_int32, 7, UpdateIndex), Update.NativeStamp.Token);
				uint64 Current = 0;
				if (!Scheduler.GetTerrainResources()->TryGetReleaseVersionExcludingOwner(Consumer, Current) ||
					Current <= Update.NativeAdmissionVersion) continue;
			}
		}
		if (bInCommit)
		{
			if (CommitGate && Group.Keys.ContainsByPredicate([this](const auto& Key) { return !CommitGate(Key); })) continue;
			const bool bCurrent = !Group.Keys.ContainsByPredicate([&Ready](const auto& Key)
			{
				const auto Validation = Ready.Publication->Validations.FindRef(Key);
				return Validation && *Validation && !(*Validation)();
			});
			if (bCurrent)
			{
				if (Group.Keys.ContainsByPredicate([this, &Ready](const auto& Key)
				{
					const uint64* First = OwnerFirstSerial.Find(Key);
					return First && *First < Ready.Publication->Serial;
				})) continue;
				bool bNeedsContinuation = false;
				const bool bDependencyBlocked = HasOlderDependency(Ready.Publication, Group, &bNeedsContinuation);
				if (bNeedsContinuation)
				{
					// 查询续跑属于真实进度；保留近组前沿，在原共享期限内继续证明。
					Cursor = Index;
					bCommitNeedsPolling = true;
					if (bOutAdvanced) *bOutAdvanced = true;
					return INDEX_NONE;
				}
				if (bDependencyBlocked) continue;
			}
		}
		FVoxelTerrainServiceCandidate Candidate;
		Candidate.Index = Index;
		Candidate.Priority = Group.Priority.Service;
		Candidate.FirstQueuedSeconds = Group.FirstQueuedAt;
		Candidate.Sequence = Group.QueueSequence;
		const double Estimate = ServiceCostEstimates[bInCommit ? 1 : 0][uint8(Candidate.Priority.Band)];
		Candidate.EstimatedMicroseconds = Estimate > 0.0 ? Estimate : 50.0;
		Candidate.bRunnable = true;
		Candidates.Add(Candidate);
		FirstRunnableIndex = Index;
		break;
	}
	FVoxelTerrainServiceCapacity Capacity;
	Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
	Capacity.bAllowOverdueOpportunity = !bInCommit && Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
	const int32 Selected = InSelection.Select(Candidates.GetCandidates(), Capacity, FPlatformTime::Seconds(), OutCharge);
	if (bInCommit)
	{
		if (Selected != INDEX_NONE) Cursor = 0;
		else if (FirstRunnableIndex != INDEX_NONE) Cursor = FirstRunnableIndex;
		else if (Cursor >= InReady.Num()) bCommitSelectionComplete = true;
	}
	else if (Selected != INDEX_NONE || Cursor >= InReady.Num()) Cursor = 0;
	if (Selected != INDEX_NONE && OutCharge.bOverdueOpportunity)
	{
		Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
	}
	return Selected;
}

void FVoxelViewPublisher::SettleReadyGroup(FVoxelTerrainWorkSelection& InSelection,
	const FVoxelTerrainServiceCharge& InCharge, const double InStartedSeconds, const bool bInCommit)
{
	if (InCharge.Serial == 0) return;
	const double Actual = FMath::Max(1.0, (FPlatformTime::Seconds() - InStartedSeconds) * 1000000.0);
	verify(InSelection.Settle(InCharge.Serial, Actual));
	double& Estimate = ServiceCostEstimates[bInCommit ? 1 : 0][uint8(InCharge.Band)];
	Estimate = Estimate > 0.0 ? FMath::Lerp(Estimate, Actual, 0.15) : Actual;
}


void FVoxelViewPublisher::SetOwnerActive(const FVoxelPublishGroupKey& InKey, const uint64 InSerial, const bool bInActive)
{
	InvalidateCommitReadiness();
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

void FVoxelViewPublisher::Tick(FVoxelPublishBudget& InBudget, bool bInAllowCommit, const double InDeadlineSeconds)
{
	check(PublisherTickSerial < MAX_uint64);
	++PublisherTickSerial;
	const double ServiceStarted = FPlatformTime::Seconds();
	const double BudgetDeadline = ServiceStarted + InBudget.RemainingMilliseconds(ServiceStarted) / 1000.0;
	const double ServiceDeadline = InDeadlineSeconds > 0.0 ? FMath::Min(BudgetDeadline, InDeadlineSeconds) : BudgetDeadline;
	TerrainRenderer->PollRenderReceipts(ServiceDeadline);
	const uint64 ResourceVersion = Scheduler.GetTerrainResources()->Snapshot().ReleaseVersion;
	const uint64 FrontendVersion = TerrainRenderer->GetCoarseAdmissionVersion();
	if (CommitResourceVersion != ResourceVersion || CommitFrontendVersion != FrontendVersion || bCommitNeedsPolling)
	{
		InvalidateCommitReadiness();
		CommitResourceVersion = ResourceVersion;
		CommitFrontendVersion = FrontendVersion;
		bCommitNeedsPolling = false;
	}
#if !UE_BUILD_SHIPPING
	if (FPlatformTime::Seconds() >= NextPublisherDiagnostic)
	{
		UE_LOG(LogTemp, Display, TEXT("Publisher continuation diagnostic: pending=%d frozen=%d phase=%d cursor=%d selected=%d blocked=%d publications=%d priorityPhase=%d readyPrepare=%d readyCommit=%d activeNative=%d/%d"),
			PendingGroups.Num(), PendingPlan ? PendingPlan->Groups.Num() : 0, PendingPlan ? PendingPlan->Phase : -1,
			PendingPlan ? PendingPlan->Cursor : -1, PendingPlan ? PendingPlan->SelectedKeys.Num() : 0,
			PendingPlan ? PendingPlan->BlockedComponents.Num() : 0, Publications.Num(), PriorityPlan ? PriorityPlan->Phase : -1,
			ReadyPrepareGroups.Num(), ReadyCommitGroups.Num(), ActiveNativeGroups, MaximumNativeGroups);
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
				if (Group.UpdateIndices.IsValidIndex(Group.PreparedUpdates))
				{
					const int32 UpdateIndex = Group.UpdateIndices[Group.PreparedUpdates];
					const FUpdate& Update = Publication->Updates[UpdateIndex];
					const auto* Entry = Entries.Find(Update.SourceId);
					const auto& Key = Update.NativeStamp.GeometryIdentity.Key;
					UE_LOG(LogTemp, Display, TEXT("Publisher update diagnostic: serial=%llu index=%d representation=%d level=%d coordinate=(%d,%d,%d) retained=%d unchanged=%d dirty=%d source=%d native=%d meshPending=%d admissionWait=%d admissionVersion=%llu entryNative=%d samePresentedSource=%d"),
						Publication->Serial, UpdateIndex, int32(Key.Representation), Key.Level, Key.Coordinate.X, Key.Coordinate.Y, Key.Coordinate.Z,
						Update.bRetainSource, Update.bUnchanged, Update.bSourceDirty, Update.Source.IsValid(), Update.NativeMesh.IsValid(),
						Update.bNativeMeshPending, Update.bNativeAdmissionWaiting, Update.NativeAdmissionVersion,
						Entry && Entry->NativeMesh.IsValid(), Entry && Entry->PresentedSource == Update.Source);
				}
				const bool bGateBlocked = CommitGate && Group.Keys.ContainsByPredicate(
					[this](const auto& Key) { return !CommitGate(Key); });
				uint64 ControlReleaseVersion = 0;
				const bool bControlReleaseKnown = Group.bNativeControlWaiting &&
					Scheduler.GetTerrainResources()->TryGetReleaseVersionExcludingOwner(Group.NativeControlConsumer, ControlReleaseVersion);
				UE_LOG(LogTemp, Display, TEXT("Publisher gate diagnostic: serial=%llu index=%d allowCommit=%d coverageGate=%d ownerGate=%d controlWait=%d bootstrapWait=%d controlVersion=%llu releaseKnown=%d releaseVersion=%llu frontendVersion=%llu currentFrontendVersion=%llu stateAdmission=%d transaction=%d submitted=%d sourceCursor=%d/%d dependencyActive=%d dependencySerial=%llu dependencyNext=%llu"),
					Publication->Serial, Index, bInAllowCommit, bGateBlocked, FirstSerial < Publication->Serial,
					Group.bNativeControlWaiting, Group.bNativeBootstrapWaiting, Group.NativeControlVersion,
					bControlReleaseKnown, ControlReleaseVersion, Group.NativeFrontendVersion, TerrainRenderer->GetCoarseAdmissionVersion(),
					static_cast<int32>(TerrainRenderer->GetNativeStateAdmission()), Group.NativeTransaction.IsValid(),
					Group.bNativeSubmitted, Group.NativeSourceCursor, Group.NativeChanges.Num(),
					Group.bDependencyQueryActive, Group.DependencyQuerySerial, Group.DependencyNextSerial);
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
	bool bPrepareSelectionExhausted = false;
	bool bCommitSelectionExhausted = false;
	uint64 ObservedPrepareReadiness = PrepareReadinessRevision;
	uint64 ObservedCommitReadiness = CommitReadinessRevision;
	const auto Run = [this, &InBudget, ServiceDeadline](EVoxelPublishPhase Phase, auto&& Operation)
	{
		return RunBudgetedPhase(InBudget, Phase, Operation, ServiceDeadline);
	};
	// 一次推进一个可恢复操作，轮换起点防止昂贵 Plan 持续饿死 Prepare/Commit。
	while (FPlatformTime::Seconds() < ServiceDeadline &&
		InBudget.RemainingMilliseconds(FPlatformTime::Seconds()) > 0.0 && IdlePhases < 6)
	{
		if (ObservedPrepareReadiness != PrepareReadinessRevision)
		{
			ObservedPrepareReadiness = PrepareReadinessRevision;
			bPrepareSelectionExhausted = false;
		}
		if (ObservedCommitReadiness != CommitReadinessRevision)
		{
			ObservedCommitReadiness = CommitReadinessRevision;
			bCommitSelectionExhausted = false;
		}
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
					return true;
				});
				break;
			}
			if (!bWorked && (PendingPlan || !PendingGroups.IsEmpty()) && !bAttemptedPendingPlan)
			{
				bWorked = Run(EVoxelPublishPhase::Plan, [this]
				{
					AdvancePendingPlan();
					return true;
				});
				bAttemptedPendingPlan = !PendingPlan;
			}
		}
		else if (Phase == 1 && bPrioritiesDirty)
		{
			bWorked = Run(EVoxelPublishPhase::Priority, [this]
			{
				AdvancePriorities();
				return true;
			});
		}
		else if (Phase == 2 && !Publications.IsEmpty())
		{
			AdmissionCursor %= Publications.Num();
			const FPublicationRef Publication = Publications[AdmissionCursor++];
			if (!Publication->bCanceled && !Publication->bFinished &&
				((!Publication->bCoveragePrepared) ||
				(Publication->bGroupsPrepared && !Publication->BuildOrder.IsEmpty()) || !Publication->bBuildOrderInitialized))
				bWorked = Run(EVoxelPublishPhase::Admission, [this, Publication, ServiceDeadline]
				{
					const bool bCoverageAdvanced = AdmitCoveragePreparation(Publication, ServiceDeadline);
					const bool bBuildAdvanced = AdmitBuilds(Publication, ServiceDeadline);
					return bCoverageAdvanced || bBuildAdvanced;
				});
		}
		else if (Phase == 3 && !bPrepareSelectionExhausted && !ReadyPrepareGroups.IsEmpty() &&
			PreparedComponents < Module.GetViewSettings().Budget.MaxPublishComponentsPerFrame)
		{
			bWorked = Run(EVoxelPublishPhase::Prepare, [&]
			{
				FVoxelTerrainServiceCharge Charge;
				const double SelectionStarted = FPlatformTime::Seconds();
				const double Deadline = ServiceDeadline;
				bool bSelectionAdvanced = false;
				const int32 ReadyIndex = SelectReadyGroup(ReadyPrepareGroups, PrepareSelection, Charge, false,
					FMath::Min(Deadline, SelectionStarted + 0.00025), &bSelectionAdvanced);
				if (ReadyIndex == INDEX_NONE)
				{
					bPrepareSelectionExhausted = !bSelectionAdvanced;
					return bSelectionAdvanced;
				}
				const double Started = FPlatformTime::Seconds();
				ON_SCOPE_EXIT { SettleReadyGroup(PrepareSelection, Charge, Started, false); };
				const FReadyGroup Ready = ReadyPrepareGroups[ReadyIndex];
				ReadyPrepareGroups.RemoveAt(ReadyIndex, 1, EAllowShrinking::No);
				const FPublicationRef Publication = Ready.Publication;
				if (Publication->bCanceled || !Publication->Groups.IsValidIndex(Ready.GroupIndex)) return true;
				FGroup& Group = Publication->Groups[Ready.GroupIndex];
				Group.bPrepareQueued = false;
				if (Group.bCommitted || Group.PreparedUpdates == Group.UpdateIndices.Num()) return true;
				const int32 Index = Group.UpdateIndices[Group.PreparedUpdates];
				if (!Publication->PreparedMeshes[Index]) return true;
				if (!Group.bNativeAdmitted)
				{
					check(ActiveNativeGroups < MaximumNativeGroups);
					Group.bNativeAdmitted = true;
					++ActiveNativeGroups;
				}
				bool bComplete = false;
				const auto* PreviousNativeMesh = Publication->Updates[Index].NativeMesh.Get();
				const bool bWasMeshPending = Publication->Updates[Index].bNativeMeshPending;
				const bool bWasAdmissionWaiting = Publication->Updates[Index].bNativeAdmissionWaiting;
				if (!PrepareUpdate(Publication, Index, Deadline, PreparedComponents, bComplete))
				{
					DiscardPublication(Publication);
					bRetry = true;
					return true;
				}
				if (bComplete) ++Group.PreparedUpdates;
				const bool bAdvanced = bComplete || Publication->bCanceled ||
					PreviousNativeMesh != Publication->Updates[Index].NativeMesh.Get() ||
					bWasMeshPending != Publication->Updates[Index].bNativeMeshPending ||
					bWasAdmissionWaiting != Publication->Updates[Index].bNativeAdmissionWaiting;
				if (!bAdvanced) Group.PrepareWaitTick = PublisherTickSerial;
				QueueReadyGroup(Publication, Ready.GroupIndex);
				return bAdvanced;
			});
		}
		else if (Phase == 4 && bInAllowCommit && !bCommitSelectionExhausted && !ReadyCommitGroups.IsEmpty() &&
			CommittedGroups < Module.GetViewSettings().Budget.MaxPublishGroupsPerFrame)
		{
			bWorked = Run(EVoxelPublishPhase::Commit, [&]
			{
				if (bFirstOwnersDirty) RebuildFirstOwners();
				FVoxelTerrainServiceCharge Charge;
				const double SelectionStarted = FPlatformTime::Seconds();
				const double Deadline = ServiceDeadline;
				bool bSelectionAdvanced = false;
				const int32 ReadyIndex = SelectReadyGroup(ReadyCommitGroups, CommitSelection, Charge, true,
					FMath::Min(Deadline, SelectionStarted + 0.00025), &bSelectionAdvanced);
				if (ReadyIndex == INDEX_NONE)
				{
					bCommitSelectionExhausted = !bSelectionAdvanced;
					return bSelectionAdvanced;
				}
				const double Started = FPlatformTime::Seconds();
				ON_SCOPE_EXIT { SettleReadyGroup(CommitSelection, Charge, Started, true); };
				const FReadyGroup Ready = ReadyCommitGroups[ReadyIndex];
				ReadyCommitGroups.RemoveAt(ReadyIndex, 1, EAllowShrinking::No);
				if (Ready.Publication->bCanceled || !Ready.Publication->Groups.IsValidIndex(Ready.GroupIndex)) return true;
				Ready.Publication->Groups[Ready.GroupIndex].bCommitQueued = false;
				const bool bAdvanced = CommitReadyGroups(Ready.Publication, CommittedGroups, Deadline,
					OwnerFirstSerial, Ready.GroupIndex);
				if (!bAdvanced)
				{
					Ready.Publication->Groups[Ready.GroupIndex].CommitWaitTick = PublisherTickSerial;
					bCommitNeedsPolling = true;
				}
				QueueReadyGroup(Ready.Publication, Ready.GroupIndex);
				return bAdvanced;
			});
		}
		else if (Phase == 5 && !RetiredComponents.IsEmpty())
			bWorked = Run(EVoxelPublishPhase::Retire, [this]
			{
				if (UVoxelMeshComponent* Component = RetiredComponents.Pop().Get()) Component->DestroyComponent();
				return true;
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

bool FVoxelViewPublisher::SetScopedCoverage(const FGuid& InGroup, FVoxelViewSourceId InSourceId, TArray<FBox> InWorldCellBoxes,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes,
	TConstArrayView<FBox> InAffectedWorldCellBoxes, const bool bInRetainPresented)
{
	bool bSuccess = false;
	auto Work =
		SetScopedCoverageSteps(InGroup, InSourceId, MoveTemp(InWorldCellBoxes), MoveTemp(InCommonBoxes), InAffectedWorldCellBoxes, bSuccess, bInRetainPresented);
	while (!Work.IsComplete())
	{
		Work.Resume();
	}
	return bSuccess;
}

FVoxelGameThreadContinuation FVoxelViewPublisher::SetScopedCoverageSteps(const FGuid& InGroup, FVoxelViewSourceId InSourceId, TArray<FBox> InWorldCellBoxes,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes, TConstArrayView<FBox> InAffectedWorldCellBoxes, bool& OutSuccess,
	const bool bInRetainPresented)
{
	OutSuccess = false;
	FUpdate Source;
	if (!CaptureCoverageSource(InSourceId, bInRetainPresented, Source)) co_return;
	FEntry* Entry = Entries.Find(FVoxelViewSourceId(InSourceId));
	if (!Entry)
	{
		co_return;
	}
	if (bInRetainPresented)
	{
		co_await VoxelGameThread::AppendSteps(InWorldCellBoxes, Source.LocalWorldBoxes);
	}
	TArray<FBox> Previous;
	Entry = Entries.Find(InSourceId);
	if (!Entry)
	{
		co_return;
	}
	const FVector Origin = Source.Location / Module.BlockSize();
	const double Step = Source.Scale / Module.BlockSize();
	if (!Entry->bHasDesiredExclusions)
	{
		for (const FBox& Box : Source.PreviousExclusions)
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
	co_await SetCoverageSourceSteps(InGroup, InSourceId, MoveTemp(Source), MoveTemp(Previous), nullptr, OutSuccess);
}

bool FVoxelViewPublisher::RunBudgetedPhase(FVoxelPublishBudget& InBudget, const EVoxelPublishPhase InPhase,
	TFunctionRef<bool()> InOperation, const double InDeadlineSeconds)
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
	if (Started + Estimate / 1000.0 > InDeadlineSeconds || !InBudget.TryStart(InPhase, Started, Estimate))
	{
		const uint64 Deferrals = ++PhaseConsecutiveDeferrals[Index];
		if (Deferrals == 60 || Deferrals % 600 == 0)
			UE_LOG(LogTemp, Warning, TEXT("Voxel publish deferral: phase=%d consecutive=%llu estimateMs=%.3f remainingMs=%.3f"), Index, Deferrals, Estimate,
				InBudget.RemainingMilliseconds(Started));
		return false;
	}
	PhaseConsecutiveDeferrals[Index] = 0;

	const bool bAdvanced = InOperation();

	const double Finished = FPlatformTime::Seconds();
	const double ElapsedMs = (Finished - Started) * 1000.0;

	// EWMA 只用于“是否应该在剩余预算中启动下一次操作”，
	// 不代替硬 wall-clock deadline。
	Estimate = FMath::Clamp(FMath::Lerp(Estimate, ElapsedMs, 0.15), FloorMs, CeilingMs);

	InBudget.Finish(InPhase, Started, Finished);
	return bAdvanced;
}

bool FVoxelViewPublisher::GetPresentedMeshCost(FVoxelViewSourceId InSourceId, uint64& OutVertices, uint64& OutTriangles, uint64& OutBytes) const
{
	const FEntry* Entry = Entries.Find(InSourceId);
	if (!Entry || !Entry->bPresented || !InSourceId || Entry->bHidden)
		return false;
	OutVertices = Entry->PresentedVertices;
	OutTriangles = Entry->PresentedTriangles;
	OutBytes = Entry->PresentedBytes;
	return true;
}
