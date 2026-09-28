#include "Voxel/Rendering/VoxelViewPublisher.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Components/VoxelMeshComponent.h"
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
		TArray<uint8> Unchanged;
		virtual uint64 GetAllocatedBytes() const override
		{
			uint64 Bytes = Boxes.GetAllocatedSize() + Unchanged.GetAllocatedSize();
			for (const auto& Entry : Boxes) Bytes += Entry.GetAllocatedSize();
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

bool FVoxelViewPublisher::Stage(AActor*& InOutActor, const FVector& InLocation,
	const double InScale, FVoxelSectionMeshResult&& InMesh, const int32 InTerrainStage)
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

void FVoxelViewPublisher::SetCoverage(AActor* InActor, TArray<FBox> InWorldCellBoxes,
	TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> InCommonBoxes)
{
	const FEntry* Entry = Entries.Find(TWeakObjectPtr<AActor>(InActor));
	if (!Entry) return;
	FUpdate& Update = Updates.AddDefaulted_GetRef();
	Update.Actor = InActor;
	Update.Source = Entry->Source;
	Update.TerrainStage = Entry->TerrainStage;
	Update.Location = Entry->Location;
	Update.Scale = Entry->Scale;
	Update.Exclusions = MoveTemp(InWorldCellBoxes);
	Update.CommonBoxes = MoveTemp(InCommonBoxes);
	Update.Bounds = Entry->Bounds;
	Update.PreviousExclusions = Entry->Exclusions;
	Update.bSourceDirty = Entry->bDirty;
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
	Updates.StableSort([](const FUpdate& A, const FUpdate& B)
	{
		return A.TerrainStage < B.TerrainStage;
	});
	++BatchSerial;
	CoverageIndex = 0;
	BuildIndex = 0;
	PrepareIndex = 0;
	PendingBuilds = 0;
	PreparedMeshes.SetNum(Updates.Num());
	bBusy = true;
	bCoveragePrepared = false;
	bCoveragePreparing = false;
	AdmitCoveragePreparation();
	return true;
}

void FVoxelViewPublisher::AdmitCoveragePreparation()
{
	if (bCoveragePrepared || bCoveragePreparing) return;
	TArray<FCoveragePreparationInput> Inputs;
	Inputs.Reserve(FMath::Min(128, Updates.Num() - CoverageIndex));
	uint64 InputBytes = 0;
	for (int32 Index = CoverageIndex; Index < Updates.Num() && Inputs.Num() < 128; ++Index)
	{
		const FUpdate& Update = Updates[Index];
		if (!Inputs.IsEmpty() && InputBytes + Update.Exclusions.GetAllocatedSize() + Update.PreviousExclusions.GetAllocatedSize() > 4ull * 1024ull * 1024ull) break;
		FCoveragePreparationInput& Input = Inputs.AddDefaulted_GetRef();
		Input.Origin = Update.Location / Module.BlockSize();
		Input.Step = Update.Scale / Module.BlockSize();
		Input.Bounds = Update.Bounds;
		Input.Boxes = Update.Exclusions;
		Input.Previous = Update.PreviousExclusions;
		Input.Common = Update.CommonBoxes;
		Input.bDirty = Update.bSourceDirty;
		InputBytes += Input.Boxes.GetAllocatedSize() + Input.Previous.GetAllocatedSize();
	}
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildViewCoverage;
	Request.WorkClass = EVoxelWorkClass::Visible;
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.Section = FIntVector(MIN_int32, 2, 0);
	Request.Stamp.Token = BatchSerial;
	Request.ReservedBytes = 32ull * 1024ull * 1024ull;
	Request.InputBytes = InputBytes + Inputs.GetAllocatedSize();
	Request.Execute = [Inputs = MoveTemp(Inputs)](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		auto Payload = MakeShared<FCoveragePreparationResult, ESPMode::ThreadSafe>();
		Payload->Boxes.SetNum(Inputs.Num());
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
		}
		Result.CustomPayload = Payload;
		Result.bSuccess = true;
		return Result;
	};
	const uint64 Serial = BatchSerial;
	const int32 First = CoverageIndex;
	Request.Apply = [this, Serial, First](FVoxelTaskResult&& Result)
	{
		if (Serial != BatchSerial) return;
		bCoveragePreparing = false;
		if (!Result.bSuccess || Result.bCanceled || Result.Stamp.WorldEpoch != WorldEpoch)
		{
			DiscardBatch();
			bRetry = true;
			return;
		}
		const auto Payload = StaticCastSharedPtr<const FCoveragePreparationResult>(Result.CustomPayload);
		int32 Changed = 0;
		for (int32 Offset = 0; Offset < Payload->Boxes.Num(); ++Offset)
		{
			const int32 Index = First + Offset;
			Updates[Index].Exclusions = Payload->Boxes[Offset];
			Updates[Index].bUnchanged = Payload->Unchanged[Offset] != 0;
			Updates[Index].PreviousExclusions.Reset();
			Updates[Index].CommonBoxes.Reset();
			Changed += Updates[Index].bUnchanged ? 0 : 1;
		}
		CoverageIndex = First + Payload->Boxes.Num();
		bCoveragePrepared = CoverageIndex == Updates.Num();
		UE_LOG(LogTemp, Display, TEXT("Voxel coverage batch: checked=%d changed=%d compareMs=%.2f"),
			Payload->Boxes.Num(), Changed, Result.ExecuteMilliseconds);
	};
	bCoveragePreparing = Scheduler.Enqueue(MoveTemp(Request));
}

void FVoxelViewPublisher::AdmitBuilds()
{
	if (!bCoveragePrepared) return;
	constexpr int32 MaximumPendingBuilds = 4;
	while (BuildIndex < Updates.Num() && PendingBuilds < MaximumPendingBuilds)
	{
		FUpdate& Update = Updates[BuildIndex];
		if (Update.bUnchanged || Update.Exclusions.IsEmpty())
		{
			PreparedMeshes[BuildIndex++] = Update.Source;
			continue;
		}
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildViewCoverage;
		Request.TerrainStage = Update.TerrainStage;
		Request.WorkClass = EVoxelWorkClass::Visible;
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.Token = HashCombineFast(GetTypeHash(BatchSerial), GetTypeHash(BuildIndex));
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
bool FVoxelViewPublisher::PrepareUpdate(const int32 InIndex, const double InDeadline,
	int32& InOutPreparedComponents, bool& bOutComplete)
{
	bOutComplete = true;
	const double PrepareStart = FPlatformTime::Seconds();
	FUpdate& Update = Updates[InIndex];
	if (Update.bUnchanged) return true;
	AActor* Actor = Update.Actor.Get();
	if (!Actor) return true;
	const FVoxelSectionMeshResult& Mesh = *PreparedMeshes[InIndex];
	while (Update.PreparedBatchIndex < Mesh.Batches.Num())
	{
		if (InOutPreparedComponents >= Module.GetViewSettings().MaxPublishComponentsPerFrame)
		{
			bOutComplete = false;
			break;
		}
		const FVoxelRenderBatch& Batch = Mesh.Batches[Update.PreparedBatchIndex++];
		if (Batch.Mesh.Triangles.IsEmpty()) continue;
		const FVoxelMaterialBank* Bank = Module.GetMaterialSet()->FindBank(Batch.Group, Batch.Bank);
		if (!Bank || !Bank->Material) return false;
		UVoxelMeshComponent* Component = NewObject<UVoxelMeshComponent>(Actor);
		Component->SetVisibility(false);
		Component->SetupAttachment(Actor->GetRootComponent());
		Component->RegisterComponent();
		Update.Components.Add(Component);
		if (!Component->Apply(Batch.Mesh, Update.Scale, Bank->Material)) return false;
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
	const double RetireDeadline = FPlatformTime::Seconds() +
		Module.GetViewSettings().PublishRetireMilliseconds / 1000.0;
	while (!RetiredComponents.IsEmpty())
	{
		if (UVoxelMeshComponent* Component = RetiredComponents.Pop().Get()) Component->DestroyComponent();
		if (FPlatformTime::Seconds() >= RetireDeadline) break;
	}
	if (!bBusy) return;
	AdmitCoveragePreparation();
	if (!bCoveragePrepared) return;
	AdmitBuilds();
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewPrepareGT);
	const double Deadline = FPlatformTime::Seconds() +
		Module.GetViewSettings().PublishPrepareMilliseconds / 1000.0;
	int32 PreparedComponents = 0;
	while (PrepareIndex < Updates.Num() && PreparedMeshes[PrepareIndex])
	{
		bool bComplete = false;
		if (!PrepareUpdate(PrepareIndex, Deadline, PreparedComponents, bComplete))
		{
			DiscardBatch();
			bRetry = true;
			return;
		}
		if (bComplete) ++PrepareIndex;
		if (PreparedComponents >= Module.GetViewSettings().MaxPublishComponentsPerFrame ||
			FPlatformTime::Seconds() >= Deadline)
		{
			break;
		}
	}
	if (PrepareIndex == Updates.Num()) CommitBatch();
}

void FVoxelViewPublisher::CommitBatch()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewCommit);
	const double CommitStart = FPlatformTime::Seconds();
	const int32 UpdateCount = Updates.Num();
	for (FUpdate& Update : Updates)
	{
		AActor* Actor = Update.Actor.Get();
		FEntry* Entry = Entries.Find(Update.Actor);
		if (!Actor || !Entry || Update.bUnchanged) continue;
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
		Entry->bDirty = Entry->Source != Update.Source;
		Entry->bPresented = true;
	}
	CommitVisibility();
#if !UE_BUILD_SHIPPING
	const double CommitMs = (FPlatformTime::Seconds() - CommitStart) * 1000.0;
	if (CommitMs > 8.0)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel publish slow commit: ms=%.2f updates=%d"), CommitMs, UpdateCount);
	}
#endif
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
	bCoveragePreparing = false;
	bCoveragePrepared = false;
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
	for (const auto& Retired : RetiredComponents)
	{
		if (UVoxelMeshComponent* Component = Retired.Get()) Component->DestroyComponent();
	}
	RetiredComponents.Reset();
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
