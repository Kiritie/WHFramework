#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

struct WHFRAMEWORK_API FVoxelStructurePiecePlacement
{
	int32 PieceIndex = INDEX_NONE;

	FIntVector Origin =
		FIntVector::ZeroValue;

	uint8 Yaw = 0;
};

struct WHFRAMEWORK_API FVoxelStructureCellWrite
{
	FIntVector Position =
		FIntVector::ZeroValue;

	uint32 Value = 0;
};

struct WHFRAMEWORK_API FVoxelStructureClearVolume
{
	FVoxelGenerationBounds Bounds;
};

struct WHFRAMEWORK_API FVoxelStructureDetailPlacement
{
	FName DetailId;
	FIntVector Position = FIntVector::ZeroValue;
	uint8 Yaw = 0;
	FVoxelStableId OwnerId;
};

struct WHFRAMEWORK_API FVoxelStructureInstance
{
	FVoxelStableId Id;

	FName DefinitionId;

	FIntVector Anchor =
		FIntVector::ZeroValue;

	TArray<FVoxelStructurePiecePlacement> Pieces;
	TArray<FVoxelStructureCellWrite> Writes;
	TArray<FVoxelStructureClearVolume> ClearVolumes;
	TArray<FVoxelStructureDetailPlacement> Details;

	uint64 GetAllocatedBytes() const;
};

struct WHFRAMEWORK_API FVoxelStructureLayoutContext
{
	int32 WorldSeed = 0;

	const FVoxelGenerationRecipe* Recipe = nullptr;

	const FVoxelStructureRuntimeDefinition* Definition = nullptr;

	FIntVector Anchor =
		FIntVector::ZeroValue;

	uint64 CandidateSeed = 0;
};

class WHFRAMEWORK_API IVoxelStructureLayoutAlgorithm
{
public:
	virtual ~IVoxelStructureLayoutAlgorithm() = default;

	virtual FName GetId() const = 0;
	virtual uint32 GetVersion() const = 0;

	virtual bool BuildLayout(
		const FVoxelStructureLayoutContext& InContext,
		TArray<FVoxelStructurePiecePlacement>& OutPieces,
		FString& OutError) const = 0;
};

class WHFRAMEWORK_API FVoxelStructureLayoutRegistry
{
public:
	static FVoxelStructureLayoutRegistry& Get();

public:
	bool Register(
		TSharedRef<
			const IVoxelStructureLayoutAlgorithm,
			ESPMode::ThreadSafe> InAlgorithm,
		FString& OutError);

	void Unregister(
		FName InAlgorithmId);

	TSharedPtr<
		const IVoxelStructureLayoutAlgorithm,
		ESPMode::ThreadSafe> Find(
			FName InAlgorithmId) const;

	void Reset();

private:
	mutable FRWLock Lock;

	TMap<
		FName,
		TSharedRef<
			const IVoxelStructureLayoutAlgorithm,
			ESPMode::ThreadSafe>> Algorithms;
};

class WHFRAMEWORK_API FVoxelStructurePlanner
{
public:
	explicit FVoxelStructurePlanner(
		TSharedRef<
			const FVoxelGenerationRecipe,
			ESPMode::ThreadSafe> InRecipe);

public:
	bool Plan(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(
			const FIntVector& InPosition,
			FVoxelColumnSample& OutColumn)> InSampleColumn,
		TArray<FVoxelStructureInstance>& OutInstances,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	void GatherCandidates(
		const FVoxelStructureRuntimeDefinition& InDefinition,
		const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutCandidates) const;

	bool BuildInstance(
		int32 InDefinitionIndex,
		const FVoxelStructureRuntimeDefinition& InDefinition,
		const FIntVector& InCandidate,
		const FVoxelColumnSample& InColumn,
		FVoxelStructureInstance& OutInstance,
		FString& OutError) const;

	bool BuildPiecePlacements(
		const FVoxelStructureRuntimeDefinition& InDefinition,
		const FIntVector& InAnchor,
		uint64 InCandidateSeed,
		TArray<FVoxelStructurePiecePlacement>& OutPieces,
		FString& OutError) const;

	bool RasterizePiece(
		const FVoxelStructureRuntimeDefinition& InDefinition,
		const FVoxelStructurePiecePlacement& InPlacement,
		const FIntVector& InStructureAnchor,
		TArray<FVoxelStructureCellWrite>& OutWrites,
		TArray<FVoxelStructureClearVolume>& OutClearVolumes,
		FString& OutError) const;

	FIntVector ResolveTerrainAdaptedAnchor(
		const FVoxelStructureRuntimeDefinition& InDefinition,
		const FIntVector& InCandidate,
		const FVoxelColumnSample& InColumn) const;

private:
	TSharedRef<
		const FVoxelGenerationRecipe,
		ESPMode::ThreadSafe> Recipe;
};
