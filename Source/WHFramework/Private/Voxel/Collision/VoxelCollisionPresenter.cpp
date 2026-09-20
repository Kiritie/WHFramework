#include "Voxel/Collision/VoxelCollisionPresenter.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Voxel/Components/VoxelCollisionComponent.h"
#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/VoxelModule.h"

FVoxelCollisionPresenter::FVoxelCollisionPresenter(
	UVoxelModule& InModule,
	FVoxelTaskScheduler& InScheduler,
	const uint64 InWorldEpoch)
	: Module(InModule),
	  Scheduler(InScheduler),
	  WorldEpoch(InWorldEpoch)
{
}

void FVoxelCollisionPresenter::Tick(
	const TMap<FIntVector, FVoxelExactDemand>& InDemand,
	const uint64 InInterestRevision)
{
	if (CurrentInterestRevision != InInterestRevision)
	{
		RebuildWanted(InDemand, InInterestRevision);
	}

	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : Wanted)
	{
		const FVoxelSection* Section = Module.GetRuntime()->FindSection(Pair.Key);
		if (!Section || Section->Status != EVoxelSectionStatus::DataReady)
		{
			continue;
		}
		const uint64* Published = PublishedRevisions.Find(Pair.Key);
		if (!Published || *Published != Section->CommittedRevision)
		{
			RequestCollision(Pair.Key, Section->CommittedRevision, Pair.Value);
		}
	}
}

void FVoxelCollisionPresenter::RebuildWanted(
	const TMap<FIntVector, FVoxelExactDemand>& InDemand,
	const uint64 InInterestRevision)
{
	Wanted.Reset();
	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : InDemand)
	{
		if (Pair.Value.bCollision)
		{
			Wanted.Add(Pair.Key, Pair.Value);
		}
	}
	for (auto Iterator = SectionComponents.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Wanted.Contains(Iterator.Key()))
		{
			if (Iterator.Value())
			{
				Iterator.Value()->DestroyComponent();
			}
			PublishedRevisions.Remove(Iterator.Key());
			Iterator.RemoveCurrent();
		}
	}
	CurrentInterestRevision = InInterestRevision;
}

bool FVoxelCollisionPresenter::OnTask(FVoxelTaskResult&& InResult)
{
	if (InResult.Kind != EVoxelTaskKind::BuildCollision)
	{
		return false;
	}
	if (!InResult.bSuccess ||
		InResult.bCanceled ||
		InResult.Stamp.WorldEpoch != WorldEpoch ||
		!InResult.Collision)
	{
		return true;
	}
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(InResult.Stamp.Section);
	if (!Section ||
		Section->Stamp.Epoch != InResult.Stamp.WorldEpoch ||
		Section->Stamp.Token != InResult.Stamp.Token ||
		Section->CommittedRevision != InResult.Stamp.Revision)
	{
		return true;
	}

	AActor* Owner = Module.GetTypedOuter<AActor>();
	if (!Owner)
	{
		return true;
	}
	TObjectPtr<UVoxelCollisionComponent>& Component = SectionComponents.FindOrAdd(InResult.Stamp.Section);
	if (!Component)
	{
		Component = NewObject<UVoxelCollisionComponent>(Owner);
		Component->RegisterComponent();
		Component->AttachToComponent(
			Owner->GetRootComponent(),
			FAttachmentTransformRules::KeepRelativeTransform);
	}
	const double CellSize = Module.BlockSize();
	Component->SetRelativeLocation(FVector(InResult.Stamp.Section * 16) * CellSize);
	if (Component->Apply(InResult.Collision->Boxes, CellSize))
	{
		PublishedRevisions.Add(InResult.Stamp.Section, InResult.Stamp.Revision);
	}
	return true;
}

void FVoxelCollisionPresenter::InvalidateSection(const FIntVector& InSection)
{
	PublishedRevisions.Remove(InSection);
	Scheduler.CancelSection(InSection);
}

bool FVoxelCollisionPresenter::IsReady(const FIntVector& InSection) const
{
	const FVoxelSection* Section = Module.GetRuntime() ? Module.GetRuntime()->FindSection(InSection) : nullptr;
	const uint64* Revision = PublishedRevisions.Find(InSection);
	return Section && Revision && *Revision == Section->CommittedRevision;
}

void FVoxelCollisionPresenter::Reset()
{
	for (const TPair<FIntVector, TObjectPtr<UVoxelCollisionComponent>>& Pair : SectionComponents)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	SectionComponents.Reset();
	PublishedRevisions.Reset();
	Wanted.Reset();
	CurrentInterestRevision = 0;
}

void FVoxelCollisionPresenter::RequestCollision(
	const FIntVector& InSection,
	const uint64 InRevision,
	const FVoxelExactDemand& InDemand)
{
	const FVoxelSection* Section =
		Module.GetRuntime()->
			FindSection(
				InSection);

	if (!Section ||
		Section->Status !=
			EVoxelSectionStatus::DataReady)
	{
		return;
	}

	FVoxelTaskStamp Stamp;
	Stamp.WorldEpoch =
		WorldEpoch;
	Stamp.Token =
		Section->Stamp.Token;
	Stamp.Revision =
		InRevision;
	Stamp.Section =
		InSection;

	if (Scheduler.Has(
		Stamp,
		EVoxelTaskKind::BuildCollision))
	{
		return;
	}

	FVoxelSectionSnapshot Snapshot;

	if (!Module.GetRuntime()->
		CaptureSnapshot(
			InSection,
			Snapshot))
	{
		return;
	}

	FVoxelTaskRequest Request;

	Request.Kind =
		EVoxelTaskKind::BuildCollision;

	Request.WorkClass =
		(InDemand.bMovementCriticalCollision || InDemand.bWarmupCollision)
			? EVoxelWorkClass::Critical
			: EVoxelWorkClass::Interactive;

	Request.DistanceScore = InDemand.DistanceCells;
	Request.ForwardScore = InDemand.ForwardScore;

	Request.Stamp =
		Stamp;

	Request.ReservedBytes =
		4ull *
		1024ull *
		1024ull;

	Request.InputBytes =
		Snapshot.Bytes();

	const TSharedPtr<
		const FVoxelRegistrySnapshot,
		ESPMode::ThreadSafe> Registry =
			Module.GetRegistry();

	const TSharedPtr<
		const FVoxelShapeRegistry,
		ESPMode::ThreadSafe> Shapes =
			Module.GetShapes();

	Request.Execute =
		[
			Snapshot =
				MoveTemp(Snapshot),
			Registry,
			Shapes
		](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;

			Result.Collision =
				MakeShared<
					FVoxelSectionCollisionResult>();

			Result.bSuccess =
				Registry &&
				Shapes &&
				FVoxelCollisionBuilder::Build(
					Snapshot,
					*Registry,
					*Shapes,
					*Result.Collision,
					&InCancel);

			return Result;
		};

	Scheduler.Enqueue(
		MoveTemp(Request));
}
