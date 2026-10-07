#include "Voxel/Scene/VoxelSceneRegion.h"

#include "SaveGame/SaveDataSerializer.h"
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
	if (!Module || bSceneReady || bSceneLoading || SceneLoadAttempt || !InSource) return false;
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources = InScheduler.GetRuntimeResources();
	check(SceneLoadSerial < MAX_uint64);
	const auto Consumer = FVoxelGenerationConsumerKey::Section(Module->GetWorldEpoch(), SceneInstanceId,
		1, RegionKey, ++SceneLoadSerial);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSceneColumnLoadAttempt>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FSceneColumnLoadPayload>();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease)) return false;
	auto State = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSceneColumnLoadAttempt>(Lease);
	State->MetadataLease = MoveTemp(Lease);
	State->Resources = Resources;
	State->Source = MoveTemp(InSource);
	State->Consumer = Consumer;
	State->Column = FIntPoint(RegionKey.X, RegionKey.Y);
	State->ExpectedBytes = InExpectedBytes;
	State->SourceIncarnation = Module->SceneSourceIncarnation;
	State->FirstRunnableSeconds = FPlatformTime::Seconds();
	State->Owner = this;
	State->Stamp.OwnerId = SceneInstanceId;
	State->Stamp.OwnerIncarnation = 1;
	State->Stamp.WorldEpoch = Consumer.WorldEpoch;
	State->Stamp.Token = Consumer.RequestSerial;
	SceneLoadAttempt = MoveTemp(State);
	bSceneLoading = true;
	++Module->SceneLoadingCount;
	return true;
}

void UVoxelSceneRegion::DetachSceneLoad()
{
	if (SceneLoadAttempt)
	{
		SceneLoadAttempt->Detached.Store(true);
		SceneLoadAttempt.Reset();
	}
	if (bSceneLoading && Module)
	{
		check(Module->SceneLoadingCount > 0);
		--Module->SceneLoadingCount;
	}
	bSceneLoading = false;
}

void UVoxelSceneRegion::RetrySceneLoad()
{
	if (!Module || !SceneLoadAttempt || bSceneLoading || bSceneReady) return;
	PendingActors.Empty();
	PendingActorStorageLease.Reset();
	SceneLoadAttempt->ActorCursor = 0;
	SceneLoadAttempt->CapacityBlocked.Store(false);
	SceneLoadAttempt->bRestoreCapacityBlocked = false;
	bSceneFailed = false;
	bSceneLoading = true;
	++Module->SceneLoadingCount;
}

void UVoxelSceneRegion::AdvanceSceneLoad(FVoxelTaskScheduler& InScheduler)
{
	check(IsInGameThread());
	const auto State = SceneLoadAttempt;
	if (!Module || !State || !bSceneLoading || State->Detached.Load()) return;
	const auto Fail = [&](const FString& InError)
	{
		if (!Module || SceneLoadAttempt != State || State->Detached.Load() || !bSceneLoading) return;
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
			const uint64 Bytes = VoxelBoundedAllocation::MapBytes<FGuid, FParameter>(Read->Actors.Num());
			uint64 Version = 0;
			if (!State->Resources->TryGetReleaseVersionExcludingOwner(State->Consumer, Version)) return;
			if (Bytes > 0 && !State->Resources->Acquire(State->Consumer, EVoxelTerrainResourcePool::Resident,
				Bytes, PendingActorStorageLease))
			{
				State->RestoreCapacityVersion = Version;
				State->bRestoreCapacityBlocked = true;
				return;
			}
			PendingActors.Reserve(Read->Actors.Num());
			check(PendingActors.GetAllocatedSize() <= Bytes);
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
		MarkSceneChanged();
		return;
	}
	if (State->bQueued) return;
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
			State->Consumer, Read, Result.Error, &InCancel, &State->Detached))
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
		InResult.MarkWorkUsed();
	};
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSceneColumnLoadAttempt>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FSceneColumnLoadPayload>() +
		VoxelBoundedAllocation::FunctionBytes<decltype(GetBlockReason)>() +
		VoxelBoundedAllocation::FunctionBytes<decltype(Execute)>() + VoxelBoundedAllocation::FunctionBytes<decltype(Apply)>();
	if (!State->MetadataLease->Resize(Bytes)) return;
	FVoxelTaskRequest Task;
	Task.Kind = EVoxelTaskKind::ProjectBackground;
	Task.Stamp = State->Stamp;
	Task.WorkClass = EVoxelWorkClass::Critical;
	Task.ServiceBand = EVoxelTerrainServiceBand::P0;
	Task.bTerrainDataOnly = true;
	Task.bResumable = true;
	Task.SourceStableKey = State->Stamp.OwnerId;
	Task.FirstRunnableSeconds = State->FirstRunnableSeconds;
	Task.GetBlockReason = MoveTemp(GetBlockReason);
	Task.Execute = MoveTemp(Execute);
	Task.Apply = MoveTemp(Apply);
	State->bQueued = InScheduler.Enqueue(MoveTemp(Task));
}
