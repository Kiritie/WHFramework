#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

void FVoxelGenerationPlanCoordinator::ApplyDonationPriority(FState& InOutState,
	const FVoxelTerrainServicePriority& InPriority, const EVoxelWorkClass InWorkClass, const uint64 InBestDonationId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PlanApplyDonationPriority);
	const EVoxelWorkClass PreviousWorkClass = InOutState.WorkClass;
	const FVoxelTerrainServicePriority PreviousPriority = InOutState.ServicePriority;
	InOutState.BestDonationId = InBestDonationId;
	InOutState.WorkClass = InWorkClass;
	PriorityGraph.SetPriority(InOutState.PriorityNode, InPriority);
	const auto* Effective = PriorityGraph.Find(InOutState.PriorityNode);
	check(Effective);
	InOutState.ServicePriority = FVoxelTerrainWorkSelection::Promote(InPriority, *Effective);
	InOutState.SourcePriority = InOutState.ServicePriority.SourcePriority;
	InOutState.DistanceScore = InOutState.ServicePriority.DistanceCells;
	InOutState.ForwardScore = InOutState.ServicePriority.ForwardScore;
	Storage->bQueuedPrioritiesDirty |= InOutState.bInFlight &&
		(PreviousWorkClass != InOutState.WorkClass || PreviousPriority != InOutState.ServicePriority);
}

void FVoxelGenerationPlanCoordinator::UpdateDonation(FState& InOutState,
	const FVoxelGenerationConsumerKey& InConsumer, const FDonation& InDonation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PlanUpdateDonation);
	check(InDonation.SubscriptionId && (InOutState.BestDonationId || InOutState.Donations.IsEmpty()));
	const auto* Own = PriorityGraph.FindOwn(InOutState.PriorityNode);
	check(Own);
	const FVoxelTerrainServicePriority PreviousOwn = *Own;
	const EVoxelWorkClass PreviousClass = InOutState.WorkClass;
	const auto* Previous = InOutState.Donations.Find(InConsumer);
	const bool bReplacingWinner = Previous && Previous->SubscriptionId == InOutState.BestDonationId;
	const bool bHigher = FVoxelTerrainWorkSelection::HigherPriority(InDonation.Priority, PreviousOwn);
	const bool bSamePriority = InDonation.Priority == PreviousOwn;
	const bool bKeepsWinner = bReplacingWinner && (bHigher || (bSamePriority && InDonation.WorkClass <= PreviousClass));
	InOutState.Donations.Add(InConsumer, InDonation);
	if (!InOutState.BestDonationId || bKeepsWinner ||
		(!bReplacingWinner && (bHigher || (bSamePriority && InDonation.WorkClass < PreviousClass))))
	{
		ApplyDonationPriority(InOutState, InDonation.Priority, InDonation.WorkClass, InDonation.SubscriptionId);
	}
	else if (bReplacingWinner)
	{
		// 胜出项降级后竞争者可能接替；从唯一原表重新归并。
		ResolveDonations(InOutState);
	}
	else
	{
		// 非胜出项不改变自身优先级，但仍消费原图当前的上游有效捐赠。
		ApplyDonationPriority(InOutState, PreviousOwn, PreviousClass, InOutState.BestDonationId);
	}
	RecordDonationMutation(InOutState);
}

void FVoxelGenerationPlanCoordinator::ResolveDonations(FState& InOutState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PlanResolveDonations);
	++Storage->DonationRescans;
	const FDonation* Best = nullptr;
	for (const auto& Donation : InOutState.Donations)
	{
		if (!Best || FVoxelTerrainWorkSelection::HigherPriority(Donation.Value.Priority, Best->Priority) ||
			(Donation.Value.Priority == Best->Priority && Donation.Value.WorkClass < Best->WorkClass))
		{
			Best = &Donation.Value;
		}
	}
	ApplyDonationPriority(InOutState, Best ? Best->Priority : FVoxelTerrainServicePriority{},
		Best ? Best->WorkClass : EVoxelWorkClass::Background, Best ? Best->SubscriptionId : 0);
}

void FVoxelGenerationPlanCoordinator::RecordDonationMutation(const FState& InState)
{
	++Storage->DonationMutations;
#if DO_CHECK
	if ((Storage->DonationMutations & 4095) != 0) return;
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PlanVerifyDonationPriority);
	const FDonation* Best = nullptr;
	const FDonation* Winner = nullptr;
	for (const auto& Donation : InState.Donations)
	{
		if (Donation.Value.SubscriptionId == InState.BestDonationId) Winner = &Donation.Value;
		if (!Best || FVoxelTerrainWorkSelection::HigherPriority(Donation.Value.Priority, Best->Priority) ||
			(Donation.Value.Priority == Best->Priority && Donation.Value.WorkClass < Best->WorkClass)) Best = &Donation.Value;
	}
	const auto* Own = PriorityGraph.FindOwn(InState.PriorityNode);
	const auto* Effective = PriorityGraph.Find(InState.PriorityNode);
	checkf(Own && Effective && (Best ? Winner && Winner->Priority == Best->Priority && Winner->WorkClass == Best->WorkClass :
		!Winner && InState.BestDonationId == 0), TEXT("Generation donation winner identity disagrees with original subscriptions"));
	checkf(*Own == (Best ? Best->Priority : FVoxelTerrainServicePriority{}) &&
		InState.WorkClass == (Best ? Best->WorkClass : EVoxelWorkClass::Background) &&
		InState.ServicePriority == FVoxelTerrainWorkSelection::Promote(*Own, *Effective),
		TEXT("Generation donation aggregation disagrees with original subscriptions or priority graph"));
	++Storage->DonationVerifications;
#endif
}
