#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSection.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Runtime/VoxelRegistry.h"
struct WHFRAMEWORK_API FVoxelPreparedSection
{
	FVoxelTaskStamp Before;
	FVoxelSectionStorage Blocks;
	FVoxelSectionOverlay Overlay;
	FVoxelSectionPatch Patch;
};
struct WHFRAMEWORK_API FVoxelPreparedEdit
{
	FGuid TransactionId;
	TArray<FVoxelPreparedSection> Sections;
};
class WHFRAMEWORK_API FVoxelWorldRuntime
{
public:
	FVoxelWorldRuntime(uint64 Epoch,
	                   bool bAuthority,
	                   TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry,
	                   TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator);
	FVoxelSection* Find(const FVoxelSectionKey& Key);
	const FVoxelSection* Find(const FVoxelSectionKey& Key) const;
	FVoxelSection* Allocate(const FVoxelSectionKey& Key, double Now);
	TArray<FVoxelSectionKey> ResidentKeys() const;
	bool TryGetBlock(const FIntVector& Position, FVoxelBlockState& Out) const;
	bool PublishLoaded(const FVoxelTaskStamp& Stamp, FVoxelSectionStorage&& Base, const FVoxelSectionOverlay& Overlay, bool bCommitted);
	bool CaptureSnapshot(const FVoxelSectionKey& Key, FVoxelSectionSnapshot& Out) const;
	bool PrepareEdit(const TArray<FVoxelCellEdit>& Cells, const TArray<FVoxelEntityEdit>& Entities, FVoxelPreparedEdit& Out, FString& Error) const;
	bool CommitEdit(FVoxelPreparedEdit&& Prepared, FVoxelEditBatch& Out);
	bool ApplyRemoteSnapshots(const TArray<FVoxelSectionOverlay>& Snapshots, TArray<FVoxelSectionStorage>&& Bases);
	bool Remove(const FVoxelSectionKey& Key, bool bDiscardDirty = false);
	void MarkCommitted(const FVoxelSectionKey& Key, uint64 Revision);
	bool IsCurrent(const FVoxelTaskStamp& Stamp, bool bGeometry = true) const;
	void MarkMeshApplied(const FVoxelTaskStamp& Stamp);
	void MarkCollisionApplied(const FVoxelTaskStamp& Stamp);
	int32 NumSections() const
	{
		return Sections.Num();
	}
	uint64 Epoch() const
	{
		return WorldEpoch;
	}
	bool Authority() const
	{
		return bAuthority;
	}
	bool ValidateOverlay(const FVoxelSectionOverlay& Overlay, const FVoxelSectionStorage& Effective) const;

private:
	void Invalidate(const FVoxelSectionKey& Key, bool bNeighbors);
	void InvalidateEdit(const FVoxelSectionPatch& Patch);
	uint64 WorldEpoch;
	uint64 NextToken = 1;
	bool bAuthority;
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TMap<FVoxelSectionKey, TSharedPtr<FVoxelSection>> Sections;
};
