#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"

/** 项目 Worker 只观察实际读缺失；身份、订阅和重试由 GT 事实所有者管理。 */
class WHFRAMEWORK_API FVoxelGenerationPlanningReadScope
{
public:
	using FObserver = TFunction<void(EVoxelGenerationPlanKind, const FIntVector&)>;

	explicit FVoxelGenerationPlanningReadScope(FObserver InObserver,
		const FVoxelTaskStamp* InStamp = nullptr, const TAtomic<bool>* InCancel = nullptr);
	~FVoxelGenerationPlanningReadScope();
	FVoxelGenerationPlanningReadScope(const FVoxelGenerationPlanningReadScope&) = delete;
	FVoxelGenerationPlanningReadScope& operator=(const FVoxelGenerationPlanningReadScope&) = delete;

	static bool IsActive();
	static bool CanPublish();
	static FString PublishBlockError();
	static const FVoxelTaskStamp* GetTaskStamp();
	static void Missing(EVoxelGenerationPlanKind InKind, const FIntVector& InCoordinate);

private:
	FObserver Observer;
	const FVoxelTaskStamp* Stamp = nullptr;
	const TAtomic<bool>* Cancel = nullptr;
	bool bMissing = false;
	FVoxelGenerationPlanningReadScope* Previous = nullptr;
	static thread_local FVoxelGenerationPlanningReadScope* Active;
};

/** 由世界 Emerge 所有者绑定，不让项目持有裸 Coordinator 或越线程修改事实。 */
struct WHFRAMEWORK_API FVoxelGenerationPlanningServices
{
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TFunction<FVoxelGenerationDependencyStatus(TConstArrayView<FVoxelGenerationPlanDependency>,
		EVoxelWorkClass, const FVoxelTerrainServicePriority&, const FVoxelGenerationConsumerKey&)> Ensure;
	TFunction<void(const FVoxelGenerationConsumerKey&)> Release;
	TFunction<void(const FVoxelTaskStamp&)> Cancel;
	TFunction<TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>(
		TConstArrayView<FVoxelGenerationPlanDependency>, const FIntPoint&)> Retain;
};
