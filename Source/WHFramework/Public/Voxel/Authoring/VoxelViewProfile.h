#pragma once

#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"
#include "VoxelViewProfile.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelLodTopologySettings
{
	GENERATED_BODY()

	// Fine Page Level 是 Presenter 级别；F0 固定等于 16-cell Section。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0", ClampMax = "4"))
	uint8 MaximumFineLevel = 3;


	// 所有粗表示都使用固定 16-cell 基准的绝对空间级；这里只配置上限。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1", ClampMax = "20"))
	uint8 MaximumVoxelProxyLevel = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "32", ClampMax = "128"))
	int32 MaximumVoxelProxySamplesPerAxis = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "16", ClampMax = "256"))
	int32 MaximumHeightfieldSamplesPerAxis = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "524288", ClampMax = "4194304"))
	int32 MaximumPageEstimatedTriangles = 524288;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "128", ClampMax = "1024"))
	int32 MaximumPageEstimatedMeshMegabytes = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "8", ClampMax = "100"))
	float MaximumPageEstimatedBuildMilliseconds = 8.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0", ClampMax = "20"))
	uint8 MaximumSurfaceSpatialLevel = 8;



	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0", ClampMax = "20"))
	uint8 MaximumMacroSpatialLevel = 12;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0.1"))
	float TargetScreenErrorPixels = 8.0f;
	bool operator==(const FVoxelLodTopologySettings& InOther) const = default;

};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelStreamingReadinessPolicy
{
	GENERATED_BODY()

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WarmupDataRadiusFraction = 1.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WarmupCollisionRadiusFraction = 1.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float PlayableFineRadiusFraction = 0.75f;
	bool operator==(const FVoxelStreamingReadinessPolicy& InOther) const = default;

};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelStreamingPlanningPolicy
{
	GENERATED_BODY()

	// 每层 Replan 步长 = 自己外半径 * Fraction，再按 2^N 量化。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReplanDistanceFraction = 0.0625f;

	// Generation Cache 只决定缓存保留，不改变可见范围。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GenerationCacheMarginFraction = 0.125f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PlanCacheMarginFraction = 0.125f;
	bool operator==(const FVoxelStreamingPlanningPolicy& InOther) const = default;

};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelStreamingPerformanceBudget
{
	GENERATED_BODY()

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 FineAdmissionPerFrame = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "128"))
	int32 CoveragePreparationBatchSize = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "32"))
	int32 PublishAdmissionBatchSize = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float PublishOperationEstimateFloorMilliseconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.05", ClampMax = "4.0"))
	float PublishOperationEstimateCeilingMilliseconds = 1.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 VoxelProxyAdmissionPerFrame = 12;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 SurfaceAdmissionPerFrame = 8;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 MacroAdmissionPerFrame = 8;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 DataAdmissionPerFrame = 128;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 FineApplyPerFrame = 12;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 VoxelProxyApplyPerFrame = 8;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 SurfaceApplyPerFrame = 4;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 MacroApplyPerFrame = 4;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 CompletedResultsPerFrame = 32;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 HeavyResultsPerFrame = 8;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0.1"))
	float AdmissionMilliseconds = 2.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0.1"))
	float ResultApplyMilliseconds = 2.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 MaxPublishComponentsPerFrame = 16;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 MaxPublishGroupsPerFrame = 2;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "0.1"))
	float TerrainPublishMilliseconds = 4.0f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 CriticalReservedTasks = 4;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 MaxConcurrentSurfaceTasks = 2;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 MaxConcurrentMacroTasks = 2;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (ClampMin = "1"))
	int32 MaxConcurrentCoarseTerrainTasks = 3;
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelViewProfile : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UVoxelViewProfile();

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Voxel|View")
	FVoxelStreamingSourceView DefaultSourceView;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Voxel|View")
	FVoxelLodTopologySettings Lod;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Voxel|View")
	FVoxelStreamingReadinessPolicy Readiness;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Voxel|View")
	FVoxelStreamingPlanningPolicy Planning;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Voxel|View")
	FVoxelStreamingPerformanceBudget Budget;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Voxel|View",
		meta = (ClampMin = "1", ClampMax = "16"))
	float MaximumTextureStretchCells = 8.0f;
};
