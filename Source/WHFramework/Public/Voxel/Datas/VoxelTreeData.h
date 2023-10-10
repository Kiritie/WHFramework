#pragma once

#include "VoxelData.h"

#include "VoxelTreeData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelTreeData : public UVoxelData
{
	GENERATED_BODY()

public:
	UVoxelTreeData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bTreeRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bTreeRoot == true"))
	FVector2D TreeHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bTreeRoot == true"))
	UVoxelData* LeavesData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bTreeRoot == true"))
	FVector2D LeavesHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bTreeRoot == true"))
	FVector2D LeavesWidth;
};
