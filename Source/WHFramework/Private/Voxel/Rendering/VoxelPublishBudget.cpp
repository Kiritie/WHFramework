#include "Voxel/Rendering/VoxelPublishBudget.h"

void FVoxelPublishBudget::Begin(const double InNowSeconds, const double InTotalMilliseconds,
	const int32 InMaximumOperations)
{
	check(FMath::IsFinite(InNowSeconds) && FMath::IsFinite(InTotalMilliseconds) && InTotalMilliseconds > 0.0);
	check(InMaximumOperations > 0);
	Started = InNowSeconds;
	Deadline = Started + InTotalMilliseconds * 0.001;
	RemainingOperations = InMaximumOperations;
	for (FVoxelPublishPhaseTiming& Timing : Timings)
	{
		Timing = {};
	}
}

bool FVoxelPublishBudget::TryStart(const EVoxelPublishPhase InPhase, const double InNowSeconds,
	const double InEstimatedMilliseconds)
{
	const uint8 Index = static_cast<uint8>(InPhase);
	check(Index > 0 && Index < static_cast<uint8>(EVoxelPublishPhase::Count));
	check(FMath::IsFinite(InNowSeconds) && FMath::IsFinite(InEstimatedMilliseconds) && InEstimatedMilliseconds >= 0.0);
	if (RemainingOperations <= 0 || InNowSeconds >= Deadline ||
		InEstimatedMilliseconds > RemainingMilliseconds(InNowSeconds))
	{
		++Timings[Index].Deferrals;
		return false;
	}
	--RemainingOperations;
	return true;
}

void FVoxelPublishBudget::Finish(const EVoxelPublishPhase InPhase, const double InStartedSeconds,
	const double InFinishedSeconds)
{
	const uint8 Index = static_cast<uint8>(InPhase);
	check(Index > 0 && Index < static_cast<uint8>(EVoxelPublishPhase::Count));
	check(InFinishedSeconds >= InStartedSeconds);
	FVoxelPublishPhaseTiming& Timing = Timings[Index];
	const double Elapsed = (InFinishedSeconds - InStartedSeconds) * 1000.0;
	Timing.Milliseconds += Elapsed;
	Timing.MaximumOperationMilliseconds = FMath::Max(Timing.MaximumOperationMilliseconds, Elapsed);
	++Timing.Operations;
}

double FVoxelPublishBudget::RemainingMilliseconds(const double InNowSeconds) const
{
	if (RemainingOperations <= 0) return 0.0;
	return FMath::Max(0.0, (Deadline - InNowSeconds) * 1000.0);
}

double FVoxelPublishBudget::TotalMilliseconds(const double InNowSeconds) const
{
	return FMath::Max(0.0, (InNowSeconds - Started) * 1000.0);
}

bool FVoxelPublishBudget::HasOverrun(const double InNowSeconds) const
{
	return InNowSeconds > Deadline;
}

const FVoxelPublishPhaseTiming& FVoxelPublishBudget::GetTiming(const EVoxelPublishPhase InPhase) const
{
	const uint8 Index = static_cast<uint8>(InPhase);
	check(Index > 0 && Index < static_cast<uint8>(EVoxelPublishPhase::Count));
	return Timings[Index];
}
