#include "Voxel/Navigation/VoxelNavigationPresenter.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"
#include "Voxel/Navigation/VoxelNavigationBounds.h"
#include "Voxel/Navigation/VoxelNavigationSurfaceComponent.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/VoxelModule.h"

namespace
{
	struct FVoxelNavigationTaskPayload : FVoxelTaskCustomPayload
	{
		FVoxelNavigationSurface Surface;
		uint64 AboveRevision = 0;
		uint64 AboveToken = 0;

		virtual uint64 GetAllocatedBytes() const override
		{
			return sizeof(*this) + Surface.Vertices.GetAllocatedSize() + Surface.Indices.GetAllocatedSize();
		}
	};
}

FVoxelNavigationPresenter::FVoxelNavigationPresenter(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler)
	: Module(InModule), Scheduler(InScheduler), WorldEpoch(InModule.GetWorldEpoch())
{
}

FVoxelNavigationPresenter::~FVoxelNavigationPresenter()
{
	Reset();
}

void FVoxelNavigationPresenter::UpdateSources(const FVoxelInterestSet& InInterest)
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld());
	if (!Module.IsAuthority() || !Navigation)
	{
		return;
	}
	TSet<FGuid> Retained;
	for (const auto& Pair : InInterest.Sources)
	{
		const FVoxelStreamingSource& Source = Pair.Value.Source;
		if (!Source.Has(EVoxelStreamingCapability::Simulation))
		{
			continue;
		}
		Retained.Add(Pair.Key);
		TWeakObjectPtr<AVoxelNavigationBounds>& Bounds = SourceBounds.FindOrAdd(Pair.Key);
		if (!Bounds.IsValid())
		{
			FActorSpawnParameters Parameters;
			Parameters.ObjectFlags |= RF_Transient;
			Bounds = Module.GetWorld()->SpawnActor<AVoxelNavigationBounds>(Parameters);
		}
		if (!Bounds.IsValid())
		{
			continue;
		}
		const double Radius = (Source.SimulationRadius + 32.0) * Module.BlockSize();
		const double Vertical = (Source.VerticalExactRadius + 32.0) * Module.BlockSize();
		const FVector Center = FVector(Source.Center) * Module.BlockSize();
		Bounds->SetBounds(FBox(Center - FVector(Radius, Radius, Vertical), Center + FVector(Radius, Radius, Vertical)));
		Navigation->RegisterNavigationInvoker(Bounds.Get(), Radius, Radius + 32.0 * Module.BlockSize());
	}
	for (auto Iterator = SourceBounds.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Retained.Contains(Iterator.Key()))
		{
			if (AVoxelNavigationBounds* Bounds = Iterator.Value().Get())
			{
				Navigation->UnregisterNavigationInvoker(Bounds);
				Bounds->Destroy();
			}
			Iterator.RemoveCurrent();
		}
	}
}

void FVoxelNavigationPresenter::ActivateSection(const FIntVector& InSection)
{
	Wanted.Add(InSection);
	InvalidateSection(InSection);
}

void FVoxelNavigationPresenter::RetireSection(const FIntVector& InSection)
{
	Wanted.Remove(InSection);
	Dirty.Remove(InSection);
	PublishedRevisions.Remove(InSection);
	if (TObjectPtr<UVoxelNavigationSurfaceComponent>* Component = Components.Find(InSection))
	{
		if (*Component)
		{
			(*Component)->DestroyComponent();
		}
		Components.Remove(InSection);
	}
	InvalidateSection(InSection - FIntVector(0, 0, 1));
}

void FVoxelNavigationPresenter::InvalidateSection(const FIntVector& InSection)
{
	for (const FIntVector& Section : {InSection, InSection - FIntVector(0, 0, 1)})
	{
		if (Wanted.Contains(Section))
		{
			MarkDirty(Section);
			PublishedRevisions.Remove(Section);
			if (const auto* Component = Components.Find(Section); Component && *Component)
			{
				(*Component)->Apply(FVoxelNavigationSurface());
			}
		}
	}
}

void FVoxelNavigationPresenter::Tick()
{
	int32 Remaining = 4;
	int32 Attempts = 16;
	FIntVector Section;
	while (Remaining > 0 && Attempts-- > 0 && PendingSections.Dequeue(Section))
	{
		if (!Dirty.Contains(Section)) continue;
		if (RequestSection(Section))
		{
			Dirty.Remove(Section);
			--Remaining;
		}
		else PendingSections.Enqueue(Section);
	}
}

void FVoxelNavigationPresenter::MarkDirty(const FIntVector& InSection)
{
	if (!Dirty.Contains(InSection))
	{
		Dirty.Add(InSection);
		PendingSections.Enqueue(InSection);
	}
}

bool FVoxelNavigationPresenter::RequestSection(const FIntVector& InSection)
{
	FVoxelSectionSnapshot Snapshot;
	if (!Module.IsCollisionReady(InSection) || !Module.GetRuntime()->CaptureSnapshot(InSection, Snapshot))
	{
		return false;
	}
	FVoxelSectionSnapshot Above;
	const bool bHasAbove = Module.GetRuntime()->CaptureSnapshot(InSection + FIntVector(0, 0, 1), Above);
	FVoxelTaskStamp Stamp;
	Stamp.WorldEpoch = WorldEpoch;
	Stamp.Token = Snapshot.Stamp.Token;
	Stamp.Revision = Snapshot.Revision;
	Stamp.Section = InSection;
	if (Scheduler.Has(Stamp, EVoxelTaskKind::BuildNavigation))
	{
		return false;
	}
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildNavigation;
	Request.WorkClass = EVoxelWorkClass::Interactive;
	Request.Stamp = Stamp;
	Request.ReservedBytes = 2ull * 1024ull * 1024ull;
	Request.InputBytes = Snapshot.Bytes() + (bHasAbove ? Above.Bytes() : 0);
	const auto Registry = Module.GetRegistry();
	const auto Shapes = Module.GetShapes();
	const double CellSize = Module.BlockSize();
	const double Clearance = Module.NavigationClearanceCentimeters;
	Request.Execute = [Snapshot = MoveTemp(Snapshot), Above = MoveTemp(Above), bHasAbove, Registry, Shapes, CellSize, Clearance](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		auto Payload = MakeShared<FVoxelNavigationTaskPayload, ESPMode::ThreadSafe>();
		Payload->AboveRevision = bHasAbove ? Above.Revision : 0;
		Payload->AboveToken = bHasAbove ? Above.Stamp.Token : 0;
		Result.bSuccess = FVoxelNavigationSurfaceBuilder::Build(Snapshot, bHasAbove ? &Above : nullptr,
			*Registry, *Shapes, CellSize, Clearance, Payload->Surface, &InCancel);
		Result.CustomPayload = Payload;
		return Result;
	};
	Request.Apply = [this](FVoxelTaskResult&& InResult)
	{
		Apply(MoveTemp(InResult));
	};
	return Scheduler.Enqueue(MoveTemp(Request));
}

void FVoxelNavigationPresenter::Apply(FVoxelTaskResult&& InResult)
{
	if (InResult.Stamp.WorldEpoch != WorldEpoch || !Wanted.Contains(InResult.Stamp.Section))
	{
		return;
	}
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(InResult.Stamp.Section);
	const FVoxelSection* Above = Module.GetRuntime()->FindSection(InResult.Stamp.Section + FIntVector(0, 0, 1));
	const auto Payload = StaticCastSharedPtr<const FVoxelNavigationTaskPayload>(InResult.CustomPayload);
	if (!InResult.bSuccess || InResult.bCanceled || !Payload || !Section || Section->CommittedRevision != InResult.Stamp.Revision ||
		Section->Stamp.Token != InResult.Stamp.Token ||
		((Above && Above->Status == EVoxelSectionStatus::DataReady) ?
			(Above->CommittedRevision != Payload->AboveRevision || Above->Stamp.Token != Payload->AboveToken) : Payload->AboveToken != 0))
	{
		MarkDirty(InResult.Stamp.Section);
		return;
	}
	AActor* Owner = Module.GetTypedOuter<AActor>();
	if (!Owner)
	{
		return;
	}
	TObjectPtr<UVoxelNavigationSurfaceComponent>& Component = Components.FindOrAdd(InResult.Stamp.Section);
	if (!Component)
	{
		Component = NewObject<UVoxelNavigationSurfaceComponent>(Owner);
		Component->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Component->RegisterComponent();
	}
	Component->SetWorldLocation(FVector(InResult.Stamp.Section * 16) * Module.BlockSize());
	FVoxelNavigationSurface Surface = Payload->Surface;
	Component->Apply(MoveTemp(Surface));
	PublishedRevisions.Add(InResult.Stamp.Section, InResult.Stamp.Revision);
}

bool FVoxelNavigationPresenter::IsReady(const FIntVector& InSection) const
{
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(InSection);
	const uint64* Revision = PublishedRevisions.Find(InSection);
	if (!Section || !Revision || *Revision != Section->CommittedRevision || Dirty.Contains(InSection))
	{
		return false;
	}
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld());
	FNavLocation Projected;
	const FVector Center = (FVector(InSection * 16) + FVector(8.0)) * Module.BlockSize();
	return Navigation && Navigation->ProjectPointToNavigation(Center, Projected, FVector(8.0 * Module.BlockSize()));
}

void FVoxelNavigationPresenter::Reset()
{
	for (auto& Pair : Components)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	for (auto& Pair : SourceBounds)
	{
		if (AVoxelNavigationBounds* Bounds = Pair.Value.Get())
		{
			if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Module.GetWorld()))
			{
				Navigation->UnregisterNavigationInvoker(Bounds);
			}
			Bounds->Destroy();
		}
	}
	Components.Reset();
	SourceBounds.Reset();
	PublishedRevisions.Reset();
	Dirty.Reset();
	PendingSections.Empty();
	Wanted.Reset();
}
