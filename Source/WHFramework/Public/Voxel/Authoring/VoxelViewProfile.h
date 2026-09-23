#pragma once

#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "VoxelViewProfile.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelViewProfile : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UVoxelViewProfile();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Warmup", meta = (ClampMin = "0"))
	int32 WarmupDataRadiusCentimeters = 600;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Warmup", meta = (ClampMin = "0"))
	int32 WarmupCollisionRadiusCentimeters = 600;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 FineRadiusCentimeters = 8000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Warmup", meta = (ClampMin = "0.01", ClampMax = "1.0", ToolTip = "世界达到可游玩状态前，精细网格必须完成的水平半径占Fine Radius的比例。1/3表示先铺满中心三分之一半径。"))
	float PlayableFineRadiusFraction = 1.0f / 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Warmup", meta = (ClampMin = "0.001", ClampMax = "1.0", ToolTip = "流送源移动达到Fine Radius的这一比例时重新规划。为避免碰撞范围落后，实际阈值最多为碰撞半径的一半。"))
	float StreamingReplanFineRadiusFraction = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0", ToolTip = "Fine精细网格相对观察者的垂直半径。较小值可避免地表下方不可见区块进入Fine构建。"))
	int32 FineVerticalRadiusCentimeters = 1600;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 FinePreloadCentimeters = 800;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1", ClampMax = "16"))
	float MaximumTextureStretchCells = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 VoxelProxyRadiusCentimeters = 80000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 SurfaceRadiusCentimeters = 800000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0"))
	int32 MacroRadiusCentimeters = 3000000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "0.1"))
	float TargetScreenErrorPixels = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1", ClampMax = "8"))
	uint8 MaximumVoxelProxyLevel = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1", ClampMax = "8"))
	uint8 MaximumSurfaceLevel = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View", meta = (ClampMin = "1", ClampMax = "8"))
	uint8 MaximumMacroLevel = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Budget", meta = (ClampMin = "32", ClampMax = "2048"))
	int32 MaximumSurfaceTilesPerSource = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|Budget", meta = (ClampMin = "32", ClampMax = "2048"))
	int32 MaximumMacroTilesPerSource = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多准入的精细区块数（包含直接确认的全空气区块）；已就绪、已排队或等待依赖的区块不占配额。"))
	int32 FineBuildsPerFrame = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多准入的体素LOD区块数。"))
	int32 VoxelProxyBuildsPerFrame = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多准入的Surface瓦片数，包含该瓦片的水面。"))
	int32 SurfaceBuildsPerFrame = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多准入的Macro远景瓦片数。"))
	int32 MacroBuildsPerFrame = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "1024", ToolTip = "每帧最多推进的数据区块数；生成入队、存档叠加入队或自然数据确认成功才计数。"))
	int32 DataBuildsPerFrame = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多消费的后台任务结果数，包含数据、网格和碰撞等任务。"))
	int32 CompletedResultsPerFrame = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "每帧最多消费的重结果数，同时受总结果数量及2毫秒软时间预算限制。"))
	int32 HeavyResultsPerFrame = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View|FrameBudget", meta = (ClampMin = "0.1", ClampMax = "8.0", ToolTip = "每个候选提交阶段的主线程软时间预算（毫秒）；数据与可视网格分别计时，不含范围规划和前沿查找，单次操作不可抢占。"))
	float BuildAdmissionMilliseconds = 2.0f;
};
