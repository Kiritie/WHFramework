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
	const FVoxelViewSettings& InSettings)
{
	// 覆盖扩大或采样达到上限后，误差仍以网格实际使用的步长为准。
	return FMath::Max(MinimumVoxelProxyLevel(InSettings.Lod), ResolveLevel(InDistanceCells, InSource,
		InSettings.Lod.MaximumVoxelProxyLevel, InSettings.Lod.TargetScreenErrorPixels,
		[](const uint8 Level) { return static_cast<double>(FVoxelViewKey{{}, Level}.GetSampleStep()); }));
}

bool VoxelViewLod::ValidateTopology(const FVoxelLodTopologySettings& InSettings, FString& OutError)
{
	const int32 ProxySide = InSettings.VoxelProxyMinimumChunkSideCells;
	if (ProxySide < 32 || ProxySide % 16 != 0 || !FMath::IsPowerOfTwo(ProxySide / 16) ||
		InSettings.MaximumFineLevel > 4 || InSettings.MaximumVoxelProxyLevel > 8 ||
		InSettings.MaximumSurfaceLevel > 8 || InSettings.MaximumMacroLevel > 8 ||
		ProxySide > (16 << 8) ||
		InSettings.MaximumVoxelProxyLevel < FMath::FloorLog2(ProxySide / 16) ||
		!FMath::IsPowerOfTwo(InSettings.SurfaceMinimumTileSideCells) ||
		!FMath::IsPowerOfTwo(InSettings.SurfaceBaseSampleStepCells) ||
		!FMath::IsPowerOfTwo(InSettings.MacroMinimumTileSideCells) ||
		!FMath::IsPowerOfTwo(InSettings.MacroBaseSampleStepCells) ||
		InSettings.SurfaceMinimumTileSideCells < InSettings.SurfaceBaseSampleStepCells ||
		InSettings.MacroMinimumTileSideCells < InSettings.MacroBaseSampleStepCells ||
		InSettings.SurfaceMinimumTileSideCells > (MAX_int32 >> InSettings.MaximumSurfaceLevel) ||
		InSettings.MacroMinimumTileSideCells > (MAX_int32 >> InSettings.MaximumMacroLevel) ||
		InSettings.SurfaceMinimumTileSideCells / FMath::Max(1, InSettings.SurfaceBaseSampleStepCells) > 512 ||
		InSettings.MacroMinimumTileSideCells / FMath::Max(1, InSettings.MacroBaseSampleStepCells) > 512 ||
		!FMath::IsFinite(InSettings.TargetScreenErrorPixels) || InSettings.TargetScreenErrorPixels < 0.1f)
	{
		OutError = TEXT("Invalid voxel LOD topology: aligned sizes, sampling steps, levels and screen error are required");
		return false;
	}
	OutError.Reset();
	return true;
}

uint8 VoxelViewLod::MinimumVoxelProxyLevel(const FVoxelLodTopologySettings& InSettings)
{
	return static_cast<uint8>(FMath::FloorLog2(InSettings.VoxelProxyMinimumChunkSideCells / 16));
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
	const int32 Radius = FMath::Max(1, InRadius);
	const double Raw = FMath::Max(1.0, Radius * FMath::Clamp(static_cast<double>(InFraction), 0.0, 1.0));
	const int32 Lower = 1 << FMath::FloorLog2(FMath::Max(1, FMath::FloorToInt(Raw)));
	const int32 Upper = Lower <= MAX_int32 / 2 ? Lower << 1 : Lower;
	const int32 Quantized = FMath::Abs(Raw - Lower) <= FMath::Abs(Upper - Raw) ? Lower : Upper;
	return FMath::Clamp(FMath::Max(InMinimumCells, Quantized), 1, Radius);
}

FVoxelResolvedViewRanges VoxelViewLod::ResolveReplanSteps(const FVoxelStreamingSource& InSource, const FVoxelViewSettings& InSettings)
{
	const FVoxelResolvedViewRanges Ranges = ResolveViewRanges(InSource);
	const float Fraction = InSettings.Planning.ReplanDistanceFraction;
	return {
		ResolveReplanCells(Ranges.Fine, Fraction, 16),
		ResolveReplanCells(Ranges.Proxy, Fraction, InSettings.Lod.VoxelProxyMinimumChunkSideCells),
		ResolveReplanCells(Ranges.Surface, Fraction, InSettings.Lod.SurfaceMinimumTileSideCells),
		ResolveReplanCells(Ranges.Macro, Fraction, InSettings.Lod.MacroMinimumTileSideCells)
	};
}

int32 VoxelViewLod::MovementCriticalFineRadius(const FVoxelStreamingSource& InSource)
{
	const double Travel = InSource.VelocityCellsPerSecond.Size2D() * FMath::Max(0.0f, InSource.View.FinePredictionSeconds);
	return FMath::Max3(InSource.View.FineLevel0RadiusCells, InSource.CollisionRadius,
		static_cast<int32>(FMath::CeilToInt(Travel)) + InSource.CollisionRadius);
}

int32 VoxelViewLod::SurfaceSide(const FVoxelLodTopologySettings& InSettings, const uint8 InLevel)
{
	return InSettings.SurfaceMinimumTileSideCells << InLevel;
}

int32 VoxelViewLod::SurfaceStep(const FVoxelLodTopologySettings& InSettings, const uint8 InLevel)
{
	return InSettings.SurfaceBaseSampleStepCells << InLevel;
}

int32 VoxelViewLod::MacroSide(const FVoxelLodTopologySettings& InSettings, const uint8 InLevel)
{
	return InSettings.MacroMinimumTileSideCells << InLevel;
}

int32 VoxelViewLod::MacroStep(const FVoxelLodTopologySettings& InSettings, const uint8 InLevel)
{
	return InSettings.MacroBaseSampleStepCells << InLevel;
}

// 纹理尺度只由采样等级决定，不能随贪心合并面的宽高变化。
double VoxelViewLod::TexturePeriodCells(const int32 InStep, const double InMaximumStretchCells)
{
	return FMath::Clamp(static_cast<double>(InStep), 1.0, FMath::Max(1.0, InMaximumStretchCells));
}
