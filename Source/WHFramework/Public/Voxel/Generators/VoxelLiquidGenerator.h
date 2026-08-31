
#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "VoxelLiquidGenerator.generated.h"

/** 体素液体生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelLiquidGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	virtual void Generate(UVoxelChunk* InChunk) override;

protected:
	void GenerateLiquid(UVoxelChunk* InChunk) const;
};
