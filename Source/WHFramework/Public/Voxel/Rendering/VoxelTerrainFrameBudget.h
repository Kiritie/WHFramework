#pragma once

#include "CoreMinimal.h"
#include "VoxelTerrainFrameBudget.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelTerrainFrameBudgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double TargetFramesPerSecond = 60.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double MaximumTerrainMilliseconds = 4.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double MinimumNearMilliseconds = 0.25;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double MinimumBackgroundMilliseconds = 0.03;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double SafetyMilliseconds = 0.75;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double SmoothingAlpha = 0.15;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaximumWorkers = 8;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinimumWorkers = 1;
};

struct WHFRAMEWORK_API FVoxelTerrainFrameObservation
{
	double GameThreadMilliseconds = 0.0;
	double TerrainGameThreadMilliseconds = 0.0;
	double RenderThreadMilliseconds = 0.0;
	double GPUMilliseconds = 0.0;
};

struct WHFRAMEWORK_API FVoxelTerrainFrameAllocation
{
	double TotalMilliseconds = 0.0;
	double NearMinimumMilliseconds = 0.0;
	double BackgroundMinimumMilliseconds = 0.0;
	double BackgroundScale = 0.0;
	double ProxyScale = 0.0;
	double SurfaceScale = 0.0;
	double MacroScale = 0.0;
	int32 MaximumWorkers = 1;
	bool bBaseFrameOverloaded = false;
	bool bInvalidTiming = false;
};

// 全 Terrain 仅计算一次配额；下游必须共享同一截止时间。
class WHFRAMEWORK_API FVoxelTerrainFrameBudget
{
public:
	bool Configure(const FVoxelTerrainFrameBudgetSettings& InSettings, FString& OutError);
	FVoxelTerrainFrameAllocation Observe(const FVoxelTerrainFrameObservation& InObservation);
	void Reset();

private:
	FVoxelTerrainFrameBudgetSettings Settings;
	double LastValidGameThreadMilliseconds = 0.0;
	double SmoothedGameThreadMilliseconds = 0.0;
	double SmoothedNonTerrainMilliseconds = 0.0;
	double SmoothedRenderMilliseconds = 0.0;
	double SmoothedGPUMilliseconds = 0.0;
	bool bHasObservation = false;
};
