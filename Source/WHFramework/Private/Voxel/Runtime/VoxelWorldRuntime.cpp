#include "Voxel/Runtime/VoxelWorldRuntime.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"

namespace VoxelWorldRuntimePrivate
{
	constexpr int32 RuntimeSectionSide = 16;
	constexpr int32 SectionVolume = RuntimeSectionSide * RuntimeSectionSide * RuntimeSectionSide;

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
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator)
	: WorldEpoch(InWorldEpoch)
	, bAuthority(bInAuthority)
	, Registry(InRegistry)
	, Generator(InGenerator)
{
}

FVoxelSection* FVoxelWorldRuntime::FindSection(const FIntVector& InSection)
{
	const TUniquePtr<FVoxelSection>* Found = Sections.Find(InSection);
	return Found ? Found->Get() : nullptr;
}

const FVoxelSection* FVoxelWorldRuntime::FindSection(const FIntVector& InSection) const
{
	const TUniquePtr<FVoxelSection>* Found = Sections.Find(InSection);
	return Found ? Found->Get() : nullptr;
}

FVoxelSection* FVoxelWorldRuntime::FindOrAllocate(const FIntVector& InSection, const uint64 InFrame)
{
	check(IsInGameThread());

	if (FVoxelSection* Existing = FindSection(InSection))
	{
		Existing->LastWantedFrame = InFrame;
		return Existing;
	}

	if (NextToken == MAX_uint64)
	{
		return nullptr;
	}

	TUniquePtr<FVoxelSection> NewSection = MakeUnique<FVoxelSection>();
	NewSection->Stamp.Epoch = WorldEpoch;
	NewSection->Stamp.Token = NextToken++;
	NewSection->Status = EVoxelSectionStatus::Allocated;
	NewSection->LastWantedFrame = InFrame;
	FVoxelSection* Result = NewSection.Get();
	Sections.Add(InSection, MoveTemp(NewSection));
	return Result;
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
	if (!Section || Section->Status != EVoxelSectionStatus::DataReady)
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
	FString& OutError)
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

	Section->BaseBlocks = MakeShared<const TArray<FVoxelBlockState>, ESPMode::ThreadSafe>(MoveTemp(InBaseBlocks));
	Section->Status = EVoxelSectionStatus::BaseReady;
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
	if (!Section || Section->Status != EVoxelSectionStatus::BaseReady || !Section->BaseBlocks)
	{
		OutError = TEXT("Voxel final publish target is not BaseReady");
		return false;
	}

	TArray<FVoxelBlockState> Candidate = *Section->BaseBlocks;
	for (const TPair<int32, FVoxelBlockState>& Pair : InOverlay)
	{
		if (!Candidate.IsValidIndex(Pair.Key) || !ValidateState(Pair.Value))
		{
			OutError = TEXT("Voxel final overlay contains an invalid cell");
			return false;
		}
		Candidate[Pair.Key] = Pair.Value;
	}

	TMap<int32, FVoxelBlockEntityState> Entities = InEntities;
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

	Section->Blocks = MoveTemp(Candidate);
	Section->Overlay = InOverlay;
	Section->Entities = MoveTemp(Entities);
	Section->CommittedRevision = InRevision;
	Section->PersistedRevision = InRevision;
	Section->Status = EVoxelSectionStatus::DataReady;
	Section->bCollisionDirty = true;
	Section->bFineMeshDirty = true;
	ChangeIndex.SetModified(InSection, !Section->Overlay.IsEmpty() || !InEntities.IsEmpty());
	ChangeHierarchy.InvalidateSection(InSection);
	OutError.Reset();
	return true;
}

bool FVoxelWorldRuntime::CaptureSnapshot(
	const FIntVector& InSection,
	FVoxelSectionSnapshot& OutSnapshot) const
{
	const FVoxelSection* Section = FindSection(InSection);
	if (!Section || Section->Status != EVoxelSectionStatus::DataReady)
	{
		return false;
	}

	FVoxelSectionSnapshot Snapshot;
	Snapshot.Section = InSection;
	Snapshot.Stamp = Section->Stamp;
	Snapshot.Revision = Section->CommittedRevision;
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
			Neighbor->Blocks.Num() == RuntimeSectionSide * RuntimeSectionSide * RuntimeSectionSide;
		Snapshot.Known[Face] = bKnown;
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

bool FVoxelWorldRuntime::PrepareEdit(
	const TArray<FVoxelCellEdit>& InCells,
	const TArray<FVoxelEntityEdit>& InEntities,
	FVoxelPreparedEdit& OutPrepared,
	FString& OutError) const
{
	check(IsInGameThread());
	if (!bAuthority || (InCells.IsEmpty() && InEntities.IsEmpty()))
	{
		OutError = TEXT("Voxel edit batch is empty or the runtime is not authoritative");
		return false;
	}

	FVoxelPreparedEdit Prepared;
	Prepared.TransactionId = FGuid::NewGuid();
	TMap<FIntVector, int32> SectionToPrepared;
	TSet<FIntVector> SeenPositions;

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
			NewPrepared.Before = Section->Stamp;
			NewPrepared.Blocks = Section->Blocks;
			NewPrepared.Overlay = Section->Overlay;
			NewPrepared.Entities = Section->Entities;
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
			NewPrepared.Before = Section->Stamp;
			NewPrepared.Blocks = Section->Blocks;
			NewPrepared.Overlay = Section->Overlay;
			NewPrepared.Entities = Section->Entities;
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
		Write.Value = Edit.Value;
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

	FVoxelEditBatch Batch;
	Batch.TransactionId = InPrepared.TransactionId;
	for (FVoxelPreparedSection& PreparedSection : InPrepared.Sections)
	{
		FVoxelSection* Section = FindSection(PreparedSection.Patch.Section);
		PublishPatchCandidate(
			*Section,
			PreparedSection.Patch,
			MoveTemp(PreparedSection.Blocks),
			MoveTemp(PreparedSection.Overlay),
			MoveTemp(PreparedSection.Entities));
		Batch.Sections.Add(MoveTemp(PreparedSection.Patch));
	}

	OutBatch = MoveTemp(Batch);
	OutError.Reset();
	return true;
}

bool FVoxelWorldRuntime::ApplyPatch(const FVoxelSectionPatch& InPatch, FString& OutError)
{
	check(IsInGameThread());
	FVoxelSection* Section = FindSection(InPatch.Section);
	if (!Section)
	{
		OutError = TEXT("Voxel patch target does not exist");
		return false;
	}

	TArray<FVoxelBlockState> CandidateBlocks;
	TMap<int32, FVoxelBlockState> CandidateOverlay;
	TMap<int32, FVoxelBlockEntityState> CandidateEntities;
	if (!BuildPatchCandidate(*Section, InPatch, CandidateBlocks, CandidateOverlay, CandidateEntities, OutError))
	{
		return false;
	}

	PublishPatchCandidate(
		*Section,
		InPatch,
		MoveTemp(CandidateBlocks),
		MoveTemp(CandidateOverlay),
		MoveTemp(CandidateEntities));
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

	struct FCandidate
	{
		FVoxelSection* Section = nullptr;
		const FVoxelSectionPatch* Patch = nullptr;
		TArray<FVoxelBlockState> Blocks;
		TMap<int32, FVoxelBlockState> Overlay;
		TMap<int32, FVoxelBlockEntityState> Entities;
	};

	TArray<FCandidate> Candidates;
	Candidates.Reserve(InBatch.Sections.Num());
	TSet<FIntVector> SeenSections;
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
				OutError))
		{
			return false;
		}
	}

	for (FCandidate& Candidate : Candidates)
	{
		PublishPatchCandidate(
			*Candidate.Section,
			*Candidate.Patch,
			MoveTemp(Candidate.Blocks),
			MoveTemp(Candidate.Overlay),
			MoveTemp(Candidate.Entities));
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
		(!bDiscardModified && bAuthority && Section->PersistedRevision != Section->CommittedRevision))
	{
		return false;
	}

	Section->Status = EVoxelSectionStatus::Unloading;
	Sections.Remove(InSection);
	ChangeHierarchy.ReleaseSection(InSection);
	return true;
}

void FVoxelWorldRuntime::MarkCommitted(
	const FIntVector& InSection,
	const uint64 InRevision)
{
	if (FVoxelSection* Section = FindSection(InSection))
	{
		if (InRevision != Section->CommittedRevision)
		{
			return;
		}
		Section->PersistedRevision = InRevision;
	}
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
		if (!Definition || !Definition->EntityKind || Definition->EntityKind != Entity.Value.Kind || !FVoxelBlockEntityCodec::Validate(Entity.Value))
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
	FString& OutError) const
{
	if (InSection.Status != EVoxelSectionStatus::DataReady)
	{
		OutError = TEXT("Voxel patch target is not DataReady");
		return false;
	}
	if (InSection.CommittedRevision != InPatch.FromRevision ||
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

	OutBlocks = InSection.Blocks;
	OutOverlay = InSection.Overlay;
	OutEntities = InSection.Entities;
	TSet<int32> SeenCells;
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
	TMap<int32, FVoxelBlockEntityState>&& InEntities)
{
	InSection.Blocks = MoveTemp(InBlocks);
	InSection.Overlay = MoveTemp(InOverlay);
	InSection.Entities = MoveTemp(InEntities);
	InSection.CommittedRevision = InPatch.ToRevision;
	InSection.bCollisionDirty = true;
	InSection.bFineMeshDirty = true;
	ChangeIndex.SetModified(InPatch.Section, !InSection.Overlay.IsEmpty() || !InSection.Entities.IsEmpty());
	ChangeHierarchy.InvalidateSection(InPatch.Section);
}
