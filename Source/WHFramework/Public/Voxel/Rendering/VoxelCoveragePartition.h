#pragma once

#include "CoreMinimal.h"

struct WHFRAMEWORK_API FVoxelCoverageRegion
{
	FIntVector Min = FIntVector::ZeroValue;
	FIntVector Max = FIntVector::ZeroValue;
	bool operator==(const FVoxelCoverageRegion& InOther) const = default;
	bool IsValid() const;
};

enum class EVoxelCoverageEvaluation : uint8
{
	None = 0,
	Pending,
	Covered,
	Uncovered,
	Canceled,
	Invalid
};

// 任意数量的覆盖盒做精确差集；时间片耗尽与真正缺口是不同结果。
class WHFRAMEWORK_API FVoxelCoverageEvaluationTask
{
public:
	bool Begin(TConstArrayView<FVoxelCoverageRegion> InRequired,
		TConstArrayView<FVoxelCoverageRegion> InCover, FString& OutError);
	EVoxelCoverageEvaluation Advance(int32 InMaximumOperations, const TAtomic<bool>* InCancel = nullptr);
	EVoxelCoverageEvaluation GetState() const { return State; }
	const TArray<FVoxelCoverageRegion>& GetUncovered() const { return Current; }
	int64 GetOperations() const { return Operations; }

	static void Subtract(const FVoxelCoverageRegion& InRegion, const FVoxelCoverageRegion& InCut,
		TArray<FVoxelCoverageRegion>& OutPieces);

private:
	TArray<FVoxelCoverageRegion> Current;
	TArray<FVoxelCoverageRegion> Next;
	TArray<FVoxelCoverageRegion> Cover;
	int32 CutIndex = 0;
	int32 RegionIndex = 0;
	int64 Operations = 0;
	EVoxelCoverageEvaluation State = EVoxelCoverageEvaluation::None;
};
