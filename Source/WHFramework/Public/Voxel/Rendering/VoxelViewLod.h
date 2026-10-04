#pragma once

#include "CoreMinimal.h"

struct FVoxelStreamingSource;
struct FVoxelViewSettings;
struct FVoxelSourceInterest;
struct FVoxelLodTopologySettings;

struct WHFRAMEWORK_API FVoxelResolvedViewRanges
{
	int32 Fine = 0;
	int32 Proxy = 0;
	int32 Surface = 0;
	int32 Macro = 0;
};

namespace VoxelViewLod
{
	WHFRAMEWORK_API bool ValidateTopology(const FVoxelLodTopologySettings& InSettings, FString& OutError);
	WHFRAMEWORK_API uint8 MinimumVoxelProxyLevel(const FVoxelLodTopologySettings& InSettings);
	WHFRAMEWORK_API FVoxelResolvedViewRanges ResolveViewRanges(const FVoxelStreamingSource& InSource);
	WHFRAMEWORK_API FVoxelResolvedViewRanges ResolveReplanSteps(const FVoxelStreamingSource& InSource, const FVoxelViewSettings& InSettings, const FVoxelSourceInterest* InAccepted = nullptr);
	WHFRAMEWORK_API int32 ResolveReplanCells(int32 InRadius, float InFraction, int32 InMinimumCells);
	WHFRAMEWORK_API int32 MovementCriticalFineRadius(const FVoxelStreamingSource& InSource);
	WHFRAMEWORK_API int32 SurfaceSide(const FVoxelLodTopologySettings& InSettings, uint8 InLevel);
	WHFRAMEWORK_API int32 SurfaceStep(const FVoxelLodTopologySettings& InSettings, uint8 InLevel);
	WHFRAMEWORK_API int32 MacroSide(const FVoxelLodTopologySettings& InSettings, uint8 InLevel);
	WHFRAMEWORK_API int32 MacroStep(const FVoxelLodTopologySettings& InSettings, uint8 InLevel);
	WHFRAMEWORK_API double TexturePeriodCells(int32 InStep, double InMaximumStretchCells);

	WHFRAMEWORK_API uint8 ResolveVoxelProxyLevel(
		int32 InDistanceCells,
		const FVoxelStreamingSource& InSource,
		const FVoxelViewSettings& InSettings);

	WHFRAMEWORK_API uint8 ResolveScreenErrorLevel(
		int32 InDistanceCells,
		int32 InBaseSampleStepCells,
		const FVoxelStreamingSource& InSource,
		uint8 InMaximumLevel,
		float InTargetScreenErrorPixels);
}
