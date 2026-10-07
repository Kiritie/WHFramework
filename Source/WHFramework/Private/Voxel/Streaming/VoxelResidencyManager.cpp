#include "Voxel/Streaming/VoxelResidencyManager.h"

#include "EngineGlobals.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"

namespace
{
	bool TimerBefore(const FVoxelSection* InLeft, const FVoxelSection* InRight)
	{
		const auto& Left = InLeft->Retirement;
		const auto& Right = InRight->Retirement;
		return Left.DueFrame < Right.DueFrame ||
			(Left.DueFrame == Right.DueFrame && InLeft->Stamp.Token < InRight->Stamp.Token);
	}
}

FVoxelResidencyManager::FVoxelResidencyManager(
	FVoxelWorldRuntime& InRuntime,
	FVoxelTaskScheduler& InScheduler,
	TFunction<void(const FIntVector&)> InBeforeEvict,
	TFunction<bool(const FIntVector&)> InRetainSection,
	TFunction<void(const FIntVector&, const FVoxelSectionStamp&, EVoxelRuntimeSectionEvent)> InSectionEvent)
	: Runtime(InRuntime)
	, Scheduler(InScheduler)
	, BeforeEvict(MoveTemp(InBeforeEvict))
	, RetainSection(MoveTemp(InRetainSection))
	, SectionEvent(MoveTemp(InSectionEvent))
{
	check(IsInGameThread());
	check(!Runtime.SectionEventCallback);
	Runtime.SetSectionEventCallback([this](const FIntVector& Key, const FVoxelSectionStamp& Stamp,
		const EVoxelRuntimeSectionEvent Event) { OnSectionEvent(Key, Stamp, Event); });
	Scheduler.SetSectionIdleCallback([this](const FIntVector& Key)
	{
		// 聚合任务计数归零只唤醒当前票据，不能直接释放或接纳旧任务正文。
		if (const auto* Section = Runtime.FindSection(Key))
			WakeRetirement(Key, Section->Stamp, Section->Retirement.Serial);
	});
	// 接管已有 Runtime 的一次性登记；生产世界在首个 Section 分配前创建 Manager。
	for (const FIntVector& Key : Runtime.GetResidencyOrder())
	{
		Revive(*Runtime.FindSection(Key));
		RequestRetirement(Key);
	}
}

FVoxelResidencyManager::~FVoxelResidencyManager()
{
	Runtime.SetSectionEventCallback({});
	Scheduler.SetSectionIdleCallback({});
	for (const FIntVector& Key : Runtime.GetResidencyOrder()) Revive(*Runtime.FindSection(Key));
	DemandEvents.Reset();
}

void FVoxelResidencyManager::QueueDemandEvents(TSharedPtr<const FVoxelResidencyDemandEvents, ESPMode::ThreadSafe> InEvents)
{
	check(IsInGameThread());
	check(InEvents && !InEvents->Next && InEvents.Get() != DemandEventsTail);
	if (DemandEventsTail) DemandEventsTail->Next = InEvents;
	else DemandEvents = InEvents;
	DemandEventsTail = InEvents.Get();
}

void FVoxelResidencyManager::NotifyDemandAdded(const FIntVector& InSection)
{
	check(IsInGameThread());
	if (auto* Section = Runtime.FindSection(InSection)) Revive(*Section);
}

void FVoxelResidencyManager::AdvanceDemandEvents(const double InDeadline, FVoxelTerrainFrameContext* InFrameContext)
{
	const auto ConsumeOne = [this]
	{
		const auto& Delta = DemandEvents->GetResidencyDelta();
		const auto& Keys = bReadingRemovedDemand ? Delta.RemovedExact : Delta.AddedExact;
		if (DemandEventCursor >= Keys.GetMaxIndex())
		{
			DemandEventCursor = 0;
			if (!bReadingRemovedDemand) bReadingRemovedDemand = true;
			else
			{
				bReadingRemovedDemand = false;
				const auto Previous = MoveTemp(DemandEvents);
				DemandEvents = MoveTemp(Previous->Next);
				if (!DemandEvents) DemandEventsTail = nullptr;
			}
			return;
		}
		const FSetElementId Id = FSetElementId::FromInteger(DemandEventCursor++);
		if (!Keys.IsValidId(Id)) return;
		if (bReadingRemovedDemand) RequestRetirement(Keys.Get(Id));
		else NotifyDemandAdded(Keys.Get(Id));
	};
	for (int32 Step = 0; Step < 32 && DemandEvents && FPlatformTime::Seconds() < InDeadline; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		if (InFrameContext && !InFrameContext->GetPublishBudget().TryStart(EVoxelPublishPhase::Retire,
			Started, DemandEventCostMilliseconds)) break;
		ConsumeOne();
		const double Finished = FPlatformTime::Seconds();
		DemandEventCostMilliseconds = FMath::Lerp(DemandEventCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (InFrameContext) InFrameContext->GetPublishBudget().Finish(EVoxelPublishPhase::Retire, Started, Finished);
	}
}

void FVoxelResidencyManager::RequestRetirement(const FIntVector& InSection)
{
	check(IsInGameThread());
	auto* Section = Runtime.FindSection(InSection);
	if (!Section || Section->Status == EVoxelSectionStatus::Unloading) return;
	auto& Ticket = Section->Retirement;
	if (Ticket.State == EVoxelRuntimeRetirementState::Detaching || Ticket.State == EVoxelRuntimeRetirementState::ReadyToFree ||
		Ticket.State == EVoxelRuntimeRetirementState::Freed) return;
	if (Ticket.State == EVoxelRuntimeRetirementState::Active || Ticket.State == EVoxelRuntimeRetirementState::Revived)
	{
		check(Ticket.Serial < MAX_uint64);
		++Ticket.Serial;
		Ticket.Section = InSection;
		Ticket.Stamp = Section->Stamp;
		Ticket.Resource = Section->GetResourceIdentity();
		Ticket.State = EVoxelRuntimeRetirementState::Candidate;
	}
	QueueReady(*Section);
}

bool FVoxelResidencyManager::WakeRetirement(const FIntVector& InSection,
	const FVoxelSectionStamp& InStamp, const uint64 InSerial)
{
	check(IsInGameThread());
	auto* Section = Runtime.FindSection(InSection);
	if (!Section || Section->Stamp != InStamp) return false;
	auto& Ticket = Section->Retirement;
	if (Ticket.Serial != InSerial || Ticket.Stamp != InStamp || Ticket.Resource != Section->GetResourceIdentity() ||
		(Ticket.State != EVoxelRuntimeRetirementState::Candidate &&
		 Ticket.State != EVoxelRuntimeRetirementState::Waiting &&
		 Ticket.State != EVoxelRuntimeRetirementState::ReadyToDetach)) return false;
	QueueReady(*Section);
	return true;
}

void FVoxelResidencyManager::OnSectionEvent(const FIntVector& InSection,
	const FVoxelSectionStamp& InStamp, const EVoxelRuntimeSectionEvent InEvent)
{
	auto* Section = Runtime.FindSection(InSection);
	if (!Section || Section->Stamp != InStamp) return;
	if (SectionEvent) SectionEvent(InSection, InStamp, InEvent);
	Section = Runtime.FindSection(InSection);
	if (!Section || Section->Stamp != InStamp) return;
	if (InEvent == EVoxelRuntimeSectionEvent::Removing)
	{
		RemoveReady(*Section);
		RemoveTimer(*Section);
		// 目录已解绑；实际正文/元数据和原租约由 Worker deleter 在最后读者结束后释放。
		Section->Retirement.State = EVoxelRuntimeRetirementState::ReadyToFree;
		return;
	}
	if (InEvent == EVoxelRuntimeSectionEvent::Wanted || InEvent == EVoxelRuntimeSectionEvent::IdentityChanging)
	{
		Revive(*Section);
		return;
	}
	if (InEvent == EVoxelRuntimeSectionEvent::Allocated) RequestRetirement(InSection);
	else WakeRetirement(InSection, InStamp, Section->Retirement.Serial);
}

void FVoxelResidencyManager::Revive(FVoxelSection& InSection)
{
	RemoveReady(InSection);
	RemoveTimer(InSection);
	auto& Ticket = InSection.Retirement;
	if (Ticket.State == EVoxelRuntimeRetirementState::Active || Ticket.State == EVoxelRuntimeRetirementState::Revived) return;
	check(Ticket.Serial < MAX_uint64);
	++Ticket.Serial;
	Ticket.State = EVoxelRuntimeRetirementState::Revived;
	Ticket.Blockers = EVoxelRuntimeRetirementBlocker::None;
}

void FVoxelResidencyManager::QueueReady(FVoxelSection& InSection)
{
	auto& Ticket = InSection.Retirement;
	if (Ticket.bQueued) return;
	Ticket.bQueued = true;
	Ticket.ReadyPrevious = ReadyTail;
	Ticket.ReadyNext = nullptr;
	if (ReadyTail) ReadyTail->Retirement.ReadyNext = &InSection;
	else ReadyHead = &InSection;
	ReadyTail = &InSection;
}

void FVoxelResidencyManager::RemoveReady(FVoxelSection& InSection)
{
	auto& Ticket = InSection.Retirement;
	if (!Ticket.bQueued) return;
	if (Ticket.ReadyPrevious) Ticket.ReadyPrevious->Retirement.ReadyNext = Ticket.ReadyNext;
	else ReadyHead = Ticket.ReadyNext;
	if (Ticket.ReadyNext) Ticket.ReadyNext->Retirement.ReadyPrevious = Ticket.ReadyPrevious;
	else ReadyTail = Ticket.ReadyPrevious;
	Ticket.ReadyPrevious = Ticket.ReadyNext = nullptr;
	Ticket.bQueued = false;
}

void FVoxelResidencyManager::SiftTimer(int32 InIndex)
{
	auto& Timers = Runtime.RetirementTimers;
	const auto SwapAt = [&Timers](const int32 Left, const int32 Right)
	{
		Timers.Swap(Left, Right);
		Timers[Left]->Retirement.TimerIndex = Left;
		Timers[Right]->Retirement.TimerIndex = Right;
	};
	while (InIndex > 0)
	{
		const int32 Parent = (InIndex - 1) / 2;
		if (!TimerBefore(Timers[InIndex], Timers[Parent])) break;
		SwapAt(InIndex, Parent);
		InIndex = Parent;
	}
	while (InIndex < Timers.Num() / 2)
	{
		int32 Child = InIndex * 2 + 1;
		if (Child + 1 < Timers.Num() && TimerBefore(Timers[Child + 1], Timers[Child])) ++Child;
		if (!TimerBefore(Timers[Child], Timers[InIndex])) break;
		SwapAt(InIndex, Child);
		InIndex = Child;
	}
}

void FVoxelResidencyManager::AddTimer(FVoxelSection& InSection)
{
	auto& Ticket = InSection.Retirement;
	if (Ticket.TimerIndex == INDEX_NONE)
	{
		check(Runtime.RetirementTimers.Num() < Runtime.RetirementTimers.Max());
		Ticket.TimerIndex = Runtime.RetirementTimers.Add(&InSection);
	}
	SiftTimer(Ticket.TimerIndex);
}

void FVoxelResidencyManager::RemoveTimer(FVoxelSection& InSection)
{
	auto& Ticket = InSection.Retirement;
	if (Ticket.TimerIndex == INDEX_NONE) return;
	auto& Timers = Runtime.RetirementTimers;
	const int32 Index = Ticket.TimerIndex;
	check(Timers.IsValidIndex(Index) && Timers[Index] == &InSection);
	Timers.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Ticket.TimerIndex = INDEX_NONE;
	if (Timers.IsValidIndex(Index))
	{
		Timers[Index]->Retirement.TimerIndex = Index;
		SiftTimer(Index);
	}
}

EVoxelRuntimeRetirementBlocker FVoxelResidencyManager::CollectBlockers(
	const FIntVector& InKey, const FVoxelSection& InSection, const uint64 InFrame) const
{
	auto Blockers = EVoxelRuntimeRetirementBlocker::None;
	if (InSection.GetPinCount() > 0) Blockers |= EVoxelRuntimeRetirementBlocker::Pin;
	if (Scheduler.HasSectionTask(InKey)) Blockers |= EVoxelRuntimeRetirementBlocker::Task;
	if (RetainSection && RetainSection(InKey)) Blockers |= EVoxelRuntimeRetirementBlocker::Replacement;
	if (Runtime.IsServer() && Runtime.GetPersistedRevision(InKey) != InSection.CommittedRevision &&
		(!Runtime.FindCommittedOverlay(InKey) || Runtime.GetCommittedOverlayRevision(InKey) != InSection.CommittedRevision))
		Blockers |= EVoxelRuntimeRetirementBlocker::Save;
	if (InFrame < InSection.Retirement.DueFrame) Blockers |= EVoxelRuntimeRetirementBlocker::Grace;
	return Blockers;
}

void FVoxelResidencyManager::Tick(const TMap<FIntVector, FVoxelExactDemand>& InDemand,
	const uint64 InInterestRevision, const double InNow, FVoxelTerrainFrameContext* InFrameContext)
{
	check(IsInGameThread());
	(void)InNow;
	(void)InInterestRevision;
	const uint64 Frame = GFrameCounter;
	const double LocalDeadline = FPlatformTime::Seconds() + 0.00025;
	const double Deadline = InFrameContext ? FMath::Min(LocalDeadline, InFrameContext->Deadline()) : LocalDeadline;
	AdvanceDemandEvents(Deadline, InFrameContext);
	// 只检查最小到期项；未到期或等待领域事件的票据不参与逐帧扫描。
	for (int32 Count = 0; Count < 32 && !Runtime.RetirementTimers.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Count)
	{
		auto* Section = Runtime.RetirementTimers[0];
		if (Section->Retirement.DueFrame > Frame) break;
		const double Started = FPlatformTime::Seconds();
		if (InFrameContext && !InFrameContext->GetPublishBudget().TryStart(EVoxelPublishPhase::Retire,
			Started, TimerCostMilliseconds)) break;
		RemoveTimer(*Section);
		QueueReady(*Section);
		const double Finished = FPlatformTime::Seconds();
		TimerCostMilliseconds = FMath::Lerp(TimerCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (InFrameContext) InFrameContext->GetPublishBudget().Finish(EVoxelPublishPhase::Retire, Started, Finished);
	}
	for (int32 Count = 0; Count < 32 && ReadyHead && FPlatformTime::Seconds() < Deadline; ++Count)
	{
		const double Started = FPlatformTime::Seconds();
		if (InFrameContext && !InFrameContext->GetPublishBudget().TryStart(EVoxelPublishPhase::Retire,
			Started, RetirementCostMilliseconds)) break;
		auto* Section = ReadyHead;
		RemoveReady(*Section);
		auto& Ticket = Section->Retirement;
		const FIntVector Key = Ticket.Section;
		if (Section->Stamp != Ticket.Stamp || Ticket.Resource != Section->GetResourceIdentity() || InDemand.Contains(Key)) Revive(*Section);
		else
		{
			Ticket.DueFrame = Section->LastWantedFrame > MAX_uint64 - EvictGraceFrames
				? MAX_uint64 : Section->LastWantedFrame + EvictGraceFrames;
			Ticket.Blockers = CollectBlockers(Key, *Section, Frame);
			if (Ticket.Blockers != EVoxelRuntimeRetirementBlocker::None)
			{
				Ticket.State = EVoxelRuntimeRetirementState::Waiting;
				if (EnumHasAnyFlags(Ticket.Blockers, EVoxelRuntimeRetirementBlocker::Grace)) AddTimer(*Section);
				else RemoveTimer(*Section);
			}
			else
			{
				RemoveTimer(*Section);
				Ticket.State = EVoxelRuntimeRetirementState::ReadyToDetach;
				const auto Stamp = Ticket.Stamp;
				const uint64 Serial = Ticket.Serial;
				Ticket.State = EVoxelRuntimeRetirementState::Detaching;
				if (BeforeEvict) BeforeEvict(Key);
				// 回调可能更换实例；重新寻找 Owner，不能继续解引用旧 Section。
				auto* Current = Runtime.FindSection(Key);
				if (Current && Current->Stamp == Stamp && Current->Retirement.Serial == Serial &&
					Current->Retirement.State == EVoxelRuntimeRetirementState::Detaching)
				{
					if (!Runtime.RemoveSection(Key))
					{
						Current->Retirement.State = EVoxelRuntimeRetirementState::Waiting;
						Current->Retirement.Blockers = CollectBlockers(Key, *Current, Frame);
					}
				}
			}
		}
		const double Finished = FPlatformTime::Seconds();
		RetirementCostMilliseconds = FMath::Lerp(RetirementCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (InFrameContext) InFrameContext->GetPublishBudget().Finish(EVoxelPublishPhase::Retire, Started, Finished);
	}
	Runtime.AdvanceJournalRetirement(32, Deadline, InFrameContext);
	Runtime.AdvanceResidentPromotion(32, Deadline, InFrameContext);
}

void FVoxelResidencyManager::SetEvictGraceFrames(const uint64 InFrames)
{
	EvictGraceFrames = InFrames;
}
