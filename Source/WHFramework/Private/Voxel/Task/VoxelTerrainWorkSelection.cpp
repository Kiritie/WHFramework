#include "Voxel/Task/VoxelTerrainWorkSelection.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

FVoxelTerrainWorkSelection::FVoxelTerrainWorkSelection()
{
}

FVoxelTerrainWorkSelection::~FVoxelTerrainWorkSelection()
{
	OutstandingCharges.Empty();
	ResourceLease.Reset();
}

void FVoxelTerrainWorkSelection::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	check(!Resources && InConsumer.IsValid());
	Resources = InResources;
	Consumer = InConsumer;
}

bool FVoxelTerrainWorkSelection::ReserveCharges()
{
	if (OutstandingCharges.Num() < ChargeCapacity)
	{
		Admission = EVoxelTerrainResourceAdmission::Granted;
		return true;
	}
	if (OutstandingCharges.Num() >= MaximumOutstandingCharges)
	{
		Admission = EVoxelTerrainResourceAdmission::CapacityBlocked;
		return false;
	}
	const int32 Capacity = FMath::Min(MaximumOutstandingCharges, FMath::Max(8, ChargeCapacity * 2));
	const uint64 Bytes = VoxelGenerationAllocation::Private::SetReserveBytes<TPair<uint64, FVoxelTerrainServiceCharge>>(OutstandingCharges, Capacity);
	const uint64 Previous = VoxelGenerationAllocation::AllocatedBytes(OutstandingCharges);
	if (Resources)
	{
		FVoxelGenerationRequiredCapacity Required;
		FString Error;
		if (Bytes > MAX_uint64 - Previous ||
			!FVoxelGenerationRequiredSourceScope::Collect(*Resources, ResourceLease.Get(), Required, Error))
		{
			Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
			return false;
		}
		Admission = ResourceLease ? ResourceLease->TryResize(Previous + Bytes, Required.Bytes[0], Required.LeaseCount) :
			Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, Previous + Bytes, ResourceLease,
				0, Required.Bytes[0], Required.LeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted) return false;
	}
	else
	{
#if !WITH_DEV_AUTOMATION_TESTS
		Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
#endif
	}
	OutstandingCharges.Reserve(Capacity);
	check(VoxelGenerationAllocation::AllocatedBytes(OutstandingCharges) <= Bytes);
	if (ResourceLease)
	{
		verify(ResourceLease->TryResize(VoxelGenerationAllocation::AllocatedBytes(OutstandingCharges)) == EVoxelTerrainResourceAdmission::Granted);
	}
	ChargeCapacity = Capacity;
	Admission = EVoxelTerrainResourceAdmission::Granted;
	return true;
}

bool FVoxelTerrainWorkSelection::Prepare(FString& OutError)
{
	if (ReserveCharges()) { OutError.Reset(); return true; }
	OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: terrain selection charge backing") :
		Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: terrain selection charge backing") :
		TEXT("InvalidRequest: terrain selection charge owner");
	return false;
}

bool FVoxelTerrainWorkSelection::IsNear(const EVoxelTerrainServiceBand InBand)
{
	return InBand == EVoxelTerrainServiceBand::P0 || InBand == EVoxelTerrainServiceBand::P1;
}

bool FVoxelTerrainWorkSelection::HigherPriority(const FVoxelTerrainServicePriority& InFirst,
	const FVoxelTerrainServicePriority& InSecond)
{
	if (InFirst.Band != InSecond.Band)
	{
		return uint8(InFirst.Band) < uint8(InSecond.Band);
	}
	if (InFirst.DistanceCells != InSecond.DistanceCells)
	{
		return InFirst.DistanceCells < InSecond.DistanceCells;
	}
	if (InFirst.ForwardScore != InSecond.ForwardScore)
	{
		return InFirst.ForwardScore > InSecond.ForwardScore;
	}
	if (InFirst.SourcePriority != InSecond.SourcePriority)
	{
		return InFirst.SourcePriority < InSecond.SourcePriority;
	}
	if (InFirst.SourceStableKey.A != InSecond.SourceStableKey.A)
	{
		return InFirst.SourceStableKey.A < InSecond.SourceStableKey.A;
	}
	if (InFirst.SourceStableKey.B != InSecond.SourceStableKey.B)
	{
		return InFirst.SourceStableKey.B < InSecond.SourceStableKey.B;
	}
	if (InFirst.SourceStableKey.C != InSecond.SourceStableKey.C)
	{
		return InFirst.SourceStableKey.C < InSecond.SourceStableKey.C;
	}
	return InFirst.SourceStableKey.D < InSecond.SourceStableKey.D;
}

FVoxelTerrainServicePriority FVoxelTerrainWorkSelection::Promote(const FVoxelTerrainServicePriority& InFirst,
	const FVoxelTerrainServicePriority& InSecond)
{
	return HigherPriority(InFirst, InSecond) ? InFirst : InSecond;
}

int32 FVoxelTerrainWorkSelection::Select(TConstArrayView<FVoxelTerrainServiceCandidate> InCandidates,
	const FVoxelTerrainServiceCapacity& InCapacity, const double InNowSeconds,
	FVoxelTerrainServiceCharge& OutCharge)
{
	OutCharge = {};
	if (bAccountingFault || OutstandingCharges.Num() >= MaximumOutstandingCharges ||
		InCapacity.MaximumWorkers < 1 || InCapacity.MaximumWorkers > MaximumOutstandingCharges || InCapacity.RunningWorkers < 0 ||
		InCapacity.RunningWorkers >= InCapacity.MaximumWorkers || InCapacity.RunningNearWorkers < 0 ||
		InCapacity.RunningNearWorkers > InCapacity.RunningWorkers || !FMath::IsFinite(InNowSeconds) ||
		!FMath::IsFinite(InCapacity.MaximumWaitSeconds) || InCapacity.MaximumWaitSeconds <= 0.0)
	{
		return INDEX_NONE;
	}

	int32 Normal[BandCount];
	int32 Oldest[BandCount];
	for (int32 Band = 0; Band < BandCount; ++Band)
	{
		Normal[Band] = INDEX_NONE;
		Oldest[Band] = INDEX_NONE;
	}
	bool bHasNear = false;
	bool bHasNonNear = false;
	const auto IsOlder = [&InCandidates](const int32 FirstIndex, const int32 SecondIndex)
	{
		const FVoxelTerrainServiceCandidate& First = InCandidates[FirstIndex];
		const FVoxelTerrainServiceCandidate& Second = InCandidates[SecondIndex];
		return First.FirstQueuedSeconds != Second.FirstQueuedSeconds ?
			First.FirstQueuedSeconds < Second.FirstQueuedSeconds : First.Sequence < Second.Sequence;
	};
	for (int32 CandidateIndex = 0; CandidateIndex < InCandidates.Num(); ++CandidateIndex)
	{
		const FVoxelTerrainServiceCandidate& Candidate = InCandidates[CandidateIndex];
		const int32 Band = static_cast<int32>(Candidate.Priority.Band);
		if (!Candidate.bRunnable || Candidate.Index == INDEX_NONE || Band <= 0 || Band >= BandCount ||
			!FMath::IsFinite(Candidate.Priority.DistanceCells) || Candidate.Priority.DistanceCells < 0.0 ||
			!FMath::IsFinite(Candidate.Priority.ForwardScore) ||
			!FMath::IsFinite(Candidate.FirstQueuedSeconds) || Candidate.FirstQueuedSeconds > InNowSeconds ||
			!IsValidCost(Candidate.EstimatedMicroseconds))
		{
			continue;
		}
		bHasNear |= IsNear(Candidate.Priority.Band);
		bHasNonNear |= !IsNear(Candidate.Priority.Band);
		const int32 Previous = Normal[Band];
		if (Previous == INDEX_NONE || HigherPriority(Candidate.Priority, InCandidates[Previous].Priority) ||
			(Candidate.Priority == InCandidates[Previous].Priority && IsOlder(CandidateIndex, Previous)))
		{
			Normal[Band] = CandidateIndex;
		}
		if (Oldest[Band] == INDEX_NONE || IsOlder(CandidateIndex, Oldest[Band]))
		{
			Oldest[Band] = CandidateIndex;
		}
	}
	if (!bHasNear && !bHasNonNear)
	{
		return INDEX_NONE;
	}
	if (!ReserveCharges()) return INDEX_NONE;

	EVoxelTerrainServicePool Pool = EVoxelTerrainServicePool::All;
	if (InCapacity.MaximumWorkers > 1)
	{
		const int32 NearFloor = FMath::Clamp(InCapacity.NearReservedWorkers, 1, InCapacity.MaximumWorkers - 1);
		const int32 NonNearFloor = FMath::Clamp(InCapacity.MinimumNonNearWorkers, 1, InCapacity.MaximumWorkers - NearFloor);
		const int32 RunningNonNear = InCapacity.RunningWorkers - InCapacity.RunningNearWorkers;
		if (bHasNear && InCapacity.RunningNearWorkers < NearFloor)
		{
			Pool = EVoxelTerrainServicePool::Near;
		}
		else if (bHasNonNear && RunningNonNear < NonNearFloor)
		{
			Pool = EVoxelTerrainServicePool::NonNear;
		}
	}
	FPoolState& State = Pools[static_cast<int32>(Pool)];
	// 超期只获得一次有界微步骤；其真实耗时仍记入原池的欠额。
	int32 OverdueIndex = INDEX_NONE;
	if (InCapacity.bAllowOverdueOpportunity && IsValidCost(InCapacity.MaximumOverdueMicroseconds))
	{
		for (int32 Band = 1; Band < BandCount; ++Band)
		{
			const int32 Index = Oldest[Band];
			if (Index == INDEX_NONE ||
				(Pool == EVoxelTerrainServicePool::Near && !IsNear(EVoxelTerrainServiceBand(Band))) ||
				(Pool == EVoxelTerrainServicePool::NonNear && IsNear(EVoxelTerrainServiceBand(Band))) ||
				InNowSeconds - InCandidates[Index].FirstQueuedSeconds < InCapacity.MaximumWaitSeconds ||
				InCandidates[Index].EstimatedMicroseconds > InCapacity.MaximumOverdueMicroseconds)
			{
				continue;
			}
			if (OverdueIndex == INDEX_NONE || IsOlder(Index, OverdueIndex))
			{
				OverdueIndex = Index;
			}
		}
	}
	if (OverdueIndex != INDEX_NONE && NextChargeSerial < MAX_uint64)
	{
		const auto& Candidate = InCandidates[OverdueIndex];
		const double Cost = FMath::Max(1.0, Candidate.EstimatedMicroseconds);
		if (!TryChangeCredit(State, int32(Candidate.Priority.Band), -Cost))
		{
			return INDEX_NONE;
		}
		OutCharge.Serial = ++NextChargeSerial;
		OutCharge.Pool = Pool;
		OutCharge.Band = Candidate.Priority.Band;
		OutCharge.EstimatedMicroseconds = Cost;
		OutCharge.bOverdueOpportunity = true;
		OutstandingCharges.Add(OutCharge.Serial, OutCharge);
		return Candidate.Index;
	}
	constexpr double Quantum[BandCount] = {0.0, 8000.0, 4000.0, 2000.0, 1000.0, 500.0, 250.0, 250.0};
	int32 Heads[BandCount];
	for (int32 Band = 0; Band < BandCount; ++Band)
	{
		Heads[Band] = INDEX_NONE;
		if (Band == 0 || Normal[Band] == INDEX_NONE ||
			(Pool == EVoxelTerrainServicePool::Near && !IsNear(EVoxelTerrainServiceBand(Band))) ||
			(Pool == EVoxelTerrainServicePool::NonNear && IsNear(EVoxelTerrainServiceBand(Band))))
		{
			continue;
		}
		const bool bOldestTurn = !InCapacity.bPreserveDistanceOrder && (State.DispatchCounts[Band] % 4 == 3 ||
			InNowSeconds - InCandidates[Oldest[Band]].FirstQueuedSeconds >= InCapacity.MaximumWaitSeconds);
		Heads[Band] = bOldestTurn ? Oldest[Band] : Normal[Band];
	}

	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 Visit = 0; Visit < BandCount - 1; ++Visit)
		{
			const int32 Band = State.Cursor;
			const int32 CandidateIndex = Heads[Band];
			if (CandidateIndex != INDEX_NONE)
			{
				if (!State.bVisitOpen)
				{
					if (!TryChangeCredit(State, Band, Quantum[Band]))
					{
						return INDEX_NONE;
					}
					State.bVisitOpen = true;
				}
				const double Cost = FMath::Max(1.0, InCandidates[CandidateIndex].EstimatedMicroseconds);
				if (State.Deficits[Band] >= Cost)
				{
					if (NextChargeSerial == MAX_uint64)
					{
						return INDEX_NONE;
					}
					if (!TryChangeCredit(State, Band, -Cost))
					{
						return INDEX_NONE;
					}
					++State.DispatchCounts[Band];
					OutCharge.Serial = ++NextChargeSerial;
					OutCharge.Pool = Pool;
					OutCharge.Band = EVoxelTerrainServiceBand(Band);
					OutCharge.EstimatedMicroseconds = Cost;
					OutstandingCharges.Add(OutCharge.Serial, OutCharge);
					return InCandidates[CandidateIndex].Index;
				}
			}
			State.bVisitOpen = false;
			State.Cursor = Band == BandCount - 1 ? 1 : Band + 1;
		}

		double AdditionalRounds = MAX_dbl;
		for (int32 Band = 1; Band < BandCount; ++Band)
		{
			if (Heads[Band] == INDEX_NONE)
			{
				continue;
			}
			const double Cost = FMath::Max(1.0, InCandidates[Heads[Band]].EstimatedMicroseconds);
			const double Missing = FMath::Max(0.0, Cost - State.Deficits[Band]);
			AdditionalRounds = FMath::Min(AdditionalRounds, FMath::CeilToDouble(Missing / Quantum[Band]));
		}
		if (!FMath::IsFinite(AdditionalRounds) || AdditionalRounds == MAX_dbl)
		{
			bAccountingFault = true;
			return INDEX_NONE;
		}
		const double SkippedRounds = FMath::Max(0.0, AdditionalRounds - 1.0);
		for (int32 Band = 1; Band < BandCount; ++Band)
		{
			if (Heads[Band] != INDEX_NONE)
			{
				if (!TryChangeCredit(State, Band, SkippedRounds * Quantum[Band]))
				{
					return INDEX_NONE;
				}
			}
		}
	}
	return INDEX_NONE;
}

bool FVoxelTerrainWorkSelection::Settle(const uint64 InChargeSerial, const double InActualMicroseconds)
{
	const FVoxelTerrainServiceCharge* Found = OutstandingCharges.Find(InChargeSerial);
	if (!Found)
	{
		return false;
	}
	const FVoxelTerrainServiceCharge Charge = *Found;
	OutstandingCharges.Remove(InChargeSerial);
	if (!IsValidCost(InActualMicroseconds))
	{
		bAccountingFault = true;
		return false;
	}
	FPoolState& State = Pools[static_cast<int32>(Charge.Pool)];
	return TryChangeCredit(State, static_cast<int32>(Charge.Band),
		Charge.EstimatedMicroseconds - FMath::Max(1.0, InActualMicroseconds));
}

bool FVoxelTerrainWorkSelection::RefundBeforeStart(const uint64 InChargeSerial)
{
	const FVoxelTerrainServiceCharge* Found = OutstandingCharges.Find(InChargeSerial);
	if (!Found)
	{
		return false;
	}
	const FVoxelTerrainServiceCharge Charge = *Found;
	OutstandingCharges.Remove(InChargeSerial);
	return TryChangeCredit(Pools[static_cast<int32>(Charge.Pool)], static_cast<int32>(Charge.Band), Charge.EstimatedMicroseconds);
}

bool FVoxelTerrainWorkSelection::Reset()
{
	if (!OutstandingCharges.IsEmpty())
	{
		return false;
	}
	for (FPoolState& State : Pools)
	{
		State = {};
	}
	bAccountingFault = false;
	return true;
}

bool FVoxelTerrainWorkSelection::HasAccountingFault() const
{
	return bAccountingFault;
}

bool FVoxelTerrainWorkSelection::IsValidCost(const double InMicroseconds)
{
	return FMath::IsFinite(InMicroseconds) && InMicroseconds >= 0.0 && InMicroseconds <= MaximumTaskCostMicroseconds;
}

bool FVoxelTerrainWorkSelection::TryChangeCredit(FPoolState& InOutState, const int32 InBand, const double InDelta)
{
	const double Candidate = InOutState.Deficits[InBand] + InDelta;
	if (!FMath::IsFinite(InDelta) || !FMath::IsFinite(Candidate) || FMath::Abs(Candidate) > MaximumAccountingMagnitude)
	{
		bAccountingFault = true;
		return false;
	}
	InOutState.Deficits[InBand] = Candidate;
	return true;
}
