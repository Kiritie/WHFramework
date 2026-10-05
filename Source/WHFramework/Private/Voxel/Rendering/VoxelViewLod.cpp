#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"

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
	const FVoxelViewSettings& InSettings)
{
	// 覆盖扩大或采样达到上限后，误差仍以网格实际使用的步长为准。
	return FMath::Max(MinimumVoxelProxyLevel(InSettings.Lod), ResolveLevel(InDistanceCells, InSource,
		InSettings.Lod.MaximumVoxelProxyLevel, InSettings.Lod.TargetScreenErrorPixels,
		[](const uint8 Level) { return static_cast<double>(FVoxelViewKey{{}, Level}.GetSampleStep()); }));
}

bool VoxelViewLod::ValidateTopology(const FVoxelLodTopologySettings& InSettings, FString& OutError)
{
	if (InSettings.MaximumFineLevel > 4 || InSettings.MaximumVoxelProxyLevel < 1 || InSettings.MaximumVoxelProxyLevel > 20 ||
		InSettings.MaximumVoxelProxySamplesPerAxis < 32 || InSettings.MaximumVoxelProxySamplesPerAxis > 128 ||
		InSettings.MaximumHeightfieldSamplesPerAxis < 16 || InSettings.MaximumHeightfieldSamplesPerAxis > 256 ||
		!FMath::IsPowerOfTwo(InSettings.MaximumVoxelProxySamplesPerAxis) || !FMath::IsPowerOfTwo(InSettings.MaximumHeightfieldSamplesPerAxis) ||
		InSettings.MaximumPageEstimatedTriangles < 524288 || InSettings.MaximumPageEstimatedTriangles > 4194304 ||
		InSettings.MaximumPageEstimatedMeshMegabytes < 128 || InSettings.MaximumPageEstimatedMeshMegabytes > 1024 ||
		!FMath::IsFinite(InSettings.MaximumPageEstimatedBuildMilliseconds) || InSettings.MaximumPageEstimatedBuildMilliseconds < 8.0f ||
		InSettings.MaximumPageEstimatedBuildMilliseconds > 100.0f || InSettings.MaximumSurfaceSpatialLevel > 20 || InSettings.MaximumMacroSpatialLevel > 20 ||
		!FMath::IsFinite(InSettings.TargetScreenErrorPixels) || InSettings.TargetScreenErrorPixels < 0.1f)
	{
		OutError = TEXT("Invalid absolute terrain level limit or screen error policy");
		return false;
	}
	OutError.Reset();
	return true;
}

uint8 VoxelViewLod::MinimumVoxelProxyLevel(const FVoxelLodTopologySettings& InSettings)
{
	return 1;
}

FVoxelResolvedViewRanges VoxelViewLod::ResolveViewRanges(const FVoxelStreamingSource& InSource)
{
	FVoxelResolvedViewRanges Result;
	Result.Fine = FMath::Max(0, InSource.View.FineRadiusCells);
	Result.Proxy = FMath::Max(Result.Fine, InSource.View.VoxelProxyRadiusCells);
	Result.Surface = FMath::Max(Result.Proxy, InSource.View.SurfaceRadiusCells);
	Result.Macro = FMath::Max(Result.Surface, InSource.View.MacroRadiusCells);
	return Result;
}

int32 VoxelViewLod::ResolveReplanCells(const int32 InRadius, const float InFraction, const int32 InMinimumCells)
{
	return VoxelStreamingGeometry::ResolveReplanStep(FMath::Max(1, InRadius),
		FMath::Clamp(double(InFraction), 0.0, 1.0), FMath::Max(1, InMinimumCells));
}

FVoxelResolvedViewRanges VoxelViewLod::ResolveReplanSteps(
	const FVoxelStreamingSource& InSource, const FVoxelViewSettings& InSettings, const FVoxelSourceInterest* InAccepted)
{
	(void)InAccepted;
	const FVoxelResolvedViewRanges Ranges = ResolveViewRanges(InSource);
	const float Fraction = InSettings.Planning.ReplanDistanceFraction;
	return {ResolveReplanCells(Ranges.Fine, Fraction, InSource.View.InitialChunkSideCells), ResolveReplanCells(Ranges.Proxy, Fraction, 16),
		ResolveReplanCells(Ranges.Surface, Fraction, 16), ResolveReplanCells(Ranges.Macro, Fraction, 16)};
}

int32 VoxelViewLod::MovementCriticalFineRadius(const FVoxelStreamingSource& InSource)
{
	const double Travel = InSource.VelocityCellsPerSecond.Size2D() * FMath::Max(0.0f, InSource.View.FinePredictionSeconds);
	return FMath::Max3(InSource.View.FineLevel0RadiusCells, InSource.CollisionRadius,
		static_cast<int32>(FMath::CeilToInt(Travel)) + InSource.CollisionRadius);
}

int32 VoxelViewLod::SurfaceSide(const FVoxelLodTopologySettings& InSettings, const uint8 InLevel)
{
	return 16 << InLevel;
}

int32 VoxelViewLod::SurfaceStep(const FVoxelLodTopologySettings& InSettings, const uint8 InLevel)
{
	return 1 << FMath::Max(0, int32(InLevel) - 5);
}

int32 VoxelViewLod::MacroSide(const FVoxelLodTopologySettings& InSettings, const uint8 InLevel)
{
	return 16 << InLevel;
}

int32 VoxelViewLod::MacroStep(const FVoxelLodTopologySettings& InSettings, const uint8 InLevel)
{
	return 1 << FMath::Max(0, int32(InLevel) - 5);
}

// 纹理尺度只由采样等级决定，不能随贪心合并面的宽高变化。
double VoxelViewLod::TexturePeriodCells(const int32 InStep, const double InMaximumStretchCells)
{
	return FMath::Clamp(static_cast<double>(InStep), 1.0, FMath::Max(1.0, InMaximumStretchCells));
}

bool VoxelViewLod::ValidatePerformanceBudget(const FVoxelStreamingPerformanceBudget& InBudget, FString& OutError)
{
	const double Total = InBudget.TerrainPublishMilliseconds;
	const double Floor = InBudget.PublishOperationEstimateFloorMilliseconds;
	const double Ceiling = InBudget.PublishOperationEstimateCeilingMilliseconds;
	if (!FMath::IsFinite(Total) || Total < 0.1f || !FMath::IsFinite(Floor) || Floor < 0.01f || Floor > Total * 0.5 || !FMath::IsFinite(Ceiling) ||
		Ceiling < Floor || Ceiling > 4.0 || InBudget.CoveragePreparationBatchSize < 1 || InBudget.CoveragePreparationBatchSize > 128 ||
		InBudget.PublishAdmissionBatchSize < 1 || InBudget.PublishAdmissionBatchSize > 32)
	{
		OutError = TEXT("Invalid terrain publish budget: microstep floor must fit within half the frame budget, batches and estimates must be bounded");
		return false;
	}
	OutError.Reset();
	return true;
}
