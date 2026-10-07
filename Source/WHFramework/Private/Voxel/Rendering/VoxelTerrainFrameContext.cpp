#include "Voxel/Rendering/VoxelTerrainFrameContext.h"

bool FVoxelTerrainFrameContext::Configure(const FVoxelTerrainFrameBudgetSettings& InSettings, FString& OutError)
{
	check(IsInGameThread());
	return Controller.Configure(InSettings, OutError);
}

bool FVoxelTerrainFrameContext::Begin(const uint64 InWorldEpoch, const uint64 InFrameCounter,
	const double InNowSeconds, const FVoxelTerrainFrameObservation& InObservation)
{
	check(IsInGameThread());
	check(InWorldEpoch != 0 && FMath::IsFinite(InNowSeconds));
	if (WorldEpoch == InWorldEpoch && FrameCounter == InFrameCounter)
	{
		return false;
	}
	check(!bActive);
	if (WorldEpoch != 0 && WorldEpoch != InWorldEpoch)
	{
		Controller.Reset();
	}
	WorldEpoch = InWorldEpoch;
	FrameCounter = InFrameCounter;
	StartedSeconds = InNowSeconds;
	Allocation = Controller.Observe(InObservation);
	DeadlineSeconds = StartedSeconds + Allocation.TotalMilliseconds * 0.001;
	SharedBudget.Begin(StartedSeconds, Allocation.TotalMilliseconds, 8192);
	bActive = true;
	bOverdueOpportunityUsed = false;
	return true;
}

double FVoxelTerrainFrameContext::Deadline(const double InMaximumMilliseconds) const
{
	check(IsInGameThread());
	const double Now = FPlatformTime::Seconds();
	if (!bActive)
	{
		return Now;
	}
	return FMath::Min(DeadlineSeconds, Now + FMath::Max(0.0, InMaximumMilliseconds) * 0.001);
}

double FVoxelTerrainFrameContext::RemainingMilliseconds() const
{
	check(IsInGameThread());
	return bActive ? SharedBudget.RemainingMilliseconds(FPlatformTime::Seconds()) : 0.0;
}

void FVoxelTerrainFrameContext::End(const double InNowSeconds)
{
	check(IsInGameThread());
	check(bActive && FMath::IsFinite(InNowSeconds) && InNowSeconds >= StartedSeconds);
	LastTerrainMilliseconds = (InNowSeconds - StartedSeconds) * 1000.0;
	bActive = false;
}

void FVoxelTerrainFrameContext::Reset()
{
	check(IsInGameThread());
	check(!bActive);
	Controller.Reset();
	WorldEpoch = FrameCounter = 0;
	StartedSeconds = DeadlineSeconds = LastTerrainMilliseconds = 0.0;
	Allocation = {};
}

FVoxelPublishBudget& FVoxelTerrainFrameContext::GetPublishBudget()
{
	check(IsInGameThread());
	return SharedBudget;
}

const FVoxelTerrainFrameAllocation& FVoxelTerrainFrameContext::GetAllocation() const
{
	return Allocation;
}

double FVoxelTerrainFrameContext::GetLastTerrainMilliseconds() const
{
	return LastTerrainMilliseconds;
}

uint64 FVoxelTerrainFrameContext::GetWorldEpoch() const
{
	return WorldEpoch;
}

uint64 FVoxelTerrainFrameContext::GetFrameCounter() const
{
	return FrameCounter;
}

bool FVoxelTerrainFrameContext::IsOverdueOpportunityAvailable() const
{
	check(IsInGameThread());
	return bActive && !bOverdueOpportunityUsed;
}

bool FVoxelTerrainFrameContext::IsActive() const
{
	check(IsInGameThread());
	return bActive;
}

void FVoxelTerrainFrameContext::ConsumeOverdueOpportunity() const
{
	check(IsInGameThread());
	check(IsOverdueOpportunityAvailable());
	bOverdueOpportunityUsed = true;
}
