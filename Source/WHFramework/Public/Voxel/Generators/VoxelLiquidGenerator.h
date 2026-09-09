
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "VoxelGenerator.h"
#include "VoxelLiquidGenerator.generated.h"

/** 体素液体生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelLiquidGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	virtual void PrepareBatch(const TArray<FIndex>& InChunkIndices) override;

	virtual void Generate(UVoxelChunk* InChunk) override;

	virtual void CompleteBatch(bool bCancelled) override;

protected:
	static bool IsIndexBefore(const FIndex& A, const FIndex& B);
	TSharedPtr<const FVoxelLiquidSnapshotGrid, ESPMode::ThreadSafe> CreateChunkSnapshot(UVoxelChunk* InChunk) const;
	void CalculateLiquidUpdates(UVoxelChunk* InChunk);
	void ApplyBatchUpdates();

	TMap<FIndex, TSharedPtr<const FVoxelLiquidSnapshotGrid, ESPMode::ThreadSafe>> BatchSnapshots;
	TMap<FIndex, TMap<FIndex, FVoxelLiquidUpdate>> BatchUpdates;
	FCriticalSection BatchUpdatesCriticalSection;
};
