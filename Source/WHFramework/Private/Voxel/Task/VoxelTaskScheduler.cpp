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
		switch (Request.Kind)
		{
		case EVoxelTaskKind::BuildFineMesh: return 0;
		case EVoxelTaskKind::GenerateVoxelProxy:
		case EVoxelTaskKind::BuildVoxelProxy: return 1;
		case EVoxelTaskKind::GenerateSurface:
		case EVoxelTaskKind::BuildSurface: return 2;
		case EVoxelTaskKind::GenerateMacro:
		case EVoxelTaskKind::BuildMacro: return 3;
		case EVoxelTaskKind::BuildViewCoverage: return Request.bPublicationContinuation ? 4 : INDEX_NONE;
		default: return INDEX_NONE;
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
		switch (InKind)
		{
		case EVoxelTaskKind::BuildFineMesh: return 0;
		case EVoxelTaskKind::BuildVoxelProxy: return 1;
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

bool FVoxelTaskScheduler::Enqueue(FVoxelTaskRequest&& InRequest)
{
	check(IsInGameThread());

	if (InRequest.ReservedBytes > Budget.MaxReservedBytes || InRequest.InputBytes > Budget.MaxInputBytes)
	{
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
		return false;
	}

	if (Has(InRequest.Stamp, InRequest.Kind))
	{
		return true;
	}

	if (IsCoarseTerrainKind(InRequest.Kind))
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
			return false;
		}
	}

	const bool bBackground = IsBackgroundTerrainWork(InRequest.Kind, InRequest.WorkClass, InRequest.TerrainStage);
	if (bBackground)
	{
		uint64 BackgroundInput = 0;
		int32 BackgroundPending = 0;
		for (const FVoxelTaskRequest& Existing : Pending)
		{
			if (!IsBackgroundTerrainWork(Existing.Kind, Existing.WorkClass, Existing.TerrainStage)) continue;
			BackgroundInput += Existing.InputBytes;
			++BackgroundPending;
		}
		if (InRequest.InputBytes + BackgroundInput > Budget.MaxInputBytes / 2 ||
			BackgroundPending >= FMath::Max(1, Budget.MaxPendingTasks - Budget.CriticalReservedTasks)) return false;
	}
	InRequest.QueuedAt = FPlatformTime::Seconds();
	const int32 Lane = TerrainLane(InRequest);
	const int32 ReservedPerLane = Budget.MaxPendingTasks >= 16 ? 2 : 0;
	int32 Counts[5] = {};
	for (const FVoxelTaskRequest& Request : Pending)
	{
		const int32 Index = TerrainLane(Request);
		if (Index != INDEX_NONE) ++Counts[Index];
	}
	int32 PendingLimit = Budget.MaxPendingTasks;
	// 数据准备与网格共用各层保留位；普通生成、计划和关键请求都保留其它层的最小队列空间。
	if (ReservedPerLane > 0)
	{
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const int32 Reserve = Index == 4 ? 1 : ReservedPerLane;
			if (Index != Lane) PendingLimit -= FMath::Max(0, Reserve - Counts[Index]);
		}
	}
	// 先完整验证替换集合，再取消旧请求，内存不足不能导致部分取消后仍然拒绝新请求。
	TArray<int32, TInlineAllocator<8>> DisplacedIndices;
	uint64 RemainingInput = QueuedInputBytes;
	while (Pending.Num() - DisplacedIndices.Num() >= PendingLimit ||
		InRequest.InputBytes > Budget.MaxInputBytes - RemainingInput)
	{
		int32 Worst = INDEX_NONE;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			if (DisplacedIndices.Contains(Index)) continue;
			const FVoxelTaskRequest& Existing = Pending[Index];
			const int32 ExistingLane = TerrainLane(Existing);
			const int32 Reserve = ExistingLane == 4 ? (ReservedPerLane > 0 ? 1 : 0) : ReservedPerLane;
			if (ExistingLane != INDEX_NONE && ExistingLane != Lane && Counts[ExistingLane] <= Reserve) continue;
			const bool bExistingBackground = IsBackgroundTerrainWork(Existing.Kind, Existing.WorkClass, Existing.TerrainStage);
			const bool bMayReplace = (!bBackground && bExistingBackground) ||
				(bBackground == bExistingBackground && IsHigherPriority(InRequest, Existing));
			if (bMayReplace && (Worst == INDEX_NONE || IsHigherPriority(Pending[Worst], Existing))) Worst = Index;
		}
		if (Worst == INDEX_NONE) return false;
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
		RemoveActive(Displaced.Stamp, Displaced.Kind, Displaced.WorkClass);
		QueueCanceled(MoveTemp(Displaced));
	}

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
	Running.StableSort([](const FRunning& A, const FRunning& B)
	{
		FVoxelTaskRequest PriorityA;
		PriorityA.Kind = A.Kind;
		PriorityA.TerrainStage = A.TerrainStage;
		PriorityA.bPublicationContinuation = A.bPublicationContinuation;
		PriorityA.WorkClass = A.WorkClass;
		PriorityA.SourcePriority = A.SourcePriority;
		PriorityA.DistanceScore = A.DistanceScore;
		PriorityA.ForwardScore = A.ForwardScore;
		PriorityA.QueuedAt = A.QueuedAt;
		FVoxelTaskRequest PriorityB;
		PriorityB.Kind = B.Kind;
		PriorityB.TerrainStage = B.TerrainStage;
		PriorityB.bPublicationContinuation = B.bPublicationContinuation;
		PriorityB.WorkClass = B.WorkClass;
		PriorityB.SourcePriority = B.SourcePriority;
		PriorityB.DistanceScore = B.DistanceScore;
		PriorityB.ForwardScore = B.ForwardScore;
		PriorityB.QueuedAt = B.QueuedAt;
		return IsHigherPriority(PriorityA, PriorityB);
	});

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
		for (int32 Index = 0;
			Index < Running.Num() &&
			Applied < ApplyLimit &&
			FPlatformTime::Seconds() <= EndTime;)
		{
			if (IsBackgroundTerrainWork(Running[Index].Kind, Running[Index].WorkClass,
				Running[Index].TerrainStage) != bBackgroundPass ||
				(!Running[Index].Slot->Cancel.Load() && !CanBuildTerrainStage(TerrainMeshStage(
					Running[Index].Kind, Running[Index].WorkClass, Running[Index].TerrainStage,
					Running[Index].bPublicationContinuation, Running[Index].bTerrainDataOnly))) ||
				!Running[Index].Task.IsCompleted())
			{
				++Index;
				continue;
			}

			if (Running[Index].Slot->Cancel.Load())
			{
				Running[Index].Slot->Result.bCanceled = true;
				Running[Index].Slot->Result.bSuccess = false;
			}
			const bool bHeavy = Running[Index].Slot->Result.HasHeavyApply();

			if (!CanApplyTerrainKind(Running[Index].Kind))
			{
				++Index;
				continue;
			}

			if (bHeavy &&
				HeavyApplied >=
					HeavyLimit)
			{
				++Index;
				continue;
			}

			FRunning Completed =
				MoveTemp(Running[Index]);

			Running.RemoveAt(Index, 1, EAllowShrinking::No);

			ReservedBytes -=
				Completed.ReservedBytes;

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
	Running.Reset();
	Canceled.Reset();

	ActiveKeys.Reset();
	SectionTaskCounts.Reset();

	QueuedInputBytes = 0;
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

void FVoxelTaskScheduler::SetTerrainBuildStage(const int32 InStage)
{
	check(IsInGameThread());
	const int32 Stage = FMath::Clamp(InStage, 0, 3);
	if (Stage < TerrainBuildStage)
	{
		for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
		{
			if (TerrainMeshStage(Pending[Index].Kind, Pending[Index].WorkClass,
				Pending[Index].TerrainStage, Pending[Index].bPublicationContinuation, Pending[Index].bTerrainDataOnly) <= Stage) continue;
			FVoxelTaskRequest Request = MoveTemp(Pending[Index]);
			Pending.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			QueuedInputBytes -= Request.InputBytes;
			RemoveActive(Request.Stamp, Request.Kind, Request.WorkClass);
			QueueCanceled(MoveTemp(Request));
		}
		for (FRunning& Task : Running)
			if (TerrainMeshStage(Task.Kind, Task.WorkClass, Task.TerrainStage,
				Task.bPublicationContinuation, Task.bTerrainDataOnly) > Stage) Task.Slot->Cancel.Store(true);
	}
	TerrainBuildStage = Stage;
}

bool FVoxelTaskScheduler::CanBuildTerrainStage(const int32 InStage) const
{
	return InStage == INDEX_NONE || InStage <= TerrainBuildStage;
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
		const bool bHasCriticalPending = Pending.ContainsByPredicate(
			[](const FVoxelTaskRequest& Request)
			{
				return Request.WorkClass == EVoxelWorkClass::Critical;
			});
		int32 NonCriticalRunning = 0;
		for (const FRunning& Task : Running)
		{
			NonCriticalRunning += Task.WorkClass != EVoxelWorkClass::Critical ? 1 : 0;
		}
		const int32 NonCriticalLimit = FMath::Max(
			0, Budget.MaxConcurrentTasks - Budget.CriticalReservedTasks);
		const int32 BackgroundLimit = Budget.MaxConcurrentTasks > 1
			? FMath::Clamp(Budget.MaxConcurrentCoarseTerrainTasks, 1,
				FMath::Max(1, Budget.MaxConcurrentTasks - Budget.CriticalReservedTasks)) : 1;
		const int32 ForegroundLimit = FMath::Max(1, Budget.MaxConcurrentTasks - BackgroundLimit);
		int32 BackgroundRunning = 0;
		uint64 BackgroundBytes = 0;
		for (const FRunning& Task : Running)
		{
			if (!IsBackgroundTerrainWork(Task.Kind, Task.WorkClass, Task.TerrainStage)) continue;
			++BackgroundRunning;
			BackgroundBytes += Task.ReservedBytes;
		}
		int32 BestIndex = INDEX_NONE;

		for (int32 Index = 0;
			Index < Pending.Num();
			++Index)
		{
			const FVoxelTaskRequest& Request =
				Pending[Index];
			const bool bBackground = IsBackgroundTerrainWork(Request.Kind, Request.WorkClass, Request.TerrainStage);
			if (bHasCriticalPending && !bBackground && Request.WorkClass != EVoxelWorkClass::Critical)
			{
				if (NonCriticalRunning >= NonCriticalLimit ||
					Request.Kind == EVoxelTaskKind::BuildSurface ||
					Request.Kind == EVoxelTaskKind::BuildMacro ||
					Request.WorkClass == EVoxelWorkClass::Prefetch)
				{
					++CriticalAdmissionDeferrals;
					continue;
				}
			}

			if (!CanBuildTerrainStage(TerrainMeshStage(Request.Kind, Request.WorkClass,
				Request.TerrainStage, Request.bPublicationContinuation, Request.bTerrainDataOnly)) ||
				(bBackground && (BackgroundRunning >= BackgroundLimit ||
					Request.ReservedBytes + BackgroundBytes > Budget.MaxReservedBytes / 2)) ||
				(!bBackground && Running.Num() - BackgroundRunning >= ForegroundLimit)) continue;
			if (!CanStartKind(Request.Kind))
			{
				continue;
			}

			if (Request.ReservedBytes >
				Budget.MaxReservedBytes -
					ReservedBytes)
			{
				continue;
			}

			if (BestIndex == INDEX_NONE ||
				IsHigherPriority(
					Request,
					Pending[BestIndex]))
			{
				BestIndex =
					Index;
			}
		}

		if (BestIndex ==
			INDEX_NONE)
		{
			break;
		}

		// 等待时间只能调整同阶段顺序，不能让下一阶段越过上一阶段。
		if (IsVisualWorkClass(Pending[BestIndex].WorkClass))
		{
			const double Deadline = FPlatformTime::Seconds() - 1.0;
			int32 Oldest = INDEX_NONE;
			for (int32 Index = 0; Index < Pending.Num(); ++Index)
			{
				const FVoxelTaskRequest& Candidate = Pending[Index];
				const bool bBackground = IsBackgroundTerrainWork(Candidate.Kind, Candidate.WorkClass, Candidate.TerrainStage);
				if (!CanBuildTerrainStage(TerrainMeshStage(Candidate.Kind, Candidate.WorkClass,
					Candidate.TerrainStage, Candidate.bPublicationContinuation, Candidate.bTerrainDataOnly)) ||
					(bBackground && (BackgroundRunning >= BackgroundLimit ||
						Candidate.ReservedBytes + BackgroundBytes > Budget.MaxReservedBytes / 2)) ||
					(!bBackground && Running.Num() - BackgroundRunning >= ForegroundLimit)) continue;
				if (TerrainPriorityBand(Candidate) == TerrainPriorityBand(Pending[BestIndex]) &&
					Candidate.SourcePriority == Pending[BestIndex].SourcePriority &&
					Candidate.bPublicationContinuation == Pending[BestIndex].bPublicationContinuation &&
					IsVisualWorkClass(Candidate.WorkClass) && Candidate.QueuedAt <= Deadline &&
					(!bHasCriticalPending || bBackground || Candidate.WorkClass == EVoxelWorkClass::Critical ||
						(NonCriticalRunning < NonCriticalLimit && Candidate.Kind != EVoxelTaskKind::BuildSurface &&
						Candidate.Kind != EVoxelTaskKind::BuildMacro && Candidate.WorkClass != EVoxelWorkClass::Prefetch)) &&
					CanStartKind(Candidate.Kind) && Candidate.ReservedBytes <= Budget.MaxReservedBytes - ReservedBytes &&
					(Oldest == INDEX_NONE || Candidate.QueuedAt < Pending[Oldest].QueuedAt))
				{
					Oldest = Index;
				}
			}
			if (Oldest != INDEX_NONE) BestIndex = Oldest;
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
		RunningTask.QueuedAt =
			Request.QueuedAt;
		RunningTask.ReservedBytes =
			Request.ReservedBytes;
		RunningTask.Slot =
			MakeShared<
				FSlot,
				ESPMode::ThreadSafe>();
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

		RunningTask.Task =
			UE::Tasks::Launch(
				UE_SOURCE_LOCATION,
				[
					Slot,
					Stamp,
					Kind,
					ResultBudget,
					QueuedAt,
					Execute =
						MoveTemp(
							Request.Execute)
				]() mutable
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

					Slot->Result.ExecuteMilliseconds =
						FMath::Max(
							0.0,
							(End - Start) *
								1000.0);

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
				}, IsBackgroundTerrainWork(RunningTask.Kind, RunningTask.WorkClass, RunningTask.TerrainStage)
					? UE::Tasks::ETaskPriority::BackgroundNormal : UE::Tasks::ETaskPriority::Normal);

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
