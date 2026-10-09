#include "Voxel/Runtime/VoxelOverlaySnapshot.h"

#include "Voxel/Chunks/VoxelSectionKey.h"

bool FVoxelOverlaySnapshotSet::VisitModifiedOverlays(const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(const FIntVector&, const FVoxelOverlaySnapshot&)> InVisit,
	FString& OutError, const TAtomic<bool>* InCancel) const
{
	OutError.Reset();
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	if (!InBounds.IsValid())
	{
		OutError = TEXT("InvalidRequest: Overlay visitor bounds are invalid");
		return false;
	}
	const auto VisitSection = [&](const FIntVector& Section, const FVoxelOverlaySnapshot& Snapshot)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		FIntVector Min;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const int64 Cell = int64(Section[Axis]) * 16;
			if (Cell < MIN_int32 || Cell > MAX_int32 - 16)
			{
				OutError = TEXT("InvalidRequest: Overlay Section bounds overflow");
				return false;
			}
			Min[Axis] = int32(Cell);
		}
		if (InBounds.Intersects({Min, Min + FIntVector(16)}) && !InVisit(Section, Snapshot))
		{
			if (OutError.IsEmpty()) OutError = TEXT("InvalidRequest: Overlay visitor rejected input");
			return false;
		}
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		return true;
	};
	for (const auto& Pair : Sections)
		if (!VisitSection(Pair.Key, Pair.Value)) return false;
	for (const auto& Pair : PinnedSections)
	{
		if (!Pair.Value || Sections.Contains(Pair.Key))
		{
			OutError = TEXT("InvalidRequest: Overlay snapshot contains duplicate or missing pinned Sections");
			return false;
		}
		if (!VisitSection(Pair.Key, *Pair.Value)) return false;
	}
	OutError.Reset();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
bool FVoxelOverlaySnapshotSet::EnumerateModifiedSections(const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutSections, const TAtomic<bool>* InCancel) const
{
	OutSections.Reset();
	for (const TPair<FIntVector, FVoxelOverlaySnapshot>& Pair : Sections)
	{
		if (InCancel && InCancel->Load())
		{
			OutSections.Reset();
			return false;
		}
		const FVoxelGenerationBounds Bounds{ Pair.Key * 16, (Pair.Key + FIntVector(1)) * 16 };
		if (Bounds.Intersects(InBounds))
		{
			OutSections.Add(Pair.Key);
		}
	}
	for (const auto& Pair : PinnedSections)
	{
		if (InCancel && InCancel->Load())
		{
			OutSections.Reset();
			return false;
		}
		check(Pair.Value && !Sections.Contains(Pair.Key));
		if (InBounds.Intersects({Pair.Key * 16, (Pair.Key + FIntVector(1)) * 16})) OutSections.Add(Pair.Key);
	}
	return true;
}

bool FVoxelOverlaySnapshotSet::ReadOverlay(const FIntVector& InSection, FVoxelOverlaySnapshot& OutOverlay) const
{
	if (const FVoxelOverlaySnapshot* Snapshot = Find(InSection))
	{
		OutOverlay = *Snapshot;
		return true;
	}
	return false;
}
#endif

void FVoxelOverlaySnapshotSet::ApplyAt(const FIntVector& InCell, FVoxelBlockState& InOutState) const
{
	const FVoxelSectionKey Key = VoxelCoord::Section(InCell);
	if (const FVoxelOverlaySnapshot* Snapshot = Find(FIntVector(Key.X, Key.Y, Key.Z)))
	{
		const int32 Index = VoxelCoord::Linear(InCell - VoxelCoord::Origin(Key));
		if (const FVoxelBlockState* State = Snapshot->Blocks.Find(Index))
		{
			InOutState = *State;
		}
	}
}

const FVoxelOverlaySnapshot* FVoxelOverlaySnapshotSet::Find(const FIntVector& InSection) const
{
	if (const auto* Pinned = PinnedSections.Find(InSection)) return Pinned->Get();
	return Sections.Find(InSection);
}

int32 FVoxelOverlaySnapshotSet::Num() const
{
	return Sections.Num() + PinnedSections.Num();
}

void FVoxelOverlaySnapshotSet::Visit(TFunctionRef<void(const FIntVector&, const FVoxelOverlaySnapshot&)> InVisit) const
{
	for (const auto& Pair : Sections) InVisit(Pair.Key, Pair.Value);
	for (const auto& Pair : PinnedSections)
	{
		check(Pair.Value && !Sections.Contains(Pair.Key));
		InVisit(Pair.Key, *Pair.Value);
	}
}

uint64 FVoxelOverlaySnapshotSet::GetAllocatedBytes() const
{
	uint64 Bytes = Sections.GetAllocatedSize() + PinnedSections.GetAllocatedSize();
	for (const auto& Pair : Sections)
	{
		Bytes += Pair.Value.Blocks.GetAllocatedSize() + Pair.Value.Entities.GetAllocatedSize();
		for (const auto& Entity : Pair.Value.Entities) Bytes += Entity.Value.Payload.GetAllocatedSize();
	}
	return Bytes;
}
