#pragma once

#include "CoreMinimal.h"

class FVoxelGenerationPipeline;

/** 使用冻结的自然环境解析地图坐标，无需目标区块常驻。 */
class WHFRAMEWORK_API FVoxelMapSurfaceResolver
{
public:
	static bool Resolve(
		const FVoxelGenerationPipeline& InGenerator,
		double InBlockSize,
		const FVector2D& InMapPosition,
		FVector& OutLocation,
		FString& OutError);
};
