#include "Voxel/Runtime/VoxelWorldRuntime.h"

#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "VoxelRuntimeStorage.h"

FVoxelWorldRuntime::~FVoxelWorldRuntime()
{
	if (StorageLease)
	{
		auto Retired = VoxelBoundedAllocation::MakeWorkerShared<VoxelRuntimeStorage::FRetiredStorage>(StorageLease);
		Retired->Sections = MoveTemp(Sections);
		Retired->ResidencyOrder = MoveTemp(ResidencyOrder);
		Retired->RetirementTimers = MoveTemp(RetirementTimers);
		Retired->ResidentPromotion = MoveTemp(PendingResidentPromotion);
	}
	if (OverlaySequenceLease)
	{
		auto Retired = VoxelBoundedAllocation::MakeWorkerShared<TMap<FIntVector, FOverlayRegionSequence>>(
			OverlaySequenceLease, MoveTemp(OverlayRegionSequences));
	}
}

FVoxelPreparedEdit::~FVoxelPreparedEdit()
{
	if (ResourceLease && !Sections.IsEmpty())
	{
		auto Retired = VoxelBoundedAllocation::MakeWorkerShared<TArray<FVoxelPreparedSection>>(ResourceLease, MoveTemp(Sections));
	}
}

FVoxelPreparedEdit& FVoxelPreparedEdit::operator=(FVoxelPreparedEdit&& InOther)
{
	if (this == &InOther) return *this;
	FVoxelPreparedEdit Previous(MoveTemp(*this));
	TransactionId = InOther.TransactionId;
	Sections = MoveTemp(InOther.Sections);
	ResourceLease = MoveTemp(InOther.ResourceLease);
	return *this;
}

bool FVoxelWorldRuntime::ReserveRuntimeStorage(const int32 InSectionCount)
{
	if (InSectionCount <= StorageCapacity) return true;
	if (InSectionCount > MAX_int32 / 2) return false;
	const int32 Capacity = FMath::Max(32, int32(FMath::RoundUpToPowerOfTwo(InSectionCount)));
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<VoxelRuntimeStorage::FRetiredStorage>() +
		VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<FVoxelSection, ESPMode::ThreadSafe>>(Capacity) +
		VoxelBoundedAllocation::SparseArrayBytes<FIntVector>(Capacity) + VoxelBoundedAllocation::SetBytes<FIntVector>(Capacity) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelSection*>(Capacity);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NewLease;
	if (!AcquireRuntimeData({}, Bytes, StorageLease.IsValid(), NewLease)) return false;
	Sections.Reserve(Capacity);
	ResidencyOrder.Reserve(Capacity);
	PendingResidentPromotion.Reserve(Capacity);
	RetirementTimers.Reserve(Capacity);
	const uint64 Actual = VoxelBoundedAllocation::WorkerSharedBytes<VoxelRuntimeStorage::FRetiredStorage>() +
		Sections.GetAllocatedSize() + ResidencyOrder.GetAllocatedSize() + PendingResidentPromotion.GetAllocatedSize() +
		RetirementTimers.GetAllocatedSize();
	check(Actual <= Bytes);
	verify(NewLease->Resize(Actual));
	StorageLease = MoveTemp(NewLease);
	StorageLease->Transfer(EVoxelTerrainResourcePool::Resident);
	StorageCapacity = Capacity;
	return true;
}

bool FVoxelWorldRuntime::AcquireRuntimeData(const FIntVector& InSection, const uint64 InBytes,
	const bool bInReplacing, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease) const
{
	check(TerrainResources && NextAllocationSerial < MAX_uint64);
	const auto* Section = FindSection(InSection);
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId,
		Section ? Section->Stamp.Token : 1, InSection, ++NextAllocationSerial);
	return TerrainResources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, InBytes, OutLease) ||
		(bInReplacing && TerrainResources->Acquire(Consumer, EVoxelTerrainResourcePool::ResidentOverlap, InBytes, OutLease));
}

void FVoxelWorldRuntime::FinishRuntimeData(FVoxelSection& InSection, const FIntVector& InKey,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
{
	check(InLease);
	InSection.DataLease = MoveTemp(InLease);
	uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<VoxelRuntimeStorage::FRetiredData>() +
		InSection.Blocks.GetAllocatedSize() + InSection.Overlay.GetAllocatedSize() + InSection.Entities.GetAllocatedSize();
	for (const auto& Pair : InSection.Entities) Bytes += Pair.Value.Payload.GetAllocatedSize();
	check(Bytes <= InSection.DataLease->Bytes());
	verify(InSection.DataLease->Resize(Bytes));
	if (InSection.DataLease->Transfer(EVoxelTerrainResourcePool::Resident)) PendingResidentPromotion.Remove(InKey);
	else
	{
		PendingResidentPromotion.Add(InKey);
		ResidentPromotionReleaseVersion = MAX_uint64;
	}
}

void FVoxelWorldRuntime::RetireRuntimeData(FVoxelSection& InSection)
{
	if (!InSection.DataLease) return;
	auto Retired = VoxelBoundedAllocation::MakeWorkerShared<VoxelRuntimeStorage::FRetiredData>(InSection.DataLease);
	Retired->Blocks = MoveTemp(InSection.Blocks);
	Retired->Overlay = MoveTemp(InSection.Overlay);
	Retired->Entities = MoveTemp(InSection.Entities);
	InSection.DataLease.Reset();
}

TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> FVoxelWorldRuntime::GetTerrainResources() const
{
	return TerrainResources.ToSharedRef();
}

void FVoxelWorldRuntime::AdvanceResidentPromotion(const int32 InMaximumSections, const double InDeadline,
	FVoxelTerrainFrameContext* InFrameContext)
{
	const uint64 Version = TerrainResources->Snapshot().ReleaseVersion;
	for (int32 Step = 0; Step < InMaximumSections; ++Step)
	{
		const bool bReleased = Version != ResidentPromotionReleaseVersion;
		if (!bReleased && (ResidentPromotionRemaining <= 0 || PendingResidentPromotion.IsEmpty())) break;
		const double Started = FPlatformTime::Seconds();
		const bool bStarted = Started < InDeadline && (!InFrameContext || InFrameContext->GetPublishBudget().TryStart(
			EVoxelPublishPhase::Retire, Started, ResidentPromotionCostMilliseconds));
		if (!bStarted)
		{
			if (!InFrameContext || !InFrameContext->IsOverdueOpportunityAvailable()) break;
			InFrameContext->ConsumeOverdueOpportunity();
		}
		if (bReleased)
		{
			ResidentPromotionReleaseVersion = Version;
			if (StorageLease) StorageLease->Transfer(EVoxelTerrainResourcePool::Resident);
			if (OverlaySequenceLease) OverlaySequenceLease->Transfer(EVoxelTerrainResourcePool::Resident);
			ResidentPromotionRemaining = PendingResidentPromotion.GetMaxIndex();
		}
		else
		{
			--ResidentPromotionRemaining;
			ResidentPromotionCursor %= PendingResidentPromotion.GetMaxIndex();
			const FSetElementId Id = FSetElementId::FromInteger(ResidentPromotionCursor++);
			if (PendingResidentPromotion.IsValidId(Id))
			{
				const FIntVector Key = PendingResidentPromotion.Get(Id);
				auto* Section = FindSection(Key);
				if (!Section || !Section->DataLease || Section->DataLease->Transfer(EVoxelTerrainResourcePool::Resident))
				PendingResidentPromotion.Remove(Key);
			}
		}
		const double Finished = FPlatformTime::Seconds();
		ResidentPromotionCostMilliseconds = FMath::Lerp(ResidentPromotionCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (InFrameContext) InFrameContext->GetPublishBudget().Finish(EVoxelPublishPhase::Retire, Started, Finished);
		if (!bStarted) break;
	}
}

bool FVoxelWorldRuntime::ReserveOverlaySequences(const TConstArrayView<FVoxelSectionPatch> InPatches)
{
	if (!bAuthority || InPatches.IsEmpty()) return true;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FIntVector>(InPatches.Num());
	if (!TerrainResources->Acquire(FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, 1, {},
		++NextAllocationSerial), EVoxelTerrainResourcePool::Working, Bytes, Scratch)) return false;
	TArray<FIntVector> Keys;
	Keys.Reserve(InPatches.Num());
	for (const auto& Patch : InPatches) Keys.Add(Patch.Section);
	return ReserveOverlaySequenceKeys(Keys);
}

bool FVoxelWorldRuntime::ReserveOverlaySequenceKeys(const TConstArrayView<FIntVector> InSections)
{
	if (!bAuthority || InSections.IsEmpty()) return true;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	const uint64 ScratchBytes = VoxelBoundedAllocation::SetBytes<FIntVector>(InSections.Num());
	if (!TerrainResources->Acquire(FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, 1, {},
		++NextAllocationSerial), EVoxelTerrainResourcePool::Working, ScratchBytes, Scratch)) return false;
	TSet<FIntVector> Regions;
	Regions.Reserve(InSections.Num());
	for (const auto& Section : InSections)
	{
		Regions.Add(FIntVector(VoxelGeneration::FloorDivide(Section.X, 8),
			VoxelGeneration::FloorDivide(Section.Y, 8), VoxelGeneration::FloorDivide(Section.Z, 8)));
	}
	int32 Required = OverlayRegionSequences.Num();
	for (const auto& Region : Regions) Required += OverlayRegionSequences.Contains(Region) ? 0 : 1;
	if (Required > MAX_int32 / 2) return false;
	if (Required > OverlaySequenceCapacity)
	{
		const int32 Capacity = FMath::Max(32, int32(FMath::RoundUpToPowerOfTwo(Required)));
		const uint64 HeadBytes = VoxelBoundedAllocation::WorkerSharedBytes<TMap<FIntVector, FOverlayRegionSequence>>();
		const uint64 Bytes = HeadBytes + VoxelBoundedAllocation::MapBytes<FIntVector, FOverlayRegionSequence>(Capacity);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NewLease;
		if (!AcquireRuntimeData({}, Bytes, OverlaySequenceLease.IsValid(), NewLease)) return false;
		OverlayRegionSequences.Reserve(Capacity);
		check(HeadBytes + OverlayRegionSequences.GetAllocatedSize() <= Bytes);
		verify(NewLease->Resize(HeadBytes + OverlayRegionSequences.GetAllocatedSize()));
		OverlaySequenceLease = MoveTemp(NewLease);
		OverlaySequenceLease->Transfer(EVoxelTerrainResourcePool::Resident);
		OverlaySequenceCapacity = Capacity;
	}
	for (const auto& Region : Regions) OverlayRegionSequences.FindOrAdd(Region);
	return true;
}
