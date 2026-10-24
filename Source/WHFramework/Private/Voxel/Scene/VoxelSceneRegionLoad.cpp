#include "Voxel/Scene/VoxelSceneRegion.h"

#include "SaveGame/SaveDataSerializer.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Save/VoxelSceneColumnRead.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/VoxelModule.h"

struct FVoxelSceneColumnLoadAttempt
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> Source;
	TSharedPtr<const FVoxelSceneColumnRead, ESPMode::ThreadSafe> Read;
	TWeakObjectPtr<UVoxelSceneRegion> Owner;
	FVoxelTaskStamp Stamp;
	FVoxelGenerationConsumerKey Consumer;
	FIntPoint Column;
	uint32 ExpectedBytes = 0;
	uint64 SourceIncarnation = 0;
	TAtomic<bool> Detached{false};
	TAtomic<bool> CapacityBlocked{false};
	TAtomic<uint64> CapacityVersion{0};
	int32 ActorCursor = 0;
	double FirstRunnableSeconds = -1.0;
	uint64 RestoreCapacityVersion = 0;
	bool bRestoreCapacityBlocked = false;
	bool bQueued = false;
	TOptional<FVoxelTaskRequest> PendingTask;
	uint64 BindingCapacityVersion = 0;
	bool bBindingCapacityBlocked = false;
};

namespace
{
	struct FSceneColumnLoadPayload final : FVoxelTaskCustomPayload
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		TSharedPtr<const FVoxelSceneColumnRead, ESPMode::ThreadSafe> Read;
		virtual uint64 GetAllocatedBytes() const override
		{
			return sizeof(*this) + (Read ? sizeof(*Read) + Read->Actors.GetAllocatedSize() + Read->Body.Bytes.GetAllocatedSize() : 0);
		}
		virtual uint64 GetResourceAccountedBytes() const override
		{
			return (MetadataLease ? MetadataLease->Bytes() : 0) +
				(Read && Read->MetadataLease ? Read->MetadataLease->Bytes() : 0) +
				(Read && Read->Body.ResourceLease ? Read->Body.ResourceLease->Bytes() : 0);
		}
	};
}

bool UVoxelSceneRegion::BeginSceneLoad(FVoxelTaskScheduler& InScheduler,
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> InSource, const uint32 InExpectedBytes)
{
	check(IsInGameThread());
	if (!Module || bSceneReady || bSceneLoading || SceneLoadAttempt || SceneLoadSource || !InSource) return false;
	SceneLoadResources = InScheduler.GetRuntimeResources();
	if (!InSource->IsResourceAccountedFor(SceneLoadResources.Get(), Module->GetWorldEpoch()))
	{ SceneLoadResources.Reset(); return false; }
	check(SceneLoadSerial < MAX_uint64);
	SceneLoadConsumer = FVoxelGenerationConsumerKey::Section(Module->GetWorldEpoch(), SceneInstanceId,
		1, RegionKey, ++SceneLoadSerial);
	SceneLoadSource = MoveTemp(InSource);
	SceneLoadSourceIncarnation = Module->SceneSourceIncarnation;
	SceneLoadExpectedBytes = InExpectedBytes;
	SceneLoadFirstRunnableSeconds = FPlatformTime::Seconds();
	SceneLoadAdmission = EVoxelTerrainResourceAdmission::None;
	bSceneLoading = true;
	++Module->SceneLoadingCount;
	// The original Region owns the scalar cut and directory pin before native admission.
	// A blocked grant is an accepted load obligation, not a reason to recreate the Region.
	PrepareSceneLoadAttempt();
	return true;
}

bool UVoxelSceneRegion::PrepareSceneLoadAttempt()
{
	if (SceneLoadAttempt) return true;
	if (!Module || !bSceneLoading || !SceneLoadResources || !SceneLoadSource) return false;
	const auto Fail = [&]()
	{
		bSceneFailed = true;
		bSceneLoading = false;
		check(Module->SceneLoadingCount > 0);
		--Module->SceneLoadingCount;
		UE_LOG(LogTemp, Error, TEXT("Scene load control admission failed; original Region and source retained: %d"), int32(SceneLoadAdmission));
		return false;
	};
	if (Module->GetWorldEpoch() != SceneLoadConsumer.WorldEpoch ||
		Module->SceneSourceIncarnation != SceneLoadSourceIncarnation)
	{ SceneLoadAdmission = EVoxelTerrainResourceAdmission::InvalidRequest; return Fail(); }
	if (SceneLoadAdmission != EVoxelTerrainResourceAdmission::None &&
		SceneLoadAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked) return Fail();
	if (SceneLoadAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked)
	{
		uint64 Version = 0;
		if (!SceneLoadResources->TryGetReleaseVersionExcludingOwner(SceneLoadConsumer, Version) ||
			Version <= SceneLoadCapacityVersion) return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSceneColumnLoadAttempt>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FSceneColumnLoadPayload>();
	const auto& SourceLease = SceneLoadSource->GetResourceLease();
	const uint64 InputFloor = SourceLease && SourceLease->GetPool() == EVoxelTerrainResourcePool::Input ? SourceLease->Bytes() : 0;
	SceneLoadCapacityVersion = SceneLoadResources->ReleaseVersionExcludingOwner(SceneLoadConsumer);
	SceneLoadAdmission = SceneLoadResources->TryAcquire(SceneLoadConsumer, EVoxelTerrainResourcePool::Input,
		Bytes, Lease, 0, InputFloor, SourceLease ? 1 : 0);
	if (SceneLoadAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked) return false;
	if (SceneLoadAdmission != EVoxelTerrainResourceAdmission::Granted) return Fail();
	auto State = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSceneColumnLoadAttempt>(Lease);
	State->MetadataLease = MoveTemp(Lease);
	State->Resources = SceneLoadResources;
	State->Source = SceneLoadSource;
	State->Consumer = SceneLoadConsumer;
	State->Column = FIntPoint(RegionKey.X, RegionKey.Y);
	State->ExpectedBytes = SceneLoadExpectedBytes;
	State->SourceIncarnation = SceneLoadSourceIncarnation;
	State->FirstRunnableSeconds = SceneLoadFirstRunnableSeconds;
	State->Owner = this;
	State->Stamp.OwnerId = SceneInstanceId;
	State->Stamp.OwnerIncarnation = 1;
	State->Stamp.WorldEpoch = SceneLoadConsumer.WorldEpoch;
	State->Stamp.Token = SceneLoadConsumer.RequestSerial;
	SceneLoadAttempt = MoveTemp(State);
	SceneLoadAttempt->MetadataLease->MarkPublished();
	return true;
}

void UVoxelSceneRegion::DetachSceneLoad()
{
	if (SceneLoadAttempt)
	{
		SceneLoadAttempt->Detached.Store(true);
		SceneLoadAttempt->PendingTask.Reset();
		SceneLoadAttempt.Reset();
	}
	SceneLoadSource.Reset();
	SceneLoadResources.Reset();
	SceneLoadConsumer = {};
	SceneLoadAdmission = EVoxelTerrainResourceAdmission::None;
	if (bSceneLoading && Module)
	{
		check(Module->SceneLoadingCount > 0);
		--Module->SceneLoadingCount;
	}
	bSceneLoading = false;
}

void UVoxelSceneRegion::RetrySceneLoad()
{
	if (!Module || (!SceneLoadAttempt && !SceneLoadSource) || bSceneLoading || bSceneReady) return;
	PendingActors.Empty();
	PendingActorStorageLease.Reset();
	if (SceneLoadAttempt)
	{
		SceneLoadAttempt->ActorCursor = 0;
		SceneLoadAttempt->CapacityBlocked.Store(false);
		SceneLoadAttempt->bRestoreCapacityBlocked = false;
		SceneLoadAttempt->bBindingCapacityBlocked = false;
	}
	else SceneLoadAdmission = EVoxelTerrainResourceAdmission::None;
	bSceneFailed = false;
	bSceneLoading = true;
	++Module->SceneLoadingCount;
}

void UVoxelSceneRegion::AdvanceSceneLoad(FVoxelTaskScheduler& InScheduler)
{
	check(IsInGameThread());
	if (!SceneLoadAttempt && !PrepareSceneLoadAttempt()) return;
	const auto State = SceneLoadAttempt;
	if (!Module || !State || !bSceneLoading || State->Detached.Load()) return;
	const auto Fail = [&](const FString& InError)
	{
		if (!Module || SceneLoadAttempt != State || State->Detached.Load() || !bSceneLoading) return;
		State->PendingTask.Reset();
		State->bBindingCapacityBlocked = false;
		bSceneFailed = true;
		bSceneLoading = false;
		check(Module->SceneLoadingCount > 0);
		--Module->SceneLoadingCount;
		UE_LOG(LogTemp, Error, TEXT("Scene column restore failed; exact region/source retained: %s"), *InError);
	};
	if (Module->GetWorldEpoch() != State->Stamp.WorldEpoch || SceneInstanceId != State->Stamp.OwnerId ||
		!bRegisteredRegion || Module->SceneSourceIncarnation != State->SourceIncarnation)
	{
		Fail(TEXT("Scene read no longer belongs to its accepted world/region/file generation"));
		return;
	}
	if (State->Read)
	{
		const auto Read = State->Read;
		if (State->bRestoreCapacityBlocked)
		{
			uint64 Version = 0;
			if (!State->Resources->TryGetReleaseVersionExcludingOwner(State->Consumer, Version) ||
				Version <= State->RestoreCapacityVersion) return;
			State->bRestoreCapacityBlocked = false;
		}
		if (!PendingActorStorageLease)
		{
			const uint64 Bytes = VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FGuid, FParameter>>(PendingActors, Read->Actors.Num());
			uint64 Version = 0;
			if (!State->Resources->TryGetReleaseVersionExcludingOwner(State->Consumer, Version)) return;
			uint64 InputFloor = 0;
			int32 LeaseCount = 0;
			const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Required[] =
				{State->MetadataLease, State->Source->GetResourceLease(), Read->MetadataLease, Read->Body.ResourceLease};
			const FVoxelTerrainResourceLease* Seen[UE_ARRAY_COUNT(Required)] = {};
			for (const auto& Lease : Required)
			{
				if (!Lease) continue;
				if (!Lease->IsOwnedBy(*State->Resources)) { Fail(TEXT("InvalidRequest: Scene restore source domain")); return; }
				bool bSeen = false;
				for (int32 Index = 0; Index < LeaseCount; ++Index) bSeen |= Seen[Index]->Serial() == Lease->Serial();
				if (bSeen) continue;
				const uint64 Input = Lease->GetPool() == EVoxelTerrainResourcePool::Input ? Lease->Bytes() : 0;
				if (Input > MAX_uint64 - InputFloor) { Fail(TEXT("CapacityUnachievable: Scene restore source floor")); return; }
				InputFloor += Input;
				Seen[LeaseCount++] = Lease.Get();
			}
			const auto Admission = Bytes == 0 ? EVoxelTerrainResourceAdmission::Granted :
				State->Resources->TryAcquire(State->Consumer, EVoxelTerrainResourcePool::Resident,
					Bytes, PendingActorStorageLease, 0, InputFloor, LeaseCount);
			if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
			{
				State->RestoreCapacityVersion = Version;
				State->bRestoreCapacityBlocked = true;
				return;
			}
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{ Fail(Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
				TEXT("CapacityUnachievable: Scene restore actor directory") : TEXT("InvalidRequest: Scene restore actor directory")); return; }
			PendingActors.Reserve(Read->Actors.Num());
			checkf(VoxelGenerationAllocation::AllocatedBytes(PendingActors) <= Bytes,
				TEXT("Scene actor directory capacity: saved=%d pending=%d native=%llu heap=%llu granted=%llu"),
				Read->Actors.Num(), PendingActors.Num(), uint64(PendingActors.GetAllocatedSize()), VoxelGenerationAllocation::AllocatedBytes(PendingActors), Bytes);
		}
		FString Error;
		if (State->ActorCursor < Read->Actors.Num())
		{
			const auto& Actor = Read->Actors[State->ActorCursor];
			FParameter Data;
			if (Module->FindSceneActor(Actor.Id) || PendingActors.Contains(Actor.Id) ||
				!FSaveDataSerializer::DeserializeParameterView(Read->ActorPayload(State->ActorCursor), Data))
			{
				Fail(TEXT("Invalid or duplicate Scene actor parameter payload"));
				return;
			}
			if (!Module || Module->GetWorldEpoch() != State->Stamp.WorldEpoch ||
				Module->SceneSourceIncarnation != State->SourceIncarnation ||
				SceneInstanceId != State->Stamp.OwnerId || SceneLoadAttempt != State || State->Detached.Load()) return;
			PendingActors.Add(Actor.Id, MoveTemp(Data));
			++State->ActorCursor;
			return;
		}
		FParameter ProjectData;
		if ((Read->ProjectBytes > 0 && !FSaveDataSerializer::DeserializeParameterView(Read->ProjectPayload(), ProjectData)) ||
			!RestoreProjectData(ProjectData, Error))
		{
			if (Error.IsEmpty()) Error = TEXT("Invalid Scene project parameter payload");
			Fail(Error);
			return;
		}
		if (!Module || Module->GetWorldEpoch() != State->Stamp.WorldEpoch ||
			Module->SceneSourceIncarnation != State->SourceIncarnation ||
			SceneInstanceId != State->Stamp.OwnerId || SceneLoadAttempt != State || State->Detached.Load()) return;
		bSceneReady = true;
		bSceneLoading = false;
		check(Module->SceneLoadingCount > 0);
		--Module->SceneLoadingCount;
		SceneLoadAttempt.Reset();
		SceneLoadSource.Reset();
		SceneLoadResources.Reset();
		MarkSceneChanged();
		return;
	}
	if (State->bQueued) return;
	const auto BindingResources = InScheduler.GetTerrainResources();
	const auto BindingConsumer = InScheduler.GetControlConsumer();
	if (State->bBindingCapacityBlocked)
	{
		uint64 Current = 0;
		if (!BindingResources->TryGetReleaseVersionExcludingOwner(BindingConsumer, Current) ||
			Current <= State->BindingCapacityVersion) return;
		State->bBindingCapacityBlocked = false;
	}
	const uint64 BeforeBinding = BindingResources->ReleaseVersionExcludingOwner(BindingConsumer);
	if (!State->PendingTask.IsSet())
	{
		State->PendingTask.Emplace();
		auto& Task = State->PendingTask.GetValue();
		Task.Kind = EVoxelTaskKind::ProjectBackground;
		Task.Stamp = State->Stamp;
		Task.WorkClass = EVoxelWorkClass::Critical;
		Task.ServiceBand = EVoxelTerrainServiceBand::P0;
		Task.bTerrainDataOnly = true;
		Task.bResumable = true;
		Task.SourceStableKey = State->Stamp.OwnerId;
		Task.FirstRunnableSeconds = State->FirstRunnableSeconds;
		Task.ReservedBytes = 1;
	}
	auto& Task = State->PendingTask.GetValue();
	auto GetBlockReason = [State]
	{
		if (State->Detached.Load() || !State->CapacityBlocked.Load()) return EVoxelTaskBlockReason::None;
		uint64 Version = 0;
		return State->Resources->TryGetReleaseVersionExcludingOwner(State->Consumer, Version) &&
			Version > State->CapacityVersion.Load() ? EVoxelTaskBlockReason::None : EVoxelTaskBlockReason::Memory;
	};
	auto Execute = [State](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		if (InCancel.Load() || State->Detached.Load())
		{
			Result.bCanceled = true;
			return Result;
		}
		State->CapacityBlocked.Store(false);
		const uint64 Version = State->Resources->ReleaseVersionExcludingOwner(State->Consumer);
		TSharedPtr<const FVoxelSceneColumnRead, ESPMode::ThreadSafe> Read;
		if (!FVoxelSceneColumnRead::Load(State->Source, State->Column, State->ExpectedBytes, State->Resources,
			State->Consumer, Read, Result.Error, &InCancel, &State->Detached, State->MetadataLease))
		{
			if (Result.Error.StartsWith(TEXT("CapacityBlocked:")))
			{
				State->CapacityVersion.Store(Version);
				State->CapacityBlocked.Store(true);
				Result.bSuccess = Result.bContinue = true;
				Result.Error.Reset();
			}
			else Result.bCanceled = InCancel.Load() || State->Detached.Load();
			return Result;
		}
		auto Payload = VoxelBoundedAllocation::MakeWorkerShared<FSceneColumnLoadPayload>(State->MetadataLease);
		Payload->MetadataLease = State->MetadataLease;
		Payload->Read = MoveTemp(Read);
		Result.CustomPayload = MoveTemp(Payload);
		Result.bSuccess = true;
		return Result;
	};
	auto Apply = [State](FVoxelTaskResult&& InResult)
	{
		auto* Owner = State->Owner.Get();
		if (!Owner || !Owner->Module || State->Detached.Load() || Owner->SceneLoadAttempt != State ||
			Owner->SceneInstanceId != State->Stamp.OwnerId || Owner->Module->GetWorldEpoch() != State->Stamp.WorldEpoch ||
			Owner->Module->SceneSourceIncarnation != State->SourceIncarnation ||
			InResult.Stamp != State->Stamp) return;
		State->bQueued = false;
		if (!InResult.bSuccess || !InResult.CustomPayload || InResult.bCanceled)
		{
			Owner->bSceneFailed = true;
			Owner->bSceneLoading = false;
			check(Owner->Module->SceneLoadingCount > 0);
			--Owner->Module->SceneLoadingCount;
			UE_LOG(LogTemp, Error, TEXT("Scene column read failed; source retained: %s"), *InResult.Error);
			return;
		}
		const auto Payload = StaticCastSharedPtr<const FSceneColumnLoadPayload>(InResult.CustomPayload);
		State->Read = Payload->Read;
		State->Read->MetadataLease->MarkPublished();
		State->Read->Body.ResourceLease->MarkPublished();
		InResult.MarkWorkUsed();
	};
	FString BindingError;
	if ((!Task.bBlockBound && !Task.BindGetBlockReason(BindingResources, BindingConsumer, MoveTemp(GetBlockReason), BindingError)) ||
		(!Task.bExecuteBound && !Task.BindExecute(BindingResources, BindingConsumer, MoveTemp(Execute), BindingError)) ||
		(!Task.bApplyBound && !Task.BindApply(BindingResources, BindingConsumer, MoveTemp(Apply), BindingError)))
	{
		if (BindingError.StartsWith(TEXT("CapacityBlocked")))
		{
			State->bBindingCapacityBlocked = true;
			State->BindingCapacityVersion = BeforeBinding;
		}
		else Fail(BindingError);
		return;
	}
	FVoxelTaskRequest AdmissionRequest = MoveTemp(Task);
	State->PendingTask.Reset();
	State->bQueued = true;
	if (InScheduler.Enqueue(MoveTemp(AdmissionRequest)))
	{
		return;
	}
	if (State->Detached.Load() || !Module || SceneLoadAttempt != State || !bSceneLoading) return;
	State->bQueued = false;
	if (AdmissionRequest.Blocked == EVoxelTaskBlockReason::Oversized || AdmissionRequest.Blocked == EVoxelTaskBlockReason::TerminalFailure)
	{
		Fail(TEXT("Scene task admission cannot fit its original resource domain"));
	}
	else
	{
		State->PendingTask.Emplace(MoveTemp(AdmissionRequest));
		if (State->PendingTask->Blocked == EVoxelTaskBlockReason::InputMemory || State->PendingTask->Blocked == EVoxelTaskBlockReason::Memory)
		{
			State->bBindingCapacityBlocked = true;
			State->BindingCapacityVersion = BeforeBinding;
		}
	}
}
