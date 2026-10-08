#pragma once

#include "Components/PrimitiveComponent.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "VoxelCollisionComponent.generated.h"

class UBodySetup;
class FVoxelTerrainResources;
class FVoxelTerrainResourceLease;
class FVoxelTaskScheduler;
struct FVoxelTerrainServicePriority;
struct FVoxelDetachedPhysicsCook;
class FVoxelPhysicsRetirement;
class FVoxelTerrainWorkReceipt;
struct FVoxelPhysicsRetirementTicket;

UCLASS()
class WHFRAMEWORK_API UVoxelCollisionComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UVoxelCollisionComponent();
	bool Apply(const TArray<FBox>& InBoxes, double InBlockSize);
	static uint64 PreparedReclaimBytes();
	bool BeginPrepare(int32 InBoxCount, double InBlockSize,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InPhysicalLease = nullptr);
	bool AppendPreparedBoxes(TConstArrayView<FBox> InBoxes);
	bool BeginDetachedCook(FVoxelTaskScheduler& InScheduler,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InPhysicalLease,
		const FVoxelTerrainServicePriority& InPriority, double InFirstQueuedSeconds,
		TSharedPtr<FVoxelPhysicsRetirement, ESPMode::ThreadSafe> InRetirement);
	bool IsDetachedCookRunning() const;
	bool HasDetachedCookFailed() const;
	bool PrepareNativeRetirement(TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> InInputWorkReceipt = nullptr);
	TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe> CaptureNativeTicket() const;
	bool IsNativeRetirementCapacityBlocked() const { return bNativeCapacityBlocked; }
	bool InstallPreparedPhysics();
	bool HasInstalledPhysics() const;
	void MarkPhysicsCommitted();
	void ReleasePreparedInputs(TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InPhysicalLease);
	virtual bool ShouldCreatePhysicsState() const override;
	virtual UBodySetup* GetBodySetup() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& InLocalToWorld) const override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

protected:
	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBodySetup> Setup;
	FBox LocalBounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
	double BlockSize = 0.0;
	int32 ExpectedBoxes = 0;
	bool bPreparing = false;
	bool bInstalled = false;
	bool bNativeCapacityBlocked = false;
	FGuid CookOwnerId = FGuid::NewGuid();
	TSharedPtr<FVoxelDetachedPhysicsCook, ESPMode::ThreadSafe> Cook;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PreparedPhysicalLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PreparedReclaimLease;
	TSharedPtr<FVoxelPhysicsRetirement, ESPMode::ThreadSafe> Retirement;
	TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe> NativeTicket;
	TOptional<FVoxelTaskRequest> PendingCookTask;
	FVoxelGenerationConsumerKey CookAttemptConsumer;
	FVoxelGenerationConsumerKey CookCapacityConsumer;
	uint64 CookCapacityVersion = 0;
	bool bCookCapacityBlocked = false;
	bool bCookAdmissionFailed = false;
};
