#include "Voxel/Rendering/VoxelViewPublisher.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Rendering/VoxelMeshClipper.h"
#include "Voxel/VoxelModule.h"

FVoxelViewPublisher::FVoxelViewPublisher(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler, const uint64 InWorldEpoch)
	: Module(InModule), Scheduler(InScheduler), WorldEpoch(InWorldEpoch)
{
}

FVoxelViewPublisher::~FVoxelViewPublisher()
{
	Reset();
}

bool FVoxelViewPublisher::Stage(AActor*& InOutActor, const FVector& InLocation,
	const double InScale, FVoxelSectionMeshResult&& InMesh)
{
	if (!Module.GetWorld() || !Module.GetMaterialSet() || !FMath::IsFinite(InScale) || InScale <= 0.0)
	{
		return false;
	}
	for (const FVoxelRenderBatch& Batch : InMesh.Batches)
	{
		const FVoxelMaterialBank* Bank = Module.GetMaterialSet()->FindBank(Batch.Group, Batch.Bank);
		if (!Bank || !Bank->Material || !Batch.Mesh.Validate()) return false;
	}
	if (!InOutActor)
	{
		FActorSpawnParameters Parameters;
		Parameters.ObjectFlags |= RF_Transient;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		InOutActor = Module.GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Parameters);
		if (!InOutActor) return false;
		InOutActor->SetReplicates(false);
		InOutActor->SetActorEnableCollision(false);
		USceneComponent* Root = NewObject<USceneComponent>(InOutActor);
		InOutActor->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	FEntry& Entry = Entries.FindOrAdd(TWeakObjectPtr<AActor>(InOutActor));
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

void FVoxelViewPublisher::BeginBatch()
{
	check(!bBusy);
	Updates.Reset();
	Visibility.Reset();
	bRetry = false;
}

void FVoxelViewPublisher::SetCoverage(AActor* InActor, TArray<FBox> InWorldCellBoxes)
{
	const FEntry* Entry = Entries.Find(TWeakObjectPtr<AActor>(InActor));
	if (!Entry) return;
	const FVector Origin = Entry->Location / Module.BlockSize();
	const double Step = Entry->Scale / Module.BlockSize();
	for (FBox& Box : InWorldCellBoxes)
	{
		Box.Min = (Box.Min - Origin) / Step;
		Box.Max = (Box.Max - Origin) / Step;
	}
	InWorldCellBoxes.RemoveAll([Entry](const FBox& Box) { return !Entry->Bounds.Intersect(Box); });
	VoxelMeshClipper::NormalizeBoxes(InWorldCellBoxes);
	if (!Entry->bDirty && Entry->Exclusions == InWorldCellBoxes) return;
	FUpdate& Update = Updates.AddDefaulted_GetRef();
	Update.Actor = InActor;
	Update.Source = Entry->Source;
	Update.Location = Entry->Location;
	Update.Scale = Entry->Scale;
	Update.Exclusions = MoveTemp(InWorldCellBoxes);
}

void FVoxelViewPublisher::SetHidden(AActor* InActor, const bool bInHidden)
{
	if (!InActor)
	{
		return;
	}

	const TWeakObjectPtr<AActor> Key(InActor);
	if (const bool* Pending = Visibility.Find(Key))
	{
		if (*Pending == bInHidden)
		{
			return;
		}
	}
	else if (InActor->IsHidden() == bInHidden)
	{
		return;
	}

	Visibility.Add(Key, bInHidden);
}

uint64 FVoxelViewCoverageResult::GetAllocatedBytes() const
{
	return Meshes.GetAllocatedSize() + OwnedMeshBytes;
}

bool FVoxelViewPublisher::EndBatch(TFunction<void()> InOnCommitted)
{
	OnCommitted = MoveTemp(InOnCommitted);
	if (Updates.IsEmpty())
	{
		CommitBatch();
		return true;
	}
	++BatchSerial;
	BuildIndex = 0;
	PrepareIndex = 0;
	PendingBuilds = 0;
	PreparedMeshes.SetNum(Updates.Num());
	bBusy = true;
	AdmitBuilds();
	return true;
}

void FVoxelViewPublisher::AdmitBuilds()
{
	constexpr int32 MaximumPendingBuilds = 4;
	while (BuildIndex < Updates.Num() && PendingBuilds < MaximumPendingBuilds)
	{
		const FUpdate& Update = Updates[BuildIndex];
		if (Update.Exclusions.IsEmpty())
		{
			PreparedMeshes[BuildIndex++] = Update.Source;
			continue;
		}
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildViewCoverage;
		Request.WorkClass = EVoxelWorkClass::Visible;
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.Token = HashCombineFast(GetTypeHash(BatchSerial), GetTypeHash(BuildIndex));
		Request.ReservedBytes = 48ull * 1024ull * 1024ull;
		Request.Execute = [Source = Update.Source, Exclusions = Update.Exclusions](const TAtomic<bool>& Cancel)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewCoverageBuild);
			FVoxelTaskResult TaskResult;
			if (Cancel.Load())
			{
				return TaskResult;
			}
			const auto Payload = MakeShared<FVoxelViewCoverageResult, ESPMode::ThreadSafe>();
			auto Mesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
			VoxelMeshClipper::Subtract(*Source, Exclusions, *Mesh, &Cancel);
			Payload->OwnedMeshBytes = Mesh->Bytes();
			Payload->Meshes.Add(MoveTemp(Mesh));
			TaskResult.CustomPayload = Payload;
			TaskResult.bSuccess = true;
			return TaskResult;
		};
		const uint64 Serial = BatchSerial;
		const int32 Index = BuildIndex;
		Request.Apply = [this, Serial, Index](FVoxelTaskResult&& TaskResult)
		{
			if (Serial != BatchSerial)
			{
				return;
			}
			--PendingBuilds;
			if (!TaskResult.bSuccess || TaskResult.bCanceled || TaskResult.Stamp.WorldEpoch != WorldEpoch)
			{
				if (!TaskResult.Error.IsEmpty())
				{
					UE_LOG(LogTemp, Warning, TEXT("Voxel coverage publication failed: %s"), *TaskResult.Error);
				}
				DiscardBatch();
				bRetry = true;
				return;
			}
			const auto Payload = StaticCastSharedPtr<const FVoxelViewCoverageResult>(TaskResult.CustomPayload);
			PreparedMeshes[Index] = Payload->Meshes[0];
		};
		if (!Scheduler.Enqueue(MoveTemp(Request)))
		{
			break;
		}
		++BuildIndex;
		++PendingBuilds;
	}
}
bool FVoxelViewPublisher::PrepareUpdate(const int32 InIndex)
{
	FUpdate& Update = Updates[InIndex];
	AActor* Actor = Update.Actor.Get();
	if (!Actor) return true;
	const FVoxelSectionMeshResult& Mesh = *PreparedMeshes[InIndex];
	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		if (Batch.Mesh.Triangles.IsEmpty()) continue;
		const FVoxelMaterialBank* Bank = Module.GetMaterialSet()->FindBank(Batch.Group, Batch.Bank);
		if (!Bank || !Bank->Material) return false;
		UVoxelMeshComponent* Component = NewObject<UVoxelMeshComponent>(Actor);
		Component->SetVisibility(false);
		Component->SetupAttachment(Actor->GetRootComponent());
		Component->RegisterComponent();
		Update.Components.Add(Component);
		if (!Component->Apply(Batch.Mesh, Update.Scale, Bank->Material)) return false;
	}
	return true;
}

void FVoxelViewPublisher::Tick()
{
	if (!bBusy) return;
	AdmitBuilds();
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewPrepareGT);
	const double Deadline = FPlatformTime::Seconds() + 0.002;
	while (PrepareIndex < Updates.Num() && PreparedMeshes[PrepareIndex])
	{
		if (!PrepareUpdate(PrepareIndex++))
		{
			DiscardBatch();
			bRetry = true;
			return;
		}
		if (FPlatformTime::Seconds() >= Deadline)
		{
			break;
		}
	}
	if (PrepareIndex == Updates.Num()) CommitBatch();
}

void FVoxelViewPublisher::CommitBatch()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewCommit);
	for (FUpdate& Update : Updates)
	{
		AActor* Actor = Update.Actor.Get();
		FEntry* Entry = Entries.Find(Update.Actor);
		if (!Actor || !Entry) continue;
		for (UVoxelMeshComponent* Component : Entry->Components)
		{
			if (Component) Component->DestroyComponent();
		}
		Actor->SetActorLocation(Update.Location);
		Entry->Components = MoveTemp(Update.Components);
		for (UVoxelMeshComponent* Component : Entry->Components) Component->SetVisibility(true);
		Entry->Exclusions = MoveTemp(Update.Exclusions);
		Entry->bDirty = Entry->Source != Update.Source;
		Entry->bPresented = true;
	}
	CommitVisibility();
	Updates.Reset();
	PreparedMeshes.Reset();
	bBusy = false;
	if (OnCommitted)
	{
		TFunction<void()> Completed = MoveTemp(OnCommitted);
		Completed();
	}
}

void FVoxelViewPublisher::CommitVisibility()
{
	for (const auto& Pair : Visibility)
	{
		if (AActor* Actor = Pair.Key.Get(); Actor && Actor->IsHidden() != Pair.Value)
		{
			Actor->SetActorHiddenInGame(Pair.Value);
		}
	}
	Visibility.Reset();
}

void FVoxelViewPublisher::DiscardBatch()
{
	OnCommitted = {};
	++BatchSerial;
	PendingBuilds = 0;
	for (FUpdate& Update : Updates)
	{
		if (!Update.Actor.IsValid()) continue;
		for (UVoxelMeshComponent* Component : Update.Components)
		{
			if (Component) Component->DestroyComponent();
		}
	}
	Updates.Reset();
	Visibility.Reset();
	PreparedMeshes.Reset();
	bBusy = false;
}

void FVoxelViewPublisher::Forget(AActor* InActor)
{
	Entries.Remove(TWeakObjectPtr<AActor>(InActor));
}

void FVoxelViewPublisher::Reset()
{
	++BatchSerial;
	DiscardBatch();
	Entries.Reset();
	bRetry = false;
}

bool FVoxelViewPublisher::IsBusy() const
{
	return bBusy;
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
