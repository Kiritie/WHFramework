#include "Voxel/Save/VoxelRegionStore.h"

#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

struct FVoxelRegionDirectoryLoadAttempt
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> SourceLease;
	FString SourceDirectory;
	FVoxelTaskStamp Stamp;
	FVoxelGenerationConsumerKey Consumer;
	FVoxelRegionStore* Owner = nullptr;
	TAtomic<bool> Detached{false};
	TAtomic<bool> CapacityBlocked{false};
	TAtomic<uint64> CapacityVersion{0};
	bool bQueued = false;
	TFunction<void(TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe>)> OnReady;
	TOptional<FVoxelTaskRequest> PendingTask;
	uint64 BindingCapacityVersion = 0;
	bool bBindingCapacityBlocked = false;
};

namespace
{
	struct FDirectoryLoadPayload final : FVoxelTaskCustomPayload
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> Directory;
		virtual uint64 GetAllocatedBytes() const override
		{
			return sizeof(*this) + (Directory && Directory->ResourceLease ? Directory->ResourceLease->Bytes() : 0);
		}
		virtual uint64 GetResourceAccountedBytes() const override
		{
			return (MetadataLease ? MetadataLease->Bytes() : 0) +
				(Directory && Directory->ResourceLease ? Directory->ResourceLease->Bytes() : 0);
		}
	};
	struct FDirectoryPayloadReclaimer
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe> Payload;
		void operator()() const
		{
		}
	};
}

FVoxelRegionStore::FVoxelRegionStore()
{
}

FVoxelRegionStore::~FVoxelRegionStore()
{
	DetachDirectoryLoad();
}

void FVoxelRegionStore::DetachDirectoryLoad()
{
	check(DirectoryOwnerIncarnation < MAX_uint64);
	++DirectoryOwnerIncarnation;
	if (DirectoryLoadAttempt)
	{
		DirectoryLoadAttempt->Owner = nullptr;
		DirectoryLoadAttempt->Detached.Store(true);
		DirectoryLoadAttempt->PendingTask.Reset();
		DirectoryLoadAttempt.Reset();
	}
}

bool FVoxelRegionStore::IsDirectoryReady() const
{
	return bDirectoryReady && DirectoryError.IsEmpty();
}

void FVoxelRegionStore::AdvanceDirectoryLoad(FVoxelTaskScheduler& InScheduler, const uint64 InWorldEpoch,
	TFunction<void(TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe>)> InOnReady)
{
	check(IsInGameThread());
	if (bDirectoryReady || !DirectoryError.IsEmpty()) return;
	if (!SourceLease || !SourceLease->Matches(SourceDirectory))
	{
		DirectoryError = TEXT("Voxel启动目录缺少固定文件代租约");
		return;
	}
	if (!DirectoryLoadAttempt)
	{
		check(DirectoryTaskSerial < MAX_uint64);
		const auto Consumer = FVoxelGenerationConsumerKey::Section(InWorldEpoch, DirectoryOwnerId,
			DirectoryOwnerIncarnation, {}, ++DirectoryTaskSerial);
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources = InScheduler.GetRuntimeResources();
		if (!SourceLease->IsResourceAccountedFor(Resources.Get(), InWorldEpoch))
		{
			TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> GrantedSource;
			bool bCapacityBlocked = false;
			if (!FSaveGameGenerationReadLease::AcquireGranted(SourceDirectory, Resources, Consumer, GrantedSource, bCapacityBlocked))
			{
				if (!bCapacityBlocked) DirectoryError = TEXT("Voxel启动文件代已进入退休，不能接纳其固定目录");
				return;
			}
			SourceLease = MoveTemp(GrantedSource);
		}
		const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FVoxelRegionDirectoryLoadAttempt>() +
			VoxelBoundedAllocation::SharedBytes<FDirectoryLoadPayload>() +
			VoxelBoundedAllocation::ArrayBytes<TCHAR>(SourceDirectory.Len() + 1) +
			VoxelBoundedAllocation::TaskBytes<FDirectoryPayloadReclaimer>();
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease)) return;
		auto State = MakeShared<FVoxelRegionDirectoryLoadAttempt, ESPMode::ThreadSafe>();
		State->MetadataLease = MoveTemp(Lease);
		State->Resources = Resources;
		State->SourceLease = SourceLease;
		State->SourceDirectory = SourceDirectory;
		State->Consumer = Consumer;
		State->Stamp.OwnerId = DirectoryOwnerId;
		State->Stamp.OwnerIncarnation = DirectoryOwnerIncarnation;
		State->Stamp.WorldEpoch = InWorldEpoch;
		State->Stamp.Token = Consumer.RequestSerial;
		State->Owner = this;
		State->OnReady = MoveTemp(InOnReady);
		DirectoryLoadAttempt = MoveTemp(State);
	}
	const auto State = DirectoryLoadAttempt;
	if (State->bQueued) return;
	if (State->Stamp.WorldEpoch != InWorldEpoch || State->Stamp.OwnerId != DirectoryOwnerId ||
		State->Stamp.OwnerIncarnation != DirectoryOwnerIncarnation || State->Detached.Load())
	{
		State->PendingTask.Reset();
		DirectoryError = TEXT("Voxel启动目录任务不再属于原世界与文件代");
		return;
	}
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
		Task.bTerrainDataOnly = true;
		Task.ServiceBand = EVoxelTerrainServiceBand::P0;
		Task.SourceStableKey = DirectoryOwnerId;
		Task.FirstRunnableSeconds = DirectoryFirstRunnable;
		Task.bResumable = true;
		Task.ReservedBytes = 1;
	}
	auto& Task = State->PendingTask.GetValue();
	auto GetBlockReason = [State]
	{
		if (State->Detached.Load() || !State->CapacityBlocked.Load()) return EVoxelTaskBlockReason::None;
		uint64 ReleaseVersion = 0;
		return State->Resources->TryGetReleaseVersionExcludingOwner(State->Consumer, ReleaseVersion) &&
			ReleaseVersion > State->CapacityVersion.Load() ? EVoxelTaskBlockReason::None : EVoxelTaskBlockReason::Memory;
	};
	auto Execute = [State](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		if (Cancel.Load() || State->Detached.Load()) { Result.bCanceled = true; return Result; }
		State->CapacityBlocked.Store(false);
		const uint64 ReleaseVersion = State->Resources->ReleaseVersionExcludingOwner(State->Consumer);
		TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> Directory;
		if (!FVoxelRegionStore::BuildDirectory(State->SourceDirectory, State->Resources, State->Consumer,
			Directory, Result.Error, &Cancel, &State->Detached))
		{
			if (Result.Error.StartsWith(TEXT("CapacityBlocked")))
			{
				State->CapacityVersion.Store(ReleaseVersion);
				State->CapacityBlocked.Store(true);
				Result.bSuccess = Result.bContinue = true;
				Result.Error.Reset();
			}
			else Result.bCanceled = Cancel.Load() || State->Detached.Load();
			return Result;
		}
		if (Cancel.Load() || State->Detached.Load()) { Result.bCanceled = true; return Result; }
		auto Payload = MakeShared<FDirectoryLoadPayload, ESPMode::ThreadSafe>();
		Payload->MetadataLease = State->MetadataLease;
		Payload->Directory = MoveTemp(Directory);
		Result.CustomPayload = MoveTemp(Payload);
		Result.bSuccess = true;
		return Result;
	};
	auto Apply = [State](FVoxelTaskResult&& Result)
	{
		FVoxelRegionStore* Owner = State->Owner;
		if (!Owner || Owner->DirectoryLoadAttempt != State || State->Detached.Load() ||
			Owner->DirectoryOwnerIncarnation != Result.Stamp.OwnerIncarnation || State->Stamp != Result.Stamp)
		{
			if (Result.CustomPayload) UE::Tasks::Launch(UE_SOURCE_LOCATION,
				FDirectoryPayloadReclaimer{State->MetadataLease, MoveTemp(Result.CustomPayload)});
			return;
		}
		State->bQueued = false;
		if (Result.bCanceled) return;
		if (!Result.bSuccess || !Result.CustomPayload)
		{
			Owner->DirectoryError = Result.Error.IsEmpty() ? TEXT("Voxel启动目录读取失败") : Result.Error;
			return;
		}
		const auto Payload = StaticCastSharedPtr<const FDirectoryLoadPayload>(Result.CustomPayload);
		Owner->Directory = Payload->Directory;
		if (State->OnReady) State->OnReady(Owner->Directory);
		Owner->bDirectoryReady = true;
		Result.MarkWorkUsed();
		State->SourceLease.Reset();
		Owner->DirectoryLoadAttempt.Reset();
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
		else
		{
			State->PendingTask.Reset();
			DirectoryError = BindingError;
		}
		return;
	}
	FVoxelTaskRequest AdmissionRequest = MoveTemp(Task);
	State->PendingTask.Reset();
	if (InScheduler.Enqueue(MoveTemp(AdmissionRequest)))
	{
		State->bQueued = true;
	}
	else if (State->Detached.Load()) return;
	else if (AdmissionRequest.Blocked == EVoxelTaskBlockReason::Oversized || AdmissionRequest.Blocked == EVoxelTaskBlockReason::TerminalFailure)
	{
		State->PendingTask.Reset();
		DirectoryError = TEXT("Voxel启动目录任务无法在原资源域内接纳");
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
