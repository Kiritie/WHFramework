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
	FVoxelResidencyManager(FVoxelWorldRuntime& InRuntime, FVoxelTaskScheduler& InScheduler);
#if WITH_DEV_AUTOMATION_TESTS
	FVoxelResidencyManager(
		FVoxelWorldRuntime& InRuntime,
		FVoxelTaskScheduler& InScheduler,
		TFunction<void(const FIntVector&)> InBeforeEvict,
		TFunction<bool(const FIntVector&)> InRetainSection = {},
		TFunction<void(const FIntVector&, const FVoxelSectionStamp&, EVoxelRuntimeSectionEvent)> InSectionEvent = {});
#endif
	~FVoxelResidencyManager();
	FVoxelResidencyManager(const FVoxelResidencyManager&) = delete;
	FVoxelResidencyManager& operator=(const FVoxelResidencyManager&) = delete;

	template<typename BeforeFunctor, typename RetainFunctor, typename EventFunctor>
	bool Initialize(BeforeFunctor&& InBeforeEvict, RetainFunctor&& InRetainSection,
		EventFunctor&& InSectionEvent, FString& OutError)
	{
		check(IsInGameThread());
		static_assert(!std::is_same_v<std::decay_t<BeforeFunctor>, TFunction<void(const FIntVector&)>>,
			"Initialize with the concrete residency callback before allocation");
		static_assert(!std::is_same_v<std::decay_t<RetainFunctor>, TFunction<bool(const FIntVector&)>>,
			"Initialize with the concrete residency retention callback before allocation");
		static_assert(!std::is_same_v<std::decay_t<EventFunctor>,
			TFunction<void(const FIntVector&, const FVoxelSectionStamp&, EVoxelRuntimeSectionEvent)>>,
			"Initialize with the concrete residency event callback before allocation");
		if (bInitialized || Runtime.SectionEventCallback || !Scheduler.GetControlConsumer().IsValid())
		{
			OutError = TEXT("InvalidConsumer: residency callback initialization domain");
			return false;
		}
		auto RuntimeEvent = [this](const FIntVector& Key, const FVoxelSectionStamp& Stamp,
			const EVoxelRuntimeSectionEvent Event)
		{
			OnSectionEvent(Key, Stamp, Event);
		};
		const uint64 Bytes = VoxelBoundedAllocation::FunctionBytes<std::decay_t<BeforeFunctor>, false>() +
			VoxelBoundedAllocation::FunctionBytes<std::decay_t<RetainFunctor>, false>() +
			VoxelBoundedAllocation::FunctionBytes<std::decay_t<EventFunctor>, false>() +
			VoxelBoundedAllocation::FunctionBytes<decltype(RuntimeEvent), false>();
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (Bytes)
		{
			const auto Admission = Scheduler.GetTerrainResources()->TryAcquire(Scheduler.GetControlConsumer(),
				EVoxelTerrainResourcePool::Input, Bytes, Lease);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				OutError = Admission == EVoxelTerrainResourceAdmission::InvalidRequest ?
					TEXT("InvalidConsumer: residency callback admission request") : Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
					TEXT("Oversized: residency callback input") : TEXT("CapacityBlocked: residency callback input");
				return false;
			}
		}
		if (!Scheduler.SetSectionIdleCallback([this](const FIntVector& Key)
			{
				if (const auto* Section = Runtime.FindSection(Key))
					WakeRetirement(Key, Section->Stamp, Section->Retirement.Serial);
			}, OutError)) return false;
		CallbackLease = MoveTemp(Lease);
		BeforeEvict = Forward<BeforeFunctor>(InBeforeEvict);
		RetainSection = Forward<RetainFunctor>(InRetainSection);
		SectionEvent = Forward<EventFunctor>(InSectionEvent);
		Runtime.SetSectionEventCallback(MoveTemp(RuntimeEvent));
		bInitialized = true;
		RegisterExistingSections();
		OutError.Reset();
		return true;
	}

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
	void RegisterExistingSections();
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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CallbackLease;
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
	bool bInitialized = false;
};
