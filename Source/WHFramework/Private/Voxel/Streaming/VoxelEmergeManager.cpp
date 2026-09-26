#include "Voxel/Streaming/VoxelEmergeManager.h"

#include "EngineGlobals.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelRegionStore.h"

FVoxelEmergeManager::FVoxelEmergeManager(
	FVoxelWorldRuntime& InRuntime,
	FVoxelTaskScheduler& InScheduler,
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	const FVoxelRegionStore& InRegionStore,
	const FVoxelWorldManifest& InManifest,
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry)
	: Runtime(InRuntime)
	, Scheduler(InScheduler)
	, Generator(InGenerator)
	, RegionStore(InRegionStore)
	, Manifest(InManifest)
	, Registry(InRegistry)
{
}

void FVoxelEmergeManager::Tick(
	const FVoxelInterestSet& InInterest,
	const uint64 InInterestRevision,
	const double InNow,
	const double InDataAdmissionLimit,
	const int32 InMaxBuildsPerFrame,
	const double InAdmissionMilliseconds)
{
	(void)InNow;

	if (CurrentInterestRevision != InInterestRevision)
	{
		RebuildDemand(
			InInterest,
			InInterestRevision);
	}

	if (OrderedKeys.IsEmpty())
	{
		return;
	}

	const int32 BuildLimit = FMath::Clamp(InMaxBuildsPerFrame, 1, 1024);
	const int32 MaximumAttempts = FMath::Min(OrderedKeys.Num(), FMath::Max(64, BuildLimit * 8));
	const double Deadline = FPlatformTime::Seconds() + FMath::Max(0.1, InAdmissionMilliseconds) / 1000.0;
	int32 Submitted = 0;

	for (int32 Attempt = 0; Attempt < MaximumAttempts && Submitted < BuildLimit &&
		FPlatformTime::Seconds() < Deadline; ++Attempt)
	{
		if (NextAdmissionIndex >= OrderedKeys.Num())
		{
			NextAdmissionIndex = 0;
		}

		const FIntVector Key =
			OrderedKeys[NextAdmissionIndex++];

		const FVoxelExactDemand* Demand =
			CurrentDemand.Find(Key);

		if (Demand)
		{
			const bool bGameplayData = Demand->bExact || Demand->bCollision || Demand->bSimulation || Demand->bWarmupData;
			if (!bGameplayData && Demand->DistanceCells > InDataAdmissionLimit)
			{
				NextAdmissionIndex = 0;
				break;
			}
			Submitted += RequestSection(Key, *Demand) ? 1 : 0;
		}
	}
}

bool FVoxelEmergeManager::OnTask(
	FVoxelTaskResult&& InResult)
{
	if (InResult.Kind != EVoxelTaskKind::GenerateExactBase &&
		InResult.Kind != EVoxelTaskKind::DecodeOverlay)
	{
		return false;
	}

	FVoxelSection* Section =
		Runtime.FindSection(InResult.Stamp.Section);

	if (!Section)
	{
		return true;
	}

	const FVoxelSectionStamp ExpectedStamp {
		InResult.Stamp.WorldEpoch,
		InResult.Stamp.Token,
		Section->Stamp.RecipeHash
	};

	if (!Runtime.IsCurrentStamp(
		InResult.Stamp.Section,
		ExpectedStamp))
	{
		return true;
	}

	if (InResult.bCanceled)
	{
		return true;
	}

	if (!InResult.bSuccess)
	{
		Section->Status =
			EVoxelSectionStatus::Failed;

		return true;
	}

	FString Error;

	if (InResult.Kind == EVoxelTaskKind::DecodeOverlay)
	{
		if (!InResult.PersistentSection ||
			!Runtime.PublishFinal(
				InResult.Stamp.Section,
				InResult.PersistentSection->Revision,
				InResult.PersistentSection->Blocks,
				InResult.PersistentSection->Entities,
				Error))
		{
			Section->Status =
				EVoxelSectionStatus::Failed;
		}

		return true;
	}

	if (!Runtime.PublishBase(
		InResult.Stamp.Section,
		ExpectedStamp,
		MoveTemp(InResult.BaseBlocks),
		Error))
	{
		Section->Status =
			EVoxelSectionStatus::Failed;

		return true;
	}

	if (const FVoxelExactDemand* Demand =
		CurrentDemand.Find(InResult.Stamp.Section))
	{
		ResolveOverlay(
			*Section,
			InResult.Stamp.Section,
			*Demand);
	}

	return true;
}

void FVoxelEmergeManager::SetRemoteChangeState(
	const FIntVector& InSection,
	const EVoxelSectionChangeState InState)
{
	if (InState == EVoxelSectionChangeState::Unknown)
	{
		RemoteChangeStates.Remove(InSection);
	}
	else
	{
		RemoteChangeStates.Add(
			InSection,
			InState);
	}

	FVoxelSection* Section =
		Runtime.FindSection(InSection);

	const FVoxelExactDemand* Demand =
		CurrentDemand.Find(InSection);

	if (Section &&
		Demand &&
		Section->Status == EVoxelSectionStatus::BaseReady)
	{
		ResolveOverlay(
			*Section,
			InSection,
			*Demand);
	}
}

void FVoxelEmergeManager::Reset()
{
	CurrentDemand.Reset();
	RemoteChangeStates.Reset();
	OrderedKeys.Reset();

	CurrentInterestRevision = 0;
	NextAdmissionIndex = 0;
}

EVoxelWorkClass FVoxelEmergeManager::ResolveWorkClass(const FVoxelExactDemand& InDemand)
{
	if (InDemand.bMovementCriticalCollision || InDemand.bWarmupCollision) return EVoxelWorkClass::Critical;
	if (InDemand.bWarmupData) return EVoxelWorkClass::Warmup;
	return InDemand.bExact || InDemand.bCollision || InDemand.bSimulation
		? EVoxelWorkClass::ExactData : EVoxelWorkClass::Visible;
}

void FVoxelEmergeManager::RebuildDemand(
	const FVoxelInterestSet& InInterest,
	const uint64 InInterestRevision)
{
	CurrentDemand = InInterest.Exact;
	OrderedKeys = InInterest.ExactOrder;
	Scheduler.UpdatePriorities([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, double& Distance, double& Forward)
	{
		if (Kind != EVoxelTaskKind::GenerateExactBase && Kind != EVoxelTaskKind::DecodeOverlay) return;
		if (const FVoxelExactDemand* Demand = CurrentDemand.Find(Stamp.Section))
		{
			WorkClass = ResolveWorkClass(*Demand);
			Distance = Demand->DistanceCells;
			Forward = Demand->ForwardScore;
		}
	});

	CurrentInterestRevision =
		InInterestRevision;

	NextAdmissionIndex = 0;
}

bool FVoxelEmergeManager::RequestSection(
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	FVoxelSection* Section =
		Runtime.FindOrAllocate(
			InKey,
			GFrameCounter);

	if (!Section)
	{
		return false;
	}

	switch (Section->Status)
	{
	case EVoxelSectionStatus::Allocated:
		return RequestBase(
			*Section,
			InKey,
			InDemand);

	case EVoxelSectionStatus::BaseReady:
		return ResolveOverlay(
			*Section,
			InKey,
			InDemand);

	case EVoxelSectionStatus::DataReady:
		Section->bSimulationWanted =
			InDemand.bSimulation;
		break;

	case EVoxelSectionStatus::None:
	case EVoxelSectionStatus::Failed:
	case EVoxelSectionStatus::Unloading:
	default:
		break;
	}
	return false;
}

bool FVoxelEmergeManager::RequestBase(
	FVoxelSection& InSection,
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	FVoxelTaskStamp TaskStamp;
	TaskStamp.WorldEpoch =
		InSection.Stamp.Epoch;
	TaskStamp.Token =
		InSection.Stamp.Token;
	TaskStamp.Section =
		InKey;

	if (Scheduler.Has(
		TaskStamp,
		EVoxelTaskKind::GenerateExactBase))
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.Kind =
		EVoxelTaskKind::GenerateExactBase;

	Request.WorkClass = ResolveWorkClass(InDemand);

	Request.Stamp =
		TaskStamp;

	Request.DistanceScore = InDemand.DistanceCells;
	Request.ForwardScore = InDemand.ForwardScore;

	Request.ReservedBytes =
		2ull * 1024ull * 1024ull;

	const TSharedRef<
		const FVoxelGenerationPipeline,
		ESPMode::ThreadSafe> CapturedGenerator =
			Generator;

	Request.Execute =
		[CapturedGenerator, InKey](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;

			Result.bSuccess =
				CapturedGenerator->GenerateSection(
					InKey,
					Result.BaseBlocks,
					Result.Error,
					&InCancel);

			Result.bCanceled =
				InCancel.Load();

			return Result;
		};

	return Scheduler.Enqueue(MoveTemp(Request));
}

bool FVoxelEmergeManager::ResolveOverlay(
	FVoxelSection& InSection,
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	if (InSection.Status !=
		EVoxelSectionStatus::BaseReady)
	{
		return false;
	}

	bool bKnownNatural = false;

	if (Runtime.IsServer())
	{
		bKnownNatural =
			!Runtime.GetChangeIndex().
				IsModified(InKey);
	}
	else if (
		const EVoxelSectionChangeState* State =
			RemoteChangeStates.Find(InKey))
	{
		bKnownNatural =
			*State ==
			EVoxelSectionChangeState::Natural;
	}

	if (bKnownNatural)
	{
		FString Error;

		return Runtime.PublishFinal(
			InKey,
			0,
			{},
			{},
			Error);
	}

	if (!Runtime.IsServer())
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.Kind =
		EVoxelTaskKind::DecodeOverlay;

	Request.WorkClass = ResolveWorkClass(InDemand);

	Request.DistanceScore = InDemand.DistanceCells;
	Request.ForwardScore = InDemand.ForwardScore;

	Request.Stamp.WorldEpoch =
		InSection.Stamp.Epoch;

	Request.Stamp.Token =
		InSection.Stamp.Token;

	Request.Stamp.Section =
		InKey;

	Request.ReservedBytes =
		4ull * 1024ull * 1024ull;

	if (Scheduler.Has(
		Request.Stamp,
		Request.Kind))
	{
		return false;
	}

	const FVoxelRegionReadView Read =
		RegionStore.CaptureRead(InKey);

	const FVoxelWorldManifest CapturedManifest =
		Manifest;

	const TSharedRef<
		const FVoxelRegistrySnapshot,
		ESPMode::ThreadSafe> CapturedRegistry =
			Registry;

	Request.Execute =
		[
			Read,
			CapturedManifest,
			CapturedRegistry
		](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;

			if (InCancel.Load())
			{
				return Result;
			}

			TArray<uint8> Bytes;

			const EVoxelRegionRead Status =
				FVoxelRegionStore::Read(
					Read,
					Bytes,
					Result.Error);

			if (Status !=
				EVoxelRegionRead::Loaded)
			{
				Result.Error =
					Status ==
					EVoxelRegionRead::Missing
						? TEXT("Modified voxel section is missing from its region file")
						: Result.Error;

				return Result;
			}

			Result.PersistentSection =
				MakeShared<FVoxelPersistentSection>();

			Result.bSuccess =
				FVoxelDeltaCodec::Decode(
					Bytes,
					CapturedManifest,
					*CapturedRegistry,
					*Result.PersistentSection);

			if (Result.bSuccess &&
				Result.PersistentSection->Section !=
					Read.Section)
			{
				Result.bSuccess = false;
				Result.Error =
					TEXT("Voxel region section identity mismatch");
			}

			return Result;
		};

	return Scheduler.Enqueue(MoveTemp(Request));
}
