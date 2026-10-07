#include "Voxel/Save/VoxelWorldSaveAdapter.h"

#include "Misc/Paths.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelSceneSaveCapture.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	struct FSaveCaptureReclaimer
	{
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
	RetireTaskLease.Reset();
	FVoxelWorldSaveCapture Capture;
	Capture.TransactionId = FGuid::NewGuid();
	Capture.WorldEpoch = InRuntime.Epoch();
	Capture.Manifest = InManifest;
	Capture.Registry = InRegistry;
	Capture.Journal = InRuntime.CaptureJournal();
	Capture.SourceLease = InStore.GetSourceLease();
	Capture.SourceDirectoryRoot = InStore.GetDirectorySnapshot();
	const FString& Source = InStore.GetSourceDirectory();
	if (!Source.IsEmpty() && (!Capture.SourceLease || !Capture.SourceLease->Matches(Source)))
	{
		OutError = TEXT("Voxel save capture requires its source generation lease");
		return false;
	}
	const uint64 MetadataBytes = VoxelBoundedAllocation::SharedBytes<FString>() +
		VoxelBoundedAllocation::SharedBytes<FVoxelWorldSaveCompletion>() +
		VoxelBoundedAllocation::ArrayBytes<TCHAR>(Source.Len() + 1);
	if (Capture.Journal.Resources && !Capture.Journal.Resources->Acquire(
		FVoxelGenerationConsumerKey::Section(Capture.WorldEpoch, Capture.TransactionId, 1, FIntVector::ZeroValue, 1),
		EVoxelTerrainResourcePool::Input, MetadataBytes, Capture.MetadataLease))
	{
		OutError = TEXT("CapacityBlocked: 保存固定切点元数据未授予");
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> TaskLease;
	if (Capture.Journal.Resources && !Capture.Journal.Resources->Acquire(
		FVoxelGenerationConsumerKey::Section(Capture.WorldEpoch, Capture.TransactionId, 1, FIntVector::ZeroValue, 4),
		EVoxelTerrainResourcePool::Input, VoxelBoundedAllocation::TaskBytes<FSaveCaptureReclaimer>(), TaskLease))
	{
		OutError = TEXT("CapacityBlocked: 保存捕获的异步释放任务未授予");
		return false;
	}
	if (!Capture.Journal.PrepareSaveNotifications(Capture.TransactionId))
	{
		OutError = TEXT("CapacityBlocked: 保存落盘通知游标及异步释放未授予");
		return false;
	}
	Capture.SourceDirectory = MakeShared<const FString, ESPMode::ThreadSafe>(Source);
	Capture.Completion = MakeShared<FVoxelWorldSaveCompletion, ESPMode::ThreadSafe>();
	Capture.Completion->TransactionId = Capture.TransactionId;
	Capture.Completion->WorldEpoch = Capture.WorldEpoch;
	check(!Capture.MetadataLease || VoxelBoundedAllocation::SharedBytes<FString>() +
		VoxelBoundedAllocation::SharedBytes<FVoxelWorldSaveCompletion>() + Capture.SourceDirectory->GetAllocatedSize() <= MetadataBytes);
	Active = MoveTemp(Capture);
	RetireTaskLease = MoveTemp(TaskLease);
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
	return Active.IsSet();
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
	if (!Active.IsSet()) return false;
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
	RetireCaptureTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, FSaveCaptureReclaimer{MoveTemp(Active.GetValue())});
	Active.Reset();
	return bApplied;
}
