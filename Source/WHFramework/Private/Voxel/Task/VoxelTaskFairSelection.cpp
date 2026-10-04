#include "Voxel/Task/VoxelTaskFairSelection.h"

void FVoxelTaskFairSelection::SetFairness(const int32 InCriticalBurst, const double InMaximumBypassSeconds)
{
	check(InCriticalBurst >= 1 && FMath::IsFinite(InMaximumBypassSeconds) && InMaximumBypassSeconds >= 0.0);
	MaximumCriticalBurst = InCriticalBurst;
	MaximumBypassSeconds = InMaximumBypassSeconds;
}

bool FVoxelTaskFairSelection::Better(const FVoxelTaskServiceCandidate& InA,
	const FVoxelTaskServiceCandidate& InB, const double InNow, const double InAgeLimit)
{
	if ((InA.Lane == EVoxelTaskServiceLane::Critical) == (InB.Lane == EVoxelTaskServiceLane::Critical) &&
		InA.SourcePriority == InB.SourcePriority && InA.bPublicationContinuation != InB.bPublicationContinuation)
	{
		return InA.bPublicationContinuation;
	}
	const bool bAOverdue = InNow - InA.EnqueuedAt >= InAgeLimit;
	const bool bBOverdue = InNow - InB.EnqueuedAt >= InAgeLimit;
	if (bAOverdue != bBOverdue)
	{
		return bAOverdue;
	}
	if (bAOverdue && InA.EnqueuedAt != InB.EnqueuedAt)
	{
		return InA.EnqueuedAt < InB.EnqueuedAt;
	}
	if (InA.SourcePriority != InB.SourcePriority)
	{
		return InA.SourcePriority < InB.SourcePriority;
	}
	if (InA.Distance != InB.Distance)
	{
		return InA.Distance < InB.Distance;
	}
	return InA.Sequence < InB.Sequence;
}

int32 FVoxelTaskFairSelection::Select(TConstArrayView<FVoxelTaskServiceCandidate> InCandidates, const double InNow)
{
	check(FMath::IsFinite(InNow));
	constexpr uint8 LaneCount = static_cast<uint8>(EVoxelTaskServiceLane::Count);
	bool bOrdinaryCriticalWaiting = false;
	for (const auto& Candidate : InCandidates)
		bOrdinaryCriticalWaiting |= Candidate.Lane == EVoxelTaskServiceLane::Critical &&
			Candidate.Blocked == EVoxelTaskBlockReason::None && !Candidate.bPublicationContinuation;
	int32 Best[LaneCount];
	for (int32& Entry : Best)
	{
		Entry = INDEX_NONE;
	}
	for (int32 Index = 0; Index < InCandidates.Num(); ++Index)
	{
		const FVoxelTaskServiceCandidate& Candidate = InCandidates[Index];
		const uint8 Lane = static_cast<uint8>(Candidate.Lane);
		if (Lane == 0 || Lane >= LaneCount || Candidate.Blocked != EVoxelTaskBlockReason::None)
		{
			continue;
		}
		if (Candidate.Lane == EVoxelTaskServiceLane::Critical && Candidate.bPublicationContinuation &&
			bOrdinaryCriticalWaiting && CriticalDependencyBurst >= MaximumCriticalBurst) continue;
		if (Best[Lane] == INDEX_NONE || Better(Candidate, InCandidates[Best[Lane]], InNow, MaximumBypassSeconds))
		{
			Best[Lane] = Index;
		}
	}
	const uint8 CriticalLane = static_cast<uint8>(EVoxelTaskServiceLane::Critical);
	uint8 NonCriticalLane = 0;
	for (uint8 Lane = CriticalLane + 1; Lane < LaneCount; ++Lane)
	{
		if (Best[Lane] == INDEX_NONE)
		{
			continue;
		}
		if (NonCriticalLane == 0 || LastServed[Lane] < LastServed[NonCriticalLane] ||
			(LastServed[Lane] == LastServed[NonCriticalLane] &&
				Better(InCandidates[Best[Lane]], InCandidates[Best[NonCriticalLane]], InNow, MaximumBypassSeconds)))
		{
			NonCriticalLane = Lane;
		}
	}
	const bool bHasCritical = Best[CriticalLane] != INDEX_NONE;
	uint8 ChosenLane = 0;
	if (bHasCritical && (CriticalBurst < MaximumCriticalBurst || NonCriticalLane == 0))
	{
		ChosenLane = CriticalLane;
		CriticalBurst = FMath::Min(MaximumCriticalBurst, CriticalBurst + 1);
	}
	else if (NonCriticalLane != 0)
	{
		ChosenLane = NonCriticalLane;
		CriticalBurst = 0;
	}
	if (ChosenLane == 0)
	{
		return INDEX_NONE;
	}
	check(DispatchSerial != MAX_uint64);
	if (ChosenLane == CriticalLane)
		CriticalDependencyBurst = InCandidates[Best[ChosenLane]].bPublicationContinuation
			? FMath::Min(MaximumCriticalBurst, CriticalDependencyBurst + 1) : 0;
	LastServed[ChosenLane] = ++DispatchSerial;
	return InCandidates[Best[ChosenLane]].PendingIndex;
}

void FVoxelTaskFairSelection::Reset()
{
	for (uint64& Value : LastServed)
	{
		Value = 0;
	}
	DispatchSerial = 0;
	CriticalBurst = 0;
	CriticalDependencyBurst = 0;
}
