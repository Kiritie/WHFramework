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
	const TMap<FIntVector, FVoxelExactDemand>& InDemand,
	const uint64 InInterestRevision,
	const double InNow)
{
	(void)InNow;

	if (CurrentInterestRevision != InInterestRevision)
	{
		RebuildDemand(
			InDemand,
			InInterestRevision);
	}

	if (OrderedKeys.IsEmpty())
	{
		return;
	}

	const int32 AdmissionCount =
		FMath::Min(
			MaxSectionAdmissionsPerTick,
			OrderedKeys.Num());

	for (int32 Admission = 0;
		Admission < AdmissionCount;
		++Admission)
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
			RequestSection(Key, *Demand);
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

void FVoxelEmergeManager::RebuildDemand(
	const TMap<FIntVector, FVoxelExactDemand>& InDemand,
	const uint64 InInterestRevision)
{
	CurrentDemand = InDemand;

	CurrentDemand.GetKeys(OrderedKeys);

	OrderedKeys.Sort(
		[this](
			const FIntVector& InA,
			const FIntVector& InB)
		{
			const double PriorityA =
				CurrentDemand.FindChecked(InA).Priority;

			const double PriorityB =
				CurrentDemand.FindChecked(InB).Priority;

			if (PriorityA != PriorityB)
			{
				return PriorityA > PriorityB;
			}

			if (InA.X != InB.X)
			{
				return InA.X < InB.X;
			}

			if (InA.Y != InB.Y)
			{
				return InA.Y < InB.Y;
			}

			return InA.Z < InB.Z;
		});

	CurrentInterestRevision =
		InInterestRevision;

	NextAdmissionIndex = 0;
}

void FVoxelEmergeManager::RequestSection(
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	FVoxelSection* Section =
		Runtime.FindOrAllocate(
			InKey,
			GFrameCounter);

	if (!Section)
	{
		return;
	}

	switch (Section->Status)
	{
	case EVoxelSectionStatus::Allocated:
		RequestBase(
			*Section,
			InKey,
			InDemand);
		break;

	case EVoxelSectionStatus::BaseReady:
		ResolveOverlay(
			*Section,
			InKey,
			InDemand);
		break;

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
}

void FVoxelEmergeManager::RequestBase(
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
		return;
	}

	FVoxelTaskRequest Request;
	Request.Kind =
		EVoxelTaskKind::GenerateExactBase;

	Request.WorkClass =
		InDemand.bCollision
			? EVoxelWorkClass::Critical
			: InDemand.bFineRender
				? EVoxelWorkClass::Visible
				: EVoxelWorkClass::Interactive;

	Request.Stamp =
		TaskStamp;

	Request.DistanceScore =
		InDemand.Priority > 0.0
			? 1.0 / InDemand.Priority
			: MAX_dbl;

	Request.ForwardScore =
		InDemand.Priority;

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

	Scheduler.Enqueue(MoveTemp(Request));
}

void FVoxelEmergeManager::ResolveOverlay(
	FVoxelSection& InSection,
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	if (InSection.Status !=
		EVoxelSectionStatus::BaseReady)
	{
		return;
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

		Runtime.PublishFinal(
			InKey,
			0,
			{},
			{},
			Error);

		return;
	}

	if (!Runtime.IsServer())
	{
		return;
	}

	FVoxelTaskRequest Request;
	Request.Kind =
		EVoxelTaskKind::DecodeOverlay;

	Request.WorkClass =
		InDemand.bCollision
			? EVoxelWorkClass::Critical
			: EVoxelWorkClass::Interactive;

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
		return;
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

	Scheduler.Enqueue(MoveTemp(Request));
}
