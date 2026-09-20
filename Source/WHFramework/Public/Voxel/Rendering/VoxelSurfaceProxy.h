#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Runtime/VoxelChangeIndex.h"

enum EVoxelSurfaceFlags : uint8
{
	VoxelSurface_River = 1 << 0,
	VoxelSurface_Lake = 1 << 1,
	VoxelSurface_Ocean = 1 << 2,
	VoxelSurface_Coast = 1 << 3
};

struct WHFRAMEWORK_API FVoxelSurfaceTileData
{
	static constexpr int32 CellSide = 32;
	static constexpr int32 VertexSide = CellSide + 1;

	FVoxelSurfaceTileKey Key;
	int32 Side = VertexSide;
	int32 Step = 0;
	uint64 Revision = 0;
	TArray<int32> GroundZ;
	TArray<int32> WaterZ;
	TArray<uint16> SurfaceMaterial;
	TArray<uint16> Biome;
	TArray<uint8> Flags;

	int32 GetTileSide() const
	{
		return CellSide *
			Step;
	}

	int32 GetVertexCount() const
	{
		return Side *
			Side;
	}
};

struct WHFRAMEWORK_API FVoxelOverlaySnapshot
{
	FIntVector Section = FIntVector::ZeroValue;
	uint64 Revision = 0;
	TMap<int32, FVoxelBlockState> Blocks;
};

class WHFRAMEWORK_API IVoxelOverlaySource
{
public:
	virtual ~IVoxelOverlaySource() = default;

	virtual void EnumerateModifiedSections(
		const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutSections) const = 0;

	virtual bool ReadOverlay(
		const FIntVector& InSection,
		FVoxelOverlaySnapshot& OutOverlay) const = 0;
};

class WHFRAMEWORK_API FVoxelSurfaceProxyBuilder
{
public:
	FVoxelSurfaceProxyBuilder(
		TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
		const FVoxelGenerationSettings& InSettings,
		const IVoxelOverlaySource& InOverlaySource);

	bool Build(
		const FVoxelSurfaceTileKey& InKey,
		FVoxelSurfaceTileData& OutData,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	bool ApplyModifiedSurface(
		const FVoxelSurfaceTileKey& InKey,
		FVoxelSurfaceTileData& InOutData,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

private:
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	FVoxelGenerationSettings Settings;
	const IVoxelOverlaySource& OverlaySource;
};
