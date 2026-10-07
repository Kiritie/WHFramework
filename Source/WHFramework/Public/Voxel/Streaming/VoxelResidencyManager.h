#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

class FVoxelTerrainFrameContext;
struct FVoxelInterestDelta;

// 事件节点内嵌于已授予的规划输出；GT 入队只转移强引用，逐帧有界读取不可变 Delta。
struct WHFRAMEWORK_API FVoxelResidencyDemandEvents
{
	virtual ~FVoxelResidencyDemandEvents() = default;
	virtual const FVoxelInterestDelta& GetResidencyDelta() const = 0;

private:
	friend class FVoxelResidencyManager;
	friend class UVoxelModule;
	mutable TSharedPtr<const FVoxelResidencyDemandEvents, ESPMode::ThreadSafe> Next;
	mutable TSharedPtr<const FVoxelResidencyDemandEvents, ESPMode::ThreadSafe> SimulationNext;
};

class WHFRAMEWORK_API FVoxelResidencyManager
{
public:
	FVoxelResidencyManager(
		FVoxelWorldRuntime& InRuntime,
		FVoxelTaskScheduler& InScheduler,
		TFunction<void(const FIntVector&)> InBeforeEvict,
		TFunction<bool(const FIntVector&)> InRetainSection = {},
		TFunction<void(const FIntVector&, const FVoxelSectionStamp&, EVoxelRuntimeSectionEvent)> InSectionEvent = {});
	~FVoxelResidencyManager();
	FVoxelResidencyManager(const FVoxelResidencyManager&) = delete;
	FVoxelResidencyManager& operator=(const FVoxelResidencyManager&) = delete;

	void QueueDemandEvents(TSharedPtr<const FVoxelResidencyDemandEvents, ESPMode::ThreadSafe> InEvents);
	void NotifyDemandAdded(const FIntVector& InSection);
	void RequestRetirement(const FIntVector& InSection);
	bool WakeRetirement(const FIntVector& InSection, const FVoxelSectionStamp& InStamp, uint64 InSerial);

	void Tick(
		const TMap<FIntVector, FVoxelExactDemand>& InDemand,
		uint64 InInterestRevision,
		double InNow,
		FVoxelTerrainFrameContext* InFrameContext = nullptr);
	void SetEvictGraceFrames(uint64 InFrames);

private:
	void OnSectionEvent(const FIntVector& InSection, const FVoxelSectionStamp& InStamp, EVoxelRuntimeSectionEvent InEvent);
	void Revive(FVoxelSection& InSection);
	void QueueReady(FVoxelSection& InSection);
	void RemoveReady(FVoxelSection& InSection);
	void AddTimer(FVoxelSection& InSection);
	void RemoveTimer(FVoxelSection& InSection);
	void SiftTimer(int32 InIndex);
	void AdvanceDemandEvents(double InDeadline, FVoxelTerrainFrameContext* InFrameContext);
	EVoxelRuntimeRetirementBlocker CollectBlockers(const FIntVector& InKey,
		const FVoxelSection& InSection, uint64 InFrame) const;

private:
	FVoxelWorldRuntime& Runtime;
	FVoxelTaskScheduler& Scheduler;
	TFunction<void(const FIntVector&)> BeforeEvict;
	TFunction<bool(const FIntVector&)> RetainSection;
	TFunction<void(const FIntVector&, const FVoxelSectionStamp&, EVoxelRuntimeSectionEvent)> SectionEvent;
	FVoxelSection* ReadyHead = nullptr;
	FVoxelSection* ReadyTail = nullptr;
	TSharedPtr<const FVoxelResidencyDemandEvents, ESPMode::ThreadSafe> DemandEvents;
	const FVoxelResidencyDemandEvents* DemandEventsTail = nullptr;
	int32 DemandEventCursor = 0;
	bool bReadingRemovedDemand = false;
	double RetirementCostMilliseconds = 0.03;
	double DemandEventCostMilliseconds = 0.005;
	double TimerCostMilliseconds = 0.005;
	uint64 EvictGraceFrames = 180;
};
