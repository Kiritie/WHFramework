#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelFineSpatial.h"
#include "Voxel/Rendering/VoxelTerrainFrameBudget.h"
#include "VoxelFineStreamingPolicy.generated.h"

struct FVoxelTerrainResourceLimits;
struct FVoxelStreamingSourceView;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelFineStreamingPolicy
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PolicyVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "32", ClampMax = "64"))
	int32 ShardSideCells = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "4"))
	uint8 MaximumPageLevel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "32"))
	int32 FirstMergeDistanceCells = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.05"))
	double MinimumPreparationSeconds = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.05"))
	double MaximumPreparationSeconds = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MaximumWindowShards = 32768;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaximumMeshPieceMegabytes = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "1024"))
	int32 MaximumShardGeometryMegabytes = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MaximumInputGeometryMegabytes = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MaximumWorkingGeometryMegabytes = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MaximumStagedGeometryMegabytes = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "64"))
	int32 MaximumResidentGeometryMegabytes = 512;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 ResidentGeometryOverlapMegabytes = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 ReservedNearWorkers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	double MaximumBackgroundWaitSeconds = 0.5;

	FVoxelFineWindowSettings WindowSettings(const FVoxelStreamingSourceView& InView) const;
	int32 MinimumRadiusCells(const FVoxelStreamingSourceView& InView) const;
	FVoxelTerrainResourceLimits GeometryLimits() const;
	bool Validate(const FVoxelStreamingSourceView& InView, FString& OutError) const;
	bool operator==(const FVoxelFineStreamingPolicy& InOther) const = default;
};
