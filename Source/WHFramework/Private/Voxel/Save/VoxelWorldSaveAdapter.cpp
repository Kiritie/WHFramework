#include "Voxel/Save/VoxelWorldSaveAdapter.h"

#include "Misc/Paths.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelSceneSaveCapture.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
	struct FSaveCaptureReclaimer
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> TaskLease;
		FVoxelWorldSaveCapture Capture;
		void operator()() const
		{
			// 捕获的旧树、文件代和目录引用在Worker销毁，不触及Runtime或UObject。
		}
	};
}

bool FVoxelWorldSaveAdapter::Capture(FVoxelWorldRuntime& InRuntime, const FVoxelWorldManifest& InManifest,
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
	const FVoxelRegionStore& InStore, FString& OutError)
{
	check(IsInGameThread());
	if (Active.IsSet() || !RetireCaptureTask.IsCompleted() || !InRuntime.IsServer() || !InRegistry || !InStore.IsDirectoryReady())
	{
		OutError = TEXT("Voxel save capture is busy or not authoritative");
		return false;
	}
	RetireCaptureTask = {};
	if (!Pending.IsSet())
	{
		RetireTaskLease.Reset();
		Pending.Emplace();
		Pending->TransactionId = FGuid::NewGuid();
		Pending->WorldEpoch = InRuntime.Epoch();
		bCaptureRootsFrozen = false;
		bCaptureCapacityBlocked = false;
	}
	auto& Capture = Pending.GetValue();
	if (Capture.WorldEpoch != InRuntime.Epoch() || (bCaptureRootsFrozen && Capture.Registry != InRegistry))
	{
		OutError = TEXT("InvalidRequest: 原保存捕获的 World 或 Registry 已改变");
		return false;
	}
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources = InRuntime.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(Capture.WorldEpoch, Capture.TransactionId, 1, {}, 1);
	const uint64 BeforeVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (bCaptureCapacityBlocked && BeforeVersion <= CaptureCapacityVersion)
	{
		OutError = TEXT("CapacityBlocked: 原保存固定切点等待外部容量释放");
		return false;
	}
	bCaptureCapacityBlocked = false;
	const auto Rejected = [&](const EVoxelTerrainResourceAdmission Admission, const TCHAR* Stage)
	{
		bCaptureCapacityBlocked = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
		if (bCaptureCapacityBlocked) CaptureCapacityVersion = BeforeVersion;
		OutError = FString::Printf(TEXT("%s: 保存固定切点%s"), bCaptureCapacityBlocked ? TEXT("CapacityBlocked") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable") : TEXT("InvalidRequest"), Stage);
		return false;
	};
	const FVoxelGenerationRequiredSourceScope TaskSource(Resources.Get(), RetireTaskLease.Get());
	const FVoxelGenerationRequiredSourceScope MetadataSource(Resources.Get(), Capture.MetadataLease.Get());
	const FVoxelGenerationRequiredSourceScope DirectorySource(Resources.Get(),
		bCaptureRootsFrozen ? (Capture.SourceDirectoryRoot ? Capture.SourceDirectoryRoot->ResourceLease.Get() : nullptr) :
		(InStore.GetDirectorySnapshot() ? InStore.GetDirectorySnapshot()->ResourceLease.Get() : nullptr));
	const auto& FileGeneration = bCaptureRootsFrozen ? Capture.SourceLease : InStore.GetSourceLease();
	const FVoxelGenerationRequiredSourceScope GenerationSource(Resources.Get(),
		FileGeneration ? FileGeneration->GetResourceLease().Get() : nullptr);
	const auto Grant = [&](const uint64 Bytes, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		FVoxelGenerationRequiredCapacity Required;
		FString Error;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, Lease.Get(), Required, Error))
			return EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease,
			0, Required.Bytes[0], Required.LeaseCount);
	};
	if (!RetireTaskLease)
	{
		const auto Admission = Grant(VoxelBoundedAllocation::TaskBytes<FSaveCaptureReclaimer>(), RetireTaskLease);
		if (Admission != EVoxelTerrainResourceAdmission::Granted) return Rejected(Admission, TEXT("退休任务未授予"));
	}
	const FVoxelGenerationRequiredSourceScope AcquiredTaskSource(Resources.Get(), RetireTaskLease.Get());
	if (!bCaptureRootsFrozen)
	{
		Capture.Manifest = InManifest;
		Capture.Registry = InRegistry;
		Capture.Journal = InRuntime.CaptureJournal();
		Capture.SourceLease = InStore.GetSourceLease();
		Capture.SourceDirectoryRoot = InStore.GetDirectorySnapshot();
		bCaptureRootsFrozen = true;
	}
	const FString& Source = Capture.SourceLease ? Capture.SourceLease->Directory() : InStore.GetSourceDirectory();
	if (!Source.IsEmpty() && (!Capture.SourceLease || !Capture.SourceLease->Matches(Source)))
	{
		OutError = TEXT("Voxel save capture requires its source generation lease");
		return false;
	}
	const uint64 MetadataBytes = VoxelBoundedAllocation::SharedBytes<FString>() +
		VoxelBoundedAllocation::SharedBytes<FVoxelWorldSaveCompletion>() +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<TCHAR, FDefaultAllocator>(Source.Len() + 1);
	if (!Capture.MetadataLease)
	{
		const auto Admission = Grant(MetadataBytes, Capture.MetadataLease);
		if (Admission != EVoxelTerrainResourceAdmission::Granted) return Rejected(Admission, TEXT("元数据未授予"));
	}
	const FVoxelGenerationRequiredSourceScope AcquiredMetadataSource(Resources.Get(), Capture.MetadataLease.Get());
	EVoxelTerrainResourceAdmission NotificationAdmission = EVoxelTerrainResourceAdmission::None;
	if (!Capture.Journal.PrepareSaveNotifications(Capture.TransactionId, &NotificationAdmission))
		return Rejected(NotificationAdmission, TEXT("落盘通知游标未授予"));
	Capture.SourceDirectory = MakeShared<const FString, ESPMode::ThreadSafe>(Source);
	Capture.Completion = MakeShared<FVoxelWorldSaveCompletion, ESPMode::ThreadSafe>();
	Capture.Completion->TransactionId = Capture.TransactionId;
	Capture.Completion->WorldEpoch = Capture.WorldEpoch;
	check(!Capture.MetadataLease || VoxelBoundedAllocation::SharedBytes<FString>() +
		VoxelBoundedAllocation::SharedBytes<FVoxelWorldSaveCompletion>() + Capture.SourceDirectory->GetAllocatedSize() <= MetadataBytes);
	Active = MoveTemp(Capture);
	Pending.Reset();
	bCaptureRootsFrozen = false;
	bCaptureCapacityBlocked = false;
	OutError.Reset();
	return true;
}

const FVoxelWorldSaveCapture* FVoxelWorldSaveAdapter::GetCapture() const
{
	return Active.IsSet() ? &Active.GetValue() : nullptr;
}

bool FVoxelWorldSaveAdapter::AttachSceneCapture(TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> InCapture)
{
	check(IsInGameThread());
	if (!Active.IsSet() || !InCapture || InCapture->TransactionId != Active->TransactionId ||
		InCapture->WorldEpoch != Active->WorldEpoch || InCapture->Completion != Active->Completion) return false;
	Active->SceneCapture = MoveTemp(InCapture);
	return true;
}

bool FVoxelWorldSaveAdapter::IsBusy() const
{
	return Active.IsSet() || Pending.IsSet();
}

bool FVoxelWorldSaveAdapter::CanCapture() const
{
	check(IsInGameThread());
	return !Active.IsSet() && RetireCaptureTask.IsCompleted();
}

bool FVoxelWorldSaveAdapter::WriteCapture(const FVoxelWorldSaveCapture& InCapture,
	const FString& InTemporaryGenerationDirectory, FString& OutError)
{
	if (!InCapture.Registry || !InCapture.TransactionId.IsValid() || !InCapture.SourceDirectory ||
		!InCapture.Completion || InCapture.Completion->TransactionId != InCapture.TransactionId ||
		InCapture.Completion->WorldEpoch != InCapture.WorldEpoch ||
		InCapture.WorldEpoch != InCapture.Journal.WorldEpoch || InTemporaryGenerationDirectory.IsEmpty())
	{
		OutError = TEXT("Invalid voxel save capture");
		return false;
	}
	InCapture.Completion->Directory.Reset();
	const FString& Source = *InCapture.SourceDirectory;
	if (!Source.IsEmpty() && (!InCapture.SourceLease || !InCapture.SourceLease->Matches(Source)))
	{
		OutError = TEXT("Voxel save worker lost its source generation lease");
		return false;
	}
	if (!Source.IsEmpty() && FPaths::IsSamePath(Source, InTemporaryGenerationDirectory))
	{
		OutError = TEXT("Voxel保存目标不能覆盖捕获的不可变文件代");
		return false;
	}
	TOptional<FIntVector> LastRegion;
	const bool bWritten = InCapture.Journal.VisitDirty([&](const FVoxelOverlaySnapshot& InJournal)
	{
		const FIntVector Region(VoxelGeneration::FloorDivide(InJournal.Section.X, 8),
			VoxelGeneration::FloorDivide(InJournal.Section.Y, 8), VoxelGeneration::FloorDivide(InJournal.Section.Z, 8));
		if (LastRegion.IsSet() && LastRegion.GetValue() == Region) return true;
		LastRegion = Region;
		return FVoxelRegionStore::WritePendingRegion(Region, Source, InTemporaryGenerationDirectory,
			InCapture.SourceDirectoryRoot, InCapture.Journal.Resources,
			FVoxelGenerationConsumerKey::Section(InCapture.WorldEpoch, InCapture.TransactionId, 1, Region, 2),
			[&](const FIntVector& InSection) { return InCapture.Journal.FindDirty(InSection).IsValid(); },
			[&](const FIntVector& InSection, FVoxelBinaryBuffer& OutBuffer, FString& InOutError)
			{
				const auto Journal = InCapture.Journal.FindDirty(InSection);
				if (!Journal)
				{
					InOutError = TEXT("Voxel保存记录不属于固定切点");
					return false;
				}
				const FVoxelDeltaEncodeView Persistent{Journal->Section, Journal->Revision, Journal->Blocks, Journal->Entities};
				// 空覆盖仍编码其内容版本，防止重载、复制或恢复时复活旧编辑。
				return FVoxelDeltaCodec::EncodeGranted(InCapture.Manifest, *InCapture.Registry, Persistent,
					InCapture.Journal.Resources,
					FVoxelGenerationConsumerKey::Section(InCapture.WorldEpoch, InCapture.TransactionId, 1, InSection, 2),
					OutBuffer, InOutError);
			}, OutError);
	});
	if (!bWritten) return false;
	return FVoxelRegionStore::BuildDirectory(InTemporaryGenerationDirectory, InCapture.Journal.Resources,
		FVoxelGenerationConsumerKey::Section(InCapture.WorldEpoch, InCapture.TransactionId, 1, FIntVector::ZeroValue, 3),
		InCapture.Completion->Directory, OutError);
}

bool FVoxelWorldSaveAdapter::Complete(FVoxelWorldRuntime& InRuntime, FVoxelRegionStore& InStore,
	const bool bInSuccess, const FString& InCommittedDirectory,
	TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> InRetiredSceneAttempt,
	TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> InRetiredSceneReceipt)
{
	check(IsInGameThread());
	if (!Active.IsSet())
	{
		if (Pending.IsSet())
		{
			if (bCaptureRootsFrozen)
			{
				check(RetireTaskLease);
				RetireCaptureTask = UE::Tasks::Launch(UE_SOURCE_LOCATION,
					FSaveCaptureReclaimer{MoveTemp(RetireTaskLease), MoveTemp(Pending.GetValue())});
			}
			Pending.Reset();
		}
		bCaptureCapacityBlocked = false;
		bCaptureRootsFrozen = false;
		return false;
	}
	bool bApplied = false;
	if (bInSuccess && Active->WorldEpoch == InRuntime.Epoch() && InRuntime.CanCompleteJournalSave(Active->Journal))
	{
		FString Error;
		if (Active->Completion && Active->Completion->TargetDirectory && !Active->Completion->TargetReadLease)
			Error = TEXT("Voxel原生保存提交缺少原Worker准备的目标文件代租约");
		if (Active->Completion && Active->Completion->TransactionId == Active->TransactionId &&
			(!Active->Completion->TargetDirectory || Active->Completion->TargetReadLease) &&
			InStore.AdvanceSourcePrepared(InCommittedDirectory, Active->Completion->Directory, Error, Active->Completion->TargetReadLease))
		{
			bApplied = InRuntime.CompleteJournalSave(Active->Journal);
			check(bApplied);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Voxel保存已完成，但GT目录回执无法应用，日志保持未落盘: %s"), *Error);
		}
	}
	Active->RetiredSceneAttempt = MoveTemp(InRetiredSceneAttempt);
	Active->RetiredSceneReceipt = MoveTemp(InRetiredSceneReceipt);
	RetireCaptureTask = UE::Tasks::Launch(UE_SOURCE_LOCATION,
		FSaveCaptureReclaimer{MoveTemp(RetireTaskLease), MoveTemp(Active.GetValue())});
	Active.Reset();
	return bApplied;
}
