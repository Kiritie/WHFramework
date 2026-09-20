#pragma once

#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"
#include "Voxel/Runtime/VoxelChangeIndex.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Runtime/VoxelSection.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

struct WHFRAMEWORK_API FVoxelPreparedSection
{
	FVoxelSectionStamp Before;
	TArray<FVoxelBlockState> Blocks;
	TMap<int32, FVoxelBlockState> Overlay;
	TMap<int32, FVoxelBlockEntityState> Entities;
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
	FVoxelWorldRuntime(
		uint64 InWorldEpoch,
		bool bInAuthority,
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
		TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator);

	FVoxelWorldRuntime(const FVoxelWorldRuntime&) = delete;
	FVoxelWorldRuntime& operator=(const FVoxelWorldRuntime&) = delete;

	FVoxelSection* FindSection(const FIntVector& InSection);
	const FVoxelSection* FindSection(const FIntVector& InSection) const;
	FVoxelSection* FindOrAllocate(const FIntVector& InSection, uint64 InFrame);
	TArray<FIntVector> ResidentSections() const;

	bool TryGetBlock(const FIntVector& InPosition, FVoxelBlockState& OutState) const;
	bool PublishBase(
		const FIntVector& InSection,
		const FVoxelSectionStamp& InStamp,
		TArray<FVoxelBlockState>&& InBaseBlocks,
		FString& OutError);
	bool PublishFinal(
		const FIntVector& InSection,
		uint64 InRevision,
		const TMap<int32, FVoxelBlockState>& InOverlay,
		const TMap<int32, FVoxelBlockEntityState>& InEntities,
		FString& OutError);
	bool CaptureSnapshot(const FIntVector& InSection, FVoxelSectionSnapshot& OutSnapshot) const;

	bool PrepareEdit(
		const TArray<FVoxelCellEdit>& InCells,
		const TArray<FVoxelEntityEdit>& InEntities,
		FVoxelPreparedEdit& OutPrepared,
		FString& OutError) const;
	bool CommitPreparedEdit(
		FVoxelPreparedEdit&& InPrepared,
		FVoxelEditBatch& OutBatch,
		FString& OutError);
	bool ApplyPatch(const FVoxelSectionPatch& InPatch, FString& OutError);
	bool ApplyRemotePatchBatch(const FVoxelEditBatch& InBatch, FString& OutError);

	bool RemoveSection(const FIntVector& InSection, bool bDiscardModified = false);
	void MarkCommitted(const FIntVector& InSection, uint64 InRevision);
	bool IsCurrentStamp(const FIntVector& InSection, const FVoxelSectionStamp& InStamp) const;

	int32 NumSections() const;
	uint64 Epoch() const;
	bool IsServer() const;

	FVoxelChangeHierarchy& GetChangeHierarchy();
	const FVoxelChangeHierarchy& GetChangeHierarchy() const;
	FVoxelChangeIndex& GetChangeIndex();
	const FVoxelChangeIndex& GetChangeIndex() const;

private:
	bool ValidateState(const FVoxelBlockState& InState) const;
	bool ValidateSection(
		const TArray<FVoxelBlockState>& InBlocks,
		const TMap<int32, FVoxelBlockState>& InOverlay,
		FString& OutError) const;
	bool BuildPatchCandidate(
		const FVoxelSection& InSection,
		const FVoxelSectionPatch& InPatch,
		TArray<FVoxelBlockState>& OutBlocks,
		TMap<int32, FVoxelBlockState>& OutOverlay,
		TMap<int32, FVoxelBlockEntityState>& OutEntities,
		FString& OutError) const;
	void PublishPatchCandidate(
		FVoxelSection& InSection,
		const FVoxelSectionPatch& InPatch,
		TArray<FVoxelBlockState>&& InBlocks,
		TMap<int32, FVoxelBlockState>&& InOverlay,
		TMap<int32, FVoxelBlockEntityState>&& InEntities);

private:
	uint64 WorldEpoch = 0;
	uint64 NextToken = 1;
	bool bAuthority = false;
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TMap<FIntVector, TUniquePtr<FVoxelSection>> Sections;
	FVoxelChangeHierarchy ChangeHierarchy;
	FVoxelChangeIndex ChangeIndex;
};
