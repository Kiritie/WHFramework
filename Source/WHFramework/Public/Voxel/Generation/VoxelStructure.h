#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

class FVoxelGenerationWorkspace;
struct FVoxelStructurePlan;

struct WHFRAMEWORK_API FVoxelStructurePiecePlacement
{
	int32 PieceIndex = INDEX_NONE;

	FInt64Vector Origin =
		FInt64Vector::ZeroValue;

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

struct WHFRAMEWORK_API FVoxelPlannedStructurePlacement
{
	FVoxelStableId Id;
	FName DefinitionId;
	FIntVector Anchor = FIntVector::ZeroValue;
	uint8 Yaw = 0;

	bool IsValid() const
	{
		return Id.IsValid() && !DefinitionId.IsNone() && Yaw < 4;
	}
};

struct WHFRAMEWORK_API FVoxelStructureLayoutContext
{
	int32 WorldSeed = 0;

	const FVoxelGenerationRecipe* Recipe = nullptr;

	const FVoxelStructureRuntimeDefinition* Definition = nullptr;

	FIntVector Anchor =
		FIntVector::ZeroValue;

	uint64 CandidateSeed = 0;

	// 输出和算法私有 scratch 都由原 body scope 的工作区先授予；不得返回局部工作区的 backing。
	FVoxelGenerationWorkspace* Workspace = nullptr;
	const TAtomic<bool>* Cancel = nullptr;

	bool ReservePieces(
		TArray<FVoxelStructurePiecePlacement>& InOutPieces,
		int32 InCapacity,
		FString& OutError) const;
};

class WHFRAMEWORK_API IVoxelStructureLayoutAlgorithm
{
public:
	virtual ~IVoxelStructureLayoutAlgorithm() = default;

	virtual FName GetId() const = 0;
	virtual uint32 GetVersion() const = 0;

	// OutPieces 每次增长先调用 InContext.ReservePieces；私有 scratch 使用 InContext.Workspace 的 Reserve/Free。
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
	// bound scope 内的旧输出须同属当前 retained 授予；无 scope 的 DEV 夹具不参与账本退款。
	bool BuildPlan(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(
			const FIntVector& InPosition,
			FVoxelColumnSample& OutColumn)> InSampleColumn,
		FVoxelStructurePlan& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	// 默认输出归属当前 retained scope；指定工作区时，新旧输出均属该工作区且由调用方覆盖全部嵌套 backing 的寿命。
	bool Plan(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(
			const FIntVector& InPosition,
			FVoxelColumnSample& OutColumn)> InSampleColumn,
		TArray<FVoxelStructureInstance>& OutInstances,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		FVoxelGenerationWorkspace* InOutputWorkspace = nullptr) const;

	// bound scope 内的旧 instance backing 须同属当前 retained 授予。
	bool BuildAt(
		const FVoxelPlannedStructurePlacement& InPlacement,
		FVoxelStructureInstance& OutInstance,
		FString& OutError) const;

private:
	bool GatherCandidates(
		const FVoxelStructureRuntimeDefinition& InDefinition,
		const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutCandidates,
		FString& OutError,
		FVoxelGenerationWorkspace& InWorkspace,
		const TAtomic<bool>* InCancel) const;

	bool BuildInstance(
		int32 InDefinitionIndex,
		const FVoxelStructureRuntimeDefinition& InDefinition,
		const FIntVector& InCandidate,
		FVoxelStructureInstance& OutInstance,
		FString& OutError,
		TOptional<FVoxelStableId> InId = {},
		TOptional<uint8> InRootYaw = {},
		FVoxelGenerationWorkspace* InOutputWorkspace = nullptr,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool BuildPiecePlacements(
		const FVoxelStructureRuntimeDefinition& InDefinition,
		const FIntVector& InAnchor,
		uint64 InCandidateSeed,
		TArray<FVoxelStructurePiecePlacement>& OutPieces,
		FString& OutError,
		FVoxelGenerationWorkspace& InWorkspace,
		const TAtomic<bool>* InCancel) const;

	bool RasterizePiece(
		const FVoxelStructureRuntimeDefinition& InDefinition,
		const FVoxelStructurePiecePlacement& InPlacement,
		const FIntVector& InStructureAnchor,
		TArray<FVoxelStructureCellWrite>& OutWrites,
		TArray<FVoxelStructureClearVolume>& OutClearVolumes,
		FString& OutError,
		FVoxelGenerationWorkspace* InOutputWorkspace,
		const TAtomic<bool>* InCancel) const;

	bool ResolveTerrainAdaptedAnchor(
		const FVoxelStructureRuntimeDefinition& InDefinition,
		const FIntVector& InCandidate,
		const FVoxelColumnSample& InColumn,
		FIntVector& OutAnchor) const;

private:
	TSharedRef<
		const FVoxelGenerationRecipe,
		ESPMode::ThreadSafe> Recipe;
};
