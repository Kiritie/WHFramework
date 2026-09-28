#pragma once

#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"
#include "VoxelViewProfile.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelViewProfile : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UVoxelViewProfile();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View")
	FVoxelStreamingSourceView DefaultSourceView;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float PlayableFineRadiusFraction = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 MacroRadiusCells = 16000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1"))
	uint8 MaximumMacroLevel = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1"))
	int32 MaximumSurfaceTiles = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1"))
	int32 MaximumMacroTiles = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1", ClampMax = "16"))
	float MaximumTextureStretchCells = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多准入的精细区块数（包含直接确认的全空气区块）；已就绪、已排队或等待依赖的区块不占配额。"))
	int32 FineAdmissionPerFrame = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多准入的体素LOD区块数。"))
	int32 VoxelProxyAdmissionPerFrame = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多准入的Surface瓦片数，包含该瓦片的水面。"))
	int32 SurfaceAdmissionPerFrame = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多准入的Macro远景瓦片数。"))
	int32 MacroAdmissionPerFrame = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "1024", ToolTip = "每帧最多推进的数据区块数；生成入队、存档叠加入队或自然数据确认成功才计数。"))
	int32 DataAdmissionPerFrame = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|ResultApply", meta = (ClampMin = "1", ClampMax = "256"))
	int32 FineApplyPerFrame = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|ResultApply", meta = (ClampMin = "1", ClampMax = "256"))
	int32 VoxelProxyApplyPerFrame = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|ResultApply", meta = (ClampMin = "1", ClampMax = "256"))
	int32 SurfaceApplyPerFrame = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|ResultApply", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MacroApplyPerFrame = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多消费的后台任务结果数，包含数据、网格和碰撞等任务。"))
	int32 CompletedResultsPerFrame = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多消费的重结果数，同时受总结果数量及2毫秒软时间预算限制。"))
	int32 HeavyResultsPerFrame = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "0.1", ClampMax = "8.0", ToolTip = "每个候选提交阶段的主线程软时间预算（毫秒）；数据与可视网格分别计时，不含范围规划和前沿查找，单次操作不可抢占。"))
	float AdmissionMilliseconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|ResultApply", meta = (ClampMin = "0.1", ClampMax = "8.0"))
	float ResultApplyMilliseconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Publisher", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaxPublishComponentsPerFrame = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Publisher", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaxPublishGroupsPerFrame = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Publisher", meta = (ClampMin = "0.1", ClampMax = "8.0"))
	float PublishPrepareMilliseconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Publisher", meta = (ClampMin = "0.1", ClampMax = "8.0"))
	float PublishRetireMilliseconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Scheduler", meta = (ClampMin = "1", ClampMax = "16"))
	int32 CriticalReservedTasks = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Scheduler", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxConcurrentSurfaceTasks = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Scheduler", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxConcurrentMacroTasks = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Scheduler", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxConcurrentCoarseTerrainTasks = 3;
};
