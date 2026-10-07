#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Task/VoxelTerrainDependencyPriority.h"

enum class EVoxelPlanDependencyState : uint8
{
	Missing,
	Queued,
	Ready,
	Failed
};

struct WHFRAMEWORK_API FVoxelGenerationPlanDependency
{
	EVoxelGenerationPlanKind Kind = EVoxelGenerationPlanKind::Hydrology;
	FVoxelHydrologyRegionKey HydrologyKey;
	FVoxelGenerationTileKey TileKey;
	FVoxelEcologyTileKey EcologyKey;
	uint64 StableHash = 0;

	bool operator==(const FVoxelGenerationPlanDependency& InOther) const;
	FIntVector Coordinate() const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelGenerationPlanDependency& InDependency)
{
	return HashCombineFast(GetTypeHash(static_cast<uint8>(InDependency.Kind)),
		GetTypeHash(InDependency.Coordinate()));
}

struct WHFRAMEWORK_API FVoxelGenerationDependencyStatus
{
	bool bReady = false;
	bool bFailed = false;
	FString Error;
};

class WHFRAMEWORK_API FVoxelGenerationPlanCoordinator
{
public:
	FVoxelGenerationPlanCoordinator(
		FVoxelTaskScheduler& InScheduler,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
		uint64 InWorldEpoch);

	bool GatherForBounds(
		const FVoxelGenerationBounds& InBounds,
		TArray<FVoxelGenerationPlanDependency>& OutDependencies,
		FString& OutError) const;
	// 每个事实只等待自己的采样闭包，远处事实未完成不阻塞本地计划。
	bool GatherPrerequisites(const FVoxelGenerationPlanDependency& InDependency,
		TArray<FVoxelGenerationPlanDependency>& OutDependencies, FString& OutError) const;

	TSharedRef<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> RetainDependencies(
		TConstArrayView<FVoxelGenerationPlanDependency> InDependencies,
		const FIntPoint& InCenter) const;

	FVoxelGenerationDependencyStatus Ensure(
		TConstArrayView<FVoxelGenerationPlanDependency> InDependencies,
		EVoxelWorkClass InWorkClass, const FVoxelTerrainServicePriority& InPriority,
		const FVoxelGenerationConsumerKey& InConsumer);
	FVoxelGenerationDependencyStatus Ensure(
		TConstArrayView<FVoxelGenerationPlanDependency> InDependencies,
		EVoxelWorkClass InWorkClass,
		int32 InSourcePriority,
		double InDistanceScore,
		double InForwardScore,
		const FVoxelGenerationConsumerKey& InConsumer);

	FVoxelGenerationDependencyStatus EnsureEnvironment(const FIntPoint& InCell);
	FVoxelGenerationDependencyStatus EnsureEnvironmentBounds(const FVoxelGenerationBounds& InBounds,
		EVoxelWorkClass InWorkClass, int32 InSourcePriority, double InDistanceScore, double InForwardScore);
	void ReleaseConsumer(const FVoxelGenerationConsumerKey& InConsumer);
	void ReleaseEnvironment(const FIntPoint& InCell);
	uint64 GetWaitingConsumers(EVoxelGenerationPlanKind InKind) const;

	void SetConsumerWakeCallback(TFunction<void(const FVoxelGenerationConsumerKey&)> InCallback);
	void Reset();

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelLocalPlanClosureTest;
	friend class FVoxelEnvironmentAttemptIsolationTest;
	friend class FVoxelGenerationGraphAdmissionTest;
#endif
	struct FDonation
	{
		EVoxelWorkClass WorkClass = EVoxelWorkClass::Background;
		FVoxelTerrainServicePriority Priority;
		uint64 SubscriptionId = 0;
	};
	struct FState
	{
		FGuid PriorityNode;
		FGuid NextRegistration;
		TArray<FVoxelGenerationPlanDependency> Prerequisites;
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> PrerequisiteRetention;
		EVoxelPlanDependencyState State = EVoxelPlanDependencyState::Missing;
		EVoxelWorkClass WorkClass = EVoxelWorkClass::Background;
		int32 SourcePriority = MAX_int32;
		double DistanceScore = MAX_dbl;
		double ForwardScore = 0.0;
		FVoxelTerrainServicePriority ServicePriority;
		FString Error;
		TMap<FVoxelGenerationConsumerKey, FDonation> Donations;
		uint64 AttemptId = 0;
		bool bInFlight = false;
		bool bRegistering = false;
	};
	struct FClosureRegistration
	{
		explicit FClosureRegistration(FVoxelGenerationPlanCoordinator& InOwner);
		~FClosureRegistration();
		void Add(FState& InState);
		void Commit();
		FVoxelGenerationPlanCoordinator& Owner;
		FGuid Head;
		bool bCommitted = false;
	};

	void ResolveDonations(FState& InOutState);
	void RemoveSubscription(const FVoxelGenerationPlanDependency& InDependency,
		const FVoxelGenerationConsumerKey& InConsumer, uint64 InSubscriptionId);
	void WakeConsumers(const FVoxelGenerationPlanDependency& InDependency);
	FVoxelGenerationConsumerKey EnvironmentConsumer(const FVoxelGenerationBounds& InBounds, bool bInColumn) const;
	bool IsCached(const FVoxelGenerationPlanDependency& InDependency) const;
	bool Queue(const FVoxelGenerationPlanDependency& InDependency, FState& InOutState);
	void RefreshQueuedPriorities();
	bool RegisterClosure(const FVoxelGenerationPlanDependency& InDependency, FClosureRegistration& InRegistration, FString& OutError);
	void AdvancePriorityDonations();
	void ForgetFact(const FVoxelGenerationPlanDependency& InDependency);
	bool ArePrerequisitesReady(const FState& InState) const;

	FVoxelTaskScheduler& Scheduler;
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	uint64 WorldEpoch = 0;
	TMap<FVoxelGenerationPlanDependency, FState> States;
	TMap<FVoxelGenerationConsumerKey, TMap<FVoxelGenerationPlanDependency, uint64>> ConsumerDependencies;
	FVoxelTerrainDependencyPriority PriorityGraph;
	TMap<FVoxelGenerationConsumerKey, FGuid> ConsumerPriorityNodes;
	TMap<FGuid, FVoxelGenerationPlanDependency> PriorityFacts;
	// 环境请求按空间键保持在途身份；释放后再次请求必须使用新代，拒绝旧唤醒。
	mutable TMap<FVoxelGenerationConsumerKey, FVoxelGenerationConsumerKey> EnvironmentAttempts;
	mutable uint64 NextEnvironmentAttempt = 0;
	TFunction<void(const FVoxelGenerationConsumerKey&)> ConsumerWakeCallback;
	FGuid EnvironmentOwnerId = FGuid::NewGuid();
	uint64 EnvironmentOwnerIncarnation = 1;
	uint64 NextSubscriptionId = 0;
	uint64 NextAttemptId = 0;
};
