#include "Voxel/Rendering/VoxelViewLod.h"

#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

uint8 VoxelViewLod::ResolveScreenErrorLevel(
	const int32 InDistanceCells,
	const int32 InBaseSampleStepCells,
	const FVoxelStreamingSource& InSource,
	const FVoxelViewSettings& InSettings,
	const uint8 InMaximumLevel)
{
	const double Distance =
		FMath::Max(
			1,
			InDistanceCells);

	const double BaseStep =
		FMath::Max(
			1,
			InBaseSampleStepCells);

	const double HalfFov =
		FMath::DegreesToRadians(
			FMath::Clamp(
				InSource.
					VerticalFovDegrees,
				1.0f,
				179.0f) *
				0.5f);

	const double ProjectionScale =
		FMath::Max(
			1,
			InSource.
				ViewportHeightPixels) /
		(2.0 *
		 FMath::Tan(
			 HalfFov));

	const double TargetPixels =
		FMath::Max(
			0.1f,
			InSettings.
				TargetScreenErrorPixels);

	uint8 Level = 0;

	while (Level <
		InMaximumLevel)
	{
		const uint8 CandidateLevel =
			Level + 1;

		const double SampleStepCells =
			BaseStep *
			static_cast<double>(
				1u <<
				CandidateLevel);

		const double ProjectedErrorPixels =
			SampleStepCells *
			ProjectionScale /
			Distance;

		if (ProjectedErrorPixels >
			TargetPixels)
		{
			break;
		}

		Level =
			CandidateLevel;
	}

	return Level;
}

// 纹理尺度只由采样等级决定，不能随贪心合并面的宽高变化。
double VoxelViewLod::TexturePeriodCells(const int32 InStep, const double InMaximumStretchCells)
{
	return FMath::Clamp(static_cast<double>(InStep), 1.0, FMath::Max(1.0, InMaximumStretchCells));
}
