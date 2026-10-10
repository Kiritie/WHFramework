#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"

/** 原请求的资源身份；不改变事实读取范围的 ready-only 或发布权限。 */
class WHFRAMEWORK_API FVoxelGenerationConsumerScope
{
public:
	explicit FVoxelGenerationConsumerScope(const FVoxelGenerationConsumerKey& InConsumer);
	~FVoxelGenerationConsumerScope();
	FVoxelGenerationConsumerScope(const FVoxelGenerationConsumerScope&) = delete;
	FVoxelGenerationConsumerScope& operator=(const FVoxelGenerationConsumerScope&) = delete;

	static const FVoxelGenerationConsumerKey* GetConsumerKey();

private:
	FVoxelGenerationConsumerKey Consumer;
	FVoxelGenerationConsumerScope* Previous = nullptr;
	static FVoxelGenerationConsumerScope*& ActiveScopeSlot();
};

/** 项目 Worker 只观察实际读缺失；身份、订阅和重试由 GT 事实所有者管理。 */
class WHFRAMEWORK_API FVoxelGenerationPlanningReadScope
{
public:
	using FObserver = TFunctionRef<void(EVoxelGenerationPlanKind, const FIntVector&)>;

	// Observer及其callable由调用栈持有，必须比Scope更晚析构。
	explicit FVoxelGenerationPlanningReadScope(const FObserver* InObserver = nullptr,
		const FVoxelTaskStamp* InStamp = nullptr, const TAtomic<bool>* InCancel = nullptr,
		const FVoxelGenerationConsumerKey* InConsumer = nullptr, bool bInRequireReadyPlans = true);
	~FVoxelGenerationPlanningReadScope();
	FVoxelGenerationPlanningReadScope(const FVoxelGenerationPlanningReadScope&) = delete;
	FVoxelGenerationPlanningReadScope& operator=(const FVoxelGenerationPlanningReadScope&) = delete;

	static bool IsActive();
	static bool RequiresReadyPlans();
	static bool CanPublish();
	static FString PublishBlockError();
	static const FVoxelTaskStamp* GetTaskStamp();
	static const FVoxelGenerationConsumerKey* GetConsumerKey();
	static void Missing(EVoxelGenerationPlanKind InKind, const FIntVector& InCoordinate);

private:
	const FObserver* Observer = nullptr;
	const FVoxelTaskStamp* Stamp = nullptr;
	const TAtomic<bool>* Cancel = nullptr;
	const FVoxelGenerationConsumerKey* Consumer = nullptr;
	bool bRequireReadyPlans = true;
	bool bMissing = false;
	FVoxelGenerationPlanningReadScope* Previous = nullptr;
	static FVoxelGenerationPlanningReadScope*& ActiveScopeSlot();
};

/** 由世界 Emerge 所有者绑定，不让项目持有裸 Coordinator 或越线程修改事实。 */
struct WHFRAMEWORK_API FVoxelGenerationPlanningServices
{
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> TaskResources;
	FVoxelGenerationConsumerKey TaskControlConsumer;
	FVoxelGenerationConsumerKey RetentionConsumer;
	TFunction<FVoxelGenerationDependencyStatus(TConstArrayView<FVoxelGenerationPlanDependency>,
		EVoxelWorkClass, const FVoxelTerrainServicePriority&, const FVoxelGenerationConsumerKey&)> Ensure;
	TFunction<void(const FVoxelGenerationConsumerKey&)> Release;
	TFunction<void(const FVoxelTaskStamp&)> Cancel;
	TFunction<bool(TConstArrayView<FVoxelGenerationPlanDependency>, const FIntPoint&, const FVoxelGenerationConsumerKey&,
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>&, FString&)> Retain;
};
