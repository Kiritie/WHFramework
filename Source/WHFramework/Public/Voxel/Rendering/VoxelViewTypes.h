#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

struct WHFRAMEWORK_API FVoxelViewKey
{
	FIntVector Coordinate = FIntVector::ZeroValue;
	uint8 Level = 0;

	int32 GetStep() const;
	// 覆盖尺度保持不变，内部采样格最多为四个基础体素。
	int32 GetSampleStep() const { return FMath::Min(GetStep(), 4); }
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
