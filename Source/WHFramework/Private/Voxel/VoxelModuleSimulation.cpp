#include "Voxel/VoxelModule.h"

#include "Engine/World.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "SaveGame/SaveGameModule.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Scene/VoxelSceneRegion.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

bool UVoxelModule::ReserveSimulationStorage(const int32 InSectionCount, const int32 InRegionCount)
{
	if (!Scheduler || InSectionCount < 0 || InRegionCount < 0) return false;
	if (SimulationStorageLease && InSectionCount <= SimulationSectionCapacity && InRegionCount <= SceneRegionCapacity) return true;
	const int64 SectionRows = FMath::Max<int64>(16, InSectionCount > SimulationSectionCapacity ?
		FMath::Max<int64>(InSectionCount, int64(SimulationSectionCapacity) * 2) : SimulationSectionCapacity);
	const int64 RegionRows = FMath::Max<int64>(4, InRegionCount > SceneRegionCapacity ?
		FMath::Max<int64>(InRegionCount, int64(SceneRegionCapacity) * 2) : SceneRegionCapacity);
	if (SectionRows > MAX_int32 || RegionRows > MAX_int32) return false;
	const uint64 OldBytes = SimulationSections.GetAllocatedSize() + SceneRegions.GetAllocatedSize();
	const uint64 NewBytes = VoxelBoundedAllocation::SetBytes<FIntVector>(int32(SectionRows)) +
		VoxelBoundedAllocation::MapBytes<FIntVector, TObjectPtr<UVoxelSceneRegion>>(int32(RegionRows));
	const auto Resources = Scheduler->GetRuntimeResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(Epoch, TerrainPlanningOwnerId, 1, {}, 1);
	if ((!SimulationStorageLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
		OldBytes + NewBytes, SimulationStorageLease)) || !SimulationStorageLease->Resize(OldBytes + NewBytes)) return false;
	SimulationSections.Reserve(int32(SectionRows));
	SceneRegions.Reserve(int32(RegionRows));
	SimulationSectionCapacity = int32(SectionRows);
	SceneRegionCapacity = int32(RegionRows);
	const uint64 ActualBytes = SimulationSections.GetAllocatedSize() + SceneRegions.GetAllocatedSize();
	check(ActualBytes <= NewBytes);
	verify(SimulationStorageLease->Resize(ActualBytes));
	return true;
}

void UVoxelModule::QueueSimulationDemandEvents(TSharedPtr<const FVoxelResidencyDemandEvents, ESPMode::ThreadSafe> InEvents)
{
	check(IsInGameThread());
	if (!IsAuthority() || !bEnableProjectSceneSimulation) return;
	check(InEvents && !InEvents->SimulationNext && InEvents.Get() != SimulationDemandEventsTail);
	if (SimulationDemandEventsTail) SimulationDemandEventsTail->SimulationNext = InEvents;
	else SimulationDemandEvents = InEvents;
	SimulationDemandEventsTail = InEvents.Get();
}

void UVoxelModule::QueueSimulationSection(FVoxelSection& InSection)
{
	if (InSection.bSimulationQueued) return;
	InSection.SimulationPrevious = SimulationReadyTail;
	InSection.SimulationNext = nullptr;
	if (SimulationReadyTail) SimulationReadyTail->SimulationNext = &InSection;
	else SimulationReadyHead = &InSection;
	SimulationReadyTail = &InSection;
	InSection.bSimulationQueued = true;
}

void UVoxelModule::UnqueueSimulationSection(FVoxelSection& InSection)
{
	if (!InSection.bSimulationQueued) return;
	if (InSection.SimulationPrevious) InSection.SimulationPrevious->SimulationNext = InSection.SimulationNext;
	else SimulationReadyHead = InSection.SimulationNext;
	if (InSection.SimulationNext) InSection.SimulationNext->SimulationPrevious = InSection.SimulationPrevious;
	else SimulationReadyTail = InSection.SimulationPrevious;
	InSection.SimulationPrevious = nullptr;
	InSection.SimulationNext = nullptr;
	InSection.bSimulationQueued = false;
}

void UVoxelModule::HoldSimulationSectionForRegion(FVoxelSection& InSection, UVoxelSceneRegion& InRegion)
{
	check(IsInGameThread());
	check(InRegion.bRegisteredRegion);
	if (InSection.SimulationWaitingRegion == &InRegion) return;
	ReleaseSimulationSectionFromRegion(InSection);
	UnqueueSimulationSection(InSection);
	InSection.SimulationWaitingRegion = &InRegion;
	InSection.SimulationFailurePrevious = InRegion.SimulationFailureTail;
	InSection.SimulationFailureNext = nullptr;
	if (InRegion.SimulationFailureTail) InRegion.SimulationFailureTail->SimulationFailureNext = &InSection;
	else InRegion.SimulationFailureHead = &InSection;
	InRegion.SimulationFailureTail = &InSection;
	if (!InRegion.SimulationFailureCursor) InRegion.SimulationFailureCursor = InRegion.SimulationFailureHead;
}

void UVoxelModule::ReleaseSimulationSectionFromRegion(FVoxelSection& InSection)
{
	if (auto* Region = InSection.SimulationWaitingRegion)
	{
		if (Region->SimulationFailureCursor == &InSection) Region->SimulationFailureCursor = InSection.SimulationFailureNext;
		if (InSection.SimulationFailurePrevious) InSection.SimulationFailurePrevious->SimulationFailureNext = InSection.SimulationFailureNext;
		else Region->SimulationFailureHead = InSection.SimulationFailureNext;
		if (InSection.SimulationFailureNext) InSection.SimulationFailureNext->SimulationFailurePrevious = InSection.SimulationFailurePrevious;
		else Region->SimulationFailureTail = InSection.SimulationFailurePrevious;
		InSection.SimulationWaitingRegion = nullptr;
		InSection.SimulationFailurePrevious = nullptr;
		InSection.SimulationFailureNext = nullptr;
	}
}

void UVoxelModule::HoldSimulationSectionForSave(FVoxelSection& InSection, UVoxelSceneRegion& InRegion)
{
	check(InRegion.bRegisteredRegion && !InSection.SimulationSaveRegion && InSection.bSimulationPinned);
	UnqueueSimulationSection(InSection);
	ReleaseSimulationSectionFromRegion(InSection);
	InSection.SimulationSaveRegion = &InRegion;
	InSection.SimulationSaveRegionId = InRegion.SceneInstanceId;
	InSection.SimulationSaveRevision = InRegion.SceneRevision;
	InSection.SimulationSavePrevious = InRegion.SimulationSaveTail;
	InSection.SimulationSaveNext = nullptr;
	if (InRegion.SimulationSaveTail) InRegion.SimulationSaveTail->SimulationSaveNext = &InSection;
	else InRegion.SimulationSaveHead = &InSection;
	InRegion.SimulationSaveTail = &InSection;
	++SimulationSaveWaitCount;
	bSceneSaveRequested = true;
}

void UVoxelModule::ReleaseSimulationSectionFromSave(FVoxelSection& InSection)
{
	if (auto* Region = InSection.SimulationSaveRegion)
	{
		if (InSection.SimulationSavePrevious) InSection.SimulationSavePrevious->SimulationSaveNext = InSection.SimulationSaveNext;
		else Region->SimulationSaveHead = InSection.SimulationSaveNext;
		if (InSection.SimulationSaveNext) InSection.SimulationSaveNext->SimulationSavePrevious = InSection.SimulationSavePrevious;
		else Region->SimulationSaveTail = InSection.SimulationSavePrevious;
		InSection.SimulationSaveRegion = nullptr;
		InSection.SimulationSavePrevious = InSection.SimulationSaveNext = nullptr;
		InSection.SimulationSaveRegionId.Invalidate();
		InSection.SimulationSaveRevision = 0;
		check(SimulationSaveWaitCount > 0);
		--SimulationSaveWaitCount;
	}
}

bool UVoxelModule::ReviveSimulationSection(FVoxelSection& InSection, UVoxelSceneRegion& InRegion)
{
	const FIntVector Key = InSection.SimulationKey;
	if (bSceneTransitionActive || !Runtime || Runtime->FindSection(Key) != &InSection ||
		InSection.GetResourceIdentity() != InSection.SimulationIdentity || InSection.SimulationSaveRegion != &InRegion ||
		InSection.SimulationSaveRegionId != InRegion.SceneInstanceId || !IsCollisionReady(Key) ||
		(InRegion.bSceneFailed && !InRegion.bSceneSaveFailed)) return false;
	TGuardValue<bool> Transition(bSceneTransitionActive, true);
	ReleaseSimulationSectionFromSave(InSection);
	InRegion.bSceneFailed = InRegion.bSceneSaveFailed = false;
	InRegion.OnSectionActivated(Key);
	if (NavigationPresenter && !NavigationPresenter->ActivateSection(Key)) InSection.bSimulationCapacityBlocked = true;
	FVoxelSimulationResidencyDelta Delta;
	Delta.AddedSections.Add(Key);
	Delta.Revision = ++SimulationResidencyRevision;
	OnSimulationResidencyChanged(Delta);
	return true;
}

void UVoxelModule::FinishSimulationSectionRetirement(FVoxelSection& InSection)
{
	check(bSceneTransitionActive && Runtime && Runtime->FindSection(InSection.SimulationKey) == &InSection &&
		InSection.GetResourceIdentity() == InSection.SimulationIdentity && InSection.bSimulationPinned);
	const FIntVector Key = InSection.SimulationKey;
	const auto Identity = InSection.SimulationIdentity;
	ReleaseSimulationSectionFromSave(InSection);
	SimulationSections.Remove(Key);
	if (NavigationPresenter) NavigationPresenter->RetireSection(Key);
	InSection.bSimulationPinned = false;
	verify(Runtime->ReleaseSectionPin(Key, Identity));
}

void UVoxelModule::ApplySceneSaveReceipt(UVoxelSceneRegion& InRegion, const double InDeadline)
{
	if (DurableSceneSaveReceipt && DurableSceneSaveReceipt->WorldEpoch == Epoch)
	{
		const auto* Cut = DurableSceneSaveReceipt->Regions.Find(InRegion.GetRegionKey());
		if (Cut && Cut->RegionInstanceId == InRegion.SceneInstanceId)
			InRegion.DurableSceneRevision = FMath::Max(InRegion.DurableSceneRevision, Cut->Revision);
	}
	if (LatestSceneSaveAttempt && LatestSceneSaveAttempt->WorldEpoch == Epoch && !bLatestSceneSaveSucceeded &&
		LatestSceneSaveAttempt->TransactionId != InRegion.IgnoredSceneSaveFailureId)
	{
		const auto* Cut = LatestSceneSaveAttempt->Regions.Find(InRegion.GetRegionKey());
		const uint64 Required = InRegion.SimulationSaveHead ? InRegion.SimulationSaveHead->SimulationSaveRevision :
			(InRegion.bIdleSaveWaiting ? InRegion.SceneRevision : 0);
		if (Cut && Cut->RegionInstanceId == InRegion.SceneInstanceId && Required > InRegion.DurableSceneRevision &&
			Cut->Revision >= Required)
		{
			InRegion.bSceneFailed = InRegion.bSceneSaveFailed = true;
		}
	}
	for (int32 Step = 0; Step < 16 && InRegion.SimulationSaveHead && FPlatformTime::Seconds() < InDeadline; ++Step)
	{
		auto* Section = InRegion.SimulationSaveHead;
		if (Section->SimulationSaveRegionId != InRegion.SceneInstanceId || !Runtime ||
			Runtime->FindSection(Section->SimulationKey) != Section ||
			Section->GetResourceIdentity() != Section->SimulationIdentity)
		{
			WorldState = EVoxelWorldState::Failed;
			UE_LOG(LogTemp, Error, TEXT("Scene save waiter lost its exact retained runtime identity"));
			break;
		}
		if (Section->SimulationSaveRevision > InRegion.DurableSceneRevision) break;
		const auto* Demand = CurrentInterest->Exact.Find(Section->SimulationKey);
		// 新需求由既有协调队列恢复；旧保存回执不能先释放它的 pin。
		if (Demand && Demand->bSimulation)
		{
			WakeSimulationSection(Section->SimulationKey);
			break;
		}
		FinishSimulationSectionRetirement(*Section);
	}
	if (InRegion.bSceneSaveFailed && !InRegion.SimulationSaveHead &&
		(!InRegion.bIdleSaveWaiting || InRegion.DurableSceneRevision >= InRegion.SceneRevision))
		InRegion.bSceneFailed = InRegion.bSceneSaveFailed = false;
}

void UVoxelModule::RequestSceneDurableSave()
{
	if (!bSceneSaveRequested || bSceneTransitionActive || !Runtime || !IsAuthority() || SaveAdapter.IsBusy() || SceneSaveBuilder.IsSet()) return;
	auto* SaveGame = USaveGameModule::GetPtr(GetWorld());
	if (!SaveGame || SaveGame->GetWorld() != GetWorld() || !SaveGame->HasActiveSave() || SaveGame->IsSaveOperationRunning()) return;
	const double Started = FPlatformTime::Seconds();
	if (TerrainFrameContext.IsActive() && !TerrainFrameContext.GetPublishBudget().TryStart(EVoxelPublishPhase::Prepare,
		Started, SceneSaveCaptureCostMilliseconds))
	{
		if (!TerrainFrameContext.IsOverdueOpportunityAvailable()) return;
		TerrainFrameContext.ConsumeOverdueOpportunity();
	}
	const auto Result = SaveGame->SaveActiveSlotAsync();
	if (!Result || (!SaveAdapter.IsBusy() && !SceneSaveBuilder.IsSet()))
	{
		bSceneSaveRequested = false;
		UE_LOG(LogTemp, Error, TEXT("Scene retirement could not start its required SaveGame transaction; owners and pins retained"));
	}
	const double Finished = FPlatformTime::Seconds();
	if (Result && (Finished - Started) * 1000.0 > MaximumSceneCaptureStepMilliseconds)
		LastSaveError = TEXT("Scene save coordinator admission exceeded its configured single-step capability");
	SceneSaveCaptureCostMilliseconds = FMath::Lerp(SceneSaveCaptureCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
	if (TerrainFrameContext.IsActive()) TerrainFrameContext.GetPublishBudget().Finish(EVoxelPublishPhase::Prepare, Started, Finished);
}

bool UVoxelModule::HasUnpersistedSceneState() const
{
	if (SimulationSaveWaitCount > 0) return true;
	if (!bPersistenceEnabledForCurrentWorld) return false;
	for (auto* Region = SceneRegionHead; Region; Region = Region->NextRegion)
	{
		uint64 DurableRevision = Region->DurableSceneRevision;
		if (DurableSceneSaveReceipt && DurableSceneSaveReceipt->WorldEpoch == Epoch)
		{
			const auto* Cut = DurableSceneSaveReceipt->Regions.Find(Region->GetRegionKey());
			if (Cut && Cut->RegionInstanceId == Region->SceneInstanceId) DurableRevision = FMath::Max(DurableRevision, Cut->Revision);
		}
		if (Region->bSceneFailed || Region->SceneRevision > DurableRevision) return true;
	}
	return false;
}

void UVoxelModule::WakeSimulationSection(const FIntVector& InSection)
{
	check(IsInGameThread());
	if (!Runtime || !IsAuthority() || !bEnableProjectSceneSimulation || WorldState == EVoxelWorldState::Closing) return;
	const auto* Demand = CurrentInterest->Exact.Find(InSection);
	auto* Section = Runtime->FindSection(InSection);
	if (!Section || Section->Status == EVoxelSectionStatus::Unloading) return;
	const bool bRequested = Demand && Demand->bSimulation;
	if (!bRequested && !SimulationSections.Contains(InSection) && !Section->SimulationWaitingRegion) return;
	if (Section->SimulationWaitingRegion)
	{
		if (!bRequested) ReleaseSimulationSectionFromRegion(*Section);
		else if (Section->SimulationWaitingRegion->bSceneFailed || !Section->SimulationWaitingRegion->bSceneReady) return;
	}
	Section->SimulationKey = InSection;
	Section->SimulationIdentity = Section->GetResourceIdentity();
	Section->bSimulationCapacityBlocked = false;
	QueueSimulationSection(*Section);
}

void UVoxelModule::ReconcileSimulationDemand()
{
	if (!Runtime || !Scheduler || !IsAuthority() || !bEnableProjectSceneSimulation) return;
	const double Deadline = TerrainFrameContext.Deadline(0.35);
	for (int32 Step = 0; Step < 32 && SimulationDemandEvents && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		if (TerrainFrameContext.IsActive() && !TerrainFrameContext.GetPublishBudget().TryStart(EVoxelPublishPhase::Admission,
			Started, SimulationEventCostMilliseconds)) break;
		const auto& Delta = SimulationDemandEvents->GetResidencyDelta();
		const auto& Keys = bReadingRemovedSimulation ? Delta.RemovedSimulation : Delta.AddedSimulation;
		if (SimulationDemandCursor >= Keys.GetMaxIndex())
		{
			SimulationDemandCursor = 0;
			if (!bReadingRemovedSimulation) bReadingRemovedSimulation = true;
			else
			{
				bReadingRemovedSimulation = false;
				const auto Previous = MoveTemp(SimulationDemandEvents);
				SimulationDemandEvents = MoveTemp(Previous->SimulationNext);
				if (!SimulationDemandEvents) SimulationDemandEventsTail = nullptr;
			}
		}
		else
		{
			const auto Id = FSetElementId::FromInteger(SimulationDemandCursor++);
			if (Keys.IsValidId(Id)) WakeSimulationSection(Keys.Get(Id));
		}
		const double Finished = FPlatformTime::Seconds();
		SimulationEventCostMilliseconds = FMath::Lerp(SimulationEventCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (TerrainFrameContext.IsActive()) TerrainFrameContext.GetPublishBudget().Finish(EVoxelPublishPhase::Admission, Started, Finished);
	}
	// 仅处理由需求、数据完成、真实物理提交或显式恢复唤醒的局部票据。
	for (int32 Step = 0; Step < 16 && SimulationReadyHead && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		if (TerrainFrameContext.IsActive() && !TerrainFrameContext.GetPublishBudget().TryStart(EVoxelPublishPhase::Prepare,
			Started, SimulationStepCostMilliseconds)) break;
		auto* Section = SimulationReadyHead;
		const FIntVector Key = Section->SimulationKey;
		const auto Identity = Section->SimulationIdentity;
		UnqueueSimulationSection(*Section);
		check(Runtime->FindSection(Key) == Section && Section->GetResourceIdentity() == Identity);
		if (Section->bSimulationCapacityBlocked &&
			Scheduler->GetRuntimeResources()->Snapshot().ReleaseVersion <= Section->SimulationCapacityVersion &&
			Scheduler->GetTerrainResources()->Snapshot().ReleaseVersion <= Section->SimulationGeometryCapacityVersion)
		{
			QueueSimulationSection(*Section);
		}
		else
		{
			Section->bSimulationCapacityBlocked = false;
			const auto* Demand = CurrentInterest->Exact.Find(Key);
			auto* Region = GetSceneRegion(Key, false);
			if (Section->SimulationSaveRegion && Demand && Demand->bSimulation)
			{
				if (!ReviveSimulationSection(*Section, *Section->SimulationSaveRegion)) Section->bSimulationCapacityBlocked = true;
			}
			else if (!Demand || !Demand->bSimulation)
			{
				ReleaseSimulationSectionFromRegion(*Section);
				RemoveSimulationSection(Key);
			}
			else if (Region && Region->bSceneLoading) HoldSimulationSectionForRegion(*Section, *Region);
			else if (!Region || !Region->bSceneFailed)
			{
				if (!SimulationSections.Contains(Key))
				{
					if (ReserveSimulationStorage(SimulationSections.Num() + 1, SceneRegions.Num() + (Region ? 0 : 1))) ActivateSimulationSection(Key);
					else Section->bSimulationCapacityBlocked = true;
				}
				else if (NavigationPresenter && !NavigationPresenter->ActivateSection(Key)) Section->bSimulationCapacityBlocked = true;
			}
			else HoldSimulationSectionForRegion(*Section, *Region);
			// 领域回调可使 Section 或世界换代；不再沿用回调前的裸地址。
			if (Runtime && Runtime->Epoch() == Identity.WorldEpoch)
			{
				auto* Current = Runtime->FindSection(Key);
				if (Current && Current->GetResourceIdentity() == Identity && Current->bSimulationCapacityBlocked)
				{
					Current->SimulationCapacityVersion = Scheduler->GetRuntimeResources()->Snapshot().ReleaseVersion;
					Current->SimulationGeometryCapacityVersion = Scheduler->GetTerrainResources()->Snapshot().ReleaseVersion;
					QueueSimulationSection(*Current);
				}
			}
		}
		const double Finished = FPlatformTime::Seconds();
		SimulationStepCostMilliseconds = FMath::Lerp(SimulationStepCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (TerrainFrameContext.IsActive()) TerrainFrameContext.GetPublishBudget().Finish(EVoxelPublishPhase::Prepare, Started, Finished);
	}
}

void UVoxelModule::OnSimulationSectionEvent(const FIntVector& InSection, const FVoxelSectionStamp& InStamp,
	const EVoxelRuntimeSectionEvent InEvent)
{
	auto* Section = Runtime ? Runtime->FindSection(InSection) : nullptr;
	if (!Section || Section->Stamp != InStamp) return;
	if (InEvent == EVoxelRuntimeSectionEvent::Removing || InEvent == EVoxelRuntimeSectionEvent::IdentityChanging)
	{
		UnqueueSimulationSection(*Section);
		ReleaseSimulationSectionFromRegion(*Section);
		// 持有实际 Simulation pin 的保存等待者不能经 Runtime 移除绕过；显式停世界另行撤销。
		check(!Section->SimulationSaveRegion);
		if (NavigationPresenter) NavigationPresenter->InvalidateSection(InSection);
	}
	else if (InEvent == EVoxelRuntimeSectionEvent::Allocated || InEvent == EVoxelRuntimeSectionEvent::Committed)
	{
		if (InEvent == EVoxelRuntimeSectionEvent::Committed && NavigationPresenter) NavigationPresenter->InvalidateSection(InSection);
		WakeSimulationSection(InSection);
	}
}

bool UVoxelModule::RetrySimulationSection(const FIntVector& InSection)
{
	check(IsInGameThread());
	if (!Runtime || !IsAuthority() || !bEnableProjectSceneSimulation) return false;
	auto* Region = GetSceneRegion(InSection, false);
	if (!Region || (!Region->bSceneFailed && !Region->SimulationSaveHead && !Region->bIdleSaveWaiting)) return false;
	Region->bSceneFailed = false;
	Region->bSceneSaveFailed = false;
	Region->RetrySceneLoad();
	Region->IgnoredSceneSaveFailureId = LatestSceneSaveAttempt ? LatestSceneSaveAttempt->TransactionId : FGuid();
	if (Region->SimulationSaveHead || Region->bIdleSaveWaiting) bSceneSaveRequested = true;
	WakeSimulationSection(InSection);
	return true;
}

void UVoxelModule::RegisterSceneRegion(UVoxelSceneRegion& InRegion)
{
	check(!InRegion.bRegisteredRegion && SceneRegions.FindRef(InRegion.GetRegionKey()) == &InRegion);
	NotifySceneCaptureChanged();
	InRegion.PreviousRegion = SceneRegionTail;
	InRegion.NextRegion = nullptr;
	if (SceneRegionTail) SceneRegionTail->NextRegion = &InRegion;
	else SceneRegionHead = &InRegion;
	SceneRegionTail = &InRegion;
	if (!SceneRegionCursor) SceneRegionCursor = &InRegion;
	InRegion.bRegisteredRegion = true;
	InRegion.LastSceneTickWorldSeconds = GetWorld()->GetTimeSeconds();
}

void UVoxelModule::UnregisterSceneRegion(UVoxelSceneRegion& InRegion)
{
	if (!InRegion.bRegisteredRegion) return;
	NotifySceneCaptureChanged();
	check(!InRegion.SimulationFailureHead && !InRegion.SimulationSaveHead);
	if (SceneRegionCursor == &InRegion) SceneRegionCursor = InRegion.NextRegion;
	if (InRegion.PreviousRegion) InRegion.PreviousRegion->NextRegion = InRegion.NextRegion;
	else SceneRegionHead = InRegion.NextRegion;
	if (InRegion.NextRegion) InRegion.NextRegion->PreviousRegion = InRegion.PreviousRegion;
	else SceneRegionTail = InRegion.PreviousRegion;
	InRegion.PreviousRegion = nullptr;
	InRegion.NextRegion = nullptr;
	InRegion.bRegisteredRegion = false;
}

void UVoxelModule::AdvanceSceneRegions()
{
	const double Deadline = TerrainFrameContext.Deadline(0.25);
	const int32 Count = FMath::Min(4, SceneRegions.Num());
	for (int32 Step = 0; Step < Count && SceneRegionHead &&
		FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		if (TerrainFrameContext.IsActive() && !TerrainFrameContext.GetPublishBudget().TryStart(EVoxelPublishPhase::Prepare,
			Started, SceneRegionCostMilliseconds)) break;
		if (!SceneRegionCursor) SceneRegionCursor = SceneRegionHead;
		auto* Region = SceneRegionCursor;
		TGuardValue<bool> Transition(bSceneTransitionActive, true);
		SceneRegionCursor = Region->NextRegion;
		const FIntVector Key = Region->GetRegionKey();
		Region->AdvanceSceneLoad(*Scheduler);
		if (!Runtime || SceneRegions.FindRef(Key) != Region) return;
		ApplySceneSaveReceipt(*Region, Deadline);
		// 保存失败可能影响多个局部 Section；恢复只消费该区域保留的票据。
		auto* Waiting = Region->SimulationFailureCursor ? Region->SimulationFailureCursor : Region->SimulationFailureHead;
		for (int32 Wake = 0; Wake < 16 && Waiting && Region->bSceneReady && !Region->bSceneFailed &&
			FPlatformTime::Seconds() < Deadline; ++Wake)
		{
			auto* Section = Waiting;
			Waiting = Section->SimulationFailureNext;
			const FIntVector SectionKey = Section->SimulationKey;
			const auto Identity = Section->SimulationIdentity;
			if (Runtime && Runtime->FindSection(SectionKey) == Section && Section->GetResourceIdentity() == Identity)
			{
				QueueSimulationSection(*Section);
			}
		}
		Region->SimulationFailureCursor = Waiting;
		const double WorldSeconds = GetWorld()->GetTimeSeconds();
		const float Elapsed = float(FMath::Max(0.0, WorldSeconds - Region->LastSceneTickWorldSeconds));
		Region->LastSceneTickWorldSeconds = WorldSeconds;
		if (!Region->bIdleSaveWaiting) Region->TickSceneActors(Elapsed);
		if (SceneRegions.FindRef(Key) == Region && Region->bSceneReady && !Region->bSceneLoading &&
			!Region->HasActiveSections() && !Region->bSceneFailed &&
			!Region->SimulationFailureHead)
		{
			TArray<uint8> Bytes;
			FString Error;
			bool bCanUnload = !Region->SimulationSaveHead;
			if (bPersistenceEnabledForCurrentWorld || Region->bIdleSaveWaiting)
			{
				if (!Region->bIdleSaveWaiting)
				{
					Region->bIdleSaveWaiting = true;
					if (Region->SceneRevision > Region->DurableSceneRevision) bSceneSaveRequested = true;
				}
				const auto* Cut = DurableSceneSaveReceipt && DurableSceneSaveReceipt->WorldEpoch == Epoch ?
					DurableSceneSaveReceipt->Regions.Find(Key) : nullptr;
				const auto* SavedBytes = Cut && Cut->RegionInstanceId == Region->SceneInstanceId &&
					Cut->Revision >= Region->SceneRevision ? DurableSceneSaveReceipt->FindEncodedFile(
						FVoxelSceneColumnCodec::RelativePath(FIntPoint(Key.X, Key.Y))) : nullptr;
				bCanUnload = bCanUnload && SavedBytes;
				if (bCanUnload) Bytes = *SavedBytes;
			}
			else if (!Region->CaptureActors(Bytes, Error))
			{
				bCanUnload = false;
				Region->bSceneFailed = true;
				UE_LOG(LogTemp, Error, TEXT("Scene migration capture failed; owner retained: %s"), *Error);
			}
			if (bCanUnload)
			{
				UnloadedSceneFiles.Add(FVoxelSceneColumnCodec::RelativePath(FIntPoint(Key.X, Key.Y)), MoveTemp(Bytes));
				UnregisterSceneRegion(*Region);
				SceneRegions.Remove(Key);
				Region->Shutdown();
			}
		}
		const double Finished = FPlatformTime::Seconds();
		SceneRegionCostMilliseconds = FMath::Lerp(SceneRegionCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (TerrainFrameContext.IsActive()) TerrainFrameContext.GetPublishBudget().Finish(EVoxelPublishPhase::Prepare, Started, Finished);
	}
	RequestSceneDurableSave();
	AdvanceSaveCapture();
	if (auto* SaveGame = USaveGameModule::GetPtr(GetWorld())) SaveGame->AdvanceWorldSaveCapture(*this);
}

AActor* UVoxelModule::FindSceneActor(const FGuid& InId) const
{
	for (const TPair<FIntVector, TObjectPtr<UVoxelSceneRegion>>& Pair : SceneRegions)
	{
		if (Pair.Value)
		{
			if (AActor* Actor = Pair.Value->GetSceneActors().FindRef(InId))
			{
				return Actor;
			}
		}
	}
	return nullptr;
}

UVoxelSceneRegion* UVoxelModule::GetSceneRegion(const FIntVector& InSection, const bool bCreate)
{
	const FIntVector Region(
		VoxelGeneration::FloorDivide(InSection.X, 8),
		VoxelGeneration::FloorDivide(InSection.Y, 8),
		0);
	if (TObjectPtr<UVoxelSceneRegion>* Existing = SceneRegions.Find(Region))
	{
		return *Existing;
	}
	if (!bCreate || !SceneRegionClass || !IsAuthority() || !SimulationSections.Contains(InSection))
	{
		return nullptr;
	}
	if (!ReserveSimulationStorage(SimulationSections.Num(), SceneRegions.Num() + 1)) return nullptr;
	if (!RegionStore.IsDirectoryReady()) return nullptr;
	UVoxelSceneRegion* NewRegion = NewObject<UVoxelSceneRegion>(this, SceneRegionClass);
	if (!NewRegion)
	{
		return nullptr;
	}
	NewRegion->Initialize(this, Region);
	const FString ScenePath = FVoxelSceneColumnCodec::RelativePath(FIntPoint(Region.X, Region.Y));
	if (TArray<uint8>* SavedBytes = UnloadedSceneFiles.Find(ScenePath))
	{
		FString RestoreError;
		if (!NewRegion->RestoreActors(*SavedBytes, RestoreError))
		{
			NewRegion->Shutdown();
			UE_LOG(LogTemp, Error, TEXT("Voxel scene restore: %s"), *RestoreError);
			return nullptr;
		}
		UnloadedSceneFiles.Remove(ScenePath);
		NotifySceneCaptureChanged();
	}
	else
	{
		const auto& Directory = RegionStore.GetDirectorySnapshot();
		const auto* Bytes = Directory ? Directory->SceneColumns.Find(FIntPoint(Region.X, Region.Y)) : nullptr;
		if (Bytes)
		{
			if (!NewRegion->BeginSceneLoad(*Scheduler, RegionStore.GetSourceLease(), *Bytes))
			{
				NewRegion->Shutdown();
				return nullptr;
			}
		}
		else NewRegion->bSceneReady = true;
	}
	SceneRegions.Add(Region, NewRegion);
	RegisterSceneRegion(*NewRegion);
	return NewRegion;
}

UVoxelSceneRegion* UVoxelModule::GetSceneRegionForActor(AActor* InActor, const FIntVector& InSection)
{
	if (!IsAuthority() || !::IsValid(InActor) || !SceneRegionClass ||
		!InActor->Implements<USceneActorInterface>() ||
		FindSceneActor(ISceneActorInterface::Execute_GetActorID(InActor)) != InActor) return nullptr;
	if (UVoxelSceneRegion* Existing = GetSceneRegion(InSection, false)) return Existing;
	if (!ReserveSimulationStorage(SimulationSections.Num(), SceneRegions.Num() + 1)) return nullptr;
	if (!RegionStore.IsDirectoryReady()) return nullptr;
	const FIntVector Region(VoxelGeneration::FloorDivide(InSection.X,8),VoxelGeneration::FloorDivide(InSection.Y,8),0);
	UVoxelSceneRegion* Storage = NewObject<UVoxelSceneRegion>(this,SceneRegionClass);
	Storage->Initialize(this,Region);
	const FString Path = FVoxelSceneColumnCodec::RelativePath(FIntPoint(Region.X,Region.Y));
	if (const TArray<uint8>* Bytes = UnloadedSceneFiles.Find(Path))
	{
		FString Error;
		if (!Storage->RestoreActors(*Bytes,Error))
		{
			Storage->Shutdown();
			UE_LOG(LogTemp,Error,TEXT("Actor destination restore failed: %s"),*Error);
			return nullptr;
		}
		UnloadedSceneFiles.Remove(Path);
		NotifySceneCaptureChanged();
	}
	else
	{
		const auto& Directory = RegionStore.GetDirectorySnapshot();
		const auto* Bytes = Directory ? Directory->SceneColumns.Find(FIntPoint(Region.X, Region.Y)) : nullptr;
		if (Bytes)
		{
			if (!Storage->BeginSceneLoad(*Scheduler, RegionStore.GetSourceLease(), *Bytes))
			{
				Storage->Shutdown();
				return nullptr;
			}
		}
		else Storage->bSceneReady = true;
	}
	SceneRegions.Add(Region,Storage);
	RegisterSceneRegion(*Storage);
	return Storage;
}

bool UVoxelModule::ActivateSimulationSection(const FIntVector& InSection)
{
	if (bSceneTransitionActive) return false;
	TGuardValue<bool> Transition(bSceneTransitionActive, true);
	const FVoxelExactDemand* Demand = CurrentInterest->Exact.Find(InSection);
	FVoxelSection* Section = Runtime ? Runtime->FindSection(InSection) : nullptr;
	if (!IsAuthority() || !bEnableProjectSceneSimulation || !Demand || !Demand->bSimulation ||
		!Section || Section->Status != EVoxelSectionStatus::DataReady || !IsCollisionReady(InSection) ||
		SimulationSections.Contains(InSection))
	{
		return false;
	}
	const auto Identity = Section->GetResourceIdentity();
	Section->SimulationKey = InSection;
	Section->SimulationIdentity = Identity;
	if (!ReserveSimulationStorage(SimulationSections.Num() + 1,
		SceneRegions.Num() + (GetSceneRegion(InSection, false) ? 0 : 1))) return false;
	if (Section->bSimulationPinned || !Runtime->TryPinSection(InSection, Identity))
	{
		return false;
	}
	Section->bSimulationPinned = true;
	SimulationSections.Add(InSection);
	UVoxelSceneRegion* Region = GetSceneRegion(InSection, true);
	if (!Region || !Region->bSceneReady || Region->bSceneFailed)
	{
		SimulationSections.Remove(InSection);
		Section->bSimulationPinned = false;
		verify(Runtime->ReleaseSectionPin(InSection, Identity));
		if (Region) HoldSimulationSectionForRegion(*Section, *Region);
		else Section->bSimulationCapacityBlocked = true;
		return false;
	}
	ReleaseSimulationSectionFromRegion(*Section);
	Region->OnSectionActivated(InSection);
	if (NavigationPresenter)
	{
		if (!NavigationPresenter->ActivateSection(InSection))
		{
			Section->bSimulationCapacityBlocked = true;
			Section->SimulationKey = InSection;
			Section->SimulationIdentity = Identity;
			Section->SimulationCapacityVersion = Scheduler->GetRuntimeResources()->Snapshot().ReleaseVersion;
			Section->SimulationGeometryCapacityVersion = Scheduler->GetTerrainResources()->Snapshot().ReleaseVersion;
			QueueSimulationSection(*Section);
		}
	}
	FVoxelSimulationResidencyDelta Delta;
	Delta.AddedSections.Add(InSection);
	Delta.Revision = ++SimulationResidencyRevision;
	OnSimulationResidencyChanged(Delta);
	return true;
}

void UVoxelModule::OnSectionEvicting(const FIntVector& InSection)
{
	if (auto* Section = Runtime ? Runtime->FindSection(InSection) : nullptr)
	{
		UnqueueSimulationSection(*Section);
		ReleaseSimulationSectionFromRegion(*Section);
	}
	if (CollisionPresenter)
	{
		CollisionPresenter->InvalidateSection(InSection);
	}
	if (NavigationPresenter)
	{
		NavigationPresenter->InvalidateSection(InSection);
	}
	RemoveSimulationSection(InSection);
}

void UVoxelModule::RemoveSimulationSection(const FIntVector& InSection)
{
	if (bSceneTransitionActive)
	{
		WakeSimulationSection(InSection);
		return;
	}
	TGuardValue<bool> Transition(bSceneTransitionActive, true);
	if (!SimulationSections.Contains(InSection))
	{
		return;
	}
	auto* Section = Runtime ? Runtime->FindSection(InSection) : nullptr;
	auto* Region = GetSceneRegion(InSection, false);
	if (!Section || !Section->bSimulationPinned || !Region)
	{
		WorldState = EVoxelWorldState::Failed;
		UE_LOG(LogTemp, Error, TEXT("Simulation section %s lost its stable runtime/scene owner"), *InSection.ToString());
		return;
	}
	if (Section->SimulationSaveRegion) return;
	const auto Identity = Section->GetResourceIdentity();
	TArray<uint8> Bytes;
	Section->SimulationKey = InSection;
	Section->SimulationIdentity = Identity;
	FString Error;
	bool bWaitingForScene = false;
	if (!Region->TryDeactivateSection(InSection, Bytes, Error, &bWaitingForScene))
	{
		if (bWaitingForScene)
		{
			HoldSimulationSectionForRegion(*Section, *Region);
			return;
		}
		Region->bSceneFailed = true;
		HoldSimulationSectionForRegion(*Section, *Region);
		UE_LOG(LogTemp, Error, TEXT("Simulation region capture failed; residency and pin retained: %s"), *Error);
		return;
	}
	if (bPersistenceEnabledForCurrentWorld)
	{
		HoldSimulationSectionForSave(*Section, *Region);
		if (!Region->HasActiveSections()) Region->bIdleSaveWaiting = true;
	}
	else
	{
		// 显式非持久世界没有磁盘提交义务，仍使用同一休眠协议和唯一驻留 owner。
		FinishSimulationSectionRetirement(*Section);
		if (!Region->HasActiveSections() && !Region->SimulationFailureHead && !Region->SimulationSaveHead)
		{
			const FIntVector Key = Region->GetRegionKey();
			UnloadedSceneFiles.Add(FVoxelSceneColumnCodec::RelativePath(FIntPoint(Key.X, Key.Y)), MoveTemp(Bytes));
			UnregisterSceneRegion(*Region);
			SceneRegions.Remove(Key);
			Region->Shutdown();
		}
	}
	// 业务模拟在休眠提交后停止；持久世界的资源释放另等实际文件代回执，不能再次广播 Removed。
	FVoxelSimulationResidencyDelta Delta;
	Delta.RemovedSections.Add(InSection);
	Delta.Revision = ++SimulationResidencyRevision;
	OnSimulationResidencyChanged(Delta);
}
