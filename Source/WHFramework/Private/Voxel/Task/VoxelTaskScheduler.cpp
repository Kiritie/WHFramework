#include "Voxel/Task/VoxelTaskScheduler.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

namespace
{
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
	return WorldEpoch == InOther.WorldEpoch &&
		Token == InOther.Token &&
		Revision == InOther.Revision &&
		GeometryRevision == InOther.GeometryRevision &&
		Section == InOther.Section &&
		ViewKey == InOther.ViewKey &&
		SurfaceKey == InOther.SurfaceKey &&
		MacroKey == InOther.MacroKey;
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
		Bytes += sizeof(FVoxelWaterSurfaceTileData) + Water->WaterZ.GetAllocatedSize() + Water->WaterKind.GetAllocatedSize();
	}
	if (Macro)
	{
		Bytes += sizeof(FVoxelMacroTileData) + Macro->Height.GetAllocatedSize() + Macro->WaterHeight.GetAllocatedSize() +
			Macro->WaterKind.GetAllocatedSize() +
			Macro->SurfaceClass.GetAllocatedSize() + Macro->ForestCoverage.GetAllocatedSize() + Macro->SnowCoverage.GetAllocatedSize() +
			Macro->LargeStructures.GetAllocatedSize() + Macro->DistantCells.GetAllocatedSize();
	}

	Bytes += Details ? Details->GetAllocatedBytes() : 0;

	if (PersistentSection)
	{
		Bytes +=
			static_cast<uint64>(PersistentSection->Blocks.Num()) *
			(sizeof(int32) + sizeof(FVoxelBlockState));

		for (const TPair<int32, FVoxelBlockEntityState>& Pair :
			PersistentSection->Entities)
		{
			Bytes +=
				sizeof(int32) +
				sizeof(FVoxelBlockEntityState) +
				Pair.Value.Payload.Num();
		}
	}

	Bytes +=
		CustomPayload
			? CustomPayload->GetAllocatedBytes()
			: 0;

	return Bytes;
}

FVoxelTaskScheduler::FVoxelTaskScheduler()
{
}

FVoxelTaskScheduler::~FVoxelTaskScheduler()
{
	StopAndJoin();
}

void FVoxelTaskScheduler::ObserveDemand(const EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp,
	const EVoxelTaskBlockReason InReason, const uint64 InInputBytes, const uint64 InResultBytes)
{
	check(IsInGameThread());
	auto& Attempt = AdmissionAttempts.FindOrAdd({InKind, InStamp});
	if (Attempt.Sequence == 0)
	{
		Attempt.FirstRequested = FPlatformTime::Seconds();
		Attempt.Sequence = ++NextSequence;
	}
	Attempt.Blocked = InReason;
	Attempt.InputBytes = InInputBytes;
	Attempt.ResultBytes = InResultBytes;
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
	const auto Reject = [&Attempt](const EVoxelTaskBlockReason Reason)
	{
		Attempt.Blocked = Reason;
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
		InRequest.Kind == EVoxelTaskKind::None ||
		InRequest.WorkClass == EVoxelWorkClass::None ||
		!FMath::IsFinite(InRequest.DistanceScore) ||
		!FMath::IsFinite(InRequest.ForwardScore) ||
		InRequest.ReservedBytes == 0 ||
		InRequest.ReservedBytes > Budget.MaxReservedBytes ||
		InRequest.InputBytes > Budget.MaxInputBytes)
	{
		return Reject(InRequest.ReservedBytes > Budget.MaxReservedBytes || InRequest.InputBytes > Budget.MaxInputBytes
			? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::TerminalFailure);
	}

	if (Has(InRequest.Stamp, InRequest.Kind))
	{
		++Diagnostics.DuplicateRequests;
		return false;
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
	TArray<int32, TInlineAllocator<8>> DisplacedIndices;
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

	Pending.Add(
		MoveTemp(InRequest));

	Pump();
	return true;
}

void FVoxelTaskScheduler::UpdatePriorities(TFunctionRef<void(EVoxelTaskKind,
	const FVoxelTaskStamp&, EVoxelWorkClass&, int32&, double&, double&)> InUpdate)
{
	check(IsInGameThread());
	auto Update = [this, &InUpdate](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, int32& SourcePriority,
		double& DistanceScore, double& ForwardScore)
	{
		const bool bWasCritical = WorkClass == EVoxelWorkClass::Critical;
		InUpdate(Kind, Stamp, WorkClass, SourcePriority, DistanceScore, ForwardScore);
		CriticalTaskCount += static_cast<int32>(WorkClass == EVoxelWorkClass::Critical) -
			static_cast<int32>(bWasCritical);
	};
	for (FVoxelTaskRequest& Request : Pending)
	{
		Update(Request.Kind, Request.Stamp, Request.WorkClass, Request.SourcePriority,
			Request.DistanceScore, Request.ForwardScore);
	}
	for (FRunning& Request : Running)
	{
		Update(Request.Kind, Request.Stamp, Request.WorkClass, Request.SourcePriority,
			Request.DistanceScore, Request.ForwardScore);
		if (Request.Resume)
		{
			Request.Resume->WorkClass = Request.WorkClass;
			Request.Resume->SourcePriority = Request.SourcePriority;
			Request.Resume->DistanceScore = Request.DistanceScore;
			Request.Resume->ForwardScore = Request.ForwardScore;
		}
	}
}

void FVoxelTaskScheduler::UpdateTerrainStages(TFunctionRef<void(EVoxelTaskKind, const FVoxelTaskStamp&, int32&)> InUpdate)
{
	check(IsInGameThread());
	for (FVoxelTaskRequest& Request : Pending) InUpdate(Request.Kind, Request.Stamp, Request.TerrainStage);
	for (FRunning& Request : Running) InUpdate(Request.Kind, Request.Stamp, Request.TerrainStage);
}

void FVoxelTaskScheduler::Tick(
	TFunctionRef<void(FVoxelTaskResult&&)> InApply,
	const double InMaxApplyMilliseconds)
{
	check(IsInGameThread());

	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TaskSchedulerTick);
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

	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const bool bBackgroundPass = Pass == 1;
		const double EndTime = FPlatformTime::Seconds() +
			(bBackgroundPass ? FMath::Min(0.25, FMath::Max(0.0, InMaxApplyMilliseconds)) :
				FMath::Max(0.0, InMaxApplyMilliseconds)) / 1000.0;
		const int32 ApplyLimit = bBackgroundPass ? FMath::Min(4, Budget.MaxCompletedResultsPerFrame) :
			Budget.MaxCompletedResultsPerFrame;
		const int32 HeavyLimit = bBackgroundPass ? 1 : Budget.MaxHeavyCompletedResultsPerFrame;
		Applied = 0;
		HeavyApplied = 0;
		while (!Running.IsEmpty() && Applied < ApplyLimit && FPlatformTime::Seconds() <= EndTime)
		{
			TArray<FVoxelTaskServiceCandidate> Candidates;
			for (int32 CandidateIndex = 0; CandidateIndex < Running.Num(); ++CandidateIndex)
			{
				const FRunning& Task = Running[CandidateIndex];
				if (!Task.Task.IsCompleted() || IsBackgroundTerrainWork(Task.Kind, Task.WorkClass, Task.TerrainStage) != bBackgroundPass ||
					!CanApplyTerrainKind(Task.Kind) || (Task.Slot->Result.HasHeavyApply() && HeavyApplied >= HeavyLimit)) continue;
				FVoxelTaskRequest Identity;
				Identity.Kind = Task.Kind; Identity.WorkClass = Task.WorkClass;
				Candidates.Add({CandidateIndex, static_cast<EVoxelTaskServiceLane>(TerrainLane(Identity)), EVoxelTaskBlockReason::None,
					Task.SourcePriority, Task.DistanceScore, Task.QueuedAt, Task.Sequence, Task.bPublicationContinuation});
			}
			const int32 Index = CompletedSelection.Select(Candidates, FPlatformTime::Seconds());
			if (Index == INDEX_NONE) break;
			if (Running[Index].Slot->Cancel.Load())
			{
				Running[Index].Slot->Result.bCanceled = true;
				Running[Index].Slot->Result.bSuccess = false;
			}
			const bool bHeavy = Running[Index].Slot->Result.HasHeavyApply();

			FRunning Completed =
				MoveTemp(Running[Index]);

			Running.RemoveAt(Index, 1, EAllowShrinking::No);

			ReservedBytes -=
				Completed.ReservedBytes;

			if (Completed.Resume) RunningContinuationInputBytes -= Completed.Resume->InputBytes;
			if (Completed.Resume && Completed.Slot->Result.bSuccess && Completed.Slot->Result.bContinue && !Completed.Slot->Result.bCanceled)
			{
				Completed.Resume->AccumulatedExecuteMilliseconds = Completed.Slot->Result.ExecuteMilliseconds;
				QueuedInputBytes += Completed.Resume->InputBytes;
				Pending.Add(MoveTemp(*Completed.Resume));
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

			if (Completed.Apply)
			{
				Completed.Apply(
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

			RecordCompletedResult(
				Sample);

			++Applied;
		}

		for (int32 Index = Canceled.Num() - 1; Index >= 0 &&
			Applied < ApplyLimit && FPlatformTime::Seconds() <= EndTime; --Index)
		{
			if (IsBackgroundTerrainWork(Canceled[Index].Result.Kind, Canceled[Index].WorkClass,
				Canceled[Index].TerrainStage) != bBackgroundPass) continue;
			FCompleted Completed = MoveTemp(Canceled[Index]);
			Canceled.RemoveAt(Index, 1, EAllowShrinking::No);

			FVoxelTaskResult Sample;
			Sample.Kind = Completed.Result.Kind;
			Sample.Stamp = Completed.Result.Stamp;
			Sample.bCanceled = true;

			const double ApplyStart =
				FPlatformTime::Seconds();

			if (Completed.Apply)
			{
				Completed.Apply(
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

		Pending.RemoveAtSwap(Index);

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
}

void FVoxelTaskScheduler::CancelMatching(
	TFunctionRef<bool(EVoxelTaskKind, const FVoxelTaskStamp&)> InPredicate)
{
	for (auto It = AdmissionAttempts.CreateIterator(); It; ++It)
		if (InPredicate(It.Key().Kind, It.Key().Stamp)) It.RemoveCurrent();
	check(IsInGameThread());
	for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
	{
		if (!InPredicate(Pending[Index].Kind, Pending[Index].Stamp))
		{
			continue;
		}
		QueuedInputBytes -= Pending[Index].InputBytes;
		FVoxelTaskRequest Request = MoveTemp(Pending[Index]);
		Pending.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		RemoveActive(Request.Stamp, Request.Kind, Request.WorkClass);
		QueueCanceled(MoveTemp(Request));
	}
	for (FRunning& Task : Running)
	{
		if (InPredicate(Task.Kind, Task.Stamp))
		{
			Task.Slot->Cancel.Store(true);
		}
	}
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
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Waiting for voxel task shutdown: kind=%d executeMs=%.2f"),
			static_cast<int32>(Task.Kind),
			Task.Slot->Result.ExecuteMilliseconds);
		Task.Task.Wait();
	}

	Pending.Reset();
	FairSelection.Reset();
	CompletedSelection.Reset();
	AdmissionAttempts.Reset();
	Running.Reset();
	Canceled.Reset();

	ActiveKeys.Reset();
	SectionTaskCounts.Reset();

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

int32 FVoxelTaskScheduler::ActiveCount() const
{
	return Running.Num();
}

int32 FVoxelTaskScheduler::CriticalCount() const
{
	return CriticalTaskCount;
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
	for (const auto& Pair : AdmissionAttempts)
	{
		if (Pair.Value.Blocked == EVoxelTaskBlockReason::None) continue;
		++Result.AdmissionBlockedByKind.FindOrAdd(Pair.Key.Kind).FindOrAdd(Pair.Value.Blocked);
		Result.OldestAdmissionWaitSeconds = FMath::Max(Result.OldestAdmissionWaitSeconds,
			FPlatformTime::Seconds() - Pair.Value.FirstRequested);
	}
	Result.BlockedByKind.Reset();
	for (const FVoxelTaskRequest& Request : Pending)
	{
		if (Request.Blocked != EVoxelTaskBlockReason::None) ++Result.BlockedByKind.FindOrAdd(Request.Kind).FindOrAdd(Request.Blocked);
	}
	Result.RunningByKind.Reset();
	for (const FRunning& Task : Running)
	{
		++Result.RunningByKind.FindOrAdd(Task.Kind);
	}

	return Result;
}

bool FVoxelTaskScheduler::IsHigherPriority(
	const FVoxelTaskRequest& InA,
	const FVoxelTaskRequest& InB)
{
	const int32 BandA = TerrainPriorityBand(InA);
	const int32 BandB = TerrainPriorityBand(InB);
	if (BandA >= -1 && BandA <= 3 && BandB >= -1 && BandB <= 3 &&
		InA.SourcePriority != InB.SourcePriority)
	{
		return InA.SourcePriority < InB.SourcePriority;
	}
	if (BandA != BandB) return BandA < BandB;
	if (InA.SourcePriority != InB.SourcePriority) return InA.SourcePriority < InB.SourcePriority;
	if ((InA.WorkClass == EVoxelWorkClass::Prefetch) != (InB.WorkClass == EVoxelWorkClass::Prefetch))
	{
		return InB.WorkClass == EVoxelWorkClass::Prefetch;
	}
	const bool bAVisual = IsVisualWorkClass(InA.WorkClass);
	const bool bBVisual = IsVisualWorkClass(InB.WorkClass);
	if (bAVisual != bBVisual)
	{
		return !bAVisual;
	}
	if (!bAVisual && InA.WorkClass != InB.WorkClass)
	{
		return
			static_cast<uint8>(
				InA.WorkClass) <
			static_cast<uint8>(
				InB.WorkClass);
	}
	if (InA.WorkClass == EVoxelWorkClass::Critical && InB.WorkClass == EVoxelWorkClass::Critical)
	{
		const bool bARepresentation = IsCriticalRepresentationWork(InA);
		const bool bBRepresentation = IsCriticalRepresentationWork(InB);
		if (bARepresentation != bBRepresentation) return !bARepresentation;
		if (bARepresentation)
		{
			if (InA.bPublicationContinuation != InB.bPublicationContinuation)
				return InA.bPublicationContinuation;
			if (InA.bPublicationContinuation && InA.QueuedAt != InB.QueuedAt)
				return InA.QueuedAt < InB.QueuedAt;
		}
	}
	if (bAVisual && InA.bPublicationContinuation != InB.bPublicationContinuation)
	{
		return InA.bPublicationContinuation;
	}
	if (bAVisual && InA.bPublicationContinuation && InA.QueuedAt != InB.QueuedAt)
	{
		return InA.QueuedAt < InB.QueuedAt;
	}
	if (InA.DistanceScore != InB.DistanceScore)
	{
		return
			InA.DistanceScore <
			InB.DistanceScore;
	}
	if (bAVisual && InA.WorkClass != InB.WorkClass)
	{
		return static_cast<uint8>(InA.WorkClass) < static_cast<uint8>(InB.WorkClass);
	}

	if (InA.ForwardScore !=
		InB.ForwardScore)
	{
		return
			InA.ForwardScore >
			InB.ForwardScore;
	}

	return
		InA.QueuedAt <
		InB.QueuedAt;
}

bool FVoxelTaskResult::HasHeavyApply() const
{
	if (bCanceled || !bSuccess) return false;
	if (Kind == EVoxelTaskKind::BuildFineMesh || Kind == EVoxelTaskKind::BuildVoxelProxy)
	{
		// 空网格只更新就绪/版本状态，不占据实体网格每帧一次的重发布配额。
		const TSharedPtr<FVoxelSectionMeshResult>& Mesh = Kind == EVoxelTaskKind::BuildFineMesh ? FineMesh : VoxelProxyMesh;
		return Mesh && Mesh->Batches.ContainsByPredicate([](const FVoxelRenderBatch& Batch)
		{
			return !Batch.Mesh.Triangles.IsEmpty();
		});
	}
	switch (Kind)
	{
	case EVoxelTaskKind::BuildCollision:
	case EVoxelTaskKind::BuildNavigation:
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
	while (!bStopped &&
		Running.Num() <
			Budget.MaxConcurrentTasks &&
		!Pending.IsEmpty())
	{

		const bool bHasCriticalPending = Pending.ContainsByPredicate([](const FVoxelTaskRequest& Request)
		{
			return Request.WorkClass == EVoxelWorkClass::Critical;
		});
		int32 BackgroundRunning = 0;
		int32 NonCriticalRunning = 0;
		for (const FRunning& Task : Running)
		{
			BackgroundRunning += IsBackgroundTerrainWork(Task.Kind, Task.WorkClass, Task.TerrainStage);
			NonCriticalRunning += Task.WorkClass != EVoxelWorkClass::Critical;
		}
		const int32 BackgroundLimit = Budget.MaxConcurrentTasks > 1
			? FMath::Clamp(Budget.MaxConcurrentCoarseTerrainTasks, 1, Budget.MaxConcurrentTasks - 1) : 1;
		const int32 ForegroundLimit = FMath::Max(1, Budget.MaxConcurrentTasks - BackgroundLimit);
		TArray<FVoxelTaskServiceCandidate> Candidates;
		Candidates.Reserve(Pending.Num());
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			FVoxelTaskRequest& Request = Pending[Index];
			Request.Blocked = EVoxelTaskBlockReason::None;
			const bool bBackground = IsBackgroundTerrainWork(Request.Kind, Request.WorkClass, Request.TerrainStage);
			const bool bReserveCritical = Budget.MaxConcurrentTasks > 1 && bHasCriticalPending &&
				Request.WorkClass != EVoxelWorkClass::Critical &&
				NonCriticalRunning >= Budget.MaxConcurrentTasks - Budget.CriticalReservedTasks;
			if (bReserveCritical || (bBackground && BackgroundRunning >= BackgroundLimit && Pending.ContainsByPredicate([](const FVoxelTaskRequest& Other)
				{ return !IsBackgroundTerrainWork(Other.Kind, Other.WorkClass, Other.TerrainStage); })) ||
				(!bBackground && Running.Num() - BackgroundRunning >= ForegroundLimit && Pending.ContainsByPredicate([](const FVoxelTaskRequest& Other)
				{ return IsBackgroundTerrainWork(Other.Kind, Other.WorkClass, Other.TerrainStage); })) || !CanStartKind(Request.Kind))
			{
				Request.Blocked = EVoxelTaskBlockReason::LaneSlots;
				CriticalAdmissionDeferrals += bReserveCritical;
			}
			if (Request.ReservedBytes > Budget.MaxReservedBytes - ReservedBytes)
			{
				Request.Blocked = static_cast<EVoxelTaskBlockReason>(static_cast<uint16>(Request.Blocked) |
					static_cast<uint16>(EVoxelTaskBlockReason::Memory));
			}
			EVoxelTaskServiceLane Lane = EVoxelTaskServiceLane::Other;
			if (Request.WorkClass == EVoxelWorkClass::Critical)
			{
				Lane = EVoxelTaskServiceLane::Critical;
			}
			else
			{
				switch (Request.Kind)
				{
				case EVoxelTaskKind::GenerateExactBase:
				case EVoxelTaskKind::BuildCollision:
				case EVoxelTaskKind::DecodeOverlay: Lane = EVoxelTaskServiceLane::Exact; break;
				case EVoxelTaskKind::BuildFineMesh: Lane = EVoxelTaskServiceLane::Fine; break;
				case EVoxelTaskKind::GenerateVoxelProxy: Lane = EVoxelTaskServiceLane::ProxyData; break;
				case EVoxelTaskKind::GenerateSurface: Lane = EVoxelTaskServiceLane::SurfaceData; break;
				case EVoxelTaskKind::GenerateMacro: Lane = EVoxelTaskServiceLane::MacroData; break;
				case EVoxelTaskKind::BuildViewCoverage: Lane = EVoxelTaskServiceLane::Coverage; break;
				case EVoxelTaskKind::BuildViewTransition:
				case EVoxelTaskKind::BuildVolumeTransition: Lane = EVoxelTaskServiceLane::Transition; break;
				case EVoxelTaskKind::BuildVoxelProxy: Lane = EVoxelTaskServiceLane::ProxyMesh; break;
				case EVoxelTaskKind::BuildSurface:
				case EVoxelTaskKind::BuildWater: Lane = EVoxelTaskServiceLane::SurfaceMesh; break;
				case EVoxelTaskKind::BuildMacro: Lane = EVoxelTaskServiceLane::MacroMesh; break;
				default: break;
				}
			}
			Candidates.Add({Index, Lane, Request.Blocked, Request.SourcePriority, Request.DistanceScore,
				Request.QueuedAt, Request.Sequence, Request.bPublicationContinuation});
		}
		const int32 BestIndex = FairSelection.Select(Candidates, FPlatformTime::Seconds());
		if (BestIndex == INDEX_NONE)
		{
			break;
		}

		FVoxelTaskRequest Request =
			MoveTemp(
				Pending[BestIndex]);

		Pending.RemoveAtSwap(
			BestIndex);

		QueuedInputBytes -=
			Request.InputBytes;

		ReservedBytes +=
			Request.ReservedBytes;

		FRunning RunningTask;
		RunningTask.Stamp =
			Request.Stamp;
		RunningTask.Kind =
			Request.Kind;
		RunningTask.WorkClass =
			Request.WorkClass;
		RunningTask.SourcePriority = Request.SourcePriority;
		RunningTask.TerrainStage = Request.TerrainStage;
		RunningTask.bPublicationContinuation = Request.bPublicationContinuation;
		RunningTask.bTerrainDataOnly = Request.bTerrainDataOnly;
		RunningTask.DistanceScore = Request.DistanceScore;
		RunningTask.ForwardScore = Request.ForwardScore;
		RunningTask.Sequence = Request.Sequence;
		RunningTask.QueuedAt =
			Request.QueuedAt;
		RunningTask.ReservedBytes =
			Request.ReservedBytes;
		RunningTask.Slot =
			MakeShared<
				FSlot,
				ESPMode::ThreadSafe>();
		if (Request.bResumable)
		{
			RunningTask.Resume = MakeUnique<FVoxelTaskRequest>(Request);
			RunningContinuationInputBytes += Request.InputBytes;
		}
		RunningTask.Apply =
			MoveTemp(Request.Apply);

		const TSharedPtr<
			FSlot,
			ESPMode::ThreadSafe> Slot =
				RunningTask.Slot;

		const FVoxelTaskStamp Stamp =
			RunningTask.Stamp;

		const EVoxelTaskKind Kind =
			RunningTask.Kind;

		const uint64 ResultBudget =
			RunningTask.ReservedBytes;

		const double QueuedAt =
			RunningTask.QueuedAt;
		const double AccumulatedExecuteMilliseconds = Request.AccumulatedExecuteMilliseconds;

		RunningTask.Task =
			UE::Tasks::Launch(
				UE_SOURCE_LOCATION,
				[Slot, Stamp, Kind, ResultBudget, QueuedAt, AccumulatedExecuteMilliseconds, Execute = MoveTemp(Request.Execute)]() mutable
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(
						Voxel_TaskExecute);

					const double Start =
						FPlatformTime::Seconds();

					if (!Slot->Cancel.Load())
					{
						Slot->Result =
							Execute(
								Slot->Cancel);
					}

					const double End =
						FPlatformTime::Seconds();

					Slot->Result.Stamp =
						Stamp;

					Slot->Result.Kind =
						Kind;

					Slot->Result.bCanceled =
						Slot->Cancel.Load();

					Slot->Result.QueueMilliseconds =
						FMath::Max(
							0.0,
							(Start - QueuedAt) *
								1000.0);

					Slot->Result.ExecuteMilliseconds = FMath::Max(0.0, (End - Start) * 1000.0) + AccumulatedExecuteMilliseconds;

					if (Slot->Result.ResultBytes() >
						ResultBudget)
					{
						FVoxelTaskResult ErrorResult;

						ErrorResult.Stamp =
							Stamp;

						ErrorResult.Kind =
							Kind;

						ErrorResult.Error =
							FString::Printf(TEXT("Voxel task result memory budget exceeded: kind=%d actual=%llu budget=%llu surface=(%d,%d,L%d)"),
								static_cast<int32>(Kind), Slot->Result.ResultBytes(), ResultBudget,
								Stamp.SurfaceKey.Coordinate.X, Stamp.SurfaceKey.Coordinate.Y, Stamp.SurfaceKey.Level);

						ErrorResult.QueueMilliseconds =
							Slot->Result.QueueMilliseconds;

						ErrorResult.ExecuteMilliseconds =
							Slot->Result.ExecuteMilliseconds;

						Slot->Result =
							MoveTemp(
								ErrorResult);
					}
				},
				UE::Tasks::ETaskPriority::Normal);

		Running.Add(
			MoveTemp(RunningTask));
	}
}

void FVoxelTaskScheduler::QueueCanceled(
	FVoxelTaskRequest&& InRequest)
{
	FCompleted Completed;
	Completed.WorkClass = InRequest.WorkClass;
	Completed.TerrainStage = InRequest.TerrainStage;

	Completed.Result.Stamp =
		InRequest.Stamp;

	Completed.Result.Kind =
		InRequest.Kind;

	Completed.Result.bCanceled =
		true;

	Completed.Apply =
		MoveTemp(InRequest.Apply);

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
