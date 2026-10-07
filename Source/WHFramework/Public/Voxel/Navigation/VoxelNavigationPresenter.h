#pragma once

#include "CoreMinimal.h"
#include "Voxel/Navigation/VoxelNavigationTypes.h"
#include "Voxel/Runtime/VoxelSection.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

class UVoxelModule;
class FVoxelTaskScheduler;
class UVoxelNavigationSurfaceComponent;
class AVoxelNavigationBounds;
struct FVoxelTaskResult;

class WHFRAMEWORK_API FVoxelNavigationPresenter
{
public:
	FVoxelNavigationPresenter(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler);
	~FVoxelNavigationPresenter();
	bool ActivateSection(const FIntVector& InSection);
	void RetireSection(const FIntVector& InSection);
	void InvalidateSection(const FIntVector& InSection);
	void Tick();
	bool IsReady(const FIntVector& InSection) const;
	bool IsInputCurrent(const FVoxelNavigationInputVersion& InVersion) const;
	bool IsResident(
		const FIntVector& InSection) const;
	bool HasReaders() const;
	bool TryCloseForWorldStop();
	void Reset();

private:
	bool RequestSection(const FIntVector& InSection);
	void MarkDirty(const FIntVector& InSection);
	void Apply(FVoxelTaskResult&& InResult);
	bool ApplyPrepared(const FIntVector& InSection);
	bool CaptureVersion(const FIntVector& InSection, FVoxelNavigationInputVersion& OutVersion) const;
	bool ReserveStorage(
		int32 InSectionCount,
		int32 InBoundsCount);
	static FIntVector BoundsGroup(const FIntVector& InSection);
	bool AcquireBounds(const FIntVector& InSection);
	void ReleaseBounds(const FIntVector& InSection);
	void RetireResources(
		const FIntVector& InSection);
	bool CreateBounds(
		const FIntVector& InGroup,
		TWeakObjectPtr<AVoxelNavigationBounds>& OutBounds);
	struct FBoundsLease
	{
		TWeakObjectPtr<AVoxelNavigationBounds> Bounds;
		int32 References = 0;
	};
	struct FInputLeases
	{
		FVoxelRuntimeSectionPin Section;
		FVoxelRuntimeSectionPin Above;
		FVoxelRuntimeSectionPin SecondAbove;
		TSharedPtr<FVoxelNavigationSectionConsumers, ESPMode::ThreadSafe> Consumers;
	};
	struct FAdmission
	{
		FVoxelNavigationInputVersion Version;
		FVoxelTaskStamp Stamp;
		double FirstRunnableSeconds = 0.0;
	};

	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	FGuid OwnerId = FGuid::NewGuid();
	uint64 NextRequestSerial = 0;
	TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> Lifetime;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StorageLease;
	int32 SectionCapacity = 0;
	int32 BoundsCapacity = 0;
	int32 DirtyCursor = 0;
	int32 PromotionCursor = 0;
	int32 RetirementCursor = 0;
	double PrepareCostMilliseconds = 0.01;
	double PromotionCostMilliseconds = 0.005;
	double RetirementCostMilliseconds = 0.005;
	bool bStopped = false;
	TSet<FIntVector> Wanted;
	TMap<FIntVector, FInputLeases> InputLeases;
	TSet<FIntVector> PendingPromotion;
	TSet<FIntVector> PendingRetirement;
	TMap<FIntVector, int32> Dirty;
	TSparseArray<FIntVector> PendingSections;
	TMap<FIntVector, FVoxelNavigationInputVersion> PublishedVersions;
	TMap<FIntVector, FVoxelTaskStamp> Requested;
	TMap<FIntVector, TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe>> Prepared;
	TMap<FIntVector, FAdmission> Admissions;
	TMap<FIntVector, FVoxelNavigationInputVersion> Failures;
	TMap<FIntVector, TObjectPtr<UVoxelNavigationSurfaceComponent>> Components;
	// 小型固定空间块的引用只来自实际 Nav consumer；Source 移动不改变已持有的边界。
	TMap<FIntVector, FBoundsLease> RegionBounds;
};
