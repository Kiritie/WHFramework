#include "Voxel/Rendering/VoxelViewLod.h"

#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

namespace
{
	uint8 ResolveLevel(
		const int32 InDistanceCells,
		const FVoxelStreamingSource& InSource,
		const uint8 InMaximumLevel,
		const float InTargetScreenErrorPixels,
		TFunctionRef<double(uint8)> InSampleStep)
	{
		const double Distance = FMath::Max(1, InDistanceCells);
		const double HalfFov = FMath::DegreesToRadians(
			FMath::Clamp(InSource.VerticalFovDegrees, 1.0f, 179.0f) * 0.5f);
		const double ProjectionScale = FMath::Max(1, InSource.ViewportHeightPixels) /
			(2.0 * FMath::Tan(HalfFov));
		const double TargetPixels = FMath::Max(0.1f, InTargetScreenErrorPixels);
		uint8 Level = 0;
		while (Level < InMaximumLevel)
		{
			const uint8 Candidate = Level + 1;
			if (InSampleStep(Candidate) * ProjectionScale / Distance > TargetPixels) break;
			Level = Candidate;
		}
		return Level;
	}
}

uint8 VoxelViewLod::ResolveScreenErrorLevel(
	const int32 InDistanceCells,
	const int32 InBaseSampleStepCells,
	const FVoxelStreamingSource& InSource,
	const uint8 InMaximumLevel,
	const float InTargetScreenErrorPixels)
{
	return ResolveLevel(InDistanceCells, InSource, InMaximumLevel, InTargetScreenErrorPixels,
		[InBaseSampleStepCells](const uint8 Level)
		{
			return FMath::Max(1, InBaseSampleStepCells) * static_cast<double>(1u << Level);
		});
}

uint8 VoxelViewLod::ResolveVoxelProxyLevel(
	const int32 InDistanceCells,
	const FVoxelStreamingSource& InSource,
	const uint8 InMaximumLevel,
	const float InTargetScreenErrorPixels)
{
	// 覆盖扩大或采样达到上限后，误差仍以网格实际使用的步长为准。
	return ResolveLevel(InDistanceCells, InSource, InMaximumLevel, InTargetScreenErrorPixels,
		[](const uint8 Level) { return static_cast<double>(FVoxelViewKey{{}, Level}.GetSampleStep()); });
}

// 纹理尺度只由采样等级决定，不能随贪心合并面的宽高变化。
double VoxelViewLod::TexturePeriodCells(const int32 InStep, const double InMaximumStretchCells)
{
	return FMath::Clamp(static_cast<double>(InStep), 1.0, FMath::Max(1.0, InMaximumStretchCells));
}
