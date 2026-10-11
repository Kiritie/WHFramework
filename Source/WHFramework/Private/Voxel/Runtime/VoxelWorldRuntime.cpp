#include "Voxel/Runtime/VoxelWorldRuntime.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Save/InventoryVoxelEntityCodec.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "VoxelRuntimeStorage.h"

namespace VoxelWorldRuntimePrivate
{
	constexpr int32 RuntimeSectionSide = 16;
	constexpr int32 SectionVolume = RuntimeSectionSide * RuntimeSectionSide * RuntimeSectionSide;
	constexpr uint64 EntityCodecScratchBytes = 64 * 1024;

	uint64 EntityCopyBytes(const TMap<int32, FVoxelBlockEntityState>& InEntities)
	{
		uint64 Bytes = 0;
		for (const auto& Pair : InEntities) Bytes += VoxelBoundedAllocation::ArrayBytes<uint8>(Pair.Value.Payload.Num());
		return Bytes;
	}

	uint64 DefaultEntityBytes()
	{
		return VoxelBoundedAllocation::InitialGrowBytes<uint8>(FVoxelBlockEntityCodec::MaximumDefaultPayloadBytes);
	}

	uint64 MutableDataBytes(const int32 InOverlayCount, const int32 InEntityCount, const uint64 InPayloadBytes)
	{
		return VoxelBoundedAllocation::WorkerSharedBytes<VoxelRuntimeStorage::FRetiredData>() +
			VoxelBoundedAllocation::ArrayBytes<FVoxelBlockState>(SectionVolume) +
			VoxelBoundedAllocation::MapBytes<int32, FVoxelBlockState>(InOverlayCount) +
			VoxelBoundedAllocation::MapBytes<int32, FVoxelBlockEntityState>(InEntityCount) + InPayloadBytes;
	}

	struct FEditStorageCount
	{
		int32 Cells = 0;
		int32 Entities = 0;
		uint64 PayloadBytes = 0;
		uint64 DefaultPayloadBytes = 0;
	};

	FIntVector ToSection(const FIntVector& InPosition)
	{
		return FIntVector(
			VoxelGeneration::FloorDivide(InPosition.X, RuntimeSectionSide),
			VoxelGeneration::FloorDivide(InPosition.Y, RuntimeSectionSide),
			VoxelGeneration::FloorDivide(InPosition.Z, RuntimeSectionSide));
	}

	int32 PositiveMod(const int32 InValue)
	{
		const int32 Result = InValue % RuntimeSectionSide;
		return Result < 0 ? Result + RuntimeSectionSide : Result;
	}

	int32 ToCellIndex(const FIntVector& InPosition)
	{
		const int32 X = PositiveMod(InPosition.X);
		const int32 Y = PositiveMod(InPosition.Y);
		const int32 Z = PositiveMod(InPosition.Z);
		return X + Y * RuntimeSectionSide + Z * RuntimeSectionSide * RuntimeSectionSide;
	}
}

using namespace VoxelWorldRuntimePrivate;

FVoxelWorldRuntime::FVoxelWorldRuntime(
	const uint64 InWorldEpoch,
	const bool bInAuthority,
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources)
	: WorldEpoch(InWorldEpoch)
	, bAuthority(bInAuthority)
	, Registry(InRegistry)
	, Generator(InGenerator)
	, CommittedJournal(InWorldEpoch)
	, TerrainResources(InResources)
{
	CommittedJournal.BindResources(TerrainResources.ToSharedRef());
	const auto Control = FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, 1, {}, 1);
	ChangeIndex.BindResources(TerrainResources.ToSharedRef(), Control);
	ChangeHierarchy.BindResources(TerrainResources.ToSharedRef(), Control);
}

#if WITH_DEV_AUTOMATION_TESTS
FVoxelWorldRuntime::FVoxelWorldRuntime(const uint64 InWorldEpoch, const bool bInAuthority,
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator)
	: FVoxelWorldRuntime(InWorldEpoch, bInAuthority, InRegistry, InGenerator,
		MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>())
{
}
#endif

FVoxelSection* FVoxelWorldRuntime::FindSection(const FIntVector& InSection)
{
	const TSharedPtr<FVoxelSection, ESPMode::ThreadSafe>* Found = Sections.Find(InSection);
	FVoxelSection* Section = Found ? Found->Get() : nullptr;
	if (Section && Section->PersistedRevision < Section->CommittedRevision)
		Section->PersistedRevision = FMath::Max(Section->PersistedRevision, CommittedJournal.PersistedRevision(InSection));
	return Section;
}

const FVoxelSection* FVoxelWorldRuntime::FindSection(const FIntVector& InSection) const
{
	const TSharedPtr<FVoxelSection, ESPMode::ThreadSafe>* Found = Sections.Find(InSection);
	return Found ? Found->Get() : nullptr;
}

FVoxelSection* FVoxelWorldRuntime::FindOrAllocate(const FIntVector& InSection, const uint64 InFrame)
{
	check(IsInGameThread());

	if (FVoxelSection* Existing = FindSection(InSection))
	{
		if (Existing->Retirement.State == EVoxelRuntimeRetirementState::Detaching ||
			Existing->Status == EVoxelSectionStatus::Unloading) return nullptr;
		Existing->LastWantedFrame = InFrame;
		NotifySectionEvent(InSection, EVoxelRuntimeSectionEvent::Wanted);
		return Existing;
	}

	if (NextToken == MAX_uint64)
	{
		return nullptr;
	}

	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, NextToken, InSection, 1);
	const uint64 SectionBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSection>();
	const uint64 SlotBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTaskResult>();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
	if (!TerrainResources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, SectionBytes + SlotBytes, Metadata, 1) ||
		!ReserveRuntimeStorage(Sections.Num() + 1)) return nullptr;
	auto SlotLease = Metadata->Split(SlotBytes);
	check(SlotLease);
	auto NewSection = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSection>(Metadata);
	NewSection->MetadataLease = MoveTemp(Metadata);
	NewSection->PendingPublish = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTaskResult>(MoveTemp(SlotLease));
	NewSection->Stamp.Epoch = WorldEpoch;
	NewSection->Stamp.Token = NextToken++;
	NewSection->Incarnation = NewSection->Stamp.Token;
	NewSection->Status = EVoxelSectionStatus::Allocated;
	NewSection->LastWantedFrame = InFrame;
	NewSection->ResidencyOrderIndex = ResidencyOrder.Add(InSection);
	FVoxelSection* Result = NewSection.Get();
	Sections.Add(InSection, MoveTemp(NewSection));
	NotifySectionEvent(InSection, EVoxelRuntimeSectionEvent::Allocated);
	return Result;
}

bool FVoxelWorldRuntime::TryPinSection(const FIntVector& InSection, const FVoxelRuntimeSectionIdentity& InIdentity)
{
	check(IsInGameThread());
	auto* Section = FindSection(InSection);
	if (!Section || Section->GetResourceIdentity() != InIdentity || Section->Status == EVoxelSectionStatus::Unloading ||
		Section->Retirement.State == EVoxelRuntimeRetirementState::Detaching ||
		Section->PinCount.Load() == MAX_int32) return false;
	Section->PinCount.Store(Section->PinCount.Load() + 1);
	return true;
}

bool FVoxelWorldRuntime::ReleaseSectionPin(const FIntVector& InSection, const FVoxelRuntimeSectionIdentity& InIdentity)
{
	check(IsInGameThread());
	auto* Section = FindSection(InSection);
	if (!Section || Section->GetResourceIdentity() != InIdentity || Section->PinCount.Load() <= 0) return false;
	Section->PinCount.Store(Section->PinCount.Load() - 1);
	if (Section->PinCount.Load() == 0) NotifySectionEvent(InSection, EVoxelRuntimeSectionEvent::PinReleased);
	return true;
}

void FVoxelWorldRuntime::SetSectionEventCallback(
	TFunction<void(const FIntVector&, const FVoxelSectionStamp&, EVoxelRuntimeSectionEvent)> InCallback)
{
	check(IsInGameThread());
	SectionEventCallback = MoveTemp(InCallback);
}

void FVoxelWorldRuntime::NotifySectionEvent(const FIntVector& InSection, const EVoxelRuntimeSectionEvent InEvent)
{
	if (SectionEventCallback)
		if (const auto* Section = FindSection(InSection)) SectionEventCallback(InSection, Section->Stamp, InEvent);
}

const TSparseArray<FIntVector>& FVoxelWorldRuntime::GetResidencyOrder() const
{
	return ResidencyOrder;
}

TArray<FIntVector> FVoxelWorldRuntime::ResidentSections() const
{
	TArray<FIntVector> Result;
	Sections.GetKeys(Result);
	Result.Sort([](const FIntVector& InA, const FIntVector& InB)
	{
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
	return Result;
}

bool FVoxelWorldRuntime::TryGetBlock(const FIntVector& InPosition, FVoxelBlockState& OutState) const
{
	const FVoxelSection* Section = FindSection(ToSection(InPosition));
	if (!Section || Section->Status != EVoxelSectionStatus::DataReady ||
		(!bAuthority && (!RemoteSectionValidator || !RemoteSectionValidator(ToSection(InPosition), Section->CommittedRevision))))
	{
		return false;
	}

	const int32 CellIndex = ToCellIndex(InPosition);
	if (!Section->Blocks.IsValidIndex(CellIndex))
	{
		return false;
	}

	OutState = Section->Blocks[CellIndex];
	return true;
}

bool FVoxelWorldRuntime::PublishBase(
	const FIntVector& InSection,
	const FVoxelSectionStamp& InStamp,
	TArray<FVoxelBlockState>&& InBaseBlocks,
	FString& OutError,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResourceLease)
{
	check(IsInGameThread());
	FVoxelSection* Section = FindSection(InSection);
	if (!Section ||
		Section->Status != EVoxelSectionStatus::Allocated ||
		Section->Stamp != InStamp)
	{
		OutError = TEXT("Voxel natural base publish stamp is stale");
		return false;
	}

	if (InBaseBlocks.Num() != SectionVolume)
	{
		OutError = TEXT("Voxel natural base must contain exactly 4096 cells");
		return false;
	}

	for (const FVoxelBlockState& State : InBaseBlocks)
	{
		if (!ValidateState(State))
		{
			OutError = TEXT("Voxel natural base contains an invalid block state");
			return false;
		}
	}

	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<TArray<FVoxelBlockState>>() + VoxelGenerationAllocation::AllocatedBytes(InBaseBlocks);
	if (InResourceLease)
	{
		if (InResourceLease->Bytes() < Bytes || !InResourceLease->Transfer(EVoxelTerrainResourcePool::Resident))
		{
			OutError = TEXT("CapacityBlocked: Runtime自然基底接纳配额");
			return false;
		}
		verify(InResourceLease->Resize(Bytes));
	}
	else if (!AcquireRuntimeData(InSection, Bytes, false, InResourceLease))
	{
		OutError = TEXT("CapacityBlocked: Runtime自然基底接纳配额");
		return false;
	}
	Section->BaseBlocks = VoxelBoundedAllocation::MakeWorkerShared<TArray<FVoxelBlockState>>(MoveTemp(InResourceLease), MoveTemp(InBaseBlocks));
	Section->Status = EVoxelSectionStatus::BaseReady;
	Section->Error.Reset();
	OutError.Reset();
	return true;
}

bool FVoxelWorldRuntime::PublishFinal(
	const FIntVector& InSection,
	const uint64 InRevision,
	const TMap<int32, FVoxelBlockState>& InOverlay,
	const TMap<int32, FVoxelBlockEntityState>& InEntities,
	FString& OutError)
{
	check(IsInGameThread());
	FVoxelSection* Section = FindSection(InSection);
	const bool bRemoteReplacement = Section && !IsServer() && Section->Status == EVoxelSectionStatus::DataReady &&
		InRevision >= Section->CommittedRevision;
	if (!Section || (Section->Status != EVoxelSectionStatus::BaseReady && !bRemoteReplacement) || !Section->BaseBlocks)
	{
		OutError = TEXT("Voxel final publish target is not BaseReady");
		return false;
	}

	if (InOverlay.Num() > SectionVolume || InEntities.Num() > SectionVolume)
	{
		OutError = TEXT("Voxel final overlay or entity count exceeds section bounds");
		return false;
	}
	for (const auto& Pair : InOverlay)
	{
		if (Pair.Key < 0 || Pair.Key >= SectionVolume || !ValidateState(Pair.Value))
		{
			OutError = TEXT("Voxel final overlay contains an invalid cell");
			return false;
		}
	}
	for (const auto& Pair : InEntities)
	{
		if (Pair.Key < 0 || Pair.Key >= SectionVolume || Pair.Value.Payload.Num() > FVoxelDeltaCodec::MaximumEntityPayloadBytes ||
			(bAuthority && FInventoryVoxelEntityCodec::IsPublic(Pair.Value)))
		{
			OutError = TEXT("Voxel最终实体格子或Payload超出Codec容量");
			return false;
		}
	}
	int32 DefaultCount = 0;
	for (int32 Index = 0; Index < SectionVolume; ++Index)
	{
		const auto* Override = InOverlay.Find(Index);
		const auto& State = Override ? *Override : (*Section->BaseBlocks)[Index];
		const auto* Definition = Registry->Find(State.TypeId);
		DefaultCount += Definition && Definition->EntityKind && !Override && !InEntities.Contains(Index) ? 1 : 0;
	}
	const int32 EntityCapacity = FMath::Min(SectionVolume, InEntities.Num() + DefaultCount);
	const uint64 DataBytes = MutableDataBytes(InOverlay.Num(), EntityCapacity,
		EntityCopyBytes(InEntities) + uint64(DefaultCount) * DefaultEntityBytes());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DataLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
	if (!AcquireRuntimeData(InSection, DataBytes, Section->DataLease.IsValid(), DataLease) ||
		!TerrainResources->Acquire(FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, Section->Stamp.Token,
			InSection, ++NextAllocationSerial), EVoxelTerrainResourcePool::Working, EntityCodecScratchBytes, ScratchLease))
	{
		OutError = TEXT("CapacityBlocked: Runtime最终数据新旧重叠及Codec工作区");
		return false;
	}
	TArray<FVoxelBlockState> Candidate;
	Candidate.Reserve(SectionVolume);
	Candidate.Append(*Section->BaseBlocks);
	for (const TPair<int32, FVoxelBlockState>& Pair : InOverlay)
	{
		if (!Candidate.IsValidIndex(Pair.Key) || !ValidateState(Pair.Value))
		{
			OutError = TEXT("Voxel final overlay contains an invalid cell");
			return false;
		}
		Candidate[Pair.Key] = Pair.Value;
	}

	TMap<int32, FVoxelBlockEntityState> Entities;
	Entities.Reserve(EntityCapacity);
	for (const auto& Pair : InEntities) Entities.Add(Pair.Key, Pair.Value);
	for (int32 Index = 0; Index < Candidate.Num(); ++Index)
	{
		const FVoxelRuntimeDefinition* Definition = Registry->Find(Candidate[Index].TypeId);
		if (Definition && Definition->EntityKind && !InOverlay.Contains(Index) && !Entities.Contains(Index))
		{
			FVoxelBlockEntityState Entity;
			if (!FVoxelBlockEntityCodec::MakeDefault(Definition->EntityKind, Entity, Definition->EntityVariant))
			{
				OutError = TEXT("Generated voxel entity has no registered default codec");
				return false;
			}
			Entities.Add(Index, MoveTemp(Entity));
		}
	}
	if (!ValidateSection(Candidate, InOverlay, Entities, OutError))
	{
		return false;
	}

	const bool bPreviouslyModified = ChangeIndex.IsModified(InSection);
	const bool bModified = !InOverlay.IsEmpty() || !InEntities.IsEmpty();
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Journal;
	const auto ExistingJournal = FindCommittedOverlay(InSection);
	if (ExistingJournal && InRevision <= ExistingJournal->Revision)
	{
		if (InRevision != ExistingJournal->Revision ||
			!ExistingJournal->Blocks.OrderIndependentCompareEqual(InOverlay) ||
			!ExistingJournal->Entities.OrderIndependentCompareEqual(InEntities))
		{
			OutError = TEXT("Voxel物化内容与当前完整Journal版本不一致");
			return false;
		}
		Journal = ExistingJournal;
	}
	else if ((InRevision > 0 || bModified || bPreviouslyModified || bRemoteReplacement) &&
		!PrepareJournal(InSection, InRevision, InOverlay, InEntities, Journal, OutError, bAuthority)) return false;
	TMap<int32, FVoxelBlockState> Overlay;
	Overlay.Reserve(InOverlay.Num());
	for (const auto& Pair : InOverlay) Overlay.Add(Pair.Key, Pair.Value);
	if (!ChangeIndex.SetModified(InSection, bModified, OutError)) return false;
	RetireRuntimeData(*Section);
	Section->Blocks = MoveTemp(Candidate);
	Section->Overlay = MoveTemp(Overlay);
	Section->Entities = MoveTemp(Entities);
	FinishRuntimeData(*Section, InSection, MoveTemp(DataLease));
	Section->CommittedRevision = InRevision;
	Section->PersistedRevision = ExistingJournal ? GetPersistedRevision(InSection) : InRevision;
	Section->Status = EVoxelSectionStatus::DataReady;
	check(DataReadyRevision < MAX_uint64);
	++DataReadyRevision;
	Section->Error.Reset();
	Section->bCollisionDirty = true;
	Section->bFineMeshDirty = true;
	if (Journal && Journal != ExistingJournal) RetainJournal(InSection, MoveTemp(Journal));
	// 物化已提交事实不产生新的内容版本；编辑提交与 Replica 根交换负责内容失效。
	OutError.Reset();
	return true;
}

bool FVoxelWorldRuntime::CaptureSnapshot(
	const FIntVector& InSection,
	FVoxelSectionSnapshot& OutSnapshot,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResourceLease) const
{
	const FVoxelSection* Section = FindSection(InSection);
	if (!Section || Section->Status != EVoxelSectionStatus::DataReady ||
		(!bAuthority && (!RemoteSectionValidator || !RemoteSectionValidator(InSection, Section->CommittedRevision))))
	{
		return false;
	}

	FVoxelSectionSnapshot Snapshot;
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<uint32>(Section->Blocks.Num()) +
		6 * VoxelBoundedAllocation::ArrayBytes<uint32>(RuntimeSectionSide * RuntimeSectionSide);
	if (!InResourceLease)
	{
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, Section->Stamp.Token,
			InSection, ++NextAllocationSerial);
		if (!TerrainResources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, InResourceLease)) return false;
	}
	else if (InResourceLease->Bytes() < Bytes) return false;
	Snapshot.ResourceLease = MoveTemp(InResourceLease);
	Snapshot.Section = InSection;
	Snapshot.Stamp = Section->Stamp;
	Snapshot.Revision = Section->CommittedRevision;
	Snapshot.Blocks.Reserve(Section->Blocks.Num());
	Snapshot.Blocks.SetNumUninitialized(Section->Blocks.Num());
	for (int32 Index = 0; Index < Section->Blocks.Num(); ++Index)
	{
		Snapshot.Blocks[Index] = Section->Blocks[Index].Pack();
	}

	for (uint8 Face = 0; Face < 6; ++Face)
	{
		const int32 Axis = Face / 2;
		const int32 UAxis = (Axis + 1) % 3;
		const int32 VAxis = (Axis + 2) % 3;
		FIntVector NeighborKey = InSection;
		NeighborKey[Axis] += Face % 2 == 0 ? 1 : -1;
		const FVoxelSection* Neighbor = FindSection(NeighborKey);
		const bool bKnown = Neighbor && Neighbor->Status == EVoxelSectionStatus::DataReady &&
			Neighbor->Blocks.Num() == RuntimeSectionSide * RuntimeSectionSide * RuntimeSectionSide &&
			(bAuthority || (RemoteSectionValidator && RemoteSectionValidator(NeighborKey, Neighbor->CommittedRevision)));
		Snapshot.Known[Face] = bKnown;
		Snapshot.Halo[Face].Reserve(RuntimeSectionSide * RuntimeSectionSide);
		Snapshot.Halo[Face].SetNumUninitialized(RuntimeSectionSide * RuntimeSectionSide);
		for (int32 V = 0; V < RuntimeSectionSide; ++V)
		{
			for (int32 U = 0; U < RuntimeSectionSide; ++U)
			{
				FIntVector Local = FIntVector::ZeroValue;
				Local[Axis] = Face % 2 == 0 ? 0 : RuntimeSectionSide - 1;
				Local[UAxis] = U;
				Local[VAxis] = V;
				const int32 Index = Local.X + RuntimeSectionSide * (Local.Y + RuntimeSectionSide * Local.Z);
				Snapshot.Halo[Face][U + RuntimeSectionSide * V] = bKnown ? Neighbor->Blocks[Index].Pack() : FVoxelBlockState().Pack();
			}
		}
	}

	OutSnapshot = MoveTemp(Snapshot);
	return true;
}

bool FVoxelWorldRuntime::CompleteSnapshotHalo(FVoxelSectionSnapshot& InOutSnapshot, const uint8 InFace) const
{
	check(IsInGameThread());
	if (InFace >= 6 || InOutSnapshot.GridSide != RuntimeSectionSide ||
		InOutSnapshot.Halo[InFace].Num() != RuntimeSectionSide * RuntimeSectionSide) return false;
	if (InOutSnapshot.Known[InFace]) return true;
	const auto* Section = FindSection(InOutSnapshot.Section);
	if (!Section || Section->Status != EVoxelSectionStatus::DataReady || Section->Stamp != InOutSnapshot.Stamp ||
		Section->CommittedRevision != InOutSnapshot.Revision) return false;
	const int32 Axis = InFace / 2, UAxis = (Axis + 1) % 3, VAxis = (Axis + 2) % 3;
	FIntVector NeighborKey = InOutSnapshot.Section;
	const int64 NeighborCoordinate = int64(NeighborKey[Axis]) + ((InFace & 1) ? -1 : 1);
	if (NeighborCoordinate < MIN_int32 || NeighborCoordinate > MAX_int32) return false;
	NeighborKey[Axis] = int32(NeighborCoordinate);
	const auto* Neighbor = FindSection(NeighborKey);
	if (!Neighbor || Neighbor->Status != EVoxelSectionStatus::DataReady ||
		Neighbor->Blocks.Num() != RuntimeSectionSide * RuntimeSectionSide * RuntimeSectionSide ||
		(!bAuthority && (!RemoteSectionValidator || !RemoteSectionValidator(NeighborKey, Neighbor->CommittedRevision)))) return false;
	for (int32 V = 0; V < RuntimeSectionSide; ++V) for (int32 U = 0; U < RuntimeSectionSide; ++U)
	{
		FIntVector Local = FIntVector::ZeroValue;
		Local[Axis] = (InFace & 1) ? RuntimeSectionSide - 1 : 0;
		Local[UAxis] = U;
		Local[VAxis] = V;
		const int32 Index = Local.X + RuntimeSectionSide * (Local.Y + RuntimeSectionSide * Local.Z);
		InOutSnapshot.Halo[InFace][U + RuntimeSectionSide * V] = Neighbor->Blocks[Index].Pack();
	}
	InOutSnapshot.Known[InFace] = true;
	return true;
}

bool FVoxelWorldRuntime::PrepareEdit(
	const TArray<FVoxelCellEdit>& InCells,
	const TArray<FVoxelEntityEdit>& InEntities,
	FVoxelPreparedEdit& OutPrepared,
	FString& OutError) const
{
	check(IsInGameThread());
	if (!bAuthority)
	{
		OutError = TEXT("Voxel edit runtime is not authoritative");
		return false;
	}
	if (InCells.IsEmpty() && InEntities.IsEmpty())
	{
		OutPrepared = FVoxelPreparedEdit();
		OutError.Reset();
		return true;
	}

	const int64 TotalEdits64 = int64(InCells.Num()) + InEntities.Num();
	if (TotalEdits64 > MAX_int32 / 2)
	{
		OutError = TEXT("Oversized: Voxel编辑批次目录超出容器范围");
		return false;
	}
	const int32 TotalEdits = int32(TotalEdits64);
	const uint64 ScratchBytes = VoxelBoundedAllocation::MapBytes<FIntVector, FEditStorageCount>(TotalEdits) +
		VoxelBoundedAllocation::MapBytes<FIntVector, int32>(TotalEdits) +
		VoxelBoundedAllocation::SetBytes<FIntVector>(InCells.Num()) +
		VoxelBoundedAllocation::SetBytes<FIntVector>(InEntities.Num()) + EntityCodecScratchBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
	if (!TerrainResources->Acquire(FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, 1, {},
		++NextAllocationSerial), EVoxelTerrainResourcePool::Working, ScratchBytes, ScratchLease))
	{
		OutError = TEXT("CapacityBlocked: Runtime编辑计数与Codec工作区");
		return false;
	}
	TMap<FIntVector, FEditStorageCount> Counts;
	Counts.Reserve(TotalEdits);
	uint64 DefaultPayloadBytes = 0;
	uint64 WrittenPayloadBytes = 0;
	for (const auto& Edit : InCells)
	{
		auto& Count = Counts.FindOrAdd(ToSection(Edit.Position));
		++Count.Cells;
		const auto* Definition = Registry->Find(Edit.Value.TypeId);
		if (Edit.Expected.TypeId != Edit.Value.TypeId && Definition && Definition->EntityKind)
		{
			Count.DefaultPayloadBytes += DefaultEntityBytes();
			DefaultPayloadBytes += DefaultEntityBytes();
		}
	}
	for (const auto& Edit : InEntities)
	{
		auto& Count = Counts.FindOrAdd(ToSection(Edit.Position));
		++Count.Entities;
		if (!Edit.bRemove)
		{
			if (Edit.Value.Payload.Num() > FVoxelDeltaCodec::MaximumEntityPayloadBytes)
			{
				OutError = TEXT("Voxel实体编辑Payload超出Codec容量");
				return false;
			}
			const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<uint8>(Edit.Value.Payload.Num());
			Count.PayloadBytes += Bytes;
			WrittenPayloadBytes += Bytes;
		}
	}
	FVoxelPreparedEdit Prepared;
	uint64 OutputBytes = FMath::Max(VoxelBoundedAllocation::WorkerSharedBytes<TArray<FVoxelPreparedSection>>(),
		VoxelBoundedAllocation::WorkerSharedBytes<TArray<FVoxelSectionPatch>>()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelPreparedSection>(Counts.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelSectionPatch>(Counts.Num()) + DefaultPayloadBytes + WrittenPayloadBytes;
	for (const auto& Pair : Counts)
	{
		OutputBytes += VoxelBoundedAllocation::ArrayBytes<FVoxelSectionCellEdit>(Pair.Value.Cells) +
			VoxelBoundedAllocation::ArrayBytes<FVoxelEntityWrite>(Pair.Value.Cells + Pair.Value.Entities);
	}
	if (!TerrainResources->Acquire(FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, 1, {},
		++NextAllocationSerial), EVoxelTerrainResourcePool::Staged, OutputBytes, Prepared.ResourceLease))
	{
		OutError = TEXT("CapacityBlocked: Runtime编辑Delta输出");
		return false;
	}
	Prepared.Sections.Reserve(Counts.Num());
	const auto PrepareSection = [&](const FIntVector& Key, const FVoxelSection& Source, FVoxelPreparedSection& Target)
	{
		if (Source.CommittedRevision == MAX_uint64)
		{
			OutError = TEXT("Voxel Section内容版本耗尽");
			return false;
		}
		const auto& Count = Counts.FindChecked(Key);
		const int32 OverlayCapacity = FMath::Min(SectionVolume, Source.Overlay.Num() + Count.Cells);
		const int32 EntityCapacity = FMath::Min(SectionVolume, Source.Entities.Num() + Count.Cells + Count.Entities);
		const uint64 Bytes = MutableDataBytes(OverlayCapacity, EntityCapacity,
			EntityCopyBytes(Source.Entities) + Count.PayloadBytes + Count.DefaultPayloadBytes);
		if (!AcquireRuntimeData(Key, Bytes, Source.DataLease.IsValid(), Target.DataLease))
		{
			OutError = TEXT("CapacityBlocked: Runtime编辑数据新旧重叠");
			return false;
		}
		Target.Before = Source.Stamp;
		Target.Blocks.Reserve(SectionVolume);
		Target.Blocks.Append(Source.Blocks);
		Target.Overlay.Reserve(OverlayCapacity);
		Target.Entities.Reserve(EntityCapacity);
		for (const auto& Pair : Source.Overlay) Target.Overlay.Add(Pair.Key, Pair.Value);
		for (const auto& Pair : Source.Entities) Target.Entities.Add(Pair.Key, Pair.Value);
		Target.Patch.Edits.Reserve(Count.Cells);
		Target.Patch.Entities.Reserve(Count.Cells + Count.Entities);
		return true;
	};
	Prepared.TransactionId = FGuid::NewGuid();
	TMap<FIntVector, int32> SectionToPrepared;
	SectionToPrepared.Reserve(Counts.Num());
	TSet<FIntVector> SeenPositions;
	SeenPositions.Reserve(InCells.Num());

	for (const FVoxelCellEdit& Edit : InCells)
	{
		if (SeenPositions.Contains(Edit.Position) || !ValidateState(Edit.Value))
		{
			OutError = TEXT("Voxel edit contains a duplicate position or invalid block state");
			return false;
		}
		SeenPositions.Add(Edit.Position);

		const FIntVector SectionKey = ToSection(Edit.Position);
		int32* PreparedIndex = SectionToPrepared.Find(SectionKey);
		if (!PreparedIndex)
		{
			const FVoxelSection* Section = FindSection(SectionKey);
			if (!Section || Section->Status != EVoxelSectionStatus::DataReady || !Section->BaseBlocks)
			{
				OutError = TEXT("Voxel edit target is not DataReady");
				return false;
			}

			FVoxelPreparedSection NewPrepared;
			if (!PrepareSection(SectionKey, *Section, NewPrepared)) return false;
			NewPrepared.Patch.Section = SectionKey;
			NewPrepared.Patch.FromRevision = Section->CommittedRevision;
			NewPrepared.Patch.ToRevision = Section->CommittedRevision + 1;
			const int32 NewIndex = Prepared.Sections.Add(MoveTemp(NewPrepared));
			SectionToPrepared.Add(SectionKey, NewIndex);
			PreparedIndex = SectionToPrepared.Find(SectionKey);
		}

		FVoxelPreparedSection& Target = Prepared.Sections[*PreparedIndex];
		const int32 CellIndex = ToCellIndex(Edit.Position);
		if (Target.Blocks[CellIndex] != Edit.Expected)
		{
			OutError = TEXT("Voxel edit expected state is stale");
			return false;
		}
		if (Target.Blocks[CellIndex] == Edit.Value)
		{
			continue;
		}

		const FVoxelSection* Source = FindSection(SectionKey);
		const FVoxelBlockState& Natural = (*Source->BaseBlocks)[CellIndex];
		Target.Blocks[CellIndex] = Edit.Value;
		if (Edit.Value == Natural)
		{
			Target.Overlay.Remove(CellIndex);
		}
		else
		{
			Target.Overlay.Add(CellIndex, Edit.Value);
		}

		FVoxelSectionCellEdit PatchEdit;
		PatchEdit.CellIndex = CellIndex;
		PatchEdit.bNatural = Edit.Value == Natural;
		PatchEdit.State = Edit.Value;
		Target.Patch.Edits.Add(PatchEdit);
		if (Edit.Expected.TypeId != Edit.Value.TypeId)
		{
			const FVoxelRuntimeDefinition* Definition = Registry->Find(Edit.Value.TypeId);
			if (Definition->EntityKind || Target.Entities.Contains(CellIndex))
			{
				FVoxelEntityWrite Write;
				Write.CellIndex = CellIndex;
				Write.bRemove = Definition->EntityKind == 0;
				if (Write.bRemove)
				{
					Target.Entities.Remove(CellIndex);
				}
				else
				{
					if (!FVoxelBlockEntityCodec::MakeDefault(Definition->EntityKind, Write.Value, Definition->EntityVariant))
					{
						OutError = TEXT("Edited voxel entity has no registered default codec");
						return false;
					}
					Target.Entities.Add(CellIndex, Write.Value);
				}
				Target.Patch.Entities.Add(MoveTemp(Write));
			}
		}
	}

	TSet<FIntVector> SeenEntityPositions;
	SeenEntityPositions.Reserve(InEntities.Num());
	for (const FVoxelEntityEdit& Edit : InEntities)
	{
		if (SeenEntityPositions.Contains(Edit.Position))
		{
			OutError = TEXT("Voxel entity edit contains a duplicate position");
			return false;
		}
		SeenEntityPositions.Add(Edit.Position);
		const FIntVector SectionKey = ToSection(Edit.Position);
		int32* PreparedIndex = SectionToPrepared.Find(SectionKey);
		if (!PreparedIndex)
		{
			const FVoxelSection* Section = FindSection(SectionKey);
			if (!Section || Section->Status != EVoxelSectionStatus::DataReady || !Section->BaseBlocks)
			{
				OutError = TEXT("Voxel entity edit target is not DataReady");
				return false;
			}
			FVoxelPreparedSection NewPrepared;
			if (!PrepareSection(SectionKey, *Section, NewPrepared)) return false;
			NewPrepared.Patch.Section = SectionKey;
			NewPrepared.Patch.FromRevision = Section->CommittedRevision;
			NewPrepared.Patch.ToRevision = Section->CommittedRevision + 1;
			const int32 NewIndex = Prepared.Sections.Add(MoveTemp(NewPrepared));
			SectionToPrepared.Add(SectionKey, NewIndex);
			PreparedIndex = SectionToPrepared.Find(SectionKey);
		}

		FVoxelPreparedSection& Target = Prepared.Sections[*PreparedIndex];
		const int32 CellIndex = ToCellIndex(Edit.Position);
		FVoxelEntityWrite Write;
		Write.CellIndex = CellIndex;
		Write.bRemove = Edit.bRemove;
		if (!Write.bRemove) Write.Value = Edit.Value;
		if (Write.bRemove)
		{
			Target.Entities.Remove(CellIndex);
		}
		else
		{
			Target.Entities.Add(CellIndex, Write.Value);
		}
		if (FVoxelEntityWrite* Existing = Target.Patch.Entities.FindByPredicate([CellIndex](const FVoxelEntityWrite& InWrite)
		{
			return InWrite.CellIndex == CellIndex;
		}))
		{
			*Existing = MoveTemp(Write);
		}
		else
		{
			Target.Patch.Entities.Add(MoveTemp(Write));
		}
	}

	for (auto& Candidate : Prepared.Sections)
	{
		if (!Candidate.Patch.Edits.IsEmpty() || !Candidate.Patch.Entities.IsEmpty() || !Candidate.DataLease) continue;
		auto Retired = VoxelBoundedAllocation::MakeWorkerShared<VoxelRuntimeStorage::FRetiredData>(Candidate.DataLease);
		Retired->Blocks = MoveTemp(Candidate.Blocks);
		Retired->Overlay = MoveTemp(Candidate.Overlay);
		Retired->Entities = MoveTemp(Candidate.Entities);
		Candidate.DataLease.Reset();
	}
	Prepared.Sections.RemoveAll([](const FVoxelPreparedSection& InSection)
	{
		return InSection.Patch.Edits.IsEmpty() && InSection.Patch.Entities.IsEmpty();
	});

	for (const FVoxelPreparedSection& Section : Prepared.Sections)
	{
		if (!ValidateSection(Section.Blocks, Section.Overlay, Section.Entities, OutError))
		{
			return false;
		}
	}

	OutPrepared = MoveTemp(Prepared);
	OutError.Reset();
	return true;
}

bool FVoxelWorldRuntime::CommitPreparedEdit(
	FVoxelPreparedEdit&& InPrepared,
	FVoxelEditBatch& OutBatch,
	FString& OutError)
{
	check(IsInGameThread());
	if (!bAuthority)
	{
		OutError = TEXT("Only the authoritative voxel runtime may commit local edits");
		return false;
	}
	if (InPrepared.Sections.IsEmpty())
	{
		OutBatch = FVoxelEditBatch();
		InPrepared.ResourceLease.Reset();
		OutError.Reset();
		return true;
	}

	for (const FVoxelPreparedSection& PreparedSection : InPrepared.Sections)
	{
		if (!IsCurrentStamp(PreparedSection.Patch.Section, PreparedSection.Before))
		{
			OutError = TEXT("Voxel prepared edit stamp is stale");
			return false;
		}
		const FVoxelSection* Section = FindSection(PreparedSection.Patch.Section);
		if (!Section || Section->CommittedRevision != PreparedSection.Patch.FromRevision)
		{
			OutError = TEXT("Voxel prepared edit revision is stale");
			return false;
		}
	}

	if (!InPrepared.Sections.IsEmpty() && AuthorityCommitCut == MAX_uint64)
	{
		OutError = TEXT("Voxel事务提交序号耗尽");
		return false;
	}
	if (!InPrepared.ResourceLease || !InPrepared.ResourceLease->Transfer(EVoxelTerrainResourcePool::Input))
	{
		OutError = TEXT("CapacityBlocked: Runtime已准备编辑Delta输入接纳");
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
	const uint64 ScratchBytes = VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>>(InPrepared.Sections.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FIntVector>(InPrepared.Sections.Num());
	if (ScratchBytes && !TerrainResources->Acquire(FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, 1, {},
		++NextAllocationSerial), EVoxelTerrainResourcePool::Working, ScratchBytes, ScratchLease))
	{
		OutError = TEXT("CapacityBlocked: Runtime编辑Journal事务工作区");
		return false;
	}
	TArray<TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>> Journals;
	Journals.Reserve(InPrepared.Sections.Num());
	for (const FVoxelPreparedSection& Candidate : InPrepared.Sections)
	{
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Journal;
		if (!PrepareJournal(Candidate.Patch.Section, Candidate.Patch.ToRevision, Candidate.Overlay, Candidate.Entities, Journal, OutError)) return false;
		Journals.Add(MoveTemp(Journal));
	}
	FVoxelEditBatch Batch;
	Batch.TransactionId = InPrepared.TransactionId;
	Batch.Sections.Reserve(InPrepared.Sections.Num());
	TArray<FIntVector> SectionKeys;
	SectionKeys.Reserve(InPrepared.Sections.Num());
	for (const auto& Candidate : InPrepared.Sections) SectionKeys.Add(Candidate.Patch.Section);
	if (!ReserveOverlaySequenceKeys(SectionKeys))
	{
		OutError = TEXT("CapacityBlocked: Runtime编辑Region序号目录");
		return false;
	}
	const auto SectionAt = [&InPrepared](int32 Index) { return InPrepared.Sections[Index].Patch.Section; };
	FVoxelPreparedChangeInvalidations Changes;
	if (!ChangeIndex.PrepareModified(InPrepared.Sections.Num(), SectionAt, OutError)) return false;
	const auto ModifiedLease = ChangeIndex.GetStorageLease();
	const auto DirectoryLease = ChangeIndex.GetBaselineLease();
	FVoxelGenerationRequiredSourceScope ModifiedSource(TerrainResources.Get(), ModifiedLease.Get());
	FVoxelGenerationRequiredSourceScope DirectorySource(TerrainResources.Get(), DirectoryLease.Get());
	if (!ChangeHierarchy.PrepareInvalidations(InPrepared.Sections.Num(), SectionAt, Changes, OutError) ||
		!ChangeHierarchy.CommitInvalidations(MoveTemp(Changes), SectionAt, OutError)) return false;
	for (const auto& Candidate : InPrepared.Sections)
	{
		const bool bIndexed = ChangeIndex.SetModified(Candidate.Patch.Section, !Candidate.Overlay.IsEmpty() || !Candidate.Entities.IsEmpty(), OutError);
		checkf(bIndexed, TEXT("Prepared change-index capacity must cover the stable edit source"));
	}
	Batch.ResourceLease = MoveTemp(InPrepared.ResourceLease);
	int32 JournalIndex = 0;
	for (FVoxelPreparedSection& PreparedSection : InPrepared.Sections)
	{
		FVoxelSection* Section = FindSection(PreparedSection.Patch.Section);
		PublishPatchCandidate(
			*Section,
			PreparedSection.Patch,
			MoveTemp(PreparedSection.Blocks),
			MoveTemp(PreparedSection.Overlay),
			MoveTemp(PreparedSection.Entities), MoveTemp(Journals[JournalIndex++]), MoveTemp(PreparedSection.DataLease));
		Batch.Sections.Add(MoveTemp(PreparedSection.Patch));
	}

	AdvanceOverlaySequences(Batch.Sections);
	InPrepared.Sections.Empty();
	uint64 BatchBytes = VoxelBoundedAllocation::WorkerSharedBytes<TArray<FVoxelSectionPatch>>() + VoxelGenerationAllocation::AllocatedBytes(Batch.Sections);
	for (const auto& Patch : Batch.Sections)
	{
		BatchBytes += VoxelGenerationAllocation::AllocatedBytes(Patch.Edits) + VoxelGenerationAllocation::AllocatedBytes(Patch.Entities);
		for (const auto& Write : Patch.Entities) BatchBytes += VoxelGenerationAllocation::AllocatedBytes(Write.Value.Payload);
	}
	check(BatchBytes <= Batch.ResourceLease->Bytes());
	verify(Batch.ResourceLease->Resize(BatchBytes));
	OutBatch = MoveTemp(Batch);
	if (!OutBatch.Sections.IsEmpty())
	{
		check(AuthorityCommitCut < MAX_uint64);
		++AuthorityCommitCut;
	}
	OutError.Reset();
	return true;
}

bool FVoxelWorldRuntime::ApplyPatch(const FVoxelSectionPatch& InPatch, FString& OutError)
{
	check(IsInGameThread());
	if (AuthorityCommitCut == MAX_uint64)
	{
		OutError = TEXT("Voxel事务提交序号耗尽");
		return false;
	}
	FVoxelSection* Section = FindSection(InPatch.Section);
	if (!Section)
	{
		OutError = TEXT("Voxel patch target does not exist");
		return false;
	}

	TArray<FVoxelBlockState> CandidateBlocks;
	TMap<int32, FVoxelBlockState> CandidateOverlay;
	TMap<int32, FVoxelBlockEntityState> CandidateEntities;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DataLease;
	if (!BuildPatchCandidate(*Section, InPatch, CandidateBlocks, CandidateOverlay, CandidateEntities, OutError, DataLease))
	{
		return false;
	}

	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Journal;
	if (!PrepareJournal(InPatch.Section, InPatch.ToRevision, CandidateOverlay, CandidateEntities, Journal, OutError)) return false;
	if (!ReserveOverlaySequences(MakeArrayView(&InPatch, 1)))
	{
		OutError = TEXT("CapacityBlocked: Runtime补丁Region序号目录");
		return false;
	}
	const auto SectionAt = [&InPatch](int32) { return InPatch.Section; };
	FVoxelPreparedChangeInvalidations Changes;
	if (!ChangeIndex.PrepareModified(1, SectionAt, OutError)) return false;
	const auto ModifiedLease = ChangeIndex.GetStorageLease();
	const auto DirectoryLease = ChangeIndex.GetBaselineLease();
	FVoxelGenerationRequiredSourceScope ModifiedSource(TerrainResources.Get(), ModifiedLease.Get());
	FVoxelGenerationRequiredSourceScope DirectorySource(TerrainResources.Get(), DirectoryLease.Get());
	if (!ChangeHierarchy.PrepareInvalidations(1, SectionAt, Changes, OutError) ||
		!ChangeHierarchy.CommitInvalidations(MoveTemp(Changes), SectionAt, OutError)) return false;
	const bool bIndexed = ChangeIndex.SetModified(InPatch.Section, !CandidateOverlay.IsEmpty() || !CandidateEntities.IsEmpty(), OutError);
	checkf(bIndexed, TEXT("Prepared change-index capacity must cover the stable patch source"));
	PublishPatchCandidate(
		*Section,
		InPatch,
		MoveTemp(CandidateBlocks),
		MoveTemp(CandidateOverlay),
		MoveTemp(CandidateEntities), MoveTemp(Journal), MoveTemp(DataLease));
	AdvanceOverlaySequences(MakeArrayView(&InPatch, 1));
	check(AuthorityCommitCut < MAX_uint64);
	++AuthorityCommitCut;
	OutError.Reset();
	return true;
}

bool FVoxelWorldRuntime::ApplyRemotePatchBatch(
	const FVoxelEditBatch& InBatch,
	FString& OutError)
{
	check(IsInGameThread());
	if (bAuthority)
	{
		OutError = TEXT("Authoritative voxel runtime cannot apply a remote patch batch");
		return false;
	}
	if (AuthorityCommitCut == MAX_uint64)
	{
		OutError = TEXT("Voxel事务提交序号耗尽");
		return false;
	}

	struct FCandidate
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DataLease;
		FVoxelSection* Section = nullptr;
		const FVoxelSectionPatch* Patch = nullptr;
		TArray<FVoxelBlockState> Blocks;
		TMap<int32, FVoxelBlockState> Overlay;
		TMap<int32, FVoxelBlockEntityState> Entities;
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Journal;
	};

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
	const uint64 ScratchBytes = VoxelBoundedAllocation::ArrayBytes<FCandidate>(InBatch.Sections.Num()) +
		VoxelBoundedAllocation::SetBytes<FIntVector>(InBatch.Sections.Num());
	if (ScratchBytes && !TerrainResources->Acquire(FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, 1, {},
		++NextAllocationSerial), EVoxelTerrainResourcePool::Working, ScratchBytes, ScratchLease))
	{
		OutError = TEXT("CapacityBlocked: Runtime远端批次候选工作区");
		return false;
	}
	TArray<FCandidate> Candidates;
	Candidates.Reserve(InBatch.Sections.Num());
	TSet<FIntVector> SeenSections;
	SeenSections.Reserve(InBatch.Sections.Num());
	for (const FVoxelSectionPatch& Patch : InBatch.Sections)
	{
		if (SeenSections.Contains(Patch.Section))
		{
			OutError = TEXT("Voxel remote patch batch contains a duplicate section");
			return false;
		}
		SeenSections.Add(Patch.Section);

		FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Section = FindSection(Patch.Section);
		Candidate.Patch = &Patch;
		if (!Candidate.Section ||
			!BuildPatchCandidate(
				*Candidate.Section,
				Patch,
				Candidate.Blocks,
				Candidate.Overlay,
				Candidate.Entities,
				OutError, Candidate.DataLease))
		{
			return false;
		}
		if (!PrepareJournal(Patch.Section, Patch.ToRevision, Candidate.Overlay, Candidate.Entities, Candidate.Journal, OutError)) return false;
	}

	const auto SectionAt = [&Candidates](int32 Index) { return Candidates[Index].Patch->Section; };
	FVoxelPreparedChangeInvalidations Changes;
	if (!ChangeIndex.PrepareModified(Candidates.Num(), SectionAt, OutError)) return false;
	const auto ModifiedLease = ChangeIndex.GetStorageLease();
	const auto DirectoryLease = ChangeIndex.GetBaselineLease();
	FVoxelGenerationRequiredSourceScope ModifiedSource(TerrainResources.Get(), ModifiedLease.Get());
	FVoxelGenerationRequiredSourceScope DirectorySource(TerrainResources.Get(), DirectoryLease.Get());
	if (!ChangeHierarchy.PrepareInvalidations(Candidates.Num(), SectionAt, Changes, OutError) ||
		!ChangeHierarchy.CommitInvalidations(MoveTemp(Changes), SectionAt, OutError)) return false;
	for (const auto& Candidate : Candidates)
	{
		const bool bIndexed = ChangeIndex.SetModified(Candidate.Patch->Section, !Candidate.Overlay.IsEmpty() || !Candidate.Entities.IsEmpty(), OutError);
		checkf(bIndexed, TEXT("Prepared change-index capacity must cover the stable remote source"));
	}
	for (FCandidate& Candidate : Candidates)
	{
		PublishPatchCandidate(
			*Candidate.Section,
			*Candidate.Patch,
			MoveTemp(Candidate.Blocks),
			MoveTemp(Candidate.Overlay),
			MoveTemp(Candidate.Entities), MoveTemp(Candidate.Journal), MoveTemp(Candidate.DataLease));
	}

	check(AuthorityCommitCut < MAX_uint64);
	++AuthorityCommitCut;
	OutError.Reset();
	return true;
}

void FVoxelWorldRuntime::SetRemoteSectionValidator(TFunction<bool(const FIntVector&, uint64)> InValidator)
{
	RemoteSectionValidator = MoveTemp(InValidator);
}

bool FVoxelWorldRuntime::InvalidateRemoteSection(const FIntVector& InSection, FString& OutError)
{
	check(IsInGameThread());
	if (bAuthority) { OutError.Reset(); return true; }
	uint64 Revision = 0;
	if (!ChangeHierarchy.InvalidateSection(InSection, Revision, OutError)) return false;
	if (auto* Section = FindSection(InSection); Section && Section->Status == EVoxelSectionStatus::DataReady)
	{
		NotifySectionEvent(InSection, EVoxelRuntimeSectionEvent::IdentityChanging);
		check(NextToken < MAX_uint64);
		Section->Stamp.Token = NextToken++;
		Section->Status = EVoxelSectionStatus::BaseReady;
		Section->bCollisionDirty = Section->bFineMeshDirty = true;
		NotifySectionEvent(InSection, EVoxelRuntimeSectionEvent::Allocated);
	}
	OutError.Reset();
	return true;
}

bool FVoxelWorldRuntime::RemoveSection(
	const FIntVector& InSection,
	const bool bDiscardModified)
{
	check(IsInGameThread());
	FVoxelSection* Section = FindSection(InSection);
	if (!Section)
	{
		return true;
	}

	if (Section->PinCount.Load() > 0 ||
		(!bDiscardModified && bAuthority && GetPersistedRevision(InSection) != Section->CommittedRevision &&
			(!FindCommittedOverlay(InSection) || GetCommittedOverlayRevision(InSection) != Section->CommittedRevision)))
	{
		return false;
	}

	NotifySectionEvent(InSection, EVoxelRuntimeSectionEvent::Removing);
	Section->Status = EVoxelSectionStatus::Unloading;
	PendingResidentPromotion.Remove(InSection);
	ResidencyOrder.RemoveAt(Section->ResidencyOrderIndex);
	Sections.Remove(InSection);
	ChangeHierarchy.ReleaseSection(InSection);
	return true;
}

void FVoxelWorldRuntime::MarkCommitted(
	const FIntVector& InSection,
	const uint64 InRevision)
{
	CommittedJournal.MarkPersisted(InSection, InRevision);
	if (FVoxelSection* Section = FindSection(InSection))
	{
		if (InRevision != Section->CommittedRevision)
		{
			return;
		}
		Section->PersistedRevision = InRevision;
		NotifySectionEvent(InSection, EVoxelRuntimeSectionEvent::Persisted);
	}
}

uint64 FVoxelWorldRuntime::GetPersistedRevision(const FIntVector& InSection) const
{
	const auto* Section = FindSection(InSection);
	return FMath::Max(CommittedJournal.PersistedRevision(InSection), Section ? Section->PersistedRevision : uint64(0));
}

uint64 FVoxelWorldRuntime::GetCommittedOverlayRevision(const FIntVector& InSection) const
{
	return CommittedJournal.CommittedRevision(InSection);
}

FVoxelCommittedJournalCapture FVoxelWorldRuntime::CaptureJournal() const
{
	check(IsInGameThread());
	return CommittedJournal.Capture(AuthorityCommitCut);
}

bool FVoxelWorldRuntime::CompleteJournalSave(const FVoxelCommittedJournalCapture& InCapture)
{
	// 原切点的落盘证明原子提交；已预授予的脏树通知游标按共享帧预算推进。
	return CommittedJournal.AcceptSave(InCapture);
}

bool FVoxelWorldRuntime::CanCompleteJournalSave(const FVoxelCommittedJournalCapture& InCapture) const
{
	return CommittedJournal.CanAcceptSave(InCapture);
}

bool FVoxelWorldRuntime::HasPendingJournalSaveNotifications() const
{
	return CommittedJournal.HasSaveNotifications();
}

void FVoxelWorldRuntime::AdvanceJournalRetirement(const int32 InMaximumNodes, const double InDeadline,
	FVoxelTerrainFrameContext* InFrameContext)
{
	for (int32 Step = 0; Step < InMaximumNodes; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		const bool bNotification = bNextJournalNotification && CommittedJournal.HasSaveNotifications();
		double& Cost = bNotification ? JournalNotificationCostMilliseconds : JournalRetirementCostMilliseconds;
		const bool bWithinDeadline = Started < InDeadline;
		const bool bStarted = bWithinDeadline && (!InFrameContext || InFrameContext->GetPublishBudget().TryStart(
			EVoxelPublishPhase::Retire, Started, Cost));
		if (!bStarted)
		{
			if (!InFrameContext || !InFrameContext->IsOverdueOpportunityAvailable()) break;
			InFrameContext->ConsumeOverdueOpportunity();
		}
		bNextJournalNotification = !bNotification;
		if (bNotification)
		{
			FIntVector Key;
			uint64 Revision = 0;
			CommittedJournal.AdvanceSaveNotifications(Key, Revision);
			if (Revision > 0)
			{
				if (const auto* Section = FindSection(Key); Section && Section->CommittedRevision <= Revision &&
					GetPersistedRevision(Key) >= Section->CommittedRevision)
					NotifySectionEvent(Key, EVoxelRuntimeSectionEvent::Persisted);
			}
		}
		else CommittedJournal.AdvanceRetirement(1, bStarted ? InDeadline : MAX_dbl,
			[this](const FIntVector& InSection) { return Sections.Contains(InSection); });
		const double Finished = FPlatformTime::Seconds();
		Cost = FMath::Lerp(Cost, (Finished - Started) * 1000.0, 0.15);
		if (InFrameContext) InFrameContext->GetPublishBudget().Finish(EVoxelPublishPhase::Retire, Started, Finished);
		if (!bStarted) break;
	}
}

bool FVoxelWorldRuntime::IsJournalRetirementIdle() const
{
	return CommittedJournal.IsRetirementIdle();
}

bool FVoxelWorldRuntime::HasUnpersistedChanges() const
{
	return bAuthority && AuthorityCommitCut > CommittedJournal.GetPersistedCut();
}

bool FVoxelWorldRuntime::IsCurrentStamp(
	const FIntVector& InSection,
	const FVoxelSectionStamp& InStamp) const
{
	const FVoxelSection* Section = FindSection(InSection);
	return Section &&
		Section->Status != EVoxelSectionStatus::Unloading &&
		Section->Stamp == InStamp;
}

int32 FVoxelWorldRuntime::NumSections() const
{
	return Sections.Num();
}

uint64 FVoxelWorldRuntime::Epoch() const
{
	return WorldEpoch;
}

bool FVoxelWorldRuntime::IsServer() const
{
	return bAuthority;
}

FVoxelChangeHierarchy& FVoxelWorldRuntime::GetChangeHierarchy()
{
	return ChangeHierarchy;
}

const FVoxelChangeHierarchy& FVoxelWorldRuntime::GetChangeHierarchy() const
{
	return ChangeHierarchy;
}

FVoxelChangeIndex& FVoxelWorldRuntime::GetChangeIndex()
{
	return ChangeIndex;
}

const FVoxelChangeIndex& FVoxelWorldRuntime::GetChangeIndex() const
{
	return ChangeIndex;
}

bool FVoxelWorldRuntime::ValidateState(const FVoxelBlockState& InState) const
{
	return Registry->IsValid(InState);
}

bool FVoxelWorldRuntime::ValidateSection(
	const TArray<FVoxelBlockState>& InBlocks,
	const TMap<int32, FVoxelBlockState>& InOverlay,
	const TMap<int32, FVoxelBlockEntityState>& InEntities,
	FString& OutError) const
{
	if (InBlocks.Num() != SectionVolume || InOverlay.Num() > SectionVolume)
	{
		OutError = TEXT("Voxel section cell count or overlay size is invalid");
		return false;
	}

	int32 RequiredEntities = 0;
	for (const FVoxelBlockState& State : InBlocks)
	{
		if (!ValidateState(State))
		{
			OutError = TEXT("Voxel section contains an invalid block state");
			return false;
		}
		RequiredEntities += Registry->Find(State.TypeId)->EntityKind != 0 ? 1 : 0;
	}
	if (InEntities.Num() != RequiredEntities)
	{
		OutError = TEXT("Voxel section entity count does not match its stateful blocks");
		return false;
	}
	for (const TPair<int32, FVoxelBlockEntityState>& Entity : InEntities)
	{
		const FVoxelRuntimeDefinition* Definition = InBlocks.IsValidIndex(Entity.Key) ? Registry->Find(InBlocks[Entity.Key].TypeId) : nullptr;
		if (!Definition || !Definition->EntityKind || Definition->EntityKind != Entity.Value.Kind ||
			!FVoxelBlockEntityCodec::Validate(Entity.Value) || (bAuthority && FInventoryVoxelEntityCodec::IsPublic(Entity.Value)))
		{
			OutError = TEXT("Voxel entity kind or payload does not match its owning block");
			return false;
		}
	}

	for (const TPair<int32, FVoxelBlockState>& Pair : InOverlay)
	{
		if (!InBlocks.IsValidIndex(Pair.Key) || InBlocks[Pair.Key] != Pair.Value)
		{
			OutError = TEXT("Voxel overlay does not match final section data");
			return false;
		}
	}

	return true;
}

bool FVoxelWorldRuntime::BuildPatchCandidate(
	const FVoxelSection& InSection,
	const FVoxelSectionPatch& InPatch,
	TArray<FVoxelBlockState>& OutBlocks,
	TMap<int32, FVoxelBlockState>& OutOverlay,
	TMap<int32, FVoxelBlockEntityState>& OutEntities,
	FString& OutError,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutDataLease) const
{
	if (InSection.Status != EVoxelSectionStatus::DataReady)
	{
		OutError = TEXT("Voxel patch target is not DataReady");
		return false;
	}
	if (InPatch.FromRevision == MAX_uint64 || InSection.CommittedRevision != InPatch.FromRevision ||
		InPatch.ToRevision != InPatch.FromRevision + 1)
	{
		OutError = TEXT("Voxel patch revision mismatch");
		return false;
	}
	if (!InSection.BaseBlocks || InSection.BaseBlocks->Num() != InSection.Blocks.Num())
	{
		OutError = TEXT("Voxel patch target has no valid natural base");
		return false;
	}

	if (InPatch.Edits.Num() > SectionVolume || InPatch.Entities.Num() > SectionVolume)
	{
		OutError = TEXT("Voxel补丁的格子或实体数量超过Section范围");
		return false;
	}
	uint64 PayloadBytes = EntityCopyBytes(InSection.Entities);
	for (const auto& Write : InPatch.Entities)
	{
		if (!Write.bRemove && Write.Value.Payload.Num() > FVoxelDeltaCodec::MaximumEntityPayloadBytes)
		{
			OutError = TEXT("Voxel补丁实体Payload超出Codec容量");
			return false;
		}
		if (!Write.bRemove) PayloadBytes += VoxelBoundedAllocation::ArrayBytes<uint8>(Write.Value.Payload.Num());
	}
	const int32 OverlayCapacity = FMath::Min(SectionVolume, InSection.Overlay.Num() + InPatch.Edits.Num());
	const int32 EntityCapacity = FMath::Min(SectionVolume, InSection.Entities.Num() + InPatch.Entities.Num());
	const uint64 DataBytes = MutableDataBytes(OverlayCapacity, EntityCapacity, PayloadBytes);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
	const uint64 ScratchBytes = VoxelBoundedAllocation::SetBytes<int32>(InPatch.Edits.Num()) +
		VoxelBoundedAllocation::SetBytes<int32>(InPatch.Entities.Num()) + EntityCodecScratchBytes;
	if (!AcquireRuntimeData(InPatch.Section, DataBytes, InSection.DataLease.IsValid(), OutDataLease) ||
		!TerrainResources->Acquire(FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, InSection.Stamp.Token,
			InPatch.Section, ++NextAllocationSerial), EVoxelTerrainResourcePool::Working, ScratchBytes, ScratchLease))
	{
		OutError = TEXT("CapacityBlocked: Runtime补丁候选与Codec工作区");
		return false;
	}
	OutBlocks.Reserve(SectionVolume);
	OutBlocks.Append(InSection.Blocks);
	OutOverlay.Reserve(OverlayCapacity);
	OutEntities.Reserve(EntityCapacity);
	for (const auto& Pair : InSection.Overlay) OutOverlay.Add(Pair.Key, Pair.Value);
	for (const auto& Pair : InSection.Entities) OutEntities.Add(Pair.Key, Pair.Value);
	TSet<int32> SeenCells;
	SeenCells.Reserve(InPatch.Edits.Num());
	for (const FVoxelSectionCellEdit& Edit : InPatch.Edits)
	{
		if (!OutBlocks.IsValidIndex(Edit.CellIndex) || SeenCells.Contains(Edit.CellIndex))
		{
			OutError = TEXT("Voxel patch cell index is invalid or duplicated");
			return false;
		}
		SeenCells.Add(Edit.CellIndex);

		const FVoxelBlockState& Natural = (*InSection.BaseBlocks)[Edit.CellIndex];
		const FVoxelBlockState FinalState = Edit.bNatural ? Natural : Edit.State;
		if (!ValidateState(FinalState))
		{
			OutError = TEXT("Voxel patch contains an invalid block state");
			return false;
		}
		OutBlocks[Edit.CellIndex] = FinalState;
		if (FinalState == Natural)
		{
			OutOverlay.Remove(Edit.CellIndex);
		}
		else
		{
			OutOverlay.Add(Edit.CellIndex, FinalState);
		}
	}
	TSet<int32> SeenEntities;
	SeenEntities.Reserve(InPatch.Entities.Num());
	for (const FVoxelEntityWrite& Write : InPatch.Entities)
	{
		if (!OutBlocks.IsValidIndex(Write.CellIndex) || SeenEntities.Contains(Write.CellIndex))
		{
			OutError = TEXT("Voxel patch entity index is invalid or duplicated");
			return false;
		}
		SeenEntities.Add(Write.CellIndex);
		if (Write.bRemove)
		{
			OutEntities.Remove(Write.CellIndex);
		}
		else
		{
			OutEntities.Add(Write.CellIndex, Write.Value);
		}
	}

	return ValidateSection(OutBlocks, OutOverlay, OutEntities, OutError);
}

void FVoxelWorldRuntime::PublishPatchCandidate(
	FVoxelSection& InSection,
	const FVoxelSectionPatch& InPatch,
	TArray<FVoxelBlockState>&& InBlocks,
	TMap<int32, FVoxelBlockState>&& InOverlay,
	TMap<int32, FVoxelBlockEntityState>&& InEntities,
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> InJournal,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InDataLease)
{
	RetireRuntimeData(InSection);
	InSection.Blocks = MoveTemp(InBlocks);
	InSection.Overlay = MoveTemp(InOverlay);
	InSection.Entities = MoveTemp(InEntities);
	FinishRuntimeData(InSection, InPatch.Section, MoveTemp(InDataLease));
	InSection.CommittedRevision = InPatch.ToRevision;
	InSection.bCollisionDirty = true;
	InSection.bFineMeshDirty = true;
	// 调用方已在正文交换前授予索引容量并提交不可变change根。
	check(InJournal && InJournal->Revision == InPatch.ToRevision);
	RetainJournal(InPatch.Section, MoveTemp(InJournal));
}

void FVoxelWorldRuntime::BindTerrainResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources)
{
	check(IsInGameThread());
	check(Sections.IsEmpty() && !StorageLease && OverlayRegionSequences.IsEmpty());
	TerrainResources = InResources;
	CommittedJournal.BindResources(InResources);
	const auto Control = FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, 1, {}, 1);
	ChangeIndex.BindResources(InResources, Control);
	ChangeHierarchy.BindResources(InResources, Control);
}

bool FVoxelWorldRuntime::PrepareJournal(const FIntVector& InSection, const uint64 InRevision,
	const TMap<int32, FVoxelBlockState>& InOverlay,
	const TMap<int32, FVoxelBlockEntityState>& InEntities,
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>& OutJournal, FString& OutError,
	const bool bInPersisted) const
{
	if (InRevision == MAX_uint64)
	{
		OutError = TEXT("Voxel Journal内容版本耗尽");
		return false;
	}
	// 固定容量的 4096 格编辑目录在复制之前申请；旧版本由原读者继续持有。
	uint64 EntityPayloadBytes = 0;
	for (const auto& Pair : InEntities) EntityPayloadBytes += VoxelBoundedAllocation::ArrayBytes<uint8>(Pair.Value.Payload.Num());
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlaySnapshot>() +
		VoxelBoundedAllocation::MapBytes<int32, FVoxelBlockState>(InOverlay.Num()) +
		VoxelBoundedAllocation::MapBytes<int32, FVoxelBlockEntityState>(InEntities.Num()) + EntityPayloadBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> IndexLease;
	if (!CommittedJournal.PreparePublication(InSection, InRevision, IndexLease))
	{
		OutError = TEXT("CapacityBlocked: 权威日志持久空间索引缺少写时复制配额");
		return false;
	}
	if (TerrainResources && !TerrainResources->Acquire(
		FVoxelGenerationConsumerKey::Section(WorldEpoch, JournalOwnerId, 1, InSection, InRevision + 1),
		EVoxelTerrainResourcePool::Resident, Bytes, Lease))
	{
		OutError = TEXT("CapacityBlocked: 权威编辑日志缺少新旧版本重叠配额");
		return false;
	}
	auto Journal = VoxelBoundedAllocation::MakeWorkerShared<FVoxelOverlaySnapshot>(Lease);
	Journal->Section = InSection;
	Journal->Revision = InRevision;
	Journal->BasePersistedRevision = bInPersisted ? InRevision : GetPersistedRevision(InSection);
	Journal->IndexLease = MoveTemp(IndexLease);
	Journal->Blocks.Reserve(InOverlay.Num());
	Journal->Entities.Reserve(InEntities.Num());
	for (const auto& Pair : InOverlay) Journal->Blocks.Add(Pair.Key, Pair.Value);
	for (const auto& Pair : InEntities) Journal->Entities.Add(Pair.Key, Pair.Value);
	Journal->ResourceLease = MoveTemp(Lease);
	if (Journal->ResourceLease)
	{
		uint64 Actual = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlaySnapshot>() +
			VoxelGenerationAllocation::AllocatedBytes(Journal->Blocks) + VoxelGenerationAllocation::AllocatedBytes(Journal->Entities);
		for (const auto& Pair : Journal->Entities) Actual += VoxelGenerationAllocation::AllocatedBytes(Pair.Value.Payload);
		check(Actual <= Bytes);
		verify(Journal->ResourceLease->Resize(Actual));
	}
	OutJournal = MoveTemp(Journal);
	return true;
}

TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> FVoxelWorldRuntime::FindCommittedOverlay(const FIntVector& InSection) const
{
	return CommittedJournal.Find(InSection);
}

uint64 FVoxelWorldRuntime::GetAuthorityCommitCut() const
{
	return AuthorityCommitCut;
}

namespace
{
	FIntVector JournalRegionOf(const FIntVector& InSection)
	{
		return FIntVector(VoxelGeneration::FloorDivide(InSection.X, 8),
			VoxelGeneration::FloorDivide(InSection.Y, 8), VoxelGeneration::FloorDivide(InSection.Z, 8));
	}
}

void FVoxelWorldRuntime::RetainJournal(const FIntVector& InSection,
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> InJournal, bool bInNotify)
{
	check(InJournal && InJournal->Section == InSection);
	CommittedJournal.Publish(MoveTemp(InJournal));
	if(bInNotify) NotifySectionEvent(InSection, EVoxelRuntimeSectionEvent::Committed);
}

void FVoxelWorldRuntime::GatherCommittedOverlaySections(const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutSections) const
{
	VisitCommittedOverlaySections(InBounds, [&OutSections](const FIntVector& InSection) { OutSections.AddUnique(InSection); });
}

void FVoxelWorldRuntime::VisitCommittedOverlaySections(const FVoxelGenerationBounds& InBounds,
	TFunctionRef<void(const FIntVector&)> InVisit) const
{
	CommittedJournal.Visit(InBounds, InVisit);
}
void FVoxelWorldRuntime::AdvanceOverlaySequences(TConstArrayView<FVoxelSectionPatch> InPatches)
{
	if (!bAuthority) return;
	check(AuthorityCommitCut < MAX_uint64);
	const uint64 Cut = AuthorityCommitCut + 1;
	for (const auto& Patch : InPatches)
	{
		auto& Sequence = OverlayRegionSequences.FindChecked(JournalRegionOf(Patch.Section));
		if (Sequence.LastCommitCut == Cut) continue;
		check(Sequence.Value < MAX_uint64);
		++Sequence.Value;
		Sequence.LastCommitCut = Cut;
	}
}

uint64 FVoxelWorldRuntime::GetOverlayRegionSequence(const FIntVector& InRegion) const
{
	const auto* Sequence = OverlayRegionSequences.Find(InRegion);
	return Sequence ? Sequence->Value : 0;
}

uint64 FVoxelWorldRuntime::GetContentRevision() const
{
	return ChangeHierarchy.GetContentRevision();
}



bool FVoxelWorldRuntime::PrepareInventorySections(TConstArrayView<FVoxelInventorySectionTarget> InTargets,
    const FGuid& InTransactionId, bool bInRecovery, FVoxelPreparedInventoryCommit& OutPrepared, FString& OutError)
{
    check(IsInGameThread());
    if(!bAuthority || !InTransactionId.IsValid() || InTargets.Num() > 64 || OutPrepared.bPrepared ||
        !OutPrepared.Sections.IsEmpty() || OutPrepared.ScratchLease || OutPrepared.Batch.ResourceLease ||
        NextAllocationSerial > MAX_uint64 - uint64(InTargets.Num()) * 2 - 4)
    {
        OutError = TEXT("Inventory.VoxelPrepareInvalid");
        return false;
    }
    OutError.Reset();
    OutPrepared.WorldEpoch = WorldEpoch;
    OutPrepared.HierarchyRevision = ChangeHierarchy.GetContentRevision();
    OutPrepared.Batch.TransactionId = InTransactionId;
    uint64 PatchBytes = VoxelBoundedAllocation::WorkerSharedBytes<TArray<FVoxelSectionPatch>>() +
        VoxelBoundedAllocation::ArrayBytes<FVoxelSectionPatch>(InTargets.Num());
    for(const FVoxelInventorySectionTarget& Target : InTargets)
    {
        if(Target.After.Blocks.Num() > SectionVolume || Target.After.Entities.Num() > FVoxelDeltaCodec::MaximumEntityCount)
        {
            OutError = TEXT("Inventory.VoxelSectionBounds");
            CancelInventorySections(OutPrepared);
            return false;
        }
        for(const auto& Entity : Target.After.Entities)
        {
            if(Entity.Value.Payload.Num() > FVoxelDeltaCodec::MaximumEntityPayloadBytes)
            {
                OutError = TEXT("Inventory.VoxelEntityBounds");
                CancelInventorySections(OutPrepared);
                return false;
            }
        }
        PatchBytes += VoxelBoundedAllocation::ArrayBytes<FVoxelSectionCellEdit>(SectionVolume);
        PatchBytes += VoxelBoundedAllocation::ArrayBytes<FVoxelEntityWrite>(SectionVolume);
        PatchBytes += EntityCopyBytes(Target.After.Entities);
    }
    const uint64 ScratchBytes = VoxelBoundedAllocation::WorkerSharedBytes<TArray<FVoxelPreparedInventorySection>>() +
        VoxelBoundedAllocation::ArrayBytes<FVoxelPreparedInventorySection>(InTargets.Num());
    const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, RuntimeOwnerId, 1, {}, ++NextAllocationSerial);
    if(!TerrainResources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, FMath::Max<uint64>(ScratchBytes, 1), OutPrepared.ScratchLease) ||
        !TerrainResources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, FMath::Max<uint64>(PatchBytes, 1), OutPrepared.Batch.ResourceLease))
    {
        OutError = TEXT("CapacityBlocked: Inventory体素事务冻结工作区");
        CancelInventorySections(OutPrepared);
        return false;
    }
    OutPrepared.Sections.Reserve(InTargets.Num());
    OutPrepared.Batch.Sections.Reserve(InTargets.Num());
    auto Fail = [&]()
    {
        CancelInventorySections(OutPrepared);
        return false;
    };

    for(int32 TargetIndex = 0; TargetIndex < InTargets.Num(); ++TargetIndex)
    {
        const FVoxelInventorySectionTarget& Target = InTargets[TargetIndex];
        const FVoxelPersistentSection& After = Target.After;
        for(int32 Previous = 0; Previous < TargetIndex; ++Previous)
        {
            if(InTargets[Previous].After.Section == After.Section)
            {
                OutError = TEXT("Inventory.VoxelDuplicateSection");
                return Fail();
            }
        }
        if(After.Blocks.Num() > SectionVolume || After.Entities.Num() > FVoxelDeltaCodec::MaximumEntityCount || After.Revision == MAX_uint64)
        {
            OutError = TEXT("Inventory.VoxelSectionBounds");
            return Fail();
        }
        for(const auto& Cell : After.Blocks)
        {
            if(Cell.Key < 0 || Cell.Key >= SectionVolume || !ValidateState(Cell.Value))
            {
                OutError = TEXT("Inventory.VoxelCellInvalid");
                return Fail();
            }
        }
        for(const auto& Entity : After.Entities)
        {
            if(Entity.Key < 0 || Entity.Key >= SectionVolume || !FVoxelBlockEntityCodec::Validate(Entity.Value) ||
                FInventoryVoxelEntityCodec::IsPublic(Entity.Value))
            {
                OutError = TEXT("Inventory.VoxelEntityInvalid");
                return Fail();
            }
        }
        FVoxelSection* Loaded = FindSection(After.Section);
        const bool bLoaded = Loaded && Loaded->Status == EVoxelSectionStatus::DataReady;
        const auto ExistingJournal = FindCommittedOverlay(After.Section);
        const uint64 CurrentRevision = bLoaded ? Loaded->CommittedRevision : ExistingJournal ? ExistingJournal->Revision : 0;
        const TMap<int32, FVoxelBlockState> EmptyBlocks;
        const TMap<int32, FVoxelBlockEntityState> EmptyEntities;
        const auto& BeforeBlocks = bLoaded ? Loaded->Overlay : ExistingJournal ? ExistingJournal->Blocks : EmptyBlocks;
        const auto& BeforeEntities = bLoaded ? Loaded->Entities : ExistingJournal ? ExistingJournal->Entities : EmptyEntities;
        if(CurrentRevision == After.Revision)
        {
            if(!BeforeBlocks.OrderIndependentCompareEqual(After.Blocks) || !BeforeEntities.OrderIndependentCompareEqual(After.Entities))
            {
                OutError = TEXT("Inventory.VoxelRedoDiverged");
                return Fail();
            }
            continue;
        }
        if(CurrentRevision != Target.ExpectedRevision || CurrentRevision >= After.Revision ||
            (!bInRecovery && After.Revision != CurrentRevision + 1))
        {
            OutError = TEXT("Inventory.VoxelRevisionStale");
            return Fail();
        }
        FVoxelPreparedInventorySection& Prepared = OutPrepared.Sections.AddDefaulted_GetRef();
        Prepared.Section = After.Section;
        Prepared.bLoaded = bLoaded;
        Prepared.Data.Patch.Section = After.Section;
        Prepared.Data.Patch.FromRevision = CurrentRevision;
        Prepared.Data.Patch.ToRevision = After.Revision;
        Prepared.Data.Patch.Edits.Reserve(SectionVolume);
        Prepared.Data.Patch.Entities.Reserve(SectionVolume);
        if(bLoaded)
        {
            Prepared.Data.Before = Loaded->Stamp;
            Prepared.Resource = Loaded->GetResourceIdentity();
            Prepared.bPinned = TryPinSection(After.Section, Prepared.Resource);
            if(!Prepared.bPinned || !Loaded->BaseBlocks || Loaded->BaseBlocks->Num() != SectionVolume ||
                !AcquireRuntimeData(After.Section, MutableDataBytes(After.Blocks.Num(), After.Entities.Num(), EntityCopyBytes(After.Entities)),
                    Loaded->DataLease.IsValid(), Prepared.Data.DataLease))
            {
                OutError = TEXT("CapacityBlocked: Inventory体素数据授额或Pin失败");
                return Fail();
            }
            Prepared.Data.Blocks = *Loaded->BaseBlocks;
            Prepared.Data.Overlay = After.Blocks;
            Prepared.Data.Entities = After.Entities;
            for(const auto& Cell : After.Blocks)
            {
                Prepared.Data.Blocks[Cell.Key] = Cell.Value;
            }
            if(!ValidateSection(Prepared.Data.Blocks, Prepared.Data.Overlay, Prepared.Data.Entities, OutError))
            {
                return Fail();
            }
        }
        for(const auto& Cell : BeforeBlocks)
        {
            if(!After.Blocks.Contains(Cell.Key))
            {
                FVoxelSectionCellEdit& Edit = Prepared.Data.Patch.Edits.AddDefaulted_GetRef();
                Edit.CellIndex = Cell.Key;
                Edit.bNatural = true;
            }
        }
        for(const auto& Cell : After.Blocks)
        {
            const FVoxelBlockState* Before = BeforeBlocks.Find(Cell.Key);
            if(!Before || *Before != Cell.Value)
            {
                FVoxelSectionCellEdit& Edit = Prepared.Data.Patch.Edits.AddDefaulted_GetRef();
                Edit.CellIndex = Cell.Key;
                Edit.State = Cell.Value;
            }
        }
        for(const auto& Entity : BeforeEntities)
        {
            if(!After.Entities.Contains(Entity.Key))
            {
                FVoxelEntityWrite& Edit = Prepared.Data.Patch.Entities.AddDefaulted_GetRef();
                Edit.CellIndex = Entity.Key;
                Edit.bRemove = true;
            }
        }
        for(const auto& Entity : After.Entities)
        {
            const FVoxelBlockEntityState* Before = BeforeEntities.Find(Entity.Key);
            if(!Before || !(*Before == Entity.Value))
            {
                FVoxelEntityWrite& Edit = Prepared.Data.Patch.Entities.AddDefaulted_GetRef();
                Edit.CellIndex = Entity.Key;
                Edit.Value = Entity.Value;
            }
        }
        Prepared.Data.Patch.Edits.Sort([](const auto& A, const auto& B) { return A.CellIndex < B.CellIndex; });
        Prepared.Data.Patch.Entities.Sort([](const auto& A, const auto& B) { return A.CellIndex < B.CellIndex; });
        if(!PrepareJournal(After.Section, After.Revision, After.Blocks, After.Entities, Prepared.Journal, OutError))
        {
            return Fail();
        }
    }
    if(OutPrepared.Sections.IsEmpty())
    {
        OutPrepared.bPrepared = true;
        return true;
    }
    if(AuthorityCommitCut == MAX_uint64)
    {
        OutError = TEXT("Inventory.VoxelCommitCutExhausted");
        return Fail();
    }
    TArray<FIntVector, TInlineAllocator<64>> Keys;
    for(const auto& Section : OutPrepared.Sections)
    {
        Keys.Add(Section.Data.Patch.Section);
    }
    const auto SectionAt = [&OutPrepared](int32 Index) { return OutPrepared.Sections[Index].Data.Patch.Section; };
    if(!ReserveOverlaySequenceKeys(Keys) || !ChangeIndex.PrepareModified(OutPrepared.Sections.Num(), SectionAt, OutError))
    {
        return Fail();
    }
    const auto ModifiedLease = ChangeIndex.GetStorageLease();
    const auto DirectoryLease = ChangeIndex.GetBaselineLease();
    FVoxelGenerationRequiredSourceScope ModifiedSource(TerrainResources.Get(), ModifiedLease.Get());
    FVoxelGenerationRequiredSourceScope DirectorySource(TerrainResources.Get(), DirectoryLease.Get());
    if(!ChangeHierarchy.PrepareInvalidations(OutPrepared.Sections.Num(), SectionAt, OutPrepared.Invalidations, OutError))
    {
        return Fail();
    }
    OutPrepared.bPrepared = true;
    return true;
}

bool FVoxelWorldRuntime::ValidateInventorySections(const FVoxelPreparedInventoryCommit& InPrepared, FString& OutError) const
{
    if(!IsInGameThread() || !bAuthority || !InPrepared.bPrepared || !InPrepared.Batch.TransactionId.IsValid() ||
        !InPrepared.ScratchLease || !InPrepared.Batch.ResourceLease || InPrepared.WorldEpoch != WorldEpoch ||
        InPrepared.HierarchyRevision != ChangeHierarchy.GetContentRevision() ||
        (!InPrepared.Sections.IsEmpty() && AuthorityCommitCut == MAX_uint64))
    {
        OutError = TEXT("Inventory.VoxelPreparedEpochChanged");
        return false;
    }
    for(const auto& Prepared : InPrepared.Sections)
    {
        const FVoxelSection* Loaded = FindSection(Prepared.Data.Patch.Section);
        const auto Journal = FindCommittedOverlay(Prepared.Data.Patch.Section);
        const uint64 CurrentRevision = Loaded && Loaded->Status == EVoxelSectionStatus::DataReady ? Loaded->CommittedRevision : Journal ? Journal->Revision : 0;
        const bool bCurrentlyLoaded = Loaded && Loaded->Status == EVoxelSectionStatus::DataReady;
        if(Prepared.Section != Prepared.Data.Patch.Section || !Prepared.Journal ||
            CurrentRevision != Prepared.Data.Patch.FromRevision || bCurrentlyLoaded != Prepared.bLoaded ||
            (Prepared.bLoaded && (!Prepared.bPinned || Loaded->PinCount.Load() <= 0 ||
                Loaded->GetResourceIdentity() != Prepared.Resource ||
                !IsCurrentStamp(Prepared.Section, Prepared.Data.Before))))
        {
            OutError = TEXT("Inventory.VoxelPreparedRevisionChanged");
            return false;
        }
    }
    return true;
}

bool FVoxelWorldRuntime::CommitInventorySections(FVoxelPreparedInventoryCommit& InPrepared, FVoxelEditBatch& OutBatch, FString& OutError)
{
    check(IsInGameThread());
    if(!ValidateInventorySections(InPrepared, OutError))
    {
        return false;
    }
    if(!InPrepared.Sections.IsEmpty())
    {
        const auto SectionAt = [&InPrepared](int32 Index) { return InPrepared.Sections[Index].Data.Patch.Section; };
        if(!ChangeHierarchy.CommitInvalidations(MoveTemp(InPrepared.Invalidations), SectionAt, OutError))
        {
            return false;
        }
        for(auto& Prepared : InPrepared.Sections)
        {
            const FIntVector Key = Prepared.Data.Patch.Section;
            const bool bModified = !Prepared.Journal->Blocks.IsEmpty() || !Prepared.Journal->Entities.IsEmpty();
            if(!ChangeIndex.SetModified(Key, bModified, OutError))
            {
                return false;
            }
            if(Prepared.bLoaded)
            {
                FVoxelSection& Section = *FindSection(Key);
                RetireRuntimeData(Section);
                Section.Blocks = MoveTemp(Prepared.Data.Blocks);
                Section.Overlay = MoveTemp(Prepared.Data.Overlay);
                Section.Entities = MoveTemp(Prepared.Data.Entities);
                FinishRuntimeData(Section, Key, MoveTemp(Prepared.Data.DataLease));
                Section.CommittedRevision = Prepared.Data.Patch.ToRevision;
                Section.bCollisionDirty = true;
                Section.bFineMeshDirty = true;
            }
            RetainJournal(Key, MoveTemp(Prepared.Journal), false);
            InPrepared.Batch.Sections.Add(MoveTemp(Prepared.Data.Patch));
        }
        AdvanceOverlaySequences(InPrepared.Batch.Sections);
        ++AuthorityCommitCut;
    }
    OutBatch = MoveTemp(InPrepared.Batch);
    InPrepared.bPrepared = false;
    OutError.Reset();
    return true;
}

void FVoxelWorldRuntime::CancelInventorySections(FVoxelPreparedInventoryCommit& InPrepared)
{
    check(IsInGameThread());
    InPrepared.bPrepared = false;
    for(auto& Prepared : InPrepared.Sections)
    {
        if(Prepared.bPinned)
        {
            ReleaseSectionPin(Prepared.Section, Prepared.Resource);
            Prepared.bPinned = false;
        }
    }
    if(InPrepared.ScratchLease && !InPrepared.Sections.IsEmpty())
    {
        auto Retired = VoxelBoundedAllocation::MakeWorkerShared<TArray<FVoxelPreparedInventorySection>>(
            InPrepared.ScratchLease, MoveTemp(InPrepared.Sections));
    }
    else
    {
        InPrepared.Sections.Empty();
    }
    InPrepared.Batch = FVoxelEditBatch();
    InPrepared.Invalidations = FVoxelPreparedChangeInvalidations();
    InPrepared.ScratchLease.Reset();
    InPrepared.WorldEpoch = 0;
    InPrepared.HierarchyRevision = 0;
}

bool FVoxelWorldRuntime::RestoreInventorySection(const FVoxelPersistentSection& InSection, FString& OutError)
{
    const auto Existing = FindCommittedOverlay(InSection.Section);
    const FVoxelSection* Loaded = FindSection(InSection.Section);
    FVoxelInventorySectionTarget Target;
    Target.ExpectedRevision = Loaded && Loaded->Status == EVoxelSectionStatus::DataReady ? Loaded->CommittedRevision : Existing ? Existing->Revision : 0;
    Target.After = InSection;
    FVoxelPreparedInventoryCommit Prepared;
    FVoxelEditBatch Batch;
    if(!PrepareInventorySections(MakeArrayView(&Target, 1), FGuid::NewGuid(), true, Prepared, OutError) ||
        !CommitInventorySections(Prepared, Batch, OutError))
    {
        CancelInventorySections(Prepared);
        return false;
    }
    PublishInventorySections(Batch);
    CancelInventorySections(Prepared);
    return true;
}

void FVoxelWorldRuntime::PublishInventorySections(const FVoxelEditBatch& InBatch)
{
    for(const FVoxelSectionPatch& Patch : InBatch.Sections)
    {
        NotifySectionEvent(Patch.Section, EVoxelRuntimeSectionEvent::Committed);
    }
}
