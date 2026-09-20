#include "Voxel/Streaming/VoxelResidencyManager.h"

#include "EngineGlobals.h"

FVoxelResidencyManager::FVoxelResidencyManager(
	FVoxelWorldRuntime& InRuntime,
	FVoxelTaskScheduler& InScheduler,
	TFunction<void(const FIntVector&)> InBeforeEvict)
	: Runtime(InRuntime)
	, Scheduler(InScheduler)
	, BeforeEvict(MoveTemp(InBeforeEvict))
{
}

void FVoxelResidencyManager::Tick(
	const TMap<FIntVector, FVoxelExactDemand>& InDemand,
	const uint64 InInterestRevision,
	const double InNow)
{
	(void)InNow;
	if (CurrentInterestRevision != InInterestRevision)
	{
		Demanded.Reset();
		for (const TPair<FIntVector, FVoxelExactDemand>& Pair : InDemand)
		{
			Demanded.Add(Pair.Key);
		}
		CurrentInterestRevision = InInterestRevision;
	}

	const uint64 Frame = GFrameCounter;
	if (Frame < LastEvictionCheckFrame ||
		Frame - LastEvictionCheckFrame < EvictionCheckIntervalFrames)
	{
		return;
	}
	LastEvictionCheckFrame = Frame;
	const TArray<FIntVector> Resident = Runtime.ResidentSections();
	for (const FIntVector& Key : Resident)
	{
		const FVoxelSection* Section = Runtime.FindSection(Key);
		if (!Section || !CanEvict(Key, *Section, Frame))
		{
			continue;
		}
		Scheduler.CancelSection(Key);
		if (BeforeEvict)
		{
			BeforeEvict(Key);
		}
		Runtime.RemoveSection(Key);
	}
}

void FVoxelResidencyManager::SetEvictGraceFrames(const uint64 InFrames)
{
	EvictGraceFrames = InFrames;
}

bool FVoxelResidencyManager::CanEvict(
	const FIntVector& InKey,
	const FVoxelSection& InSection,
	const uint64 InFrame) const
{
	if (Demanded.Contains(InKey) || InSection.PinCount.Load() > 0)
	{
		return false;
	}
	if (Scheduler.HasSectionTask(InKey))
	{
		return false;
	}
	if (Runtime.IsServer() && Runtime.GetChangeIndex().IsModified(InKey))
	{
		return false;
	}
	return InFrame >= InSection.LastWantedFrame &&
		InFrame - InSection.LastWantedFrame >= EvictGraceFrames;
}
