#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelFineSpatial.h"
#include "Voxel/Rendering/VoxelTerrainFrameBudget.h"
#include "VoxelFineStreamingPolicy.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelFineStreamingPolicy
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PolicyVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 InteractiveRadiusCells = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 InteractiveVerticalHalfExtentCells = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 CollisionGuardRadiusCells = 80;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "32"))
	int32 ResidencyHorizontalHalfExtentCells = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "32"))
	int32 ResidencyVerticalHalfExtentCells = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "32", ClampMax = "64"))
	int32 ShardSideCells = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "32"))
	int32 HorizontalShiftStepCells = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "32"))
	int32 VerticalShiftStepCells = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 GuardMarginCells = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "4"))
	uint8 MaximumPageLevel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "32"))
	int32 FirstMergeDistanceCells = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	double PredictionSeconds = 0.75;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "64"))
	int32 MaximumResidentGeometryMegabytes = 512;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 ReservedNearWorkers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	double MaximumBackgroundWaitSeconds = 0.5;

	FVoxelFineWindowSettings WindowSettings() const;
	bool Validate(FString& OutError) const;
	bool operator==(const FVoxelFineStreamingPolicy& InOther) const = default;
};
