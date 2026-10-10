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
	if (InFirst.DistanceCells != InSecond.DistanceCells)
	{
		return InFirst.DistanceCells < InSecond.DistanceCells;
	}
	if (InFirst.Band != InSecond.Band)
	{
		return uint8(InFirst.Band) < uint8(InSecond.Band);
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

	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < InCandidates.Num(); ++Index)
	{
		const auto& Candidate = InCandidates[Index];
		const int32 Band = static_cast<int32>(Candidate.Priority.Band);
		if (!Candidate.bRunnable || Candidate.Index == INDEX_NONE || Band <= 0 || Band >= BandCount ||
			!FMath::IsFinite(Candidate.Priority.DistanceCells) || Candidate.Priority.DistanceCells < 0.0 ||
			!FMath::IsFinite(Candidate.Priority.ForwardScore) ||
			!FMath::IsFinite(Candidate.FirstQueuedSeconds) || Candidate.FirstQueuedSeconds > InNowSeconds ||
			!IsValidCost(Candidate.EstimatedMicroseconds))
		{
			continue;
		}
		if (BestIndex == INDEX_NONE || HigherPriority(Candidate.Priority, InCandidates[BestIndex].Priority) ||
			(Candidate.Priority == InCandidates[BestIndex].Priority &&
			 (Candidate.FirstQueuedSeconds < InCandidates[BestIndex].FirstQueuedSeconds ||
			  (Candidate.FirstQueuedSeconds == InCandidates[BestIndex].FirstQueuedSeconds &&
			   Candidate.Sequence < InCandidates[BestIndex].Sequence))))
		{
			BestIndex = Index;
		}
	}
	if (BestIndex == INDEX_NONE || NextChargeSerial == MAX_uint64 || !ReserveCharges())
	{
		return INDEX_NONE;
	}
	const auto& Candidate = InCandidates[BestIndex];
	OutCharge.Serial = ++NextChargeSerial;
	OutCharge.Pool = EVoxelTerrainServicePool::All;
	OutCharge.Band = Candidate.Priority.Band;
	OutCharge.EstimatedMicroseconds = FMath::Max(1.0, Candidate.EstimatedMicroseconds);
	OutstandingCharges.Add(OutCharge.Serial, OutCharge);
	return Candidate.Index;
}

bool FVoxelTerrainWorkSelection::Settle(const uint64 InChargeSerial, const double InActualMicroseconds)
{
	const FVoxelTerrainServiceCharge* Found = OutstandingCharges.Find(InChargeSerial);
	if (!Found)
	{
		return false;
	}
	OutstandingCharges.Remove(InChargeSerial);
	if (!IsValidCost(InActualMicroseconds))
	{
		bAccountingFault = true;
		return false;
	}
	return true;
}

bool FVoxelTerrainWorkSelection::RefundBeforeStart(const uint64 InChargeSerial)
{
	const FVoxelTerrainServiceCharge* Found = OutstandingCharges.Find(InChargeSerial);
	if (!Found)
	{
		return false;
	}
	OutstandingCharges.Remove(InChargeSerial);
	return true;
}

bool FVoxelTerrainWorkSelection::Reset()
{
	if (!OutstandingCharges.IsEmpty())
	{
		return false;
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
