#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

struct WHFRAMEWORK_API FVoxelFrameTimings
{
	double AdmissionMilliseconds = 0.0;
	double ResultApplyMilliseconds = 0.0;
	double PublishPrepareMilliseconds = 0.0;
	double PublishCommitMilliseconds = 0.0;
	double RetireMilliseconds = 0.0;
	double CoverageMilliseconds = 0.0;
};

struct WHFRAMEWORK_API FVoxelRepresentationReadiness
{
	int32 PresentedFine = 0;
	int32 PresentedVoxelProxy = 0;
	int32 PresentedSurface = 0;
	int32 PresentedMacro = 0;
};

struct WHFRAMEWORK_API FVoxelViewKey
{
	FIntVector Coordinate = FIntVector::ZeroValue;
	uint8 Level = 0;

	static constexpr uint8 SampleLevelOffset = 1;

	int32 GetStep() const;
	// 覆盖层级比采样层级高一级，同等体素细度覆盖双倍边长；近景仍为 16 格。
	int32 GetSampleStep() const
	{
		const int32 SampleLevel = FMath::Max(0, static_cast<int32>(Level) - SampleLevelOffset);
		return FMath::Min(1 << SampleLevel, 8);
	}
	int32 GetGridSide() const { return GetSide() / GetSampleStep(); }
	int32 GetSide() const;
	FVoxelGenerationBounds GetBounds() const;
	FVoxelViewKey GetParent() const;
	void GetChildren(TArray<FVoxelViewKey>& OutChildren) const;
	bool operator==(const FVoxelViewKey& InOther) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelViewKey& InKey)
{
	return HashCombineFast(::GetTypeHash(InKey.Coordinate), ::GetTypeHash(InKey.Level));
}

struct WHFRAMEWORK_API FVoxelSurfaceTileKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	uint8 Level = 0;

	bool operator==(const FVoxelSurfaceTileKey& InOther) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelSurfaceTileKey& InKey)
{
	return HashCombineFast(::GetTypeHash(InKey.Coordinate), ::GetTypeHash(InKey.Level));
}

struct WHFRAMEWORK_API FVoxelMacroTileKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	uint8 Level = 0;

	bool operator==(const FVoxelMacroTileKey& InOther) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelMacroTileKey& InKey)
{
	return HashCombineFast(::GetTypeHash(InKey.Coordinate), ::GetTypeHash(InKey.Level));
}

enum class EVoxelViewAdmissionKind : uint8
{
	Fine = 0,
	VoxelProxy,
	Surface,
	Macro
};

struct FVoxelViewAdmission
{
	EVoxelViewAdmissionKind Kind = EVoxelViewAdmissionKind::Fine;
	double DistanceCells = 0.0;
	int32 Priority = 0;
	FIntVector FineKey = FIntVector::ZeroValue;
	FVoxelViewKey ProxyKey;
	FVoxelSurfaceTileKey SurfaceKey;
	FVoxelMacroTileKey MacroKey;

	bool operator<(const FVoxelViewAdmission& Other) const
	{
		if (Priority != Other.Priority) return Priority < Other.Priority;
		return DistanceCells != Other.DistanceCells ? DistanceCells < Other.DistanceCells :
			static_cast<uint8>(Kind) < static_cast<uint8>(Other.Kind);
	}
};
