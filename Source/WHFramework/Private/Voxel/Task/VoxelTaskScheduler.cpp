#include "Voxel/Task/VoxelTaskScheduler.h"

#include "HAL/PlatformProcess.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"

#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	constexpr TCHAR TaskBudgetError[] = TEXT("Oversized: voxel task result exceeds its declared unleased budget");
	struct FReceiptDeleter
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		void operator()(FVoxelTerrainWorkReceipt* InReceipt) const
		{
			delete InReceipt;
		}
	};
	bool IsVisualWorkClass(const EVoxelWorkClass InClass)
	{
		switch (InClass)
		{
		case EVoxelWorkClass::Visible:
		case EVoxelWorkClass::Boundary:
		case EVoxelWorkClass::Exploration:
		case EVoxelWorkClass::Background:
			return true;
		default:
			return false;
		}
	}

	bool IsCriticalRepresentationWork(const FVoxelTaskRequest& InRequest)
	{
		if (InRequest.WorkClass != EVoxelWorkClass::Critical) return false;
		switch (InRequest.Kind)
		{
		case EVoxelTaskKind::BuildFineShard:
		case EVoxelTaskKind::BuildFineMesh:
		case EVoxelTaskKind::BuildVoxelProxy:
		case EVoxelTaskKind::BuildSurface:
		case EVoxelTaskKind::BuildWater:
		case EVoxelTaskKind::BuildMacro:
		case EVoxelTaskKind::BuildViewTransition:
		case EVoxelTaskKind::BuildVolumeTransition:
			return true;
		case EVoxelTaskKind::BuildViewCoverage:
			return InRequest.bPublicationContinuation;
		default:
			return false;
		}
	}

int32 TerrainLane(const FVoxelTaskRequest& Request)
	{
		if (Request.WorkClass == EVoxelWorkClass::Critical) return static_cast<int32>(EVoxelTaskServiceLane::Critical);
		switch (Request.Kind)
		{
		case EVoxelTaskKind::GenerateExactBase:
		case EVoxelTaskKind::BuildCollision:
		case EVoxelTaskKind::DecodeOverlay: return static_cast<int32>(EVoxelTaskServiceLane::Exact);
		case EVoxelTaskKind::BuildFineShard:
		case EVoxelTaskKind::BuildFineMesh: return static_cast<int32>(EVoxelTaskServiceLane::Fine);
		case EVoxelTaskKind::GenerateVoxelProxy: return static_cast<int32>(EVoxelTaskServiceLane::ProxyData);
		case EVoxelTaskKind::GenerateSurface: return static_cast<int32>(EVoxelTaskServiceLane::SurfaceData);
		case EVoxelTaskKind::GenerateMacro: return static_cast<int32>(EVoxelTaskServiceLane::MacroData);
		case EVoxelTaskKind::BuildViewCoverage: return static_cast<int32>(EVoxelTaskServiceLane::Coverage);
		case EVoxelTaskKind::BuildViewTransition:
		case EVoxelTaskKind::BuildVolumeTransition: return static_cast<int32>(EVoxelTaskServiceLane::Transition);
		case EVoxelTaskKind::BuildVoxelProxy: return static_cast<int32>(EVoxelTaskServiceLane::ProxyMesh);
		case EVoxelTaskKind::BuildSurface:
		case EVoxelTaskKind::BuildWater: return static_cast<int32>(EVoxelTaskServiceLane::SurfaceMesh);
		case EVoxelTaskKind::BuildMacro: return static_cast<int32>(EVoxelTaskServiceLane::MacroMesh);
		default: return static_cast<int32>(EVoxelTaskServiceLane::Other);
		}
	}

	bool IsCoarseTerrainKind(const EVoxelTaskKind InKind)
	{
		return
			InKind == EVoxelTaskKind::BuildSurface ||
			InKind == EVoxelTaskKind::BuildMacro;
	}

	bool IsRepresentationDataKind(const EVoxelTaskKind InKind)
	{
		return InKind == EVoxelTaskKind::GenerateVoxelProxy ||
			InKind == EVoxelTaskKind::GenerateSurface || InKind == EVoxelTaskKind::GenerateMacro;
	}

	int32 TerrainPriorityBand(const FVoxelTaskRequest& Request)
	{
		if (Request.TerrainStage != INDEX_NONE) return Request.TerrainStage;
		switch (Request.Kind)
		{
		case EVoxelTaskKind::BuildViewCoverage:
		case EVoxelTaskKind::GenerateExactBase:
		case EVoxelTaskKind::DecodeOverlay:
		case EVoxelTaskKind::BuildFineShard:
		case EVoxelTaskKind::BuildFineMesh:
			return 0;
		case EVoxelTaskKind::GenerateVoxelProxy:
		case EVoxelTaskKind::BuildVoxelProxy:
			return 1;
		case EVoxelTaskKind::GenerateSurface:
		case EVoxelTaskKind::BuildSurface:
		case EVoxelTaskKind::BuildWater:
			return 2;
		case EVoxelTaskKind::GenerateMacro:
		case EVoxelTaskKind::BuildMacro:
			return 3;
		case EVoxelTaskKind::BuildVolumeTransition:
			return Request.WorkClass <= EVoxelWorkClass::Interactive ? 0 : 1;
		default:
			return Request.WorkClass >= EVoxelWorkClass::Critical &&
				Request.WorkClass <= EVoxelWorkClass::Interactive ? -1 : 4;
		}
	}

	int32 TerrainMeshStage(const EVoxelTaskKind InKind, const EVoxelWorkClass InClass,
		const int32 InStage, const bool bInPublicationContinuation = false, const bool bInTerrainDataOnly = false)
	{
		if (bInTerrainDataOnly) return INDEX_NONE;
		if (InClass == EVoxelWorkClass::Critical && !IsRepresentationDataKind(InKind))
			return 0;
		switch (InKind)
		{
		case EVoxelTaskKind::BuildFineShard:
		case EVoxelTaskKind::BuildFineMesh: return 0;
		case EVoxelTaskKind::BuildVoxelProxy: return InStage == INDEX_NONE ? 1 : InStage;
		case EVoxelTaskKind::BuildSurface:
		case EVoxelTaskKind::BuildWater: return 2;
		case EVoxelTaskKind::BuildMacro: return 3;
		case EVoxelTaskKind::BuildViewCoverage: return bInPublicationContinuation ? InStage : INDEX_NONE;
		case EVoxelTaskKind::BuildViewTransition: return InStage;
		case EVoxelTaskKind::BuildVolumeTransition: return InClass <= EVoxelWorkClass::Interactive ? 0 : 1;
		default: return INDEX_NONE;
		}
	}

	bool IsBackgroundTerrainWork(const EVoxelTaskKind InKind, const EVoxelWorkClass InClass, const int32 InStage)
	{
		if (InClass == EVoxelWorkClass::Critical)
			return false;
		if (IsRepresentationDataKind(InKind)) return true;
		if (InKind == EVoxelTaskKind::BuildViewCoverage && InStage > 0) return true;
		if (TerrainMeshStage(InKind, InClass, InStage) > 0) return true;
		return (InKind == EVoxelTaskKind::BuildGenerationPlan || InKind == EVoxelTaskKind::ProjectBackground) &&
			InClass >= EVoxelWorkClass::Background;
	}

}

bool FVoxelTaskStamp::operator==(const FVoxelTaskStamp& InOther) const
{
	return OwnerId == InOther.OwnerId && OwnerIncarnation == InOther.OwnerIncarnation &&
		WorldEpoch == InOther.WorldEpoch &&
		Token == InOther.Token &&
		Revision == InOther.Revision &&
		GeometryRevision == InOther.GeometryRevision &&
		bHasGeometryIdentity == InOther.bHasGeometryIdentity &&
		(!bHasGeometryIdentity || GeometryIdentity == InOther.GeometryIdentity) &&
		Section == InOther.Section &&
		ViewKey == InOther.ViewKey &&
		SurfaceKey == InOther.SurfaceKey &&
		MacroKey == InOther.MacroKey &&
		RegistryRevision == InOther.RegistryRevision &&
		ShapeRevision == InOther.ShapeRevision;
}

FVoxelTaskResult& FVoxelTaskResult::operator=(FVoxelTaskResult&& InOther)
{
	if (this != &InOther)
	{
		this->~FVoxelTaskResult();
		new(this) FVoxelTaskResult(MoveTemp(InOther));
	}
	return *this;
}

FVoxelTaskRequest& FVoxelTaskRequest::operator=(FVoxelTaskRequest&& InOther)
{
	if (this != &InOther)
	{
		this->~FVoxelTaskRequest();
		new(this) FVoxelTaskRequest(MoveTemp(InOther));
	}
	return *this;
}

#if WITH_DEV_AUTOMATION_TESTS
FVoxelTaskRequest FVoxelTaskRequest::MakePriorityCopy() const
{
	FVoxelTaskRequest Copy;
	Copy.Stamp = Stamp;
	Copy.Kind = Kind;
	Copy.WorkClass = WorkClass;
	Copy.SourcePriority = SourcePriority;
	Copy.TerrainStage = TerrainStage;
	Copy.bPublicationContinuation = bPublicationContinuation;
	Copy.bTerrainDataOnly = bTerrainDataOnly;
	Copy.DistanceScore = DistanceScore;
	Copy.ForwardScore = ForwardScore;
	Copy.SourceStableKey = SourceStableKey;
	Copy.ServiceBand = ServiceBand;
	Copy.FirstRunnableSeconds = FirstRunnableSeconds;
	Copy.QueuedAt = QueuedAt;
	Copy.Sequence = Sequence;
	Copy.InputBytes = InputBytes;
	Copy.ReservedBytes = ReservedBytes;
	return Copy;
}
#endif

bool FVoxelTaskScheduler::FNativeTask::CanRetire() const
{
	return !Pimpl || (Pimpl->IsCompleted() && Pimpl->GetRefCount(std::memory_order_acquire) == 1);
}

void FVoxelTaskScheduler::FNativeTask::Retire()
{
	check(CanRetire());
	Pimpl.SafeRelease();
}

void FVoxelTaskScheduler::FTaskBody::operator()() const
{
	FVoxelTaskRequest& Request = Slot->Request;
	FVoxelGenerationConsumerScope ConsumerScope(Request.GenerationConsumer);
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TaskExecute);
	const double Start = FPlatformTime::Seconds();
	if (!Slot->Cancel.Load())
	{
		Slot->Result = Request.Execute(Slot->Cancel);
	}
	const double End = FPlatformTime::Seconds();
	Slot->Result.Stamp = Stamp;
	Slot->Result.Kind = Request.Kind;
	Slot->Result.bCanceled = Slot->Cancel.Load();
	Slot->Result.QueueMilliseconds = FMath::Max(0.0, (Start - Request.QueuedAt) * 1000.0);
	Slot->Result.ExecuteMilliseconds = FMath::Max(0.0, (End - Start) * 1000.0) + Request.AccumulatedExecuteMilliseconds;
	Slot->ExecuteMicroseconds = FMath::Max(0.0, (End - Start) * 1000000.0);
	const uint64 ResultBytes = Slot->Result.ResultBytes();
	if (ResultBytes > Request.ReservedBytes && ResultBytes - Request.ReservedBytes > Slot->Result.GetResourceAccountedBytes())
	{
		FVoxelTaskResult ErrorResult;
		ErrorResult.Stamp = Stamp;
		ErrorResult.Kind = Request.Kind;
		ErrorResult.QueueMilliseconds = Slot->Result.QueueMilliseconds;
		ErrorResult.ExecuteMilliseconds = Slot->Result.ExecuteMilliseconds;
		ErrorResult.Error = MoveTemp(Slot->BudgetError);
		ErrorResult.InputResourceLease = Slot->BudgetErrorLease;
		Slot->Result = MoveTemp(ErrorResult);
	}
}

uint64 FVoxelTaskResult::ResultBytes() const
{
	uint64 Bytes =
		static_cast<uint64>(BaseBlocks.Num()) * sizeof(FVoxelBlockState) +
		Payload.Num();

	Bytes += Collision ? Collision->Bytes() : 0;
	Bytes += FineMesh ? FineMesh->Bytes() : 0;
	Bytes += VoxelProxyMesh ? VoxelProxyMesh->Bytes() : 0;
	Bytes += VolumeBoundaryMesh ? VolumeBoundaryMesh->Bytes() : 0;
	Bytes += SurfaceMesh ? SurfaceMesh->Bytes() : 0;
	Bytes += WaterMesh ? WaterMesh->Bytes() : 0;
	Bytes += MacroMesh ? MacroMesh->Bytes() : 0;

	if (VoxelProxy)
	{
		Bytes += VoxelProxy->GetAllocatedBytes();
	}
	if (Surface)
	{
		Bytes += sizeof(FVoxelSurfaceTileData) + Surface->GroundZ.GetAllocatedSize() +
			Surface->WaterZ.GetAllocatedSize() + Surface->SurfaceMaterial.GetAllocatedSize() +
			Surface->Biome.GetAllocatedSize() + Surface->Flags.GetAllocatedSize() + Surface->DistantCells.GetAllocatedSize();
	}
	if (Water)
	{
		Bytes += sizeof(FVoxelWaterSurfaceTileData) + Water->GroundZ.GetAllocatedSize() +
			Water->WaterZ.GetAllocatedSize() + Water->WaterKind.GetAllocatedSize();
	}
	if (Macro)
	{
		Bytes += sizeof(FVoxelMacroTileData) + Macro->Height.GetAllocatedSize() + Macro->WaterHeight.GetAllocatedSize() +
			Macro->WaterKind.GetAllocatedSize() +
			Macro->SurfaceClass.GetAllocatedSize() + Macro->ForestCoverage.GetAllocatedSize() + Macro->SnowCoverage.GetAllocatedSize() +
			Macro->LargeStructures.GetAllocatedSize() + Macro->DistantCells.GetAllocatedSize();
	}

	Bytes += Details ? Details->GetAllocatedBytes() : 0;
	Bytes += OverlayReadResult ? OverlayReadResult->AllocatedBytes() : 0;

	if (PersistentSection)
	{
		Bytes += VoxelBoundedAllocation::WorkerSharedBytes<FVoxelPersistentSection>() +
			PersistentSection->Blocks.GetAllocatedSize() + PersistentSection->Entities.GetAllocatedSize();

		for (const TPair<int32, FVoxelBlockEntityState>& Pair :
			PersistentSection->Entities)
		{
			Bytes += Pair.Value.Payload.GetAllocatedSize();
		}
	}

	Bytes +=
		CustomPayload
			? CustomPayload->GetAllocatedBytes()
			: 0;

	return Bytes;
}

uint64 FVoxelTaskResult::GetResourceAccountedBytes() const
{
	const FVoxelTerrainResourceLease* Leases[13] = {};
	int32 Count = 0;
	uint64 Bytes = CustomPayload ? CustomPayload->GetResourceAccountedBytes() : 0;
	Bytes += OverlayReadResult ? OverlayReadResult->ResourceAccountedBytes : 0;
	const auto Add = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		if (!Lease) return;
		for (int32 Index = 0; Index < Count; ++Index) if (Leases[Index] == Lease.Get()) return;
		check(Count < UE_ARRAY_COUNT(Leases));
		Leases[Count++] = Lease.Get();
		Bytes += Lease->Bytes();
	};
	Add(InputResourceLease);
	Add(OutputResourceLease);
	if (PersistentSection) Add(PersistentSection->ResourceLease);
	if (VoxelProxy) Add(VoxelProxy->ResourceLease);
	if (Surface) Add(Surface->ResourceLease);
	if (Water) Add(Water->ResourceLease);
	if (Macro) Add(Macro->ResourceLease);
	if (FineMesh) Add(FineMesh->ResourceLease);
	if (VoxelProxyMesh) Add(VoxelProxyMesh->ResourceLease);
	if (VolumeBoundaryMesh) Add(VolumeBoundaryMesh->ResourceLease);
	if (SurfaceMesh) Add(SurfaceMesh->ResourceLease);
	if (WaterMesh) Add(WaterMesh->ResourceLease);
	if (MacroMesh) Add(MacroMesh->ResourceLease);
	return Bytes;
}

FVoxelTaskScheduler::FVoxelTaskScheduler()
{
}

#if WITH_DEV_AUTOMATION_TESTS
FVoxelTaskScheduler::FVoxelTaskScheduler(const bool bInAllowPassive)
	: bAllowPassive(bInAllowPassive)
{
}
#endif

FVoxelTaskScheduler::~FVoxelTaskScheduler()
{
	StopAndJoin();
}

bool FVoxelTaskScheduler::PrepareBacking(const uint64 InNewBytes)
{
	if (InNewBytes > MAX_uint64 - BackingBytes)
	{
		BackingAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return false;
	}
	if (!ControlConsumer.IsValid())
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (!bAllowPassive)
		{
			BackingAdmission = EVoxelTerrainResourceAdmission::CapacityBlocked;
			return false;
		}
		BackingBytes += InNewBytes;
		BackingAdmission = EVoxelTerrainResourceAdmission::Granted;
		return true;
#else
		BackingAdmission = EVoxelTerrainResourceAdmission::CapacityBlocked;
		return false;
#endif
	}
	const uint64 Peak = BackingBytes + InNewBytes;
	if (RequiredAdmissionInputBytes > MAX_uint64 - (IdleBindingLease ? IdleBindingLease->Bytes() : 0) ||
		RequiredAdmissionLeaseCount > MAX_int32 - int32(IdleBindingLease.IsValid()))
	{
		BackingAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return false;
	}
	const uint64 Required = RequiredAdmissionInputBytes + (IdleBindingLease ? IdleBindingLease->Bytes() : 0);
	const int32 RequiredLeases = RequiredAdmissionLeaseCount + int32(IdleBindingLease.IsValid());
	BackingAdmission = BackingLease ? BackingLease->TryResize(Peak, Required, RequiredLeases) :
		TerrainResources->TryAcquire(ControlConsumer, EVoxelTerrainResourcePool::Input, Peak,
			BackingLease, 0, Required, RequiredLeases);
	if (BackingAdmission != EVoxelTerrainResourceAdmission::Granted) return false;
	BackingBytes = Peak;
	return true;
}

void FVoxelTaskScheduler::SettleBacking(const uint64 InOldBytes, const uint64 InReservedBytes, const uint64 InActualBytes)
{
	check(InActualBytes <= InReservedBytes && InOldBytes <= BackingBytes - InReservedBytes);
	BackingBytes = BackingBytes - InOldBytes - InReservedBytes + InActualBytes;
	if (BackingLease) verify(BackingLease->Resize(BackingBytes));
}

bool FVoxelTaskScheduler::PrepareAdmission(const FVoxelTaskKey& InKey)
{
	if (InKey.Kind <= EVoxelTaskKind::None || InKey.Kind > EVoxelTaskKind::BuildFineShard)
	{
		BackingAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	if (AdmissionAttempts.Contains(InKey)) return true;
	if (AdmissionAttempts.Num() == MAX_int32 || NextSequence == MAX_uint64)
	{
		BackingAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return false;
	}
	return ReserveSet<TPair<FVoxelTaskKey, FAdmissionAttempt>>(AdmissionAttempts, AdmissionAttempts.Num() + 1);
}

bool FVoxelTaskScheduler::PrepareQueues(const int32 InAdditionalTasks)
{
	const int64 Total = int64(Pending.Num()) + Running.Num() + InAdditionalTasks;
	const int64 CanceledTotal = Total + Canceled.Num();
	const int64 Notifications = Total + SectionIdleNotifications.Num();
	if (Total > MAX_int32 || CanceledTotal > MAX_int32 || Notifications > MAX_int32)
	{
		BackingAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return false;
	}
	const int32 Capacity = int32(Total);
	const int32 RunningCapacity = FMath::Min(Capacity, Budget.MaxConcurrentTasks);
	return ReserveArray(Pending, Capacity) && ReserveArray(Running, RunningCapacity) &&
		ReserveArray(Canceled, int32(CanceledTotal)) && ReserveArray(WorkerCandidates, Capacity) &&
		ReserveArray(CompletedCandidates, RunningCapacity) && ReserveArray(DisplacedIndices, Pending.Num()) &&
		ReserveArray(SectionIdleNotifications, int32(Notifications)) &&
		ReserveSet<FVoxelTaskKey>(ActiveKeys, Capacity) &&
		ReserveSet<TPair<FIntVector, int32>>(SectionTaskCounts, Capacity);
}

void FVoxelTaskScheduler::FreeBacking()
{
	Pending.Empty();
	Running.Empty();
	Canceled.Empty();
	WorkerCandidates.Empty();
	CompletedCandidates.Empty();
	DisplacedIndices.Empty();
	SectionIdleNotifications.Empty();
	ActiveKeys.Empty();
	SectionTaskCounts.Empty();
	AdmissionAttempts.Empty();
	SectionIdleCallback = nullptr;
	IdleBindingLease.Reset();
	BackingBytes = 0;
	BackingLease.Reset();
}

void FVoxelTaskScheduler::FlushSectionIdleNotifications()
{
	if (bFlushingIdle || bStopped) return;
	TGuardValue<bool> NotifyGuard(bFlushingIdle, true);
	while (!SectionIdleNotifications.IsEmpty())
	{
		const FIntVector Section = SectionIdleNotifications.Pop(EAllowShrinking::No);
		if (SectionTaskCounts.Contains(Section) || !SectionIdleCallback) continue;
		const uint64 Generation = IdleBindingGeneration;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CallbackLease = IdleBindingLease;
		TFunction<void(const FIntVector&)> Callback = MoveTemp(SectionIdleCallback);
		Callback(Section);
		if (!bStopped && IdleBindingGeneration == Generation) SectionIdleCallback = MoveTemp(Callback);
	}
}

void FVoxelTaskScheduler::RefreshBlockReasons()
{
	const uint64 LastInitialSequence = NextSequence;
	BlockRefreshSequence = LastInitialSequence;
	uint64 VisitedSequence = 0;
	while (true)
	{
		FVoxelTaskRequest* Next = nullptr;
		for (FVoxelTaskRequest& Request : Pending)
		{
			if (Request.Sequence > VisitedSequence && Request.Sequence <= LastInitialSequence &&
				(!Next || Request.Sequence < Next->Sequence)) Next = &Request;
		}
		if (!Next) break;
		const FVoxelTaskKey Key{Next->Kind, Next->Stamp};
		VisitedSequence = Next->Sequence;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CallbackLease = Next->BlockBindingLease;
		TFunction<EVoxelTaskBlockReason()> Callback = MoveTemp(Next->GetBlockReason);
		Next = nullptr;
		const EVoxelTaskBlockReason Reason = Callback ? Callback() : EVoxelTaskBlockReason::None;
		if (bStopped) return;
		if (FVoxelTaskRequest* Original = Pending.FindByPredicate([&Key, VisitedSequence](const FVoxelTaskRequest& Request)
			{ return Request.Sequence == VisitedSequence && Request.Kind == Key.Kind && Request.Stamp == Key.Stamp; }))
		{
			Original->GetBlockReason = MoveTemp(Callback);
			Original->Blocked = Reason;
		}
	}
}

FVoxelGenerationConsumerKey FVoxelTaskScheduler::ResolveNativeConsumer(const FVoxelTaskRequest& InRequest) const
{
	if (InRequest.GenerationConsumer.IsValid()) return InRequest.GenerationConsumer;
	// 非生成控制任务沿用 Scheduler 的原空间键，只以已冻结 Sequence 区分尝试。
	FVoxelGenerationConsumerKey Consumer = ControlConsumer;
	Consumer.RequestSerial = InRequest.Sequence;
	return Consumer;
}

bool FVoxelTaskScheduler::ObserveDemand(const EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp,
	const EVoxelTaskBlockReason InReason, const uint64 InInputBytes, const uint64 InResultBytes)
{
	check(IsInGameThread());
	if (ControlConsumer.IsValid() && !bAllowPassive && InStamp.WorldEpoch != ControlConsumer.WorldEpoch) return false;
	if (bStopped || !PrepareAdmission({InKind, InStamp})) return false;
	auto& Attempt = AdmissionAttempts.FindOrAdd({InKind, InStamp});
	if (Attempt.Sequence == 0)
	{
		Attempt.FirstRequested = FPlatformTime::Seconds();
		Attempt.Sequence = ++NextSequence;
	}
	Attempt.Blocked = InReason;
	Attempt.InputBytes = InInputBytes;
	Attempt.ResultBytes = InResultBytes;
	return true;
}

bool FVoxelTaskScheduler::IsTerminal(const FVoxelTaskKey& InKey) const
{
	const auto* Attempt = AdmissionAttempts.Find(InKey);
	return Attempt && (Attempt->Blocked == EVoxelTaskBlockReason::Oversized ||
		Attempt->Blocked == EVoxelTaskBlockReason::TerminalFailure);
}

bool FVoxelTaskScheduler::Enqueue(FVoxelTaskRequest&& InRequest)
{
	check(IsInGameThread());
	const FVoxelTaskKey AttemptKey{InRequest.Kind, InRequest.Stamp};
	const bool bOriginalBinding = InRequest.BindingResources.Get() == &TerrainResources.Get();
	if (bOriginalBinding && (InRequest.GetBindingBytes() > MAX_uint64 - InRequest.GetRequiredBindingInputBytes() ||
		InRequest.GetBindingLeaseCount() > MAX_int32 - InRequest.GetRequiredBindingInputLeaseCount()))
	{
		InRequest.Blocked = EVoxelTaskBlockReason::Oversized;
		return false;
	}
	const uint64 CallbackInputs = bOriginalBinding ? InRequest.GetBindingBytes() + InRequest.GetRequiredBindingInputBytes() : 0;
	const int32 CallbackLeases = bOriginalBinding ? InRequest.GetBindingLeaseCount() + InRequest.GetRequiredBindingInputLeaseCount() : 0;
	{
		TGuardValue<uint64> InputGuard(RequiredAdmissionInputBytes, CallbackInputs);
		TGuardValue<int32> CountGuard(RequiredAdmissionLeaseCount, CallbackLeases);
		if (bStopped || !PrepareAdmission(AttemptKey))
		{
			InRequest.Blocked = bStopped || BackingAdmission == EVoxelTerrainResourceAdmission::InvalidRequest ?
				EVoxelTaskBlockReason::TerminalFailure : BackingAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
				EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory;
			return false;
		}
	}
	FAdmissionAttempt& Attempt = AdmissionAttempts.FindOrAdd(AttemptKey);
	if (Attempt.Sequence == 0)
	{
		Attempt.FirstRequested = FPlatformTime::Seconds();
		Attempt.Sequence = ++NextSequence;
	}
	Attempt.InputBytes = InRequest.InputBytes;
	Attempt.ResultBytes = InRequest.ReservedBytes;
	Attempt.Blocked = EVoxelTaskBlockReason::None;
	InRequest.QueuedAt = Attempt.FirstRequested;
	InRequest.Sequence = Attempt.Sequence;
	const auto Reject = [&Attempt, &InRequest](const EVoxelTaskBlockReason Reason)
	{
		Attempt.Blocked = Reason;
		InRequest.Blocked = Reason;
		++Attempt.Rejections;
		return false;
	};

	if (InRequest.ReservedBytes > Budget.MaxReservedBytes || InRequest.InputBytes > Budget.MaxInputBytes)
	{
		++Diagnostics.OversizedRequests;
		const double Now = FPlatformTime::Seconds();
		if (Now - LastBudgetWarning >= 5.0)
		{
			LastBudgetWarning = Now;
			UE_LOG(LogTemp, Warning, TEXT("Voxel task exceeds budget: kind=%d input=%llu/%llu reserved=%llu/%llu"),
				static_cast<int32>(InRequest.Kind), InRequest.InputBytes, Budget.MaxInputBytes,
				InRequest.ReservedBytes, Budget.MaxReservedBytes);
		}
	}
	if (bStopped ||
		!InRequest.Execute ||
		(InRequest.GenerationConsumer.Kind != EVoxelGenerationConsumerKind::None &&
			(!InRequest.GenerationConsumer.IsValid() || InRequest.GenerationConsumer.WorldEpoch != InRequest.Stamp.WorldEpoch ||
			InRequest.GenerationConsumer.OwnerId != InRequest.Stamp.OwnerId ||
			InRequest.GenerationConsumer.OwnerIncarnation != InRequest.Stamp.OwnerIncarnation)) ||
		InRequest.Kind == EVoxelTaskKind::None ||
		InRequest.WorkClass == EVoxelWorkClass::None ||
		!FMath::IsFinite(InRequest.DistanceScore) ||
		InRequest.DistanceScore < 0.0 ||
		!FMath::IsFinite(InRequest.ForwardScore) ||
		static_cast<uint8>(InRequest.ServiceBand) >= static_cast<uint8>(EVoxelTerrainServiceBand::Count) ||
		(InRequest.ReservedBytes == 0 && (InRequest.Kind != EVoxelTaskKind::BuildGenerationPlan || !InRequest.GenerationConsumer.IsValid())) ||
		InRequest.ReservedBytes > Budget.MaxReservedBytes ||
		InRequest.InputBytes > Budget.MaxInputBytes)
	{
		return Reject(InRequest.ReservedBytes > Budget.MaxReservedBytes || InRequest.InputBytes > Budget.MaxInputBytes
			? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::TerminalFailure);
	}
	if (ControlConsumer.IsValid() && !bAllowPassive &&
		(InRequest.Stamp.WorldEpoch != ControlConsumer.WorldEpoch || !InRequest.bExecuteBound ||
			(InRequest.Apply && !InRequest.bApplyBound) || (InRequest.GetBlockReason && !InRequest.bBlockBound) ||
			InRequest.BindingResources.Get() != &TerrainResources.Get() ||
			InRequest.BindingConsumer != (InRequest.GenerationConsumer.IsValid() ? InRequest.GenerationConsumer : ControlConsumer)))
	{
		return Reject(EVoxelTaskBlockReason::TerminalFailure);
	}

	if (Has(InRequest.Stamp, InRequest.Kind))
	{
		++Diagnostics.DuplicateRequests;
		return Reject(EVoxelTaskBlockReason::Dependency);
	}

	if (IsCoarseTerrainKind(InRequest.Kind) && InRequest.WorkClass != EVoxelWorkClass::Critical)
	{
		int32 PendingCoarseTerrainTasks = 0;

		for (const FVoxelTaskRequest& PendingRequest : Pending)
		{
			PendingCoarseTerrainTasks +=
				IsCoarseTerrainKind(PendingRequest.Kind)
					? 1
					: 0;
		}

		if (PendingCoarseTerrainTasks >=
			Budget.MaxPendingCoarseTerrainTasks)
		{
			return Reject(EVoxelTaskBlockReason::PendingCapacity);
		}
	}

	const bool bBackground = IsBackgroundTerrainWork(InRequest.Kind, InRequest.WorkClass, InRequest.TerrainStage);
	const int32 Lane = TerrainLane(InRequest);
	constexpr int32 LaneCount = static_cast<int32>(EVoxelTaskServiceLane::Count);
	const int32 ReservedPerLane = Budget.MaxPendingTasks >= LaneCount - 1 ? 1 : 0;
	int32 Counts[LaneCount] = {};
	for (const auto& Existing : Pending) ++Counts[TerrainLane(Existing)];
	int32 PendingLimit = Budget.MaxPendingTasks;
	for (int32 Index = 1; Index < LaneCount - 1; ++Index)
		if (Index != Lane) PendingLimit -= FMath::Max(0, ReservedPerLane - Counts[Index]);
	// 先完整验证替换集合，再取消旧请求，内存不足不能导致部分取消后仍然拒绝新请求。
	{
		TGuardValue<uint64> InputGuard(RequiredAdmissionInputBytes, CallbackInputs);
		TGuardValue<int32> CountGuard(RequiredAdmissionLeaseCount, CallbackLeases);
		if (!PrepareQueues(1))
		{
			return Reject(BackingAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
				EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
		}
	}
	DisplacedIndices.Reset();
	uint64 RemainingInput = QueuedInputBytes + RunningContinuationInputBytes;
	while (Pending.Num() - DisplacedIndices.Num() >= PendingLimit ||
		RemainingInput > Budget.MaxInputBytes ||
		InRequest.InputBytes > Budget.MaxInputBytes - FMath::Min(RemainingInput, Budget.MaxInputBytes))
	{
		int32 Worst = INDEX_NONE;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			if (DisplacedIndices.Contains(Index)) continue;
			const FVoxelTaskRequest& Existing = Pending[Index];
			const int32 ExistingLane = TerrainLane(Existing);
			const int32 Reserve = ExistingLane == static_cast<int32>(EVoxelTaskServiceLane::Other) ? 0 : ReservedPerLane;
			if (ExistingLane != INDEX_NONE && ExistingLane != Lane && Counts[ExistingLane] <= Reserve)
				continue;
			const bool bExistingBackground = IsBackgroundTerrainWork(Existing.Kind, Existing.WorkClass, Existing.TerrainStage);
			const bool bMayReplace = (!bBackground && bExistingBackground) ||
				(bBackground == bExistingBackground && (IsHigherPriority(InRequest, Existing) ||
					(FPlatformTime::Seconds() - InRequest.QueuedAt >= 0.5 && InRequest.Sequence < Existing.Sequence && Existing.WorkClass != EVoxelWorkClass::Critical) ||
					(Counts[Lane] == 0 && Counts[ExistingLane] > Reserve)));
			if (bMayReplace && (Worst == INDEX_NONE || IsHigherPriority(Pending[Worst], Existing))) Worst = Index;
		}
		if (Worst == INDEX_NONE)
		{
			++Diagnostics.CapacityDeferrals;
			return Reject(RemainingInput > Budget.MaxInputBytes ||
				InRequest.InputBytes > Budget.MaxInputBytes - FMath::Min(RemainingInput, Budget.MaxInputBytes)
				? EVoxelTaskBlockReason::InputMemory : EVoxelTaskBlockReason::PendingCapacity);
		}
		DisplacedIndices.Add(Worst);
		RemainingInput -= Pending[Worst].InputBytes;
		const int32 ExistingLane = TerrainLane(Pending[Worst]);
		if (ExistingLane != INDEX_NONE) --Counts[ExistingLane];
	}
	DisplacedIndices.Sort([](const int32 A, const int32 B) { return A > B; });
	for (const int32 Index : DisplacedIndices)
	{
		FVoxelTaskRequest Displaced = MoveTemp(Pending[Index]);
		Pending.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		QueuedInputBytes -= Displaced.InputBytes;
		const FVoxelTaskKey DisplacedKey{Displaced.Kind, Displaced.Stamp};
		const FAdmissionAttempt Preserved = AdmissionAttempts.FindChecked(DisplacedKey);
		RemoveActive(Displaced.Stamp, Displaced.Kind, Displaced.WorkClass);
		AdmissionAttempts.Add(DisplacedKey, Preserved).Blocked = EVoxelTaskBlockReason::PendingCapacity;
		QueueCanceled(MoveTemp(Displaced));
	}

	AdmissionAttempts.FindChecked(AttemptKey).Blocked = EVoxelTaskBlockReason::None;
	QueuedInputBytes +=
		InRequest.InputBytes;

	AddActive(
		InRequest.Stamp,
		InRequest.Kind,
		InRequest.WorkClass);

	check(Pending.Num() < Pending.Max());
	Pending.Add(
		MoveTemp(InRequest));

	Pump();
	FlushSectionIdleNotifications();
	return true;
}

TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> FVoxelTaskScheduler::GetTerrainResources() const
{
	return TerrainResources;
}

bool FVoxelTaskScheduler::InitializeStatistics(const uint64 InWorldEpoch)
{
	check(IsInGameThread());
	if (Statistics || !InWorldEpoch || BackingBytes) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(InWorldEpoch, StatisticsOwnerId, 1, FIntVector::ZeroValue, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
	if (!RuntimeResources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainStatistics>(), Metadata)) return false;
	auto Candidate = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainStatistics>(Metadata);
	if (!Candidate->Initialize(RuntimeResources, Consumer)) return false;
	StatisticsWorldEpoch = InWorldEpoch;
	StatisticsRootInputBytes = Metadata->Bytes();
	const auto SelectionConsumer = FVoxelGenerationConsumerKey::Section(InWorldEpoch, StatisticsOwnerId, 1, {}, 1);
	ControlConsumer = SelectionConsumer;
	WorkerSelection.BindResources(TerrainResources, SelectionConsumer);
	CompletedSelection.BindResources(TerrainResources, SelectionConsumer);
	Statistics = MoveTemp(Candidate);
	return true;
}

TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> FVoxelTaskScheduler::GetRuntimeResources() const
{
	return RuntimeResources;
}

void FVoxelTaskScheduler::VisitPriorityStates(TFunctionRef<void(FVoxelTaskRequest&)> InUpdate)
{
	const uint64 LastInitialSequence = NextSequence;
	uint64 VisitedSequence = 0;
	while (true)
	{
		FVoxelTaskRequest Identity;
		uint64 Next = MAX_uint64;
		const auto Capture = [&Identity, &Next, VisitedSequence, LastInitialSequence](const auto& Request)
		{
			if (Request.Sequence <= VisitedSequence || Request.Sequence > LastInitialSequence || Request.Sequence >= Next) return;
			Next = Request.Sequence;
			Identity.Stamp = Request.Stamp;
			Identity.Kind = Request.Kind;
			Identity.WorkClass = Request.WorkClass;
			Identity.SourcePriority = Request.SourcePriority;
			Identity.DistanceScore = Request.DistanceScore;
			Identity.ForwardScore = Request.ForwardScore;
			Identity.SourceStableKey = Request.SourceStableKey;
			Identity.ServiceBand = Request.ServiceBand;
			Identity.TerrainStage = Request.TerrainStage;
		};
		for (const FVoxelTaskRequest& Request : Pending) Capture(Request);
		for (const FRunning& Request : Running) Capture(Request);
		if (Next == MAX_uint64) break;
		VisitedSequence = Next;
		InUpdate(Identity);
		const auto Restore = [this, &Identity, Next](auto& Request)
		{
			if (Request.Sequence != Next || Request.Kind != Identity.Kind || !(Request.Stamp == Identity.Stamp)) return false;
			CriticalTaskCount += int32(Identity.WorkClass == EVoxelWorkClass::Critical) - int32(Request.WorkClass == EVoxelWorkClass::Critical);
			Request.WorkClass = Identity.WorkClass;
			Request.SourcePriority = Identity.SourcePriority;
			Request.DistanceScore = Identity.DistanceScore;
			Request.ForwardScore = Identity.ForwardScore;
			Request.SourceStableKey = Identity.SourceStableKey;
			Request.ServiceBand = Identity.ServiceBand;
			Request.TerrainStage = Identity.TerrainStage;
			return true;
		};
		for (FVoxelTaskRequest& Request : Pending) if (Restore(Request)) break;
		for (FRunning& Request : Running)
		{
			if (!Restore(Request)) continue;
			Request.Slot->Request.WorkClass = Request.WorkClass;
			Request.Slot->Request.SourcePriority = Request.SourcePriority;
			Request.Slot->Request.DistanceScore = Request.DistanceScore;
			Request.Slot->Request.ForwardScore = Request.ForwardScore;
			Request.Slot->Request.SourceStableKey = Request.SourceStableKey;
			Request.Slot->Request.ServiceBand = Request.ServiceBand;
			Request.Slot->Request.TerrainStage = Request.TerrainStage;
			break;
		}
	}
}

void FVoxelTaskScheduler::UpdatePriorities(TFunctionRef<void(EVoxelTaskKind,
	const FVoxelTaskStamp&, EVoxelWorkClass&, int32&, double&, double&)> InUpdate)
{
	check(IsInGameThread());
	VisitPriorityStates([&InUpdate](FVoxelTaskRequest& Identity)
	{
		InUpdate(Identity.Kind, Identity.Stamp, Identity.WorkClass, Identity.SourcePriority, Identity.DistanceScore, Identity.ForwardScore);
	});
}

void FVoxelTaskScheduler::UpdateServicePriorities(TFunctionRef<void(EVoxelTaskKind,
	const FVoxelTaskStamp&, EVoxelWorkClass&, FVoxelTerrainServicePriority&)> InUpdate)
{
	check(IsInGameThread());
	VisitPriorityStates([&InUpdate](FVoxelTaskRequest& Identity)
	{
		FVoxelTerrainServicePriority Priority = ResolveServicePriority(Identity);
		EVoxelWorkClass WorkClass = Identity.WorkClass;
		InUpdate(Identity.Kind, Identity.Stamp, WorkClass, Priority);
		if (!ensureMsgf(FMath::IsFinite(Priority.DistanceCells) && Priority.DistanceCells >= 0.0 &&
			FMath::IsFinite(Priority.ForwardScore) && Priority.Band > EVoxelTerrainServiceBand::None &&
			Priority.Band < EVoxelTerrainServiceBand::Count, TEXT("Invalid terrain priority update"))) return;
		Identity.WorkClass = WorkClass;
		Identity.SourcePriority = Priority.SourcePriority;
		Identity.DistanceScore = Priority.DistanceCells;
		Identity.ForwardScore = Priority.ForwardScore;
		Identity.SourceStableKey = Priority.SourceStableKey;
		Identity.ServiceBand = Priority.Band;
	});
}

void FVoxelTaskScheduler::UpdateTerrainStages(TFunctionRef<void(EVoxelTaskKind, const FVoxelTaskStamp&, int32&)> InUpdate)
{
	check(IsInGameThread());
	VisitPriorityStates([&InUpdate](FVoxelTaskRequest& Identity)
	{
		InUpdate(Identity.Kind, Identity.Stamp, Identity.TerrainStage);
	});
}

void FVoxelTaskScheduler::Tick(
	TFunctionRef<void(FVoxelTaskResult&&)> InApply,
	const double InMaxApplyMilliseconds)
{
	check(IsInGameThread());

	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TaskSchedulerTick);
	if (bTicking) return;
	TGuardValue<bool> TickGuard(bTicking, true);
	Diagnostics.LastAdmissionMilliseconds = 0.0;
	Diagnostics.LastResultApplyMilliseconds = 0.0;

	if (bStopped)
	{
		return;
	}

	int32 Applied = 0;
	int32 HeavyApplied = 0;
	int32 FineApplied = 0;
	int32 VoxelLODApplied = 0;
	int32 SurfaceApplied = 0;
	int32 MacroApplied = 0;

	auto CanApplyTerrainKind =
		[this, &FineApplied, &VoxelLODApplied, &SurfaceApplied, &MacroApplied](const EVoxelTaskKind Kind) -> bool
		{
			switch (Kind)
			{
			case EVoxelTaskKind::BuildFineShard:
		case EVoxelTaskKind::BuildFineMesh:
				return FineApplied < Budget.MaxFineApplyPerFrame;
			case EVoxelTaskKind::BuildVoxelProxy:
			case EVoxelTaskKind::BuildVolumeTransition:
				return VoxelLODApplied < Budget.MaxVoxelLODApplyPerFrame;
			case EVoxelTaskKind::BuildSurface:
				return SurfaceApplied < Budget.MaxSurfaceApplyPerFrame;
			case EVoxelTaskKind::BuildMacro:
				return MacroApplied < Budget.MaxMacroApplyPerFrame;
			default:
				return true;
			}
		};

	const double EndTime = FrameContext ? FrameContext->Deadline(InMaxApplyMilliseconds) :
		FPlatformTime::Seconds() + FMath::Max(0.0, InMaxApplyMilliseconds) / 1000.0;
	{
		const int32 ApplyLimit = Budget.MaxCompletedResultsPerFrame;
		const int32 HeavyLimit = Budget.MaxHeavyCompletedResultsPerFrame;
		Applied = 0;
		HeavyApplied = 0;
		while (!Running.IsEmpty() && Applied < ApplyLimit && FPlatformTime::Seconds() <= EndTime)
		{
			CompletedCandidates.Reset();
			for (int32 CandidateIndex = 0; CandidateIndex < Running.Num(); ++CandidateIndex)
			{
				const FRunning& Task = Running[CandidateIndex];
				if (!Task.Task.CanRetire() ||
					!CanApplyTerrainKind(Task.Kind) || (Task.Slot->Result.HasHeavyApply() && HeavyApplied >= HeavyLimit)) continue;
				FVoxelTaskRequest Identity;
				Identity.Kind = Task.Kind;
				Identity.WorkClass = Task.WorkClass;
				Identity.SourcePriority = Task.SourcePriority;
				Identity.DistanceScore = Task.DistanceScore;
				Identity.ForwardScore = Task.ForwardScore;
				Identity.SourceStableKey = Task.SourceStableKey;
				Identity.ServiceBand = Task.ServiceBand;
				Identity.TerrainStage = Task.TerrainStage;
				FVoxelTerrainServiceCandidate& Candidate = CompletedCandidates.AddDefaulted_GetRef();
				Candidate.Index = CandidateIndex;
				Candidate.Priority = ResolveServicePriority(Identity);
				Candidate.FirstQueuedSeconds = Task.QueuedAt;
				Candidate.Sequence = Task.Sequence;
				const double* Estimate = ApplyCostEstimates.Find(Task.Kind);
				Candidate.EstimatedMicroseconds = Estimate ? *Estimate : 50.0;
				Candidate.bRunnable = true;
			}
			FVoxelTerrainServiceCapacity Capacity;
			Capacity.bAllowOverdueOpportunity = FrameContext && FrameContext->IsOverdueOpportunityAvailable();
			FVoxelTerrainServiceCharge ApplyCharge;
			const int32 Index = CompletedSelection.Select(CompletedCandidates, Capacity, FPlatformTime::Seconds(), ApplyCharge);
			if (Index == INDEX_NONE) break;
			if (ApplyCharge.bOverdueOpportunity) FrameContext->ConsumeOverdueOpportunity();
			const double ServiceStart = FPlatformTime::Seconds();
			if (Running[Index].Slot->Cancel.Load())
			{
				Running[Index].Slot->Result.bCanceled = true;
				Running[Index].Slot->Result.bSuccess = false;
			}
			const bool bHeavy = Running[Index].Slot->Result.HasHeavyApply();

			FRunning Completed =
				MoveTemp(Running[Index]);

			Running.RemoveAt(Index, 1, EAllowShrinking::No);
			Completed.Task.Retire();
			if (Completed.NativeLease) verify(Completed.NativeLease->Resize(Completed.BudgetErrorBytes));
			if (!Completed.bServiceSettled)
			{
				WorkerSelection.Settle(Completed.ServiceChargeSerial, Completed.Slot->ExecuteMicroseconds);
				Completed.bServiceSettled = true;
				const double ActualCost = FMath::Max(1.0, Completed.Slot->ExecuteMicroseconds);
				double& Estimate = WorkerCostEstimates.FindOrAdd(Completed.Kind, ActualCost);
				Estimate = FMath::Lerp(Estimate, ActualCost, 0.15);
			}

			ReservedBytes -=
				Completed.ReservedBytes;

			FVoxelTaskRequest& OriginalRequest = Completed.Slot->Request;
			if (OriginalRequest.bResumable) RunningContinuationInputBytes -= OriginalRequest.InputBytes;
			if (OriginalRequest.bResumable && Completed.Slot->Result.bSuccess && Completed.Slot->Result.bContinue && !Completed.Slot->Result.bCanceled)
			{
				OriginalRequest.AccumulatedExecuteMilliseconds = Completed.Slot->Result.ExecuteMilliseconds;
				QueuedInputBytes += OriginalRequest.InputBytes;
				check(Pending.Num() < Pending.Max());
				Pending.Add(MoveTemp(OriginalRequest));
				CompletedSelection.Settle(ApplyCharge.Serial, (FPlatformTime::Seconds() - ServiceStart) * 1000000.0);
				++Applied;
				continue;
			}
			RemoveActive(
				Completed.Stamp,
				Completed.Kind,
				Completed.WorkClass);

			if (bHeavy)
			{
				++HeavyApplied;
			}

			switch (Completed.Kind)
			{
			case EVoxelTaskKind::BuildFineShard:
		case EVoxelTaskKind::BuildFineMesh:
				++FineApplied;
				break;
			case EVoxelTaskKind::BuildVoxelProxy:
			case EVoxelTaskKind::BuildVolumeTransition:
				++VoxelLODApplied;
				break;
			case EVoxelTaskKind::BuildSurface:
				++SurfaceApplied;
				break;
			case EVoxelTaskKind::BuildMacro:
				++MacroApplied;
				break;
			default:
				break;
			}

			FVoxelTaskResult& Result =
				Completed.Slot->Result;
			Result.WorkReceipt = Completed.Slot->WorkReceipt;
			if (Result.WorkReceipt)
			{
				verify(Result.WorkReceipt->Complete(Result.ExecuteMilliseconds));
				if (Result.CustomPayload) Result.CustomPayload->WorkReceipt = Result.WorkReceipt;
				if (Result.Collision) Result.Collision->WorkReceipt = Result.WorkReceipt;
				FVoxelSectionMeshResult* Meshes[] = {Result.FineMesh.Get(), Result.VoxelProxyMesh.Get(), Result.VolumeBoundaryMesh.Get(),
					Result.SurfaceMesh.Get(), Result.WaterMesh.Get(), Result.MacroMesh.Get()};
				for (auto* Mesh : Meshes) if (Mesh) Mesh->WorkReceipt = Result.WorkReceipt;
			}

			/**
			 * Move 前缓存 diagnostics。
			 */
			const bool bResultSuccess =
				Result.bSuccess;

			const bool bResultCanceled =
				Result.bCanceled;

			const double QueueMilliseconds =
				Result.QueueMilliseconds;

			const double ExecuteMilliseconds =
				Result.ExecuteMilliseconds;

			const double ApplyStart =
				FPlatformTime::Seconds();

			if (OriginalRequest.Apply)
			{
				OriginalRequest.Apply(
					MoveTemp(Result));
			}
			else
			{
				InApply(
					MoveTemp(Result));
			}

			const double ApplyEnd =
				FPlatformTime::Seconds();

			FVoxelTaskResult Sample;

			Sample.Kind =
				Completed.Kind;

			Sample.Stamp =
				Completed.Stamp;

			Sample.bSuccess =
				bResultSuccess;

			Sample.bCanceled =
				bResultCanceled;

			Sample.QueueMilliseconds =
				QueueMilliseconds;

			Sample.ExecuteMilliseconds =
				ExecuteMilliseconds;

			Sample.ApplyMilliseconds =
				(ApplyEnd -
				 ApplyStart) *
				1000.0;
			Diagnostics.LastResultApplyMilliseconds += Sample.ApplyMilliseconds;
			const double ActualApply = FMath::Max(1.0, (ApplyEnd - ServiceStart) * 1000000.0);
			CompletedSelection.Settle(ApplyCharge.Serial, ActualApply);
			double& ApplyEstimate = ApplyCostEstimates.FindOrAdd(Completed.Kind, ActualApply);
			ApplyEstimate = FMath::Lerp(ApplyEstimate, ActualApply, 0.15);

			RecordCompletedResult(
				Sample);

			++Applied;
		}

		while (!bStopped && !Canceled.IsEmpty() && Applied < ApplyLimit && FPlatformTime::Seconds() <= EndTime)
		{
			FCompleted Completed = Canceled.Pop(EAllowShrinking::No);

			FVoxelTaskResult Sample;
			Sample.Kind = Completed.Result.Kind;
			Sample.Stamp = Completed.Result.Stamp;
			Sample.bCanceled = true;

			const double ApplyStart =
				FPlatformTime::Seconds();

			if (Completed.Request.Apply)
			{
				Completed.Request.Apply(
					MoveTemp(Completed.Result));
			}
			else
			{
				InApply(
					MoveTemp(Completed.Result));
			}

			Sample.ApplyMilliseconds =
				(FPlatformTime::Seconds() - ApplyStart) *
				1000.0;
			Diagnostics.LastResultApplyMilliseconds += Sample.ApplyMilliseconds;

			RecordCompletedResult(Sample);
			++Applied;
		}
	}

	const double AdmissionStart = FPlatformTime::Seconds();
	Pump();
	Diagnostics.LastAdmissionMilliseconds = (FPlatformTime::Seconds() - AdmissionStart) * 1000.0;
	FlushSectionIdleNotifications();
}

void FVoxelTaskScheduler::CancelSection(
	const FIntVector& InSection)
{
	check(IsInGameThread());
	for (auto It = AdmissionAttempts.CreateIterator(); It; ++It)
		if (UsesSectionKey(It.Key().Kind) && It.Key().Stamp.Section == InSection) It.RemoveCurrent();

	for (int32 Index = Pending.Num() - 1;
		Index >= 0;
		--Index)
	{
		if (Pending[Index].Stamp.Section != InSection ||
			!UsesSectionKey(Pending[Index].Kind))
		{
			continue;
		}

		QueuedInputBytes -=
			Pending[Index].InputBytes;

		FVoxelTaskRequest Request =
			MoveTemp(Pending[Index]);

		Pending.RemoveAtSwap(Index, 1, EAllowShrinking::No);

		RemoveActive(
			Request.Stamp,
			Request.Kind,
			Request.WorkClass);

		QueueCanceled(
			MoveTemp(Request));
	}

	for (FRunning& Task : Running)
	{
		if (UsesSectionKey(Task.Kind) &&
			Task.Stamp.Section == InSection)
		{
			Task.Slot->Cancel.Store(true);
		}
	}
	FlushSectionIdleNotifications();
}

void FVoxelTaskScheduler::CancelMatching(
	TFunctionRef<bool(EVoxelTaskKind, const FVoxelTaskStamp&)> InPredicate)
{
	check(IsInGameThread());
	const uint64 LastInitialSequence = NextSequence;
	uint64 VisitedSequence = 0;
	while (true)
	{
		FVoxelTaskKey Key;
		uint64 Next = MAX_uint64;
		for (const auto& Pair : AdmissionAttempts)
		{
			if (Pair.Value.Sequence > VisitedSequence && Pair.Value.Sequence <= LastInitialSequence && Pair.Value.Sequence < Next)
			{
				Next = Pair.Value.Sequence;
				Key = Pair.Key;
			}
		}
		if (Next == MAX_uint64) break;
		VisitedSequence = Next;
		if (!InPredicate(Key.Kind, Key.Stamp)) continue;
		if (const FAdmissionAttempt* Attempt = AdmissionAttempts.Find(Key); Attempt && Attempt->Sequence == Next)
			AdmissionAttempts.Remove(Key);
		const int32 PendingIndex = Pending.IndexOfByPredicate([&Key, Next](const FVoxelTaskRequest& Request)
		{
			return Request.Sequence == Next && Request.Kind == Key.Kind && Request.Stamp == Key.Stamp;
		});
		if (PendingIndex != INDEX_NONE)
		{
			FVoxelTaskRequest Request = MoveTemp(Pending[PendingIndex]);
			Pending.RemoveAtSwap(PendingIndex, 1, EAllowShrinking::No);
			QueuedInputBytes -= Request.InputBytes;
			RemoveActive(Request.Stamp, Request.Kind, Request.WorkClass);
			QueueCanceled(MoveTemp(Request));
		}
		for (FRunning& Request : Running)
		{
			if (Request.Sequence == Next && Request.Kind == Key.Kind && Request.Stamp == Key.Stamp)
				Request.Slot->Cancel.Store(true);
		}
	}
	FlushSectionIdleNotifications();
}

void FVoxelTaskScheduler::StopAndJoin()
{
	if (bStopped &&
		Running.IsEmpty())
	{
		return;
	}

	bStopped = true;

	for (FRunning& Task : Running)
	{
		Task.Slot->Cancel.Store(true);
	}

	for (FRunning& Task : Running)
	{
		Task.Task.Wait();
		while (!Task.Task.CanRetire())
		{
			FPlatformProcess::YieldThread();
		}
		Task.Task.Retire();
		if (Task.NativeLease) verify(Task.NativeLease->Resize(Task.BudgetErrorBytes));
		if (Task.Slot->WorkReceipt)
		{
			Task.Slot->WorkReceipt->Complete(Task.Slot->Result.ExecuteMilliseconds);
			Task.Slot->WorkReceipt->Resolve(false);
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Waiting for voxel task shutdown: kind=%d executeMs=%.2f"),
			static_cast<int32>(Task.Kind),
			Task.Slot->Result.ExecuteMilliseconds);
		if (!Task.bServiceSettled)
		{
			WorkerSelection.Settle(Task.ServiceChargeSerial, Task.Slot->ExecuteMicroseconds);
		}
	}

	Pending.Empty();
	WorkerSelection.Reset();
	CompletedSelection.Reset();
	AdmissionAttempts.Empty();
	Running.Empty();
	Canceled.Empty();

	ActiveKeys.Empty();
	SectionTaskCounts.Empty();
	FreeBacking();

	QueuedInputBytes = 0;
	RunningContinuationInputBytes = 0;
	ReservedBytes = 0;
	CriticalTaskCount = 0;
}

bool FVoxelTaskScheduler::Has(
	const FVoxelTaskStamp& InStamp,
	const EVoxelTaskKind InKind) const
{
	return ActiveKeys.Contains({
		InKind,
		InStamp
	});
}

bool FVoxelTaskScheduler::HasSectionTask(
	const FIntVector& InSection) const
{
	const int32* Count =
		SectionTaskCounts.Find(
			InSection);

	return Count &&
		*Count > 0;
}

#if WITH_DEV_AUTOMATION_TESTS
void FVoxelTaskScheduler::SetSectionIdleCallback(TFunction<void(const FIntVector&)> InCallback)
{
	check(IsInGameThread());
	check(bAllowPassive && !ControlConsumer.IsValid());
	SectionIdleCallback = MoveTemp(InCallback);
	++IdleBindingGeneration;
}
#endif

void FVoxelTaskScheduler::ClearSectionIdleCallback()
{
	check(IsInGameThread());
	SectionIdleCallback = nullptr;
	IdleBindingLease.Reset();
	++IdleBindingGeneration;
}

int32 FVoxelTaskScheduler::ActiveCount() const
{
	return Running.Num();
}

int32 FVoxelTaskScheduler::CriticalCount() const
{
	return CriticalTaskCount;
}

void FVoxelTaskScheduler::BindFrameContext(const FVoxelTerrainFrameContext& InFrameContext)
{
	check(IsInGameThread());
	FrameContext = &InFrameContext;
}

void FVoxelTaskScheduler::SetBudget(
	const FVoxelTaskBudget& InBudget)
{
	check(IsInGameThread());

	Budget.MaxConcurrentTasks =
		FMath::Max(
			1,
			InBudget.MaxConcurrentTasks);
	Budget.CriticalReservedTasks = FMath::Clamp(
		InBudget.CriticalReservedTasks,
		1,
		FMath::Max(1, Budget.MaxConcurrentTasks - 1));

	Budget.MaxPendingTasks =
		FMath::Max(
			1,
			InBudget.MaxPendingTasks);

	Budget.MaxReservedBytes =
		FMath::Max<uint64>(
			1,
			InBudget.MaxReservedBytes);

	Budget.MaxInputBytes =
		FMath::Max<uint64>(
			1,
			InBudget.MaxInputBytes);

	Budget.MaxCompletedResultsPerFrame =
		FMath::Max(
			1,
			InBudget.MaxCompletedResultsPerFrame);

	Budget.MaxHeavyCompletedResultsPerFrame =
		FMath::Clamp(
			InBudget.MaxHeavyCompletedResultsPerFrame,
			1,
			Budget.MaxCompletedResultsPerFrame);

	Budget.MaxFineApplyPerFrame =
		FMath::Clamp(
			InBudget.MaxFineApplyPerFrame,
			1,
			Budget.MaxCompletedResultsPerFrame);
	Budget.MaxVoxelLODApplyPerFrame =
		FMath::Clamp(
			InBudget.MaxVoxelLODApplyPerFrame,
			1,
			Budget.MaxCompletedResultsPerFrame);
	Budget.MaxSurfaceApplyPerFrame =
		FMath::Clamp(
			InBudget.MaxSurfaceApplyPerFrame,
			1,
			Budget.MaxCompletedResultsPerFrame);
	Budget.MaxMacroApplyPerFrame =
		FMath::Clamp(
			InBudget.MaxMacroApplyPerFrame,
			1,
			Budget.MaxCompletedResultsPerFrame);

	Budget.MaxConcurrentSurfaceTasks =
		FMath::Max(1, InBudget.MaxConcurrentSurfaceTasks);
	Budget.MaxConcurrentMacroTasks =
		FMath::Max(1, InBudget.MaxConcurrentMacroTasks);

	Budget.MaxConcurrentCoarseTerrainTasks =
		FMath::Clamp(
			InBudget.MaxConcurrentCoarseTerrainTasks,
			1,
			Budget.MaxConcurrentTasks);

	Budget.MaxPendingCoarseTerrainTasks =
		FMath::Clamp(
			InBudget.MaxPendingCoarseTerrainTasks,
			1,
			Budget.MaxPendingTasks);
	Budget.MaxWaitingDependencyTasks = FMath::Max(0, InBudget.MaxWaitingDependencyTasks);
}

void FVoxelTaskScheduler::SetObservedTerrainStage(const int32 InStage)
{
	check(IsInGameThread());
	ObservedTerrainStage = FMath::Clamp(InStage, 0, 3);
}



FVoxelTaskDiagnostics
FVoxelTaskScheduler::GetDiagnostics() const
{
	check(IsInGameThread());

	FVoxelTaskDiagnostics Result =
		Diagnostics;

	Result.Pending =
		Pending.Num();

	Result.Running =
		Running.Num();
	Result.CriticalAdmissionDeferrals = CriticalAdmissionDeferrals;
	Result.CriticalPending = 0;
	for (const FVoxelTaskRequest& Request : Pending)
	{
		Result.CriticalPending += Request.WorkClass == EVoxelWorkClass::Critical ? 1 : 0;
	}
	Result.CriticalRunning = 0;
	for (const FRunning& Task : Running)
	{
		Result.CriticalRunning += Task.WorkClass == EVoxelWorkClass::Critical ? 1 : 0;
	}

	Result.Critical =
		CriticalTaskCount;

	Result.ReservedBytes =
		ReservedBytes;

	Result.QueuedInputBytes =
		QueuedInputBytes;

	Result.PendingByKind.Reset();
	for (const FVoxelTaskRequest& Request : Pending)
	{
		++Result.PendingByKind.FindOrAdd(Request.Kind);
	}
	Result.AdmissionBlockedByKind.Reset();
	Result.AdmissionBlockedRequests = 0;
	for (const auto& Pair : AdmissionAttempts)
	{
		if (Pair.Value.Blocked == EVoxelTaskBlockReason::None) continue;
		++Result.AdmissionBlockedRequests;
		for (uint16 Flag = 1; Flag <= static_cast<uint16>(EVoxelTaskBlockReason::Oversized); Flag <<= 1)
		{
			if (static_cast<uint16>(Pair.Value.Blocked) & Flag)
				++Result.AdmissionBlockedByKind.FindOrAdd(Pair.Key.Kind).FindOrAdd(static_cast<EVoxelTaskBlockReason>(Flag));
		}
		Result.OldestAdmissionWaitSeconds = FMath::Max(Result.OldestAdmissionWaitSeconds,
			FPlatformTime::Seconds() - Pair.Value.FirstRequested);
	}
	Result.BlockedByKind.Reset();
	Result.BlockedRequests = 0;
	for (const FVoxelTaskRequest& Request : Pending)
	{
		if (Request.Blocked == EVoxelTaskBlockReason::None) continue;
		++Result.BlockedRequests;
		for (uint16 Flag = 1; Flag <= static_cast<uint16>(EVoxelTaskBlockReason::Oversized); Flag <<= 1)
		{
			if (static_cast<uint16>(Request.Blocked) & Flag)
				++Result.BlockedByKind.FindOrAdd(Request.Kind).FindOrAdd(static_cast<EVoxelTaskBlockReason>(Flag));
		}
	}
	Result.RunningByKind.Reset();
	for (const FRunning& Task : Running)
	{
		++Result.RunningByKind.FindOrAdd(Task.Kind);
	}

	return Result;
}

FVoxelTerrainServicePriority FVoxelTaskScheduler::ResolveServicePriority(const FVoxelTaskRequest& InRequest)
{
	FVoxelTerrainServicePriority Result;
	Result.DistanceCells = InRequest.DistanceScore;
	Result.ForwardScore = InRequest.ForwardScore;
	Result.SourcePriority = InRequest.SourcePriority;
	Result.SourceStableKey = InRequest.SourceStableKey;
	if (InRequest.ServiceBand != EVoxelTerrainServiceBand::None)
	{
		Result.Band = InRequest.ServiceBand;
		return Result;
	}
	const bool bNearRole = InRequest.WorkClass == EVoxelWorkClass::Critical ||
		InRequest.WorkClass == EVoxelWorkClass::Warmup || InRequest.WorkClass == EVoxelWorkClass::Interactive ||
		InRequest.Kind == EVoxelTaskKind::BuildCollision || InRequest.Kind == EVoxelTaskKind::BuildNavigation;
	if (bNearRole || ((InRequest.Kind == EVoxelTaskKind::GenerateExactBase || InRequest.Kind == EVoxelTaskKind::BuildFineMesh ||
		InRequest.Kind == EVoxelTaskKind::DecodeOverlay) && InRequest.DistanceScore <= 80.0))
	{
		Result.Band = InRequest.DistanceScore <= 32.0 ? EVoxelTerrainServiceBand::P0 : EVoxelTerrainServiceBand::P1;
		return Result;
	}
	switch (InRequest.Kind)
	{
	case EVoxelTaskKind::GenerateExactBase:
	case EVoxelTaskKind::BuildFineShard:
		case EVoxelTaskKind::BuildFineMesh:
	case EVoxelTaskKind::DecodeOverlay:
		Result.Band = EVoxelTerrainServiceBand::P2;
		break;
	case EVoxelTaskKind::GenerateVoxelProxy:
	case EVoxelTaskKind::BuildVoxelProxy:
	case EVoxelTaskKind::BuildVolumeTransition:
		Result.Band = EVoxelTerrainServiceBand::P3;
		break;
	case EVoxelTaskKind::GenerateSurface:
	case EVoxelTaskKind::BuildSurface:
	case EVoxelTaskKind::BuildWater:
		Result.Band = EVoxelTerrainServiceBand::P4;
		break;
	case EVoxelTaskKind::GenerateMacro:
	case EVoxelTaskKind::BuildMacro:
		Result.Band = EVoxelTerrainServiceBand::P5;
		break;
	case EVoxelTaskKind::BuildViewCoverage:
	case EVoxelTaskKind::BuildViewTransition:
		Result.Band = static_cast<EVoxelTerrainServiceBand>(static_cast<uint8>(EVoxelTerrainServiceBand::P2) +
			FMath::Clamp(InRequest.TerrainStage, 0, 3));
		break;
	default:
		Result.Band = EVoxelTerrainServiceBand::P6;
		break;
	}
	return Result;
}

bool FVoxelTaskScheduler::IsHigherPriority(const FVoxelTaskRequest& InA, const FVoxelTaskRequest& InB)
{
	const FVoxelTerrainServicePriority First = ResolveServicePriority(InA);
	const FVoxelTerrainServicePriority Second = ResolveServicePriority(InB);
	if (First != Second)
	{
		return FVoxelTerrainWorkSelection::HigherPriority(First, Second);
	}
	if (InA.QueuedAt != InB.QueuedAt)
	{
		return InA.QueuedAt < InB.QueuedAt;
	}
	return InA.Sequence < InB.Sequence;
}

bool FVoxelTaskResult::HasHeavyApply() const
{
	if (bCanceled || !bSuccess) return false;
	if (Kind == EVoxelTaskKind::BuildFineMesh || Kind == EVoxelTaskKind::BuildVoxelProxy)
	{
		// 空网格只更新就绪/版本状态，不占据实体网格每帧一次的重发布配额。
		const TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe>& Mesh = Kind == EVoxelTaskKind::BuildFineMesh ? FineMesh : VoxelProxyMesh;
		return Mesh && Mesh->Batches.ContainsByPredicate([](const FVoxelRenderBatch& Batch)
		{
			return !Batch.Mesh.Triangles.IsEmpty();
		});
	}
	switch (Kind)
	{
	case EVoxelTaskKind::BuildCollision:
	case EVoxelTaskKind::BuildNavigation:
	case EVoxelTaskKind::BuildFineShard:
		case EVoxelTaskKind::BuildFineMesh:
	case EVoxelTaskKind::BuildVoxelProxy:
	case EVoxelTaskKind::BuildVolumeTransition:
	case EVoxelTaskKind::BuildSurface:
	case EVoxelTaskKind::BuildWater:
	case EVoxelTaskKind::BuildMacro:
	case EVoxelTaskKind::BuildDetails:
		return true;

	default:
		return false;
	}
}

bool FVoxelTaskScheduler::UsesSectionKey(
	const EVoxelTaskKind InKind)
{
	switch (InKind)
	{
	case EVoxelTaskKind::GenerateExactBase:
	case EVoxelTaskKind::BuildCollision:
	case EVoxelTaskKind::BuildNavigation:
	case EVoxelTaskKind::BuildFineMesh:
	case EVoxelTaskKind::DecodeOverlay:
	case EVoxelTaskKind::EncodeRegion:
		return true;

	default:
		return false;
	}
}

int32 FVoxelTaskScheduler::RunningCount(const EVoxelTaskKind InKind) const
{
	int32 Count = 0;
	for (const FRunning& Task : Running)
	{
		Count += Task.Kind == InKind ? 1 : 0;
	}
	return Count;
}

bool FVoxelTaskScheduler::CanStartKind(const EVoxelTaskKind InKind) const
{
	if (IsRepresentationDataKind(InKind))
	{
		const int32 DataRunning = RunningCount(EVoxelTaskKind::GenerateVoxelProxy) +
			RunningCount(EVoxelTaskKind::GenerateSurface) + RunningCount(EVoxelTaskKind::GenerateMacro);
		if (DataRunning >= FMath::Max(1, Budget.MaxConcurrentTasks / 2)) return false;
	}
	if (IsCoarseTerrainKind(InKind) &&
		RunningCount(EVoxelTaskKind::BuildSurface) +
			RunningCount(EVoxelTaskKind::BuildMacro) >=
			Budget.MaxConcurrentCoarseTerrainTasks)
	{
		return false;
	}

	switch (InKind)
	{
	case EVoxelTaskKind::BuildSurface:
		return RunningCount(InKind) < Budget.MaxConcurrentSurfaceTasks;
	case EVoxelTaskKind::BuildMacro:
		return RunningCount(InKind) < Budget.MaxConcurrentMacroTasks;
	default:
		return true;
	}
}

void FVoxelTaskScheduler::Pump()
{
	if (bPumping) return;
	TGuardValue<bool> PumpGuard(bPumping, true);
	const int32 MaximumWorkers = FrameContext ? FMath::Min(Budget.MaxConcurrentTasks, FrameContext->GetAllocation().MaximumWorkers) : Budget.MaxConcurrentTasks;
	const double Deadline = FrameContext ? FrameContext->Deadline() : MAX_dbl;
	TArray<uint64, TFixedAllocator<64>> ReceiptBlocked;
	if (!PrepareQueues(0)) return;
	while (!bStopped && Running.Num() < MaximumWorkers && !Pending.IsEmpty() && FPlatformTime::Seconds() < Deadline)
	{
		RefreshBlockReasons();
		const double Now = FPlatformTime::Seconds();
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.MaximumWorkers = MaximumWorkers;
		Capacity.RunningWorkers = Running.Num();
		Capacity.NearReservedWorkers = FMath::Min(Budget.CriticalReservedTasks, MaximumWorkers);
		Capacity.bAllowOverdueOpportunity = FrameContext && FrameContext->IsOverdueOpportunityAvailable();
		for (const FRunning& Task : Running)
		{
			Capacity.RunningNearWorkers += Task.bServiceNear ? 1 : 0;
		}
		WorkerCandidates.Reset();
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			FVoxelTaskRequest& Request = Pending[Index];
			if (Request.bNativeCapacityBlocked)
			{
				const auto Consumer = ResolveNativeConsumer(Request);
				if (TerrainResources->ReleaseVersionExcludingOwner(Consumer) == Request.NativeCapacityVersion)
					Request.Blocked = EVoxelTaskBlockReason::Memory;
				else Request.bNativeCapacityBlocked = false;
			}
			if (Request.bReceiptCapacityBlocked)
			{
				const auto Consumer = FVoxelGenerationConsumerKey::Section(StatisticsWorldEpoch, StatisticsOwnerId, 1,
					Request.Stamp.Section, Request.Sequence);
				if (RuntimeResources->ReleaseVersionExcludingOwner(Consumer) == Request.ReceiptCapacityVersion)
					Request.Blocked = EVoxelTaskBlockReason::InputMemory;
				else Request.bReceiptCapacityBlocked = false;
			}
			if (ReceiptBlocked.Contains(Request.Sequence) || (ReceiptBlocked.Num() == 64 && !Request.WorkReceipt))
				Request.Blocked = EVoxelTaskBlockReason::Memory;
			if (!CanStartKind(Request.Kind))
			{
				Request.Blocked = EVoxelTaskBlockReason::LaneSlots;
			}
			if (ReservedBytes > Budget.MaxReservedBytes || Request.ReservedBytes > Budget.MaxReservedBytes - ReservedBytes)
			{
				Request.Blocked = static_cast<EVoxelTaskBlockReason>(static_cast<uint16>(Request.Blocked) |
					static_cast<uint16>(EVoxelTaskBlockReason::Memory));
			}
			if (Request.Sequence > BlockRefreshSequence)
			{
				Request.Blocked = EVoxelTaskBlockReason::RetryDelay;
			}
			const bool bRunnable = Request.Blocked == EVoxelTaskBlockReason::None;
			if (bRunnable && Request.FirstRunnableSeconds < 0.0)
			{
				Request.FirstRunnableSeconds = Now;
			}
			FVoxelTerrainServiceCandidate& Candidate = WorkerCandidates.AddDefaulted_GetRef();
			Candidate.Index = Index;
			Candidate.Priority = ResolveServicePriority(Request);
			Candidate.FirstQueuedSeconds = FMath::Max(0.0, Request.FirstRunnableSeconds);
			Candidate.Sequence = Request.Sequence;
			const double* Estimate = WorkerCostEstimates.Find(Request.Kind);
			Candidate.EstimatedMicroseconds = Estimate ? *Estimate : 1000.0;
			Candidate.bRunnable = bRunnable;
		}
		FVoxelTerrainServiceCharge Charge;
		const int32 BestIndex = WorkerSelection.Select(WorkerCandidates, Capacity, Now, Charge);
		if (BestIndex == INDEX_NONE)
		{
			break;
		}
		if (Statistics && !Pending[BestIndex].WorkReceipt)
		{
			const auto Consumer = FVoxelGenerationConsumerKey::Section(StatisticsWorldEpoch, StatisticsOwnerId, 1,
				Pending[BestIndex].Stamp.Section, Pending[BestIndex].Sequence);
			const uint64 BeforeReceiptAdmission = RuntimeResources->ReleaseVersionExcludingOwner(Consumer);
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			using FReceiptController = SharedPointerInternals::TReferenceControllerWithDeleter<FVoxelTerrainWorkReceipt,
				FReceiptDeleter, ESPMode::ThreadSafe>;
			const uint64 ReceiptBytes = FMemory::QuantizeSize(sizeof(FVoxelTerrainWorkReceipt), alignof(FVoxelTerrainWorkReceipt)) +
				FMemory::QuantizeSize(sizeof(FReceiptController), alignof(FReceiptController));
			const auto ReceiptAdmission = RuntimeResources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, ReceiptBytes, Lease, 0,
				StatisticsRootInputBytes + Statistics->GetStorageReservationBytes(), 1 + Statistics->GetStorageReservationLeaseCount());
			if (ReceiptAdmission != EVoxelTerrainResourceAdmission::Granted)
			{
				WorkerSelection.RefundBeforeStart(Charge.Serial);
				Pending[BestIndex].Blocked = ReceiptAdmission == EVoxelTerrainResourceAdmission::InvalidRequest ?
					EVoxelTaskBlockReason::TerminalFailure : ReceiptAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
					EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory;
				if (ReceiptAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable || ReceiptAdmission == EVoxelTerrainResourceAdmission::InvalidRequest)
				{
					FVoxelTaskRequest Rejected = MoveTemp(Pending[BestIndex]);
					Pending.RemoveAtSwap(BestIndex, 1, EAllowShrinking::No);
					QueuedInputBytes -= Rejected.InputBytes;
					FAdmissionAttempt Preserved = AdmissionAttempts.FindChecked({Rejected.Kind, Rejected.Stamp});
					Preserved.Blocked = Rejected.Blocked;
					RemoveActive(Rejected.Stamp, Rejected.Kind, Rejected.WorkClass);
					AdmissionAttempts.Add({Rejected.Kind, Rejected.Stamp}, Preserved);
					QueueCanceled(MoveTemp(Rejected));
					continue;
				}
				Pending[BestIndex].bReceiptCapacityBlocked = true;
				Pending[BestIndex].ReceiptCapacityVersion = BeforeReceiptAdmission;
				ReceiptBlocked.Add(Pending[BestIndex].Sequence);
				continue;
			}
			auto Receipt = TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe>(
				new FVoxelTerrainWorkReceipt(Statistics.ToSharedRef(), Lease), FReceiptDeleter{Lease});
			if (!Receipt->IsRegistered())
			{
				WorkerSelection.RefundBeforeStart(Charge.Serial);
				Pending[BestIndex].Blocked = EVoxelTaskBlockReason::Memory;
				Pending[BestIndex].bReceiptCapacityBlocked = true;
				Pending[BestIndex].ReceiptCapacityVersion = BeforeReceiptAdmission;
				ReceiptBlocked.Add(Pending[BestIndex].Sequence);
				continue;
			}
			Pending[BestIndex].WorkReceipt = MoveTemp(Receipt);
		}
		const auto Consumer = ResolveNativeConsumer(Pending[BestIndex]);
		const uint64 BeforeNativeAdmission = TerrainResources->ReleaseVersionExcludingOwner(Consumer);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SlotLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NativeLease;
		EVoxelTerrainResourceAdmission NativeAdmission = EVoxelTerrainResourceAdmission::Granted;
		const FVoxelTaskRequest& Selected = Pending[BestIndex];
		const uint64 CallbackInputs = Selected.GetBindingBytes() + Selected.GetRequiredBindingInputBytes();
		const int32 CallbackLeases = Selected.GetBindingLeaseCount() + Selected.GetRequiredBindingInputLeaseCount();
		const uint64 SlotBytes = VoxelBoundedAllocation::SharedBytes<FSlot>();
		const int32 BackingHandles = int32(BackingLease.IsValid());
		const bool bFloorOverflow = CallbackInputs > MAX_uint64 - BackingBytes ||
			CallbackInputs + BackingBytes > MAX_uint64 - SlotBytes || CallbackLeases > MAX_int32 - BackingHandles - 1;
		const uint64 RequiredInputs = bFloorOverflow ? 0 : BackingBytes + CallbackInputs;
		const int32 RequiredLeases = bFloorOverflow ? 0 : BackingHandles + CallbackLeases;
		using FEngineTask = UE::Tasks::Private::TExecutableTask<FTaskBody>;
		static_assert(sizeof(FEngineTask) > UE::Tasks::Private::SmallTaskSize, "Scheduler task requires direct native allocation accounting");
		const uint64 ErrorBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<TCHAR, FDefaultAllocator>(UE_ARRAY_COUNT(TaskBudgetError));
		const uint64 NativeBytes = FMemory::QuantizeSize(sizeof(FEngineTask), alignof(FEngineTask)) + ErrorBytes;
		if (ControlConsumer.IsValid())
		{
			NativeAdmission = bFloorOverflow ? EVoxelTerrainResourceAdmission::CapacityUnachievable :
				TerrainResources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
					SlotBytes, SlotLease, 0, RequiredInputs, RequiredLeases);
			if (NativeAdmission == EVoxelTerrainResourceAdmission::Granted)
			{
				NativeAdmission = TerrainResources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working,
					NativeBytes, NativeLease, 0, RequiredInputs + SlotLease->Bytes(), RequiredLeases + 1);
			}
		}
		if (NativeAdmission != EVoxelTerrainResourceAdmission::Granted)
		{
			WorkerSelection.RefundBeforeStart(Charge.Serial);
			SlotLease.Reset();
			Pending[BestIndex].Blocked = NativeAdmission == EVoxelTerrainResourceAdmission::InvalidRequest ?
				EVoxelTaskBlockReason::TerminalFailure : NativeAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
				EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::Memory;
			AdmissionAttempts.FindChecked({Selected.Kind, Selected.Stamp}).Blocked = Pending[BestIndex].Blocked;
			if (NativeAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable || NativeAdmission == EVoxelTerrainResourceAdmission::InvalidRequest)
			{
				FVoxelTaskRequest Rejected = MoveTemp(Pending[BestIndex]);
				Pending.RemoveAtSwap(BestIndex, 1, EAllowShrinking::No);
				QueuedInputBytes -= Rejected.InputBytes;
				const FAdmissionAttempt Preserved = AdmissionAttempts.FindChecked({Rejected.Kind, Rejected.Stamp});
				RemoveActive(Rejected.Stamp, Rejected.Kind, Rejected.WorkClass);
				AdmissionAttempts.Add({Rejected.Kind, Rejected.Stamp}, Preserved);
				QueueCanceled(MoveTemp(Rejected));
			}
			else
			{
				Pending[BestIndex].bNativeCapacityBlocked = true;
				Pending[BestIndex].NativeCapacityVersion = BeforeNativeAdmission;
			}
			continue;
		}
		if (Charge.bOverdueOpportunity) FrameContext->ConsumeOverdueOpportunity();
		FVoxelTaskRequest Request = MoveTemp(Pending[BestIndex]);
		Pending.RemoveAtSwap(BestIndex, 1, EAllowShrinking::No);
		QueuedInputBytes -= Request.InputBytes;
		ReservedBytes += Request.ReservedBytes;
		FRunning RunningTask;
		RunningTask.Stamp = Request.Stamp;
		RunningTask.Kind = Request.Kind;
		RunningTask.WorkClass = Request.WorkClass;
		RunningTask.SourcePriority = Request.SourcePriority;
		RunningTask.TerrainStage = Request.TerrainStage;
		RunningTask.bPublicationContinuation = Request.bPublicationContinuation;
		RunningTask.bTerrainDataOnly = Request.bTerrainDataOnly;
		RunningTask.DistanceScore = Request.DistanceScore;
		RunningTask.ForwardScore = Request.ForwardScore;
		RunningTask.SourceStableKey = Request.SourceStableKey;
		RunningTask.ServiceBand = Request.ServiceBand;
		RunningTask.ServiceChargeSerial = Charge.Serial;
		RunningTask.bServiceNear = FVoxelTerrainWorkSelection::IsNear(Charge.Band);
		RunningTask.Sequence = Request.Sequence;
		RunningTask.QueuedAt = Request.QueuedAt;
		RunningTask.ReservedBytes = Request.ReservedBytes;
		RunningTask.BudgetErrorBytes = ErrorBytes;
		RunningTask.SlotLease = MoveTemp(SlotLease);
		RunningTask.NativeLease = MoveTemp(NativeLease);
		RunningTask.Slot = MakeShared<FSlot, ESPMode::ThreadSafe>();
		RunningTask.Slot->BudgetErrorLease = RunningTask.NativeLease;
		RunningTask.Slot->BudgetError.GetCharArray().Reserve(UE_ARRAY_COUNT(TaskBudgetError));
		RunningTask.Slot->BudgetError.GetCharArray().Append(TaskBudgetError, UE_ARRAY_COUNT(TaskBudgetError));
		RunningTask.Slot->WorkReceipt = Request.WorkReceipt;
		if (Request.bResumable) RunningContinuationInputBytes += Request.InputBytes;
		RunningTask.Slot->Request = MoveTemp(Request);
		RunningTask.Task.Launch(UE_SOURCE_LOCATION, FTaskBody{RunningTask.Slot, RunningTask.Stamp}, UE::Tasks::ETaskPriority::Normal);
		check(Running.Num() < Running.Max());
		Running.Add(
			MoveTemp(RunningTask));
	}
}

void FVoxelTaskScheduler::QueueCanceled(
	FVoxelTaskRequest&& InRequest)
{
	FCompleted Completed;
	Completed.Result.WorkReceipt = InRequest.WorkReceipt;
	if (Completed.Result.WorkReceipt) Completed.Result.WorkReceipt->Complete(InRequest.AccumulatedExecuteMilliseconds);
	Completed.WorkClass = InRequest.WorkClass;
	Completed.TerrainStage = InRequest.TerrainStage;

	Completed.Result.Stamp =
		InRequest.Stamp;

	Completed.Result.Kind =
		InRequest.Kind;

	Completed.Result.bCanceled =
		true;

	Completed.Request = MoveTemp(InRequest);

	check(Canceled.Num() < Canceled.Max());
	Canceled.Add(
		MoveTemp(Completed));
}

void FVoxelTaskScheduler::AddActive(
	const FVoxelTaskStamp& InStamp,
	const EVoxelTaskKind InKind,
	const EVoxelWorkClass InWorkClass)
{
	ActiveKeys.Add({
		InKind,
		InStamp
	});

	if (UsesSectionKey(InKind))
	{
		++SectionTaskCounts.
			FindOrAdd(
				InStamp.Section);
	}

	if (InWorkClass ==
		EVoxelWorkClass::Critical)
	{
		++CriticalTaskCount;
	}
}

void FVoxelTaskScheduler::RemoveActive(
	const FVoxelTaskStamp& InStamp,
	const EVoxelTaskKind InKind,
	const EVoxelWorkClass InWorkClass)
{
	AdmissionAttempts.Remove({InKind, InStamp});
	ActiveKeys.Remove({
		InKind,
		InStamp
	});

	if (UsesSectionKey(InKind))
	{
		if (int32* Count =
			SectionTaskCounts.Find(
				InStamp.Section))
		{
			--(*Count);

			if (*Count <= 0)
			{
				SectionTaskCounts.Remove(
					InStamp.Section);
				check(SectionIdleNotifications.Num() < SectionIdleNotifications.Max());
				SectionIdleNotifications.AddUnique(InStamp.Section);
			}
		}
	}

	if (InWorkClass ==
		EVoxelWorkClass::Critical)
	{
		CriticalTaskCount =
			FMath::Max(
				0,
				CriticalTaskCount - 1);
	}
}

void FVoxelTaskScheduler::RecordCompletedResult(
	const FVoxelTaskResult& InResult)
{
	FVoxelTaskKindDiagnostics& Kind =
		Diagnostics.ByKind.
			FindOrAdd(
				InResult.Kind);

	++Kind.Completed;

	if (InResult.bCanceled)
	{
		++Kind.Canceled;
	}
	else if (!InResult.bSuccess)
	{
		++Kind.Failed;
	}

	Kind.TotalQueueMilliseconds +=
		InResult.QueueMilliseconds;

	Kind.TotalExecuteMilliseconds +=
		InResult.ExecuteMilliseconds;

	Kind.TotalApplyMilliseconds +=
		InResult.ApplyMilliseconds;

	Kind.MaximumQueueMilliseconds =
		FMath::Max(
			Kind.MaximumQueueMilliseconds,
			InResult.QueueMilliseconds);

	Kind.MaximumExecuteMilliseconds =
		FMath::Max(
			Kind.MaximumExecuteMilliseconds,
			InResult.ExecuteMilliseconds);

	Kind.MaximumApplyMilliseconds =
		FMath::Max(
			Kind.MaximumApplyMilliseconds,
			InResult.ApplyMilliseconds);
}
