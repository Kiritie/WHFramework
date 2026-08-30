#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"

#include "VoxelRegionGenerator.generated.h"

/** 体素区块区域生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelRegionGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelRegionGenerator();

	virtual void Generate(UVoxelChunk* InChunk) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "32"))
	int32 RegionSizeChunks;
};
