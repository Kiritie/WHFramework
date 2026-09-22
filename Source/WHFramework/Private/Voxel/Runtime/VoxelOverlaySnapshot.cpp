#include "Voxel/Runtime/VoxelOverlaySnapshot.h"

#include "Voxel/Chunks/VoxelSectionKey.h"

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
	return true;
}

bool FVoxelOverlaySnapshotSet::ReadOverlay(const FIntVector& InSection, FVoxelOverlaySnapshot& OutOverlay) const
{
	if (const FVoxelOverlaySnapshot* Snapshot = Sections.Find(InSection))
	{
		OutOverlay = *Snapshot;
		return true;
	}
	return false;
}

void FVoxelOverlaySnapshotSet::ApplyAt(const FIntVector& InCell, FVoxelBlockState& InOutState) const
{
	const FVoxelSectionKey Key = VoxelCoord::Section(InCell);
	if (const FVoxelOverlaySnapshot* Snapshot = Sections.Find(FIntVector(Key.X, Key.Y, Key.Z)))
	{
		const int32 Index = VoxelCoord::Linear(InCell - VoxelCoord::Origin(Key));
		if (const FVoxelBlockState* State = Snapshot->Blocks.Find(Index))
		{
			InOutState = *State;
		}
	}
}

uint64 FVoxelOverlaySnapshotSet::GetAllocatedBytes() const
{
	uint64 Bytes = Sections.GetAllocatedSize();
	for (const auto& Pair : Sections)
	{
		Bytes += Pair.Value.Blocks.GetAllocatedSize();
	}
	return Bytes;
}
