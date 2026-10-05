#include "Voxel/Rendering/VoxelViewManager.h"

#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Chunks/VoxelSectionKey.h"

#include "Algo/BinarySearch.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Network/VoxelGeometryWire.h"
#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
#include "Voxel/Rendering/DWHeightfieldTransitionBuilder.h"
#include "Voxel/Rendering/DWVolumeTransitionPlanner.h"
#include "Voxel/Rendering/VoxelCoverage.h"
#include "Voxel/Rendering/VoxelMeshClipper.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelProxyPageBuild.h"
#include "Voxel/Rendering/VoxelProxyPageMesh.h"
#include "Voxel/Rendering/VoxelHeightfieldPageBuild.h"
#include "Voxel/Rendering/VoxelHeightfieldPageMesh.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/VoxelModule.h"

struct FVoxelHeightfieldCoveragePlan final : FVoxelTaskCustomPayload
{
	uint64 Signature = 0;
	TArray<FVoxelHeightfieldTransitionEdge> Edges;
	virtual uint64 GetAllocatedBytes() const override { return Edges.GetAllocatedSize(); }
};

struct FVoxelFinePageBuildResult final : FVoxelTaskCustomPayload
{
	TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh;
	virtual uint64 GetAllocatedBytes() const override { return sizeof(*this) + (Mesh ? Mesh->Bytes() : 0); }
};

struct FVoxelVolumeCoveragePlan final : FVoxelTaskCustomPayload
{
	uint64 Signature = 0;
	TSet<FVoxelViewKey> Nodes;
	TArray<FVoxelVolumeTransitionFace> Faces;
	virtual uint64 GetAllocatedBytes() const override
	{
		return Nodes.GetAllocatedSize() + Faces.GetAllocatedSize();
	}
};

namespace
{
FVoxelGameThreadContinuation GatherFinePageSectionSteps(FVoxelFinePageKey Page, TArray<FIntVector>& Out);
struct FProxyMeshEstimate
{
	FVoxelProxyPageMesh Counter;
	TSharedPtr<FVoxelVoxelProxyData> Data;
};
	struct FHeightfieldMeshEstimate
	{
		FVoxelHeightfieldPageMesh Counter;
		TSharedPtr<FVoxelSurfaceTileData> Surface;
		TSharedPtr<FVoxelWaterSurfaceTileData> Water;
		TSharedPtr<FVoxelMacroTileData> Macro;
	};
	constexpr int32 ViewSectionSide = 16;

	TSharedRef<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> RetainEnvironmentData(
		const FVoxelGenerationPipeline& Generator, const FVoxelGenerationBounds& Bounds)
	{
		const int64 Width = static_cast<int64>(Bounds.Max.X) - Bounds.Min.X;
		const int64 Height = static_cast<int64>(Bounds.Max.Y) - Bounds.Min.Y;
		return Generator.RetainHydrologyForPlanning(
			FIntPoint(static_cast<int32>(static_cast<int64>(Bounds.Min.X) + Width / 2),
				static_cast<int32>(static_cast<int64>(Bounds.Min.Y) + Height / 2)),
			static_cast<int32>((FMath::Max(Width, Height) + 1) / 2));
	}

	struct FHeightfieldTransitionTaskPayload : FVoxelTaskCustomPayload
	{
		TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh;

		virtual uint64 GetAllocatedBytes() const override
		{
			return sizeof(*this) + (Mesh ? Mesh->Bytes() : 0);
		}
	};

	struct FHeightfieldTransitionTileSource
	{
		TSharedPtr<const FVoxelSurfaceTileData> Surface;
		TSharedPtr<const FVoxelMacroTileData> Macro;

		FVoxelHeightfieldTileView View() const
		{
			return Surface ? FVoxelHeightfieldTransitionBuilder::MakeView(*Surface)
				: FVoxelHeightfieldTransitionBuilder::MakeView(*Macro);
		}
	};

	TAutoConsoleVariable<int32> CVarVoxelDebugRepresentation(
		TEXT("wh.Voxel.DebugRepresentation"),
		1,
		TEXT("Logs voxel representation Wanted/Ready/Actor/Visible state. 0=off, 1=summary, 2=nearby keys."),
		ECVF_Cheat);

	void AppendNonEmptyBatches(
		FVoxelSectionMeshResult& InOutTarget,
		FVoxelSectionMeshResult&& InSource)
	{
		for (FVoxelRenderBatch& Batch : InSource.Batches)
		{
			if (!Batch.Mesh.Vertices.IsEmpty() && !Batch.Mesh.Triangles.IsEmpty())
			{
				InOutTarget.Batches.Add(MoveTemp(Batch));
			}
		}
	}

}

FVoxelViewManager::FVoxelViewManager(
	UVoxelModule& InModule,
	FVoxelTaskScheduler& InScheduler,
	const uint64 InWorldEpoch)
	: Module(InModule),
	  Scheduler(InScheduler),
	  WorldEpoch(InWorldEpoch)
{
	Publisher = MakeUnique<FVoxelViewPublisher>(Module, Scheduler, WorldEpoch);
}

FVoxelViewManager::~FVoxelViewManager()
{
	Reset();
}

void FVoxelViewManager::Tick(
	const uint64 InInterestRevision,
	const TConstArrayView<FVector> InObservers)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewManager);
	const double TickStart = FPlatformTime::Seconds();
	TerrainFrameBudget.Begin(TickStart, Module.GetViewSettings().Budget.TerrainPublishMilliseconds, 8192);
	bTerrainFrameBudgetActive = true;
	FrameTimings = {};
	if (!CoverageContinuation.IsValid())
	{
		const double ApplyDeadline = TerrainDeadline(0.25);
		TFunction<void()> Apply;
		while (FPlatformTime::Seconds() < ApplyDeadline && DeferredCoverageCallbacks.Dequeue(Apply))
		{
			Apply();
		}
		while (!DeferredCoverageResults.IsEmpty() && FPlatformTime::Seconds() < ApplyDeadline)
		{
			FVoxelTaskResult Result = MoveTemp(DeferredCoverageResults.Last());
			DeferredCoverageResults.RemoveAt(DeferredCoverageResults.Num() - 1, 1, EAllowShrinking::No);
			OnTask(MoveTemp(Result));
		}
	}
	Publisher->SetObservers(InObservers);
	const auto Run = [this](EVoxelPublishPhase Phase, auto&& Operation)
	{
		const double Start = FPlatformTime::Seconds();
		const uint8 Index = uint8(Phase);
		double& Estimate = ManagerPhaseEstimateMilliseconds[Index];
		const auto& Policy = Module.GetViewSettings().Budget;
		const double Floor = Policy.PublishOperationEstimateFloorMilliseconds;
		const double Ceiling = FMath::Min(double(Policy.PublishOperationEstimateCeilingMilliseconds), double(Policy.TerrainPublishMilliseconds) * 0.5);
		Estimate = FMath::Clamp(Estimate > 0.0 ? Estimate : Floor, Floor, Ceiling);
		if (!TerrainFrameBudget.TryStart(Phase, Start, Estimate))
		{
			const uint64 Count = ++ManagerPhaseDeferrals[Index];
			if (Count == 60 || Count % 600 == 0)
				UE_LOG(LogTemp, Warning, TEXT("Voxel manager deferral: phase=%d consecutive=%llu estimateMs=%.3f"), Index, Count, Estimate);
			return false;
		}
		ManagerPhaseDeferrals[Index] = 0;
		Operation();
		const double End = FPlatformTime::Seconds();
		Estimate = FMath::Clamp(Estimate * 0.8 + (End - Start) * 200.0, Floor, Ceiling);
		TerrainFrameBudget.Finish(Phase, Start, End);
		return true;
	};
	// 地形 GT 的发布、覆盖、准入、回收共用同一个期限；被推迟阶段保留队列/游标。
	const int32 FirstPhase = NextManagerPhase;
	NextManagerPhase = (FirstPhase + 1) % 5;
	for (int32 Attempt = 0; Attempt < 5 && TerrainFrameBudget.RemainingMilliseconds(FPlatformTime::Seconds()) > 0.0; ++Attempt)
	{
		const int32 Phase = (FirstPhase + Attempt) % 5;
		const bool bDeferredCoverage = !DeferredCoverageCallbacks.IsEmpty() || !DeferredCoverageResults.IsEmpty();
		const bool bCurrentInterest = AppliedInterestSnapshot.IsValid() && !PendingInterestSnapshot;
		if (Phase == 0 && !CoverageContinuation.IsValid() && !bDeferredCoverage &&
			(PendingInterestSnapshot || !AppliedInterestSnapshot || AppliedInterestRevision != Module.GetCurrentInterest().PublicationRevision))
			AdvanceInterestInputs(InInterestRevision, InObservers);
		else if (Phase == 0 && bTaskPrioritiesDirty && !CoverageContinuation.IsValid() && !bDeferredCoverage)
			Run(EVoxelPublishPhase::Priority, [this, InObservers]
			{
				PriorityObservers = TArray<FVector>(InObservers);
				const auto Latest = Module.GetCurrentInterestSnapshot();
				if (Latest && AppliedInterestSnapshot && Latest->PublicationRevision == AppliedInterestSnapshot->PublicationRevision &&
					!PendingInterestSnapshot)
					AppliedInterestSnapshot = Latest;
				RebuildAdmissions(InObservers);
				UpdateTaskPriorities();
				Publisher->InvalidatePriorities();
				bTaskPrioritiesDirty = false;
			});
		else if (Phase == 1 && bCurrentInterest)
		{
			const bool bAllowCommit = !CoverageContinuation.IsValid() && !bDeferredCoverage;
			Publisher->Tick(TerrainFrameBudget, bAllowCommit);
			if (bAllowCommit)
			{
				for (const auto& Key : Publisher->TakeRetryOwners())
				{
					const auto Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Key, Module.BlockSize(), Module.GetViewSettings().Lod);
					if (Bounds.IsValid)
						MarkCoverageDirty(
							{FIntPoint(FMath::FloorToInt(Bounds.Min.X / Module.BlockSize()), FMath::FloorToInt(Bounds.Min.Y / Module.BlockSize())),
								FIntPoint(FMath::CeilToInt(Bounds.Max.X / Module.BlockSize()), FMath::CeilToInt(Bounds.Max.Y / Module.BlockSize()))});
					if (Key.Representation == 0)
					{
						MarkFinePageDirty({Key.Coordinate, Key.Level});
						bFinePresentationDirty = true;
					}
				}
			}
		}
		else if (Phase == 2 && bCurrentInterest && (CoverageContinuation.IsValid() || (bCoverageDirty && !bDeferredCoverage && !Publisher->HasReadyCommits())))
			Run(EVoxelPublishPhase::Plan, [this]
			{
				const double Start = FPlatformTime::Seconds();
				if (!CoverageContinuation.IsValid())
				{
					bActiveCoverageFull = bCoverageDirtyFull;
					bCoverageDirtyFull = false;
					const int32 Count = bActiveCoverageFull ? 0 : FMath::Min(DirtyCoverageRects.Num(), 1);
					ActiveDirtyCoverageRects.Append(DirtyCoverageRects.GetData(), Count);
					DirtyCoverageRects.RemoveAt(0, Count, EAllowShrinking::No);
					bCoverageDirty = !DirtyCoverageRects.IsEmpty();
					CoverageInterestSnapshot = AppliedInterestSnapshot ? AppliedInterestSnapshot : Module.GetCurrentInterestSnapshot();
					CoverageStartedSeconds = Start;
					CoverageContinuation = ResolveTransitionVisibilitySteps();
				}
				const double Deadline = TerrainDeadline(0.25);
				for (int32 Steps = 0; Steps < 4096 && !CoverageContinuation.IsComplete() && FPlatformTime::Seconds() < Deadline; ++Steps)
				{
					CoverageContinuation.Resume();
					++CoverageMicrosteps;
				}
				if (CoverageContinuation.IsComplete())
				{
					CoverageContinuation.Reset();
					CoverageInterestSnapshot.Reset();
					ActiveDirtyCoverageRects.Reset();
					bActiveCoverageFull = true;
				}
				LastCoverageMilliseconds = (FPlatformTime::Seconds() - Start) * 1000.0;
				FrameTimings.CoverageMilliseconds += LastCoverageMilliseconds;
			});
		else if (Phase == 3 && bCurrentInterest && !CoverageContinuation.IsValid() && !bDeferredCoverage)
		{
			Run(EVoxelPublishPhase::Admission, [this] { ProcessAdmissions(); });
			Run(EVoxelPublishPhase::Admission, [this] { ProcessDataAdmissions(); });
			Run(EVoxelPublishPhase::Admission, [this] { PumpHeightfieldTransitions(); });
			Run(EVoxelPublishPhase::Admission, [this] { PumpVolumeTransitions(); });
		}
		else if (Phase == 4 && bCurrentInterest && !CoverageContinuation.IsValid() && !bDeferredCoverage && !Publisher->HasReadyCommits() &&
				 FPlatformTime::Seconds() >= NextRetireCheck)
			Run(EVoxelPublishPhase::Retire, [this]
			{
				CleanupRetiredRepresentations(FPlatformTime::Seconds());
				NextRetireCheck = FPlatformTime::Seconds();
			});
	}
	RefreshTerrainBuildStage();
#if !UE_BUILD_SHIPPING
	if (TickStart >= NextInterestProgressLog)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Voxel publication input progress: desired=%llu applied=%llu pending=%llu phase=%d cursor=%d fine=%d admissions=%d geometry=%d"),
			InInterestRevision, AppliedInterestRevision, PendingInterestRevision, InterestInputPhase, InterestInputCursor, FineWanted.Num(), Admissions.Num(),
			GetViewInterest().Geometry.Num());
		NextInterestProgressLog = TickStart + 5.0;
		LogRepresentationState(InObservers);
		const auto& Accepted = GetViewInterest();
		UE_LOG(LogTemp, Display, TEXT("Voxel publication admission inputs: priorities=%d first=%d lanes=%d/%d/%d/%d admissionMs=%.6f"),
			AdmissionPriorities.Num(), AdmissionPriorities.IsEmpty() ? -1 : AdmissionPriorities[0],
			Accepted.AdmissionLanes[0].Num(), Accepted.AdmissionLanes[1].Num(), Accepted.AdmissionLanes[2].Num(), Accepted.AdmissionLanes[3].Num(),
			Module.GetViewSettings().Budget.AdmissionMilliseconds);
	}
#endif
	FrameTimings.PublishPrepareMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Prepare).Milliseconds;
	FrameTimings.PublishCommitMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Commit).Milliseconds;
	FrameTimings.PublishPlanMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Plan).Milliseconds;
	FrameTimings.PublishPriorityMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Priority).Milliseconds;
	FrameTimings.AdmissionMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Admission).Milliseconds;
	FrameTimings.RetireMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Retire).Milliseconds;
	FrameTimings.PublishMaximumOperationMilliseconds = 0.0;
	for (int32 Phase = 1; Phase < static_cast<int32>(EVoxelPublishPhase::Count); ++Phase)
		FrameTimings.PublishMaximumOperationMilliseconds = FMath::Max(FrameTimings.PublishMaximumOperationMilliseconds,
			TerrainFrameBudget.GetTiming(static_cast<EVoxelPublishPhase>(Phase)).MaximumOperationMilliseconds);
	FrameTimings.PublishTotalMilliseconds = TerrainFrameBudget.TotalMilliseconds(FPlatformTime::Seconds());
	FrameTimings.bPublishBudgetOverrun = TerrainFrameBudget.HasOverrun(FPlatformTime::Seconds());
	bTerrainFrameBudgetActive = false;
#if !UE_BUILD_SHIPPING
	if (FrameTimings.bPublishBudgetOverrun && FPlatformTime::Seconds() >= NextBudgetLog)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel terrain budget overrun: totalMs=%.3f maximumOperationMs=%.3f budgetMs=%.3f planMs=%.3f admissionMs=%.3f prepareMs=%.3f commitMs=%.3f retireMs=%.3f"),
			FrameTimings.PublishTotalMilliseconds, FrameTimings.PublishMaximumOperationMilliseconds, Module.GetViewSettings().Budget.TerrainPublishMilliseconds,
			FrameTimings.PublishPlanMilliseconds, FrameTimings.AdmissionMilliseconds, FrameTimings.PublishPrepareMilliseconds,
			FrameTimings.PublishCommitMilliseconds, FrameTimings.RetireMilliseconds);
		NextBudgetLog = FPlatformTime::Seconds() + 5.0;
	}
#endif
}

bool FVoxelViewManager::IsAdmissionDataReady(const FVoxelViewAdmission& InAdmission, const bool bIncludePending) const
{
	if (InAdmission.Kind == EVoxelViewAdmissionKind::Fine)
	{
		const FVoxelSection* Section = Module.GetRuntime()->FindSection(InAdmission.FineKey);
		return Section && Section->Status == EVoxelSectionStatus::DataReady;
	}
	if (IsAdmissionMeshReady(InAdmission)) return true;
	FVoxelTaskKey Key;
	Key.Stamp.WorldEpoch = WorldEpoch;
	EVoxelTaskKind MeshKind = EVoxelTaskKind::None;
	const FVoxelChangeHierarchy& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	switch (InAdmission.Kind)
	{
	case EVoxelViewAdmissionKind::VoxelProxy:
		Key.Kind = EVoxelTaskKind::GenerateVoxelProxy;
		MeshKind = EVoxelTaskKind::BuildVoxelProxy;
		Key.Stamp.ViewKey = InAdmission.ProxyKey;
		Key.Stamp.Revision = Hierarchy.GetVoxelProxyRevision(InAdmission.ProxyKey);
		Key.Stamp.Token = HashCombineFast(GetTypeHash(InAdmission.ProxyKey), GetTypeHash(Key.Stamp.Revision));
		if (const auto* Geometry = GetViewInterest().FindGeometry(InAdmission.ProxyKey))
			Key.Stamp.GeometryRevision = GetTypeHash(*Geometry);
		break;
	case EVoxelViewAdmissionKind::Surface:
		Key.Kind = EVoxelTaskKind::GenerateSurface;
		MeshKind = EVoxelTaskKind::BuildSurface;
		Key.Stamp.SurfaceKey = InAdmission.SurfaceKey;
		Key.Stamp.Revision = Hierarchy.GetSurfaceRevision(InAdmission.SurfaceKey);
		Key.Stamp.Token = HashCombineFast(GetTypeHash(InAdmission.SurfaceKey), GetTypeHash(Key.Stamp.Revision));
		if (const auto* Geometry = GetViewInterest().FindGeometry(InAdmission.SurfaceKey))
			Key.Stamp.GeometryRevision = GetTypeHash(*Geometry);
		break;
	case EVoxelViewAdmissionKind::Macro:
		Key.Kind = EVoxelTaskKind::GenerateMacro;
		MeshKind = EVoxelTaskKind::BuildMacro;
		Key.Stamp.MacroKey = InAdmission.MacroKey;
		Key.Stamp.Revision = Hierarchy.GetMacroRevision(InAdmission.MacroKey);
		Key.Stamp.Token = HashCombineFast(GetTypeHash(InAdmission.MacroKey), GetTypeHash(Key.Stamp.Revision));
		if (const auto* Geometry = GetViewInterest().FindGeometry(InAdmission.MacroKey))
			Key.Stamp.GeometryRevision = GetTypeHash(*Geometry);
		break;
	default:
		return false;
	}
	return PreparedData.Contains(Key) || Scheduler.Has(Key.Stamp, MeshKind) ||
		(bIncludePending && Scheduler.Has(Key.Stamp, Key.Kind));
}

void FVoxelViewManager::ProcessDataAdmissions()
{
	const auto& Interest = GetViewInterest();
	const bool bCriticalPending = HasCriticalNearFieldPending();
	if (AdmissionPriorities.IsEmpty()) return;
	const auto& Settings = Module.GetViewSettings();
	const int32 Limits[] = {0, Settings.Budget.DataAdmissionPerFrame, Settings.Budget.DataAdmissionPerFrame, Settings.Budget.DataAdmissionPerFrame};
	int32 Submitted[4] = {};
	const int32 FirstKind = (LastActiveDataKind < 4 ? LastActiveDataKind % 3 : 0);
	LastActiveDataKind = FirstKind + 1;
	const double Deadline = TerrainDeadline(Settings.Budget.AdmissionMilliseconds);
	NextDataPriority %= AdmissionPriorities.Num();
	for (int32 PriorityAttempt = 0; PriorityAttempt < FMath::Min(16, AdmissionPriorities.Num()) && FPlatformTime::Seconds() < Deadline; ++PriorityAttempt)
	{
		const int32 Priority = AdmissionPriorities[NextDataPriority];
		NextDataPriority = (NextDataPriority + 1) % AdmissionPriorities.Num();
		for (int32 Offset = 0; Offset < 3 && FPlatformTime::Seconds() < Deadline; ++Offset)
		{
			const int32 Kind = (FirstKind + Offset) % 3 + 1;
			if (Limits[Kind] <= 0 || Submitted[Kind] >= Limits[Kind]) continue;
			const auto& Lane = Interest.AdmissionLanes[Kind];
			const auto LanePriority = [this](int32 Index) { return Admissions[Index].Priority; };
			const int32 First = Algo::LowerBoundBy(Lane, Priority, LanePriority);
			const int32 End = Algo::UpperBoundBy(Lane, Priority, LanePriority);
			if (First == End) continue;
			int32& Scan = DataScanIndices[Kind].FindOrAdd(Priority);
			if (Scan < First || Scan >= End) Scan = First;
			const double LaneDeadline = FPlatformTime::Seconds() + (Deadline - FPlatformTime::Seconds()) / (3 - Offset);
			for (int32 Attempt = 0; Attempt < FMath::Min(32, End - First) && Submitted[Kind] < Limits[Kind] && FPlatformTime::Seconds() < LaneDeadline; ++Attempt)
			{
				const auto& Admission = Admissions[Lane[Scan]];
				Scan = Scan + 1 < End ? Scan + 1 : First;
				if (bCriticalPending && !IsCriticalDependency(Admission))
					continue;
				if (!IsAdmissionDataReady(Admission, true) && !IsAdmissionTerminalFailure(Admission))
				{
					bool bSubmitted = false;
					switch (Admission.Kind)
					{
					case EVoxelViewAdmissionKind::VoxelProxy: bSubmitted = RequestVoxelProxy(Admission.ProxyKey, Priority, true); break;
					case EVoxelViewAdmissionKind::Surface: bSubmitted = RequestSurface(Admission.SurfaceKey, Priority, true); break;
					case EVoxelViewAdmissionKind::Macro: bSubmitted = RequestMacro(Admission.MacroKey, Priority, true); break;
					default: break;
					}
					Submitted[Kind] += bSubmitted ? 1 : 0;
				}
			}
		}
	}
}

bool FVoxelViewManager::IsPreparedDataCurrent(const FVoxelTaskKey& InKey) const
{
	if (InKey.Stamp.WorldEpoch != WorldEpoch) return false;
	const FVoxelChangeHierarchy& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	switch (InKey.Kind)
	{
	case EVoxelTaskKind::GenerateVoxelProxy:
		return VoxelProxyWanted.Contains(InKey.Stamp.ViewKey) && Hierarchy.GetVoxelProxyRevision(InKey.Stamp.ViewKey) == InKey.Stamp.Revision &&
			   GetViewInterest().FindGeometry(InKey.Stamp.ViewKey) &&
			   GetTypeHash(*GetViewInterest().FindGeometry(InKey.Stamp.ViewKey)) == InKey.Stamp.GeometryRevision;
	case EVoxelTaskKind::GenerateSurface:
		return SurfaceWanted.Contains(InKey.Stamp.SurfaceKey) && Hierarchy.GetSurfaceRevision(InKey.Stamp.SurfaceKey) == InKey.Stamp.Revision &&
			   GetViewInterest().FindGeometry(InKey.Stamp.SurfaceKey) &&
			   GetTypeHash(*GetViewInterest().FindGeometry(InKey.Stamp.SurfaceKey)) == InKey.Stamp.GeometryRevision;
	case EVoxelTaskKind::GenerateMacro:
		return MacroWanted.Contains(InKey.Stamp.MacroKey) && Hierarchy.GetMacroRevision(InKey.Stamp.MacroKey) == InKey.Stamp.Revision &&
			   GetViewInterest().FindGeometry(InKey.Stamp.MacroKey) &&
			   GetTypeHash(*GetViewInterest().FindGeometry(InKey.Stamp.MacroKey)) == InKey.Stamp.GeometryRevision;
	default:
		return false;
	}
}

void FVoxelViewManager::PrunePreparedData()
{
	for (auto It = PreparedEnvironmentRetentions.CreateIterator(); It; ++It)
		if (!IsPreparedDataCurrent(It.Key())) It.RemoveCurrent();
	for (auto It = PreparedData.CreateIterator(); It; ++It)
	{
		if (!IsPreparedDataCurrent(It.Key()))
		{
			PreparedDataBytes -= It.Value()->ResultBytes() + sizeof(FVoxelTaskResult) + sizeof(FVoxelTaskKey);
			It.RemoveCurrent();
		}
	}
	Scheduler.CancelMatching([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return (Kind == EVoxelTaskKind::GenerateVoxelProxy || Kind == EVoxelTaskKind::GenerateSurface ||
			Kind == EVoxelTaskKind::GenerateMacro) && !IsPreparedDataCurrent({ Kind, Stamp });
	});
}

TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> FVoxelViewManager::RetainPreparedEnvironment(
	const FVoxelTaskKey& InKey, const FVoxelGenerationPipeline& InGenerator,
	const FVoxelGenerationBounds& InBounds)
{
	auto& Retention = PreparedEnvironmentRetentions.FindOrAdd(InKey);
	if (!Retention) Retention = RetainEnvironmentData(InGenerator, InBounds);
	return Retention;
}

bool FVoxelViewManager::EnqueuePreparedData(FVoxelTaskRequest&& InRequest)
{
	if (Scheduler.Has(InRequest.Stamp, InRequest.Kind)) return false;
	const uint64 Limit = (InRequest.WorkClass == EVoxelWorkClass::Prefetch ? 128ull : 256ull) * 1024ull * 1024ull;
	const uint64 Reservation = InRequest.ReservedBytes + sizeof(FVoxelTaskResult) + sizeof(FVoxelTaskKey);
	if (PreparedDataBytes + PendingDataBytes + Reservation > Limit)
	{
		Scheduler.ObserveDemand(InRequest.Kind, InRequest.Stamp,
			Reservation > Limit ? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::Memory,
			InRequest.InputBytes, Reservation);
		return false;
	}
	InRequest.Apply = [this, Reservation](FVoxelTaskResult&& Result)
	{
		PendingDataBytes -= Reservation;
		const FVoxelTaskKey Key { Result.Kind, Result.Stamp };
		if (Result.bCanceled || Result.Error.StartsWith(TEXT("DependencyNotReady")) || !IsPreparedDataCurrent(Key)) return;
		if (!Result.bSuccess)
		{
			Scheduler.ObserveDemand(Result.Kind, Result.Stamp, EVoxelTaskBlockReason::TerminalFailure);
			UE_LOG(LogTemp, Warning, TEXT("Voxel data preparation failed: kind=%d error=%s"),
				static_cast<int32>(Result.Kind), *Result.Error);
			return;
		}
		const auto& Interest = GetViewInterest();
		const auto* Geometry = Result.VoxelProxy ? Interest.FindGeometry(Result.VoxelProxy->Key) :
			Result.Surface ? Interest.FindGeometry(Result.Surface->Key) :
			Result.Macro ? Interest.FindGeometry(Result.Macro->Key) : nullptr;
		if (!Geometry || (Result.VoxelProxy && *Geometry != Result.VoxelProxy->Geometry) ||
			(Result.Surface && *Geometry != Result.Surface->Geometry) || (Result.Macro && *Geometry != Result.Macro->Geometry)) return;
		RemovePreparedData(Key);
		if (!CoverageContinuation.IsValid() && Result.bKnownEmptyMesh && Result.VoxelProxy && !VoxelProxyActors.Contains(Result.Stamp.ViewKey))
		{
			Result.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.VoxelProxyMesh->ValidateAndCache();
			if (PublishVoxelProxy(Result))
			{
				++SkippedProxyMeshes;
				MarkCoverageDirty(VoxelProxyCoverageRect(Result.Stamp.ViewKey));
				return;
			}
		}
		PreparedDataBytes += Result.ResultBytes() + sizeof(FVoxelTaskResult) + sizeof(FVoxelTaskKey);
		PreparedData.Add(Key, MakeShared<FVoxelTaskResult, ESPMode::ThreadSafe>(MoveTemp(Result)));
	};
	const FVoxelTaskKey RetentionKey{InRequest.Kind, InRequest.Stamp};
	if (!EnqueueViewTask(MoveTemp(InRequest)))
		return false;
	PreparedEnvironmentRetentions.Remove(RetentionKey);
	PendingDataBytes += Reservation;
	return true;
}

void FVoxelViewManager::RemovePreparedData(const FVoxelTaskKey& InKey)
{
	if (const TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe>* Data = PreparedData.Find(InKey))
	{
		PreparedDataBytes -= (*Data)->ResultBytes() + sizeof(FVoxelTaskResult) + sizeof(FVoxelTaskKey);
		PreparedData.Remove(InKey);
	}
}

int32 FVoxelViewManager::VoxelProxyTerrainStage(const FVoxelViewKey& InKey) const
{
	// Fine 交接依赖的粗体素属于同一阶段，避免 Fine 等待被自身阶段门禁挡住的依赖。
	return GetViewInterest().TerrainPlan.FineDependencies.Contains(InKey) ? 0 : 1;
}

int32 FVoxelViewManager::AdmissionTerrainStage(const FVoxelViewAdmission& InAdmission) const
{
	return InAdmission.Kind == EVoxelViewAdmissionKind::VoxelProxy
		? VoxelProxyTerrainStage(InAdmission.ProxyKey) : static_cast<int32>(InAdmission.Kind);
}

EVoxelWorkClass FVoxelViewManager::VolumeTransitionWorkClass(const FVoxelViewKey& InOwner) const
{
	const auto Context = DesiredVolumeContexts.FindRef(InOwner);
	if (Context && Context->Patches.ContainsByPredicate(
					   [this](const FVoxelBoundaryTransitionPatch& Patch)
					   {
						   const auto* Demand = Patch.Face.Neighbor.Level == 0 ? GetViewInterest().Exact.Find(Patch.Face.Neighbor.Coordinate) : nullptr;
						   return Demand && (Demand->bWarmupData || Demand->bWarmupCollision || Demand->bMovementCriticalFine ||
												Demand->bMovementCriticalCollision || Demand->bMovementCriticalData);
					   }))
		return EVoxelWorkClass::Critical;
	return VoxelProxyTerrainStage(InOwner) == 0 || (Context && Context->Patches.ContainsByPredicate([](const FVoxelBoundaryTransitionPatch& Patch)
	{
		return Patch.Face.Neighbor.Level == 0;
	})) ? EVoxelWorkClass::Interactive : EVoxelWorkClass::Boundary;
}

void FVoxelViewManager::UpdateTaskPriorities()
{
	Scheduler.UpdatePriorities([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, int32& Priority,
		double& Distance, double& Forward)
	{
		switch (Kind)
		{
		case EVoxelTaskKind::BuildFineMesh:
			if (Stamp.Section == FIntVector(MIN_int32, 1, 0))
			{
				const FVoxelFinePageKey Page{Stamp.ViewKey.Coordinate, Stamp.ViewKey.Level};
				const auto& Latest = Module.GetCurrentInterest();
				WorkClass = Latest.CriticalFinePages.Contains(Page) ? EVoxelWorkClass::Critical : EVoxelWorkClass::Interactive;
				const int32* SourcePriority = Latest.FinePagePriorities.Find(Page);
				Priority = SourcePriority ? *SourcePriority : MAX_int32;
				Distance = MinimumObserverDistanceCells(Page.GetBounds());
			}
			else if (const FVoxelExactDemand* Demand = Module.GetCurrentInterest().Exact.Find(Stamp.Section))
			{
				WorkClass = Demand->bWarmupData || Demand->bWarmupCollision || Demand->bMovementCriticalData || Demand->bMovementCriticalCollision ||
									Demand->bMovementCriticalFine
								? EVoxelWorkClass::Critical
								: EVoxelWorkClass::Interactive;
				Priority = Demand->Priority;
				Distance = Demand->DistanceCells;
				Forward = Demand->ForwardScore;
			}
			break;
		case EVoxelTaskKind::BuildVolumeTransition:
			WorkClass = VolumeTransitionWorkClass(Stamp.ViewKey);
			if (const auto* Source = Module.GetCurrentInterest().VoxelProxyPriorities.Find(Stamp.ViewKey)) Priority = *Source;
			Distance = MinimumObserverDistanceCells(Stamp.ViewKey.GetBounds());
			break;
		case EVoxelTaskKind::BuildVoxelProxy:
		case EVoxelTaskKind::GenerateVoxelProxy:
			if (const int32* SourcePriority = Module.GetCurrentInterest().VoxelProxyPriorities.Find(Stamp.ViewKey))
				Priority = *SourcePriority;
			if (Kind == EVoxelTaskKind::GenerateVoxelProxy)
				WorkClass = EVoxelWorkClass::Prefetch;
			else WorkClass = VoxelProxyTerrainStage(Stamp.ViewKey) == 0
				? EVoxelWorkClass::Interactive : EVoxelWorkClass::Boundary;
			if (IsCriticalDependency({EVoxelTerrainRepresentation::VoxelProxy, Stamp.ViewKey.Coordinate, Stamp.ViewKey.Level}))
				WorkClass = EVoxelWorkClass::Critical;
			Distance = VoxelProxyTerrainStage(Stamp.ViewKey) == 0
				? 0.0 : MinimumObserverDistanceCells(Stamp.ViewKey.GetBounds());
			break;
		case EVoxelTaskKind::BuildSurface:
		case EVoxelTaskKind::GenerateSurface:
			if (const int32* SourcePriority = Module.GetCurrentInterest().SurfacePriorities.Find(Stamp.SurfaceKey))
				Priority = *SourcePriority;
			if (Kind == EVoxelTaskKind::GenerateSurface)
				WorkClass = EVoxelWorkClass::Prefetch;
			WorkClass = IsCriticalDependency({EVoxelTerrainRepresentation::Surface, FIntVector(Stamp.SurfaceKey.Coordinate.X, Stamp.SurfaceKey.Coordinate.Y, 0),
							Stamp.SurfaceKey.Level})
							? EVoxelWorkClass::Critical
							: (Kind == EVoxelTaskKind::GenerateSurface ? EVoxelWorkClass::Prefetch : EVoxelWorkClass::Visible);
			Distance = MinimumObserverDistanceCells(SurfaceWorldCenter(Stamp.SurfaceKey));
			break;
		case EVoxelTaskKind::BuildMacro:
		case EVoxelTaskKind::GenerateMacro:
			if (const int32* SourcePriority = Module.GetCurrentInterest().MacroPriorities.Find(Stamp.MacroKey))
				Priority = *SourcePriority;
			if (Kind == EVoxelTaskKind::GenerateMacro)
				WorkClass = EVoxelWorkClass::Prefetch;
			WorkClass = IsCriticalDependency(
							{EVoxelTerrainRepresentation::Macro, FIntVector(Stamp.MacroKey.Coordinate.X, Stamp.MacroKey.Coordinate.Y, 0), Stamp.MacroKey.Level})
							? EVoxelWorkClass::Critical
							: (Kind == EVoxelTaskKind::GenerateMacro ? EVoxelWorkClass::Prefetch : EVoxelWorkClass::Visible);
			Distance = MinimumObserverDistanceCells(MacroWorldCenter(Stamp.MacroKey));
			break;
		case EVoxelTaskKind::BuildViewTransition:
			if (static_cast<EVoxelHeightfieldRepresentation>(Stamp.Section.Z) == EVoxelHeightfieldRepresentation::Surface)
			{
				if (const auto* Source = Module.GetCurrentInterest().SurfacePriorities.Find(
					{FIntPoint(Stamp.Section.X, Stamp.Section.Y), Stamp.ViewKey.Level})) Priority = *Source;
			}
			else if (const auto* Source = Module.GetCurrentInterest().MacroPriorities.Find(
				{FIntPoint(Stamp.Section.X, Stamp.Section.Y), Stamp.ViewKey.Level})) Priority = *Source;
			Distance = static_cast<EVoxelHeightfieldRepresentation>(Stamp.Section.Z) == EVoxelHeightfieldRepresentation::Surface
				? MinimumObserverDistanceCells(SurfaceWorldCenter({FIntPoint(Stamp.Section.X, Stamp.Section.Y), Stamp.ViewKey.Level}))
				: MinimumObserverDistanceCells(MacroWorldCenter({FIntPoint(Stamp.Section.X, Stamp.Section.Y), Stamp.ViewKey.Level}));
			break;
		default:
			break;
		}
	});
	Scheduler.UpdateTerrainStages(
		[this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp, int32& Stage)
		{
			switch (Kind)
			{
			case EVoxelTaskKind::BuildVoxelProxy:
				Stage = IsCriticalDependency({EVoxelTerrainRepresentation::VoxelProxy, Stamp.ViewKey.Coordinate, Stamp.ViewKey.Level})
							? 0
							: VoxelProxyTerrainStage(Stamp.ViewKey);
				break;
			case EVoxelTaskKind::BuildSurface:
				Stage = IsCriticalDependency({EVoxelTerrainRepresentation::Surface, FIntVector(Stamp.SurfaceKey.Coordinate.X, Stamp.SurfaceKey.Coordinate.Y, 0),
							Stamp.SurfaceKey.Level})
							? 0
							: 2;
				break;
			case EVoxelTaskKind::BuildMacro:
				Stage = IsCriticalDependency(
							{EVoxelTerrainRepresentation::Macro, FIntVector(Stamp.MacroKey.Coordinate.X, Stamp.MacroKey.Coordinate.Y, 0), Stamp.MacroKey.Level})
							? 0
							: 3;
				break;
			default:
				break;
			}
		});
}

void FVoxelViewManager::ProcessAdmissions()
{
	const auto& Interest = GetViewInterest();
	const bool bCriticalPending = HasCriticalNearFieldPending();
	if (AdmissionPriorities.IsEmpty()) return;
	const auto& Settings = Module.GetViewSettings();
	const int32 Limits[] = {Settings.Budget.FineAdmissionPerFrame, Settings.Budget.VoxelProxyAdmissionPerFrame, Settings.Budget.SurfaceAdmissionPerFrame, Settings.Budget.MacroAdmissionPerFrame};
	int32 Submitted[4] = {};
	const int32 FirstKind = (LastActiveAdmissionKind < 4 ? (LastActiveAdmissionKind + 1) % 4 : 0);
	LastActiveAdmissionKind = FirstKind;
	const double Deadline = TerrainDeadline(Settings.Budget.AdmissionMilliseconds);
	NextMeshPriority %= AdmissionPriorities.Num();
	for (int32 PriorityAttempt = 0; PriorityAttempt < FMath::Min(16, AdmissionPriorities.Num()) && FPlatformTime::Seconds() < Deadline; ++PriorityAttempt)
	{
		const int32 Priority = AdmissionPriorities[NextMeshPriority];
		NextMeshPriority = (NextMeshPriority + 1) % AdmissionPriorities.Num();
		for (int32 Offset = 0; Offset < 4 && FPlatformTime::Seconds() < Deadline; ++Offset)
		{
			const int32 Kind = (FirstKind + Offset) % 4;
			if (Limits[Kind] <= 0 || Submitted[Kind] >= Limits[Kind]) continue;
			const auto& Lane = Interest.AdmissionLanes[Kind];
			const auto LanePriority = [this](int32 Index) { return Admissions[Index].Priority; };
			const int32 First = Algo::LowerBoundBy(Lane, Priority, LanePriority);
			const int32 End = Algo::UpperBoundBy(Lane, Priority, LanePriority);
			if (First == End) continue;
			int32& Scan = AdmissionScanIndices[Kind].FindOrAdd(Priority);
			if (Scan < First || Scan >= End) Scan = First;
			const double LaneDeadline = FPlatformTime::Seconds() + (Deadline - FPlatformTime::Seconds()) / (4 - Offset);
			for (int32 Attempt = 0; Attempt < FMath::Min(32, End - First) && Submitted[Kind] < Limits[Kind] && FPlatformTime::Seconds() < LaneDeadline; ++Attempt)
			{
				const auto& Admission = Admissions[Lane[Scan]];
				Scan = Scan + 1 < End ? Scan + 1 : First;
				if (bCriticalPending && !IsCriticalDependency(Admission))
					continue;
				if (!IsAdmissionMeshReady(Admission) && !IsAdmissionTerminalFailure(Admission))
				{
					Submitted[Kind] += TrySubmitAdmission(Admission) ? 1 : 0;
				}
			}
		}
	}
}

double FVoxelViewManager::MinimumObserverDistanceCells(const FVector& InWorldCenter) const
{
	double BestSquared = TNumericLimits<double>::Max();
	for (const FVector& Observer : PriorityObservers)
	{
		BestSquared = FMath::Min(BestSquared, FVector::DistSquared(InWorldCenter, Observer));
	}
	return PriorityObservers.IsEmpty() ? 0.0 : FMath::Sqrt(BestSquared) / FMath::Max(1.0, Module.BlockSize());
}

FVector FVoxelViewManager::SurfaceWorldCenter(const FVoxelSurfaceTileKey& InKey) const
{
	const FVoxelCoverageRect Bounds = SurfaceCoverageRect(InKey);
	return FVector(FVector2D(Bounds.Min + Bounds.Max) * (0.5 * Module.BlockSize()),
		PriorityObservers.IsEmpty() ? 0.0 : PriorityObservers[0].Z);
}

double FVoxelViewManager::MinimumObserverDistanceCells(const FVoxelGenerationBounds& InCellBounds) const
{
	const FBox Bounds(FVector(InCellBounds.Min), FVector(InCellBounds.Max));
	double BestSquared = MAX_dbl;
	for (const FVector& Observer : PriorityObservers)
	{
		BestSquared = FMath::Min(BestSquared, Bounds.ComputeSquaredDistanceToPoint(Observer / FMath::Max(1.0, Module.BlockSize())));
	}
	return PriorityObservers.IsEmpty() ? 0.0 : FMath::Sqrt(BestSquared);
}

FVector FVoxelViewManager::MacroWorldCenter(const FVoxelMacroTileKey& InKey) const
{
	const FVoxelCoverageRect Bounds = MacroCoverageRect(InKey);
	return FVector(FVector2D(Bounds.Min + Bounds.Max) * (0.5 * Module.BlockSize()),
		PriorityObservers.IsEmpty() ? 0.0 : PriorityObservers[0].Z);
}

void FVoxelViewManager::RebuildAdmissions(TConstArrayView<FVector> InObservers)
{
	Admissions = GetViewInterest().Admissions;
	AdmissionPriorities = GetViewInterest().AdmissionPriorities;
}

void FVoxelViewManager::TrackReadyTerrainNode(FVoxelViewKey InKey)
{
	if (bReadyTerrainBranchesDirty) return;
	while (InKey.Level < 24)
	{
		bool bAlreadyPresent = false;
		ReadyTerrainBranches.Add(InKey, &bAlreadyPresent);
		if (bAlreadyPresent) break;
		InKey = InKey.GetParent();
	}
}

void FVoxelViewManager::RebuildReadyTerrainBranches()
{
	ReadyTerrainBranches.Reset();
	bReadyTerrainBranchesDirty = false;
	for (const FIntVector& Key : FineReady) TrackReadyTerrainNode({ Key, 0 });
	for (const FVoxelViewKey& Key : VoxelProxyReady) TrackReadyTerrainNode(Key);
}

void FVoxelViewManager::SortAdmissionsByPriority(TArray<FVoxelViewAdmission>& InOutAdmissions)

{
	InOutAdmissions.Sort([](const FVoxelViewAdmission& A, const FVoxelViewAdmission& B)
	{
		if (A.Priority != B.Priority) return A.Priority < B.Priority;
		return A.DistanceCells != B.DistanceCells ? A.DistanceCells < B.DistanceCells :
			static_cast<uint8>(A.Kind) < static_cast<uint8>(B.Kind);
	});
}

double FVoxelViewManager::ResolveAdmissionFrontier(
	const TConstArrayView<FVoxelViewAdmission> InAdmissions,
	const TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
	const double InBandWidthCells)
{
	for (const FVoxelViewAdmission& Admission : InAdmissions)
	{
		if (!InIsReady(Admission))
		{
			return Admission.DistanceCells + InBandWidthCells;
		}
	}
	return TNumericLimits<double>::Max();
}

bool FVoxelViewManager::IsAdmissionMeshReady(const FVoxelViewAdmission& A) const
{
	switch (A.Kind)
	{
	case EVoxelViewAdmissionKind::Fine:
		return FineReady.Contains(A.FineKey);
	case EVoxelViewAdmissionKind::VoxelProxy:
		{
		const auto Data = VoxelProxyData.FindRef(A.ProxyKey);
		const auto* Geometry = GetViewInterest().FindGeometry(A.ProxyKey);
		return VoxelProxyReady.Contains(A.ProxyKey) && Data && Geometry && Data->Geometry == *Geometry;
		}
	case EVoxelViewAdmissionKind::Surface:
		{
		const auto Data = SurfaceData.FindRef(A.SurfaceKey);
		const auto* Geometry = GetViewInterest().FindGeometry(A.SurfaceKey);
		return SurfaceReady.Contains(A.SurfaceKey) && Data && Geometry && Data->Geometry == *Geometry;
		}
	case EVoxelViewAdmissionKind::Macro:
		{
		const auto Data = MacroData.FindRef(A.MacroKey);
		const auto* Geometry = GetViewInterest().FindGeometry(A.MacroKey);
		return MacroReady.Contains(A.MacroKey) && Data && Geometry && Data->Geometry == *Geometry;
		}
	default:
		return true;
	}
}

bool FVoxelViewManager::IsAdmissionPresented(const FVoxelViewAdmission& InAdmission) const
{
	if (!IsAdmissionMeshReady(InAdmission)) return false;
	AActor* Actor = nullptr;
	switch (InAdmission.Kind)
	{
	case EVoxelViewAdmissionKind::Fine:
		if (!VisibleTerrainNodes.Contains({InAdmission.FineKey, 0})) return false;
		return IsFinePresented(InAdmission.FineKey);
	case EVoxelViewAdmissionKind::VoxelProxy:
		if (!VisibleTerrainNodes.Contains(InAdmission.ProxyKey)) return false;
		Actor = VoxelProxyActors.FindRef(InAdmission.ProxyKey);
		break;
	case EVoxelViewAdmissionKind::Surface:
		Actor = SurfaceActors.FindRef(InAdmission.SurfaceKey);
		break;
	case EVoxelViewAdmissionKind::Macro:
		Actor = MacroActors.FindRef(InAdmission.MacroKey);
		break;
	default:
		return true;
	}
	return !Actor || Publisher->IsCommitted(Actor);
}

void FVoxelViewManager::RefreshTerrainBuildStage()
{
	if (!bTerrainBuildStageDirty) return;
	bTerrainBuildStageDirty = false;
	int32 Stage = 3;
	for (const FVoxelViewAdmission& Admission : GetViewInterest().Admissions)
	{
		const int32 Kind = static_cast<uint8>(Admission.Kind);
		// Fine 保持可玩呈现门禁；粗地形按网格就绪推进构建，呈现交接由发布分组保证。
		// 等待所有 Proxy 呈现会阻止其退役所需的 Surface 构建，形成跨表示循环依赖。
		bool bReady = IsAdmissionMeshReady(Admission);
		if (Admission.Kind == EVoxelViewAdmissionKind::Fine)
		{
			bReady = IsAdmissionPresented(Admission);
			// 已有可见 Page 在编辑/重排期间保留旧呈现。新 Fine 网格就绪后，
			// 不能再用等待新 commit 的状态阻挡它所依赖的粗表示交接构建。
			if (!bReady && FineReady.Contains(Admission.FineKey))
			{
				if (const FVoxelFinePageKey* Owner = PresentedFineSectionOwners.Find(Admission.FineKey))
				{
					AActor* Actor = FinePageActors.FindRef(*Owner);
					bReady = Actor && !Actor->IsHidden() && Publisher->HasPresentation(Actor);
				}
			}
		}
		if (Kind < Stage && !bReady) Stage = Kind;
		if (Stage == 0) break;
	}
	if (Stage != Scheduler.GetObservedTerrainStage())
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel terrain mesh stage: %d -> %d interest=%llu"),
			Scheduler.GetObservedTerrainStage(), Stage, Module.GetInterestRevision());
		Scheduler.SetObservedTerrainStage(Stage);
	}
}



bool FVoxelViewManager::IsAdmissionTerminalFailure(const FVoxelViewAdmission& A) const
{
	if (A.Kind == EVoxelViewAdmissionKind::Fine)
	{
		const auto* Section = Module.GetRuntime()->FindSection(A.FineKey);
		const auto* Page = FinePagePlan.SectionOwners.Find(A.FineKey);
		return (Section && Section->Status == EVoxelSectionStatus::Failed) || (Page && FinePageFailures.Contains(*Page));
	}
	FVoxelTaskStamp Stamp;
	Stamp.WorldEpoch = WorldEpoch;
	const auto& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	EVoxelTaskKind DataKind, MeshKind;
	const FVoxelTerrainGeometry* Geometry = nullptr;
	if (A.Kind == EVoxelViewAdmissionKind::VoxelProxy)
	{
		Stamp.ViewKey = A.ProxyKey;
		Stamp.Revision = Hierarchy.GetVoxelProxyRevision(A.ProxyKey);
		Stamp.Token = HashCombineFast(GetTypeHash(A.ProxyKey), GetTypeHash(Stamp.Revision));
		Geometry = GetViewInterest().FindGeometry(A.ProxyKey);
		DataKind = EVoxelTaskKind::GenerateVoxelProxy; MeshKind = EVoxelTaskKind::BuildVoxelProxy;
	}
	else if (A.Kind == EVoxelViewAdmissionKind::Surface)
	{
		Stamp.SurfaceKey = A.SurfaceKey;
		Stamp.Revision = Hierarchy.GetSurfaceRevision(A.SurfaceKey);
		Stamp.Token = HashCombineFast(GetTypeHash(A.SurfaceKey), GetTypeHash(Stamp.Revision));
		Geometry = GetViewInterest().FindGeometry(A.SurfaceKey);
		DataKind = EVoxelTaskKind::GenerateSurface; MeshKind = EVoxelTaskKind::BuildSurface;
	}
	else
	{
		Stamp.MacroKey = A.MacroKey;
		Stamp.Revision = Hierarchy.GetMacroRevision(A.MacroKey);
		Stamp.Token = HashCombineFast(GetTypeHash(A.MacroKey), GetTypeHash(Stamp.Revision));
		Geometry = GetViewInterest().FindGeometry(A.MacroKey);
		DataKind = EVoxelTaskKind::GenerateMacro; MeshKind = EVoxelTaskKind::BuildMacro;
	}
	if (Geometry) Stamp.GeometryRevision = GetTypeHash(*Geometry);
	return Scheduler.IsTerminal({DataKind, Stamp}) || Scheduler.IsTerminal({MeshKind, Stamp});
}

TMap<int32, double> FVoxelViewManager::GetDataAdmissionLimits() const
{
	TMap<int32, double> Limits;
	for (const FVoxelViewAdmission& Admission : GetViewInterest().Admissions)
	{
		if (Admission.Kind == EVoxelViewAdmissionKind::Fine && !Limits.Contains(Admission.Priority) &&
			!IsAdmissionDataReady(Admission) && !IsAdmissionTerminalFailure(Admission))
			Limits.Add(Admission.Priority, Admission.DistanceCells + AdmissionBandWidthCells + ViewSectionSide * 2.0);
	}
	return Limits;
}

void FVoxelViewManager::RefreshTaskPriorities()
{
	if (GetViewInterest().PublicationInputs)
	{
		bTaskPrioritiesDirty = true;
		bTerrainBuildStageDirty = true;
		return;
	}
	// Standalone fixtures may build a Fine plan without an accepted publication snapshot.
	// Source movement can change presentation bands without changing logical demand.
	RebuildFinePagePlan();
	bTerrainBuildStageDirty = true;
	RefreshTerrainBuildStage();
	RebuildAdmissions(PriorityObservers);
	UpdateTaskPriorities();
}

TMap<int32, uint8> FVoxelViewManager::ResolveAdmissionLanes(
	const TConstArrayView<FVoxelViewAdmission> InAdmissions,
	const TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
	const bool bMovementCriticalFinePending)
{
	TMap<int32, uint8> Lanes;
	for (const FVoxelViewAdmission& Admission : InAdmissions)
	{
		if (InIsReady(Admission)) continue;
		if (bMovementCriticalFinePending && Admission.Kind != EVoxelViewAdmissionKind::Fine) continue;
		Lanes.FindOrAdd(Admission.Priority) |= 1 << static_cast<uint8>(Admission.Kind);
	}
	return Lanes;
}

bool FVoxelViewManager::HasMovementCriticalFinePending() const
{
	for (const auto& Pair : GetViewInterest().Exact)
	{
		if (Pair.Value.bMovementCriticalFine && !FineReady.Contains(Pair.Key))
		{
			const FVoxelSection* Section = Module.GetRuntime() ? Module.GetRuntime()->FindSection(Pair.Key) : nullptr;
			if (!Section || Section->Status != EVoxelSectionStatus::Failed) return true;
		}
	}
	return false;
}

int32 FVoxelViewManager::ResolveActiveAdmissionKind(
	const TConstArrayView<FVoxelViewAdmission> InAdmissions,
	const TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
	int32* OutPriority)
{
	int32 ActiveKind = 4;
	int32 ActivePriority = MAX_int32;
	for (const FVoxelViewAdmission& Admission : InAdmissions)
	{
		if (!InIsReady(Admission))
		{
			if (Admission.Priority < ActivePriority)
			{
				ActivePriority = Admission.Priority;
				ActiveKind = static_cast<int32>(Admission.Kind);
			}
			else if (Admission.Priority == ActivePriority)
			{
				ActiveKind = FMath::Min(ActiveKind, static_cast<int32>(Admission.Kind));
			}
		}
	}
	if (OutPriority) *OutPriority = ActivePriority;
	return ActiveKind;
}

bool FVoxelViewManager::TrySubmitAdmission(const FVoxelViewAdmission& A)
{
	switch (A.Kind)
	{
	case EVoxelViewAdmissionKind::Fine:
		if (const FVoxelSection* Section = Module.GetRuntime()->FindSection(A.FineKey);
			Section && Section->Status == EVoxelSectionStatus::DataReady)
		{
			const uint64* Revision = FineRevisions.Find(A.FineKey);
			if (FineReady.Contains(A.FineKey) && Revision && *Revision == Section->CommittedRevision)
			{
				return false;
			}
			if (TryResolveFineWithoutMesh(A.FineKey, Section->CommittedRevision))
			{
				return false;
			}
			return RequestFine(A.FineKey, Section->CommittedRevision);
		}
		return false;
	case EVoxelViewAdmissionKind::VoxelProxy:
		if (IsAdmissionMeshReady(A)) return false;
		return RequestVoxelProxy(A.ProxyKey, A.Priority);
	case EVoxelViewAdmissionKind::Surface:
		if (IsAdmissionMeshReady(A)) return false;
		return RequestSurface(A.SurfaceKey, A.Priority);
	case EVoxelViewAdmissionKind::Macro:
		if (IsAdmissionMeshReady(A)) return false;
		return RequestMacro(A.MacroKey, A.Priority);
	default: return false;
	}
}

void FVoxelViewManager::CancelStaleViewTasks()
{
	PrunePreparedData();
	Scheduler.CancelMatching([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		switch (Kind)
		{
		case EVoxelTaskKind::BuildFineMesh:
			if (Stamp.Section.X == MIN_int32 && Stamp.Section.Y == 1) return !FinePagePlan.Pages.Contains({Stamp.ViewKey.Coordinate, Stamp.ViewKey.Level});
			return !FineWanted.Contains(Stamp.Section);
		case EVoxelTaskKind::BuildVoxelProxy:
			return !VoxelProxyWanted.Contains(Stamp.ViewKey) || !GetViewInterest().FindGeometry(Stamp.ViewKey) ||
				   GetTypeHash(*GetViewInterest().FindGeometry(Stamp.ViewKey)) != Stamp.GeometryRevision;
		case EVoxelTaskKind::BuildSurface:
		case EVoxelTaskKind::BuildWater:
			return !SurfaceWanted.Contains(Stamp.SurfaceKey) || !GetViewInterest().FindGeometry(Stamp.SurfaceKey) ||
				   GetTypeHash(*GetViewInterest().FindGeometry(Stamp.SurfaceKey)) != Stamp.GeometryRevision;
		case EVoxelTaskKind::BuildMacro:
			return !MacroWanted.Contains(Stamp.MacroKey) || !GetViewInterest().FindGeometry(Stamp.MacroKey) ||
				   GetTypeHash(*GetViewInterest().FindGeometry(Stamp.MacroKey)) != Stamp.GeometryRevision;
		default: return false;
		}
	});
}

void FVoxelViewManager::LogRepresentationState(const TConstArrayView<FVector> InObservers)
{
	auto CountVisible = [this](const auto& InActors)
	{
		int32 Count = 0;
		for (const auto& Pair : InActors)
		{
			if (!Pair.Value)
			{
				continue;
			}
			const bool bVisible = !Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value);
			Count += bVisible ? 1 : 0;
		}
		return Count;
	};

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Voxel representations: Fine W/R/A/V=%d/%d/%d/%d Proxy=%d/%d/%d/%d Surface=%d/%d/%d/%d Macro=%d/%d/%d/%d"),
		FineWanted.Num(),
		FineReady.Num(),
		FinePageActors.Num(),
		CountVisible(FinePageActors),
		VoxelProxyWanted.Num(),
		VoxelProxyReady.Num(),
		VoxelProxyActors.Num(),
		CountVisible(VoxelProxyActors),
		SurfaceWanted.Num(),
		SurfaceReady.Num(),
		SurfaceActors.Num(),
		CountVisible(SurfaceActors),
		MacroWanted.Num(),
		MacroReady.Num(),
		MacroActors.Num(),
		CountVisible(MacroActors));
	int32 PageLevels[5] = {};
	for (const FVoxelFinePageKey& Page : FinePagePlan.Pages) ++PageLevels[FMath::Min<int32>(Page.Level, 4)];
	UE_LOG(LogTemp, Display, TEXT("Voxel Fine pages: F0/F1/F2/F3/F4=%d/%d/%d/%d/%d sectionMeshes=%d dirty=%d pending=%d presentedSections=%d layout=%llu terrainCandidates=%llu pageCandidates=%llu snapshots=%llu heightfieldRebuilds=%llu fullActorScans=%llu"),
		PageLevels[0], PageLevels[1], PageLevels[2], PageLevels[3], PageLevels[4], FineSectionMeshes.Num(),
		DirtyFinePages.Num(), PendingFinePages.Num(), PresentedFineSectionOwners.Num(), FinePageLayoutRevision,
		TerrainCandidateCount, FinePageCandidateCount, CoverageSnapshotBuildCount, HeightfieldTopologyRebuildCount, FullTerrainActorScanCount);
	int32 FineMissingData = 0;
	int32 FineFailed = 0;
	int32 FineWaitingNeighbors = 0;
	int32 FineMeshQueued = 0;
	int32 FineEligible = 0;
	int32 FineStaleRevision = 0;
	int32 FineAwaitingPublish = 0;
	for (const FIntVector& Key : FineWanted)
	{
		if (FineReady.Contains(Key))
		{
			if (AActor* Actor = FineActor(Key);
				Actor && !Publisher->IsCommitted(Actor)) ++FineAwaitingPublish;
			continue;
		}
		const FVoxelSection* Section = Module.GetRuntime()->FindSection(Key);
		if (Section && Section->Status == EVoxelSectionStatus::Failed)
		{
			++FineFailed;
			continue;
		}
		if (!Section || Section->Status != EVoxelSectionStatus::DataReady)
		{
			++FineMissingData;
			continue;
		}
		if (FineRevisions.FindRef(Key) == Section->CommittedRevision &&
			FineRevisions.Contains(Key))
		{
			++FineStaleRevision;
			continue;
		}
		bool bWaitingNeighbors = false;
		for (int32 Face = 0; Face < 6; ++Face)
		{
			FIntVector Neighbor = Key;
			Neighbor[Face / 2] += (Face & 1) ? -1 : 1;
			if (!GetViewInterest().Exact.Contains(Neighbor))
				continue;
			const FVoxelSection* Data = Module.GetRuntime()->FindSection(Neighbor);
			if (!Data || Data->Status != EVoxelSectionStatus::DataReady)
			{
				if (FineWaitingNeighbors < 4)
				{
					const FVoxelExactDemand& Demand = GetViewInterest().Exact.FindChecked(Neighbor);
					UE_LOG(LogTemp, Warning, TEXT("Voxel fine dependency: owner=%s neighbor=%s status=%d data=%d priority=%d distance=%.1f"),
						*Key.ToString(), *Neighbor.ToString(), Data ? static_cast<int32>(Data->Status) : -1,
						Demand.bData, Demand.Priority, Demand.DistanceCells);
				}
				bWaitingNeighbors = true;
			}
		}
		if (bWaitingNeighbors)
		{
			++FineWaitingNeighbors;
			continue;
		}
		FVoxelTaskStamp Stamp;
		Stamp.WorldEpoch = WorldEpoch;
		Stamp.Token = Section->Stamp.Token;
		Stamp.Revision = Section->CommittedRevision;
		Stamp.Section = Key;
		if (Scheduler.Has(Stamp, EVoxelTaskKind::BuildFineMesh)) ++FineMeshQueued;
		else ++FineEligible;
	}
	UE_LOG(LogTemp, Display,
		TEXT("Voxel admission stage=%d fineNotReady data=%d failed=%d neighbors=%d meshQueued=%d eligible=%d staleRevision=%d publishPending=%d"),
		LastActiveAdmissionKind, FineMissingData, FineFailed,
		FineWaitingNeighbors, FineMeshQueued, FineEligible, FineStaleRevision,
		FineAwaitingPublish);
	int32 PreparedProxyCount = 0;
	int32 PreparedSurfaceCount = 0;
	int32 PreparedMacroCount = 0;
	for (const auto& Pair : PreparedData)
	{
		PreparedProxyCount += Pair.Key.Kind == EVoxelTaskKind::GenerateVoxelProxy;
		PreparedSurfaceCount += Pair.Key.Kind == EVoxelTaskKind::GenerateSurface;
		PreparedMacroCount += Pair.Key.Kind == EVoxelTaskKind::GenerateMacro;
	}
	UE_LOG(LogTemp, Display, TEXT("Voxel prepared data: Proxy=%d Surface=%d Macro=%d cachedMiB=%.2f reservedMiB=%.2f dataStage=%d meshStage=%d skippedEmptyProxy=%llu"),
		PreparedProxyCount, PreparedSurfaceCount, PreparedMacroCount,
		PreparedDataBytes / 1048576.0, PendingDataBytes / 1048576.0, LastActiveDataKind, LastActiveAdmissionKind, SkippedProxyMeshes);

	UE_LOG(LogTemp, Display, TEXT("Voxel transition dependencies: missingFine=%d missingProxy=%d invalidContext=%d skippedEmptyMeshes=%llu"),
		MissingFineBoundaryCount, MissingProxyBoundaryCount, InvalidVolumeContextCount, SkippedVolumeMeshes);
	int32 ProxyLevels[6] = {};
	for (const FVoxelViewKey& Key : VoxelProxyWanted)
		if (Key.Level < UE_ARRAY_COUNT(ProxyLevels)) ++ProxyLevels[Key.Level];
	UE_LOG(LogTemp, Display, TEXT("Voxel proxy tiling: wanted L1/L2/L3/L4/L5=%d/%d/%d/%d/%d sides=32/64/128/256/512 sampleSteps=%d/%d/%d/%d/%d"),
		ProxyLevels[1], ProxyLevels[2], ProxyLevels[3], ProxyLevels[4], ProxyLevels[5],
		FVoxelViewKey{{}, 1}.GetSampleStep(), FVoxelViewKey{{}, 2}.GetSampleStep(),
		FVoxelViewKey{{}, 3}.GetSampleStep(), FVoxelViewKey{{}, 4}.GetSampleStep(),
		FVoxelViewKey{{}, 5}.GetSampleStep());
	int32 PendingVolumePlans = 0;
	for (const auto& Pair : VolumeCoveragePlans) PendingVolumePlans += Pair.Value.PendingSignature != 0 ? 1 : 0;
	UE_LOG(LogTemp, Display, TEXT("Voxel handoff waits: volumePlans=%d pendingPlans=%d volumeRects=%d heightfieldPlans=%d pendingHeightfields=%d heightfieldRects=%d preparedVolume=%d preparedHeightfield=%d publisherBusy=%d"),
		VolumeCoveragePlans.Num(), PendingVolumePlans, PendingVolumeHandoffBoxes.Num(), HeightfieldCoveragePlans.Num(),
		PendingHeightfieldPlanOwners.Num(), PendingHeightfieldHandoffRects.Num(), PreparedVolumeSignatures.Num(),
		PreparedTransitionSignatures.Num(), Publisher->IsBusy() ? 1 : 0);
	const FVoxelTaskDiagnostics SchedulerStats = Scheduler.GetDiagnostics();
	const FVoxelTerrainViewPlan& Plan = GetViewInterest().TerrainPlan;
	int32 PlannedLevels[6] = {}, ReadyLevels[6] = {}, OwnedLevels[6] = {}, VisibleLevels[6] = {};
	for (const FVoxelViewKey& Key : Plan.Leaves)
		if (Key.Level < UE_ARRAY_COUNT(PlannedLevels)) ++PlannedLevels[Key.Level];
	for (const FVoxelViewKey& Key : VoxelProxyWanted)
		if (Key.Level < UE_ARRAY_COUNT(ReadyLevels) && VoxelProxyReady.Contains(Key)) ++ReadyLevels[Key.Level];
	for (const FVoxelViewKey& Key : VisibleTerrainNodes)
		if (Key.Level < UE_ARRAY_COUNT(OwnedLevels)) ++OwnedLevels[Key.Level];
	for (const auto& Pair : VoxelProxyActors)
		if (Pair.Key.Level < UE_ARRAY_COUNT(VisibleLevels) && Pair.Value &&
			!Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value)) ++VisibleLevels[Pair.Key.Level];
	for (uint8 Level = 1; Level < UE_ARRAY_COUNT(PlannedLevels); ++Level)
	{
		const FVoxelViewKey Key{{}, Level};
		UE_LOG(LogTemp, Display, TEXT("Voxel proxy LOD: level=%d side=%d sampleStep=%d planned=%d readyWanted=%d owned=%d visible=%d"),
			Level, Key.GetSide(), Key.GetSampleStep(), PlannedLevels[Level], ReadyLevels[Level], OwnedLevels[Level], VisibleLevels[Level]);
	}
	for (const auto& Pair : GetViewInterest().Sources)
	{
		const FVoxelStreamingSource& Source = Pair.Value.Source;
		if (!Source.Has(EVoxelStreamingCapability::WorldVisual)) continue;
		UE_LOG(LogTemp, Display, TEXT("Voxel LOD source: center=%s viewportHeight=%d verticalFov=%.2f errorPixels=%.2f proxyRadius=%d maximumLevel=%d textureStretch=%.1f"),
			*Source.Center.ToString(), Source.ViewportHeightPixels, Source.VerticalFovDegrees,
			Module.GetViewSettings().Lod.TargetScreenErrorPixels, Source.View.VoxelProxyRadiusCells,
			Module.GetViewSettings().Lod.MaximumVoxelProxyLevel, Module.GetViewSettings().MaximumTextureStretchCells);
	}
	UE_LOG(LogTemp, Display, TEXT("Voxel terrain plan: roots=%d leaves=%d required=%d presented=%d volumeTransitions=%d pending=%d unbalanced=%d"),
		Plan.Roots.Num(), Plan.Leaves.Num(), Plan.Required.Num(), VisibleTerrainNodes.Num(),
		DesiredVolumeSignatures.Num(), PendingVolumeSignatures.Num(), VolumeUnbalancedFaceCount);
	const TPair<EVoxelTaskKind, const TCHAR*> TaskKinds[] = {
		{ EVoxelTaskKind::GenerateExactBase, TEXT("Data") },
		{ EVoxelTaskKind::GenerateVoxelProxy, TEXT("ProxyData") },
		{ EVoxelTaskKind::GenerateSurface, TEXT("SurfaceData") },
		{ EVoxelTaskKind::GenerateMacro, TEXT("MacroData") },
		{ EVoxelTaskKind::BuildCollision, TEXT("Collision") },
		{ EVoxelTaskKind::BuildFineMesh, TEXT("Fine") },
		{ EVoxelTaskKind::BuildVoxelProxy, TEXT("Proxy") },
		{ EVoxelTaskKind::BuildSurface, TEXT("Surface") },
		{ EVoxelTaskKind::BuildWater, TEXT("Water") },
		{ EVoxelTaskKind::BuildMacro, TEXT("Macro") },
		{ EVoxelTaskKind::BuildViewTransition, TEXT("HeightfieldTransition") },
		{ EVoxelTaskKind::BuildVolumeTransition, TEXT("VolumeTransition") },
		{ EVoxelTaskKind::BuildViewCoverage, TEXT("ViewCoverage") },
		{ EVoxelTaskKind::BuildDetails, TEXT("Details") },
		{ EVoxelTaskKind::DecodeOverlay, TEXT("Overlay") }
	};
	for (const TPair<EVoxelTaskKind, const TCHAR*>& TaskKind : TaskKinds)
	{
		const FVoxelTaskKindDiagnostics* Kind = SchedulerStats.ByKind.Find(TaskKind.Key);
		const int32 PendingCount = SchedulerStats.PendingByKind.FindRef(TaskKind.Key);
		const int32 RunningCount = SchedulerStats.RunningByKind.FindRef(TaskKind.Key);
		if ((!Kind || Kind->Completed == 0) && PendingCount == 0 && RunningCount == 0)
		{
			continue;
		}
		const double CompletedCount = Kind ? static_cast<double>(Kind->Completed) : 0.0;
		UE_LOG(LogTemp, Display,
			TEXT("Voxel task %s: completed=%llu canceled=%llu pending=%d running=%d avgQueueMs=%.2f avgExecuteMs=%.2f avgApplyMs=%.2f maxExecuteMs=%.2f"),
			TaskKind.Value,
			Kind ? Kind->Completed : 0,
			Kind ? Kind->Canceled : 0,
			PendingCount, RunningCount,
			CompletedCount > 0.0 ? Kind->TotalQueueMilliseconds / CompletedCount : 0.0,
			CompletedCount > 0.0 ? Kind->TotalExecuteMilliseconds / CompletedCount : 0.0,
			CompletedCount > 0.0 ? Kind->TotalApplyMilliseconds / CompletedCount : 0.0,
			Kind ? Kind->MaximumExecuteMilliseconds : 0.0);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Voxel frontier=%.1f admissions=%d pending=%d running=%d surfaceP=%d surfaceR=%d macroP=%d macroR=%d heightfieldTransition=%d unbalancedEdges=%d coverageMs=%.3f retireMs=%.3f"),
		LastResolvedFrontier,
		Admissions.Num(),
		SchedulerStats.Pending,
		SchedulerStats.Running,
		SchedulerStats.PendingByKind.FindRef(EVoxelTaskKind::BuildSurface),
		SchedulerStats.RunningByKind.FindRef(EVoxelTaskKind::BuildSurface),
		SchedulerStats.PendingByKind.FindRef(EVoxelTaskKind::BuildMacro),
		SchedulerStats.RunningByKind.FindRef(EVoxelTaskKind::BuildMacro),
		HeightfieldTransitionActors.Num(),
		HeightfieldUnbalancedEdgeCount,
		LastCoverageMilliseconds,
		LastRetireMilliseconds);

	if (CVarVoxelDebugRepresentation.GetValueOnGameThread() < 2 || InObservers.IsEmpty())
	{
		return;
	}

	const FVector Observer = InObservers[0];
	int32 Logged = 0;
	for (const TPair<FVoxelViewKey, TObjectPtr<AActor>>& Pair : VoxelProxyActors)
	{
		if (!Pair.Value || Logged >= 12)
		{
			continue;
		}
		const FVoxelGenerationBounds Bounds = Pair.Key.GetBounds();
		const FVector Center = FVector(Bounds.Min + Bounds.Max) * 0.5 * Module.BlockSize();
		if (FVector::DistSquared2D(Center, Observer) > FMath::Square(6400.0 * Module.BlockSize()))
		{
			continue;
		}
		const bool bVisible = !Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Voxel proxy key=(%d,%d,%d)L%d bounds=[(%d,%d,%d)-(%d,%d,%d)] W/R/V=%d/%d/%d"),
			Pair.Key.Coordinate.X,
			Pair.Key.Coordinate.Y,
			Pair.Key.Coordinate.Z,
			Pair.Key.Level,
			Bounds.Min.X,
			Bounds.Min.Y,
			Bounds.Min.Z,
			Bounds.Max.X,
			Bounds.Max.Y,
			Bounds.Max.Z,
			VoxelProxyWanted.Contains(Pair.Key),
			VoxelProxyReady.Contains(Pair.Key),
			bVisible);
		++Logged;
	}
}

void FVoxelViewManager::UpdateWantedTimestamps(
	const double InNow)
{
	auto Refresh = [InNow](auto& Times, const auto& Wanted)
	{
		for (auto& Pair : Times)
		{
			if (Wanted.Contains(Pair.Key)) Pair.Value = InNow;
		}
	};
	Refresh(FineLastWanted, FineWanted);
	Refresh(VoxelProxyLastWanted, VoxelProxyWanted);
	Refresh(SurfaceLastWanted, SurfaceWanted);
	Refresh(MacroLastWanted, MacroWanted);
}

FVoxelCoverageBox FVoxelViewManager::FineCoverageBox(const FIntVector& InKey) const
{
	const FIntVector Min = InKey * ViewSectionSide;
	return { Min, Min + FIntVector(ViewSectionSide) };
}

FVoxelCoverageBox FVoxelViewManager::VoxelProxyCoverageBox(const FVoxelViewKey& InKey) const
{
	const FVoxelGenerationBounds Bounds = InKey.GetBounds();
	return { Bounds.Min, Bounds.Max };
}

FVoxelCoverageRect FVoxelViewManager::FineCoverageRect(const FIntVector& InKey) const
{
	return FineCoverageBox(InKey).ProjectXY();
}

FVoxelCoverageRect FVoxelViewManager::VoxelProxyCoverageRect(const FVoxelViewKey& InKey) const
{
	return VoxelProxyCoverageBox(InKey).ProjectXY();
}

FVoxelCoverageRect FVoxelViewManager::SurfaceCoverageRect(const FVoxelSurfaceTileKey& InKey) const
{
	const int32 Side = VoxelViewLod::SurfaceSide(Module.GetViewSettings().Lod, InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	return { Min, Min + FIntPoint(Side, Side) };
}

FVoxelCoverageRect FVoxelViewManager::MacroCoverageRect(const FVoxelMacroTileKey& InKey) const
{
	const int32 Side = VoxelViewLod::MacroSide(Module.GetViewSettings().Lod, InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	return { Min, Min + FIntPoint(Side, Side) };
}

void FVoxelViewManager::GatherReadyWantedFineBoxes(TArray<FVoxelCoverageBox>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(FineReady.Num());
	for (const FIntVector& Key : FineReady)
	{
		AActor* Actor = FineActor(Key);
		if (FineWanted.Contains(Key) && VisibleTerrainNodes.Contains({Key, 0}) &&
			IsFinePresented(Key)) OutCoverage.Add(FineCoverageBox(Key));
	}
}

void FVoxelViewManager::GatherReadyWantedProxyBoxes(TArray<FVoxelCoverageBox>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(VoxelProxyReady.Num());
	for (const FVoxelViewKey& Key : VoxelProxyReady)
	{
		AActor* Actor = VoxelProxyActors.FindRef(Key);
		if (VoxelProxyWanted.Contains(Key) && VisibleTerrainNodes.Contains(Key) &&
			(!Actor || (!Actor->IsHidden() && Publisher->IsCommitted(Actor)))) OutCoverage.Add(VoxelProxyCoverageBox(Key));
	}
}

void FVoxelViewManager::GatherReadyWantedSurfaceRects(TArray<FVoxelCoverageRect>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(SurfaceReady.Num());
	for (const FVoxelSurfaceTileKey& Key : SurfaceReady)
	{
		AActor* Actor = SurfaceActors.FindRef(Key);
		if (SurfaceWanted.Contains(Key) && Actor && !Actor->IsHidden() &&
			Publisher->IsCommitted(Actor)) OutCoverage.Add(SurfaceCoverageRect(Key));
	}
}

void FVoxelViewManager::GatherReadyWantedMacroRects(TArray<FVoxelCoverageRect>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(MacroReady.Num());
	for (const FVoxelMacroTileKey& Key : MacroReady)
	{
		AActor* Actor = MacroActors.FindRef(Key);
		if (MacroWanted.Contains(Key) && Actor && !Actor->IsHidden() &&
			Publisher->IsCommitted(Actor)) OutCoverage.Add(MacroCoverageRect(Key));
	}
}

void FVoxelViewManager::GatherReadyWantedProxySurfaceRects(
	const FVoxelCoverageRect& InTarget,
	TArray<FVoxelCoverageRect>& OutCoverage) const
{
	OutCoverage.Reset();
	for (const FVoxelViewKey& Key : VoxelProxyReady)
	{
		AActor* Actor = VoxelProxyActors.FindRef(Key);
		if (!VoxelProxyWanted.Contains(Key) || !VisibleTerrainNodes.Contains(Key) ||
			!Actor || Actor->IsHidden() || !Publisher->IsCommitted(Actor) || !VoxelProxyCoverageRect(Key).Intersects(InTarget))
		{
			continue;
		}
		if (const TArray<FVoxelCoverageRect>* Coverage = ProxySurfaceCoverage.Find(Key))
		{
			for (const FVoxelCoverageRect& Rect : *Coverage)
			{
				if (Rect.Intersects(InTarget))
				{
					OutCoverage.Add(Rect);
				}
			}
		}
	}
}

void FVoxelViewManager::UpdateProxySurfaceCoverage(const FVoxelVoxelProxyData& InData)
{
	bHeightfieldTopologyDirty = true;
	TArray<FVoxelCoverageRect>& Coverage = ProxySurfaceCoverage.FindOrAdd(InData.Key);
	Coverage.Reset();
	const int32 Step = InData.Geometry.SampleStepCells;
	const FVoxelGenerationBounds Bounds = InData.Key.GetBounds();
	for (int32 Y = 0; Y < InData.GridSide; ++Y)
	{
		int32 RunStart = INDEX_NONE;
		for (int32 X = 0; X <= InData.GridSide; ++X)
		{
			const bool bCovered = X < InData.GridSide && VoxelCoverage::HasProxyTopSurfaceInColumn(InData, X, Y);
			if (bCovered && RunStart == INDEX_NONE)
			{
				RunStart = X;
			}
			else if (!bCovered && RunStart != INDEX_NONE)
			{
				Coverage.Add({
					FIntPoint(Bounds.Min.X + RunStart * Step, Bounds.Min.Y + Y * Step),
					FIntPoint(Bounds.Min.X + X * Step, Bounds.Min.Y + (Y + 1) * Step)
				});
				RunStart = INDEX_NONE;
			}
		}
	}
}

void FVoxelViewManager::GatherCurrentRenderDomainRects(TArray<FVoxelCoverageRect>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(FineWanted.Num() + VoxelProxyWanted.Num() + SurfaceWanted.Num() + MacroWanted.Num());
	for (const FIntVector& Key : FineWanted) OutCoverage.Add(FineCoverageRect(Key));
	for (const FVoxelViewKey& Key : VoxelProxyWanted) OutCoverage.Add(VoxelProxyCoverageRect(Key));
	for (const FVoxelSurfaceTileKey& Key : SurfaceWanted) OutCoverage.Add(SurfaceCoverageRect(Key));
	for (const FVoxelMacroTileKey& Key : MacroWanted) OutCoverage.Add(MacroCoverageRect(Key));
}

bool FVoxelViewManager::IsFineReplacementReady(const FIntVector& InKey) const
{
	FCoveragePassSnapshot Snapshot;
	BuildCoveragePassSnapshot(Snapshot);
	return IsFineReplacementReady(InKey, Snapshot);
}

bool FVoxelViewManager::IsProxyReplacementReady(const FVoxelViewKey& InKey) const
{
	FCoveragePassSnapshot Snapshot;
	BuildCoveragePassSnapshot(Snapshot);
	return IsProxyReplacementReady(InKey, Snapshot);
}

bool FVoxelViewManager::IsSurfaceReplacementReady(const FVoxelSurfaceTileKey& InKey) const
{
	FCoveragePassSnapshot Snapshot;
	BuildCoveragePassSnapshot(Snapshot);
	return IsSurfaceReplacementReady(InKey, Snapshot);
}

bool FVoxelViewManager::IsMacroReplacementReady(const FVoxelMacroTileKey& InKey) const
{
	FCoveragePassSnapshot Snapshot;
	BuildCoveragePassSnapshot(Snapshot);
	return IsMacroReplacementReady(InKey, Snapshot);
}

bool FVoxelViewManager::IsFineInsideRenderDomain(const FIntVector& InKey) const
{
	const auto Bounds = FineCoverageBox(InKey);
	FVoxelCoverageLocalSnapshot Local;
	CoverageIndex.Capture(Bounds.Min, Bounds.Max, Local);
	return Local.Records.ContainsByPredicate([](const auto& Record) { return Record->bWanted; });
}

bool FVoxelViewManager::IsProxyInsideRenderDomain(const FVoxelViewKey& InKey) const
{
	const auto Bounds = VoxelProxyCoverageBox(InKey);
	FVoxelCoverageLocalSnapshot Local;
	CoverageIndex.Capture(Bounds.Min, Bounds.Max, Local);
	return Local.Records.ContainsByPredicate([](const auto& Record) { return Record->bWanted; });
}

bool FVoxelViewManager::IsSurfaceInsideRenderDomain(const FVoxelSurfaceTileKey& InKey) const
{
	const auto Bounds = FVoxelCoverageBox{FIntVector(SurfaceCoverageRect(InKey).Min.X, SurfaceCoverageRect(InKey).Min.Y, MIN_int32 + 1), FIntVector(SurfaceCoverageRect(InKey).Max.X, SurfaceCoverageRect(InKey).Max.Y, MAX_int32)};
	FVoxelCoverageLocalSnapshot Local;
	CoverageIndex.Capture(Bounds.Min, Bounds.Max, Local);
	return Local.Records.ContainsByPredicate([](const auto& Record) { return Record->bWanted; });
}

bool FVoxelViewManager::IsMacroInsideRenderDomain(const FVoxelMacroTileKey& InKey) const
{
	const auto Bounds = FVoxelCoverageBox{FIntVector(MacroCoverageRect(InKey).Min.X, MacroCoverageRect(InKey).Min.Y, MIN_int32 + 1), FIntVector(MacroCoverageRect(InKey).Max.X, MacroCoverageRect(InKey).Max.Y, MAX_int32)};
	FVoxelCoverageLocalSnapshot Local;
	CoverageIndex.Capture(Bounds.Min, Bounds.Max, Local);
	return Local.Records.ContainsByPredicate([](const auto& Record) { return Record->bWanted; });
}

void FVoxelViewManager::MarkCoverageDirty()
{
	bCoverageDirty = true;
	bCoverageDirtyFull = true;
	DirtyCoverageRects.Reset();
}

void FVoxelViewManager::MarkCoverageDirty(const FVoxelCoverageRect& InBounds)
{
	if (!InBounds.IsValid())
	{
		MarkCoverageDirty();
		return;
	}
	bCoverageDirty = true;
	if (bCoverageDirtyFull) return;
	RefreshIndexedCoverage(InBounds);
	MergeCoverageRect(DirtyCoverageRects, InBounds);
}

void FVoxelViewManager::MergeCoverageRect(TArray<FVoxelCoverageRect>& InOutRects, const FVoxelCoverageRect& InBounds)
{
	FVoxelCoverageRect Merged{InBounds.Min - FIntPoint(1, 1),
		InBounds.Max + FIntPoint(1, 1)};
	for (int32 Index = 0; Index < InOutRects.Num();)
	{
		const FVoxelCoverageRect& Existing = InOutRects[Index];
		if (!Merged.Intersects(Existing))
		{
			++Index;
			continue;
		}
		Merged.Min.X = FMath::Min(Merged.Min.X, Existing.Min.X);
		Merged.Min.Y = FMath::Min(Merged.Min.Y, Existing.Min.Y);
		Merged.Max.X = FMath::Max(Merged.Max.X, Existing.Max.X);
		Merged.Max.Y = FMath::Max(Merged.Max.Y, Existing.Max.Y);
		InOutRects.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		Index = 0;
	}
	InOutRects.Add(Merged);
}

void FVoxelViewManager::ExpandActiveCoverage(const FVoxelCoverageRect& InBounds)
{
	if (bActiveCoverageFull) return;
	if (!InBounds.IsValid())
	{
		bActiveCoverageFull = true;
		return;
	}
	if (CoverageAffects(InBounds)) return;
	MergeCoverageRect(ActiveDirtyCoverageRects, InBounds);
}

void FVoxelViewManager::ApplyInterestDelta(const FVoxelInterestDelta& InDelta)
{
	if (GetViewInterest().PublicationInputs)
	{
		// Index, dirty scopes and retirement events are applied by bounded input cursors.
		QueuedInterestSnapshot = Module.GetCurrentInterestSnapshot();
		bTerrainBuildStageDirty = true;
		bHeightfieldTopologyDirty |= !InDelta.AddedSurface.IsEmpty() || !InDelta.RemovedSurface.IsEmpty() ||
			!InDelta.AddedMacro.IsEmpty() || !InDelta.RemovedMacro.IsEmpty();
		return;
	}
	for (const auto& Key : InDelta.RemovedFine) QueueRetirement({4, Key, 0});
	for (const auto& Key : InDelta.RemovedProxy) QueueRetirement({1, Key.Coordinate, Key.Level});
	for (const auto& Key : InDelta.RemovedSurface) QueueRetirement({2, Key.Coordinate, Key.Level});
	for (const auto& Key : InDelta.RemovedMacro) QueueRetirement({3, Key.Coordinate, Key.Level});
	for (const auto& Key : InDelta.ChangedGeometry)
	{
		if (const auto* Geometry = GetViewInterest().Geometry.Find(Key))
		{
			Module.GetRuntime()->GetChangeHierarchy().RegisterGeometry(*Geometry);
			FIntVector Min, Max;
			if (Key.TryBounds(Geometry->WorldMinZ, Geometry->WorldMaxZ, Min, Max))
				MarkCoverageDirty({FIntPoint(Min.X, Min.Y), FIntPoint(Max.X, Max.Y)});
			bHeightfieldTopologyDirty |= Key.IsColumn();
		}
	}
	bTerrainBuildStageDirty = true;
	RefreshTerrainBuildStage();
	bHeightfieldTopologyDirty |= !InDelta.AddedSurface.IsEmpty() || !InDelta.RemovedSurface.IsEmpty() || !InDelta.AddedMacro.IsEmpty() || !InDelta.RemovedMacro.IsEmpty();
	if (bCoverageDirtyFull || !InDelta.bVisualPartitionChanged) return;
	for (const FIntVector& Key : InDelta.AddedFine) MarkCoverageDirty(FineCoverageRect(Key));
	for (const FIntVector& Key : InDelta.RemovedFine) MarkCoverageDirty(FineCoverageRect(Key));
	for (const FVoxelViewKey& Key : InDelta.AddedProxy) MarkCoverageDirty(VoxelProxyCoverageRect(Key));
	for (const FVoxelViewKey& Key : InDelta.RemovedProxy) MarkCoverageDirty(VoxelProxyCoverageRect(Key));
	for (const FVoxelSurfaceTileKey& Key : InDelta.AddedSurface) MarkCoverageDirty(SurfaceCoverageRect(Key));
	for (const FVoxelSurfaceTileKey& Key : InDelta.RemovedSurface) MarkCoverageDirty(SurfaceCoverageRect(Key));
	for (const FVoxelMacroTileKey& Key : InDelta.AddedMacro) MarkCoverageDirty(MacroCoverageRect(Key));
	for (const FVoxelMacroTileKey& Key : InDelta.RemovedMacro) MarkCoverageDirty(MacroCoverageRect(Key));
}

bool FVoxelViewManager::CoverageAffects(const FVoxelCoverageRect& InBounds) const
{
	return bActiveCoverageFull ||
		VoxelCoverage::IntersectsAny2D(InBounds, ActiveDirtyCoverageRects);
}

void FVoxelViewManager::RetryActiveCoverage()
{
	if (bActiveCoverageFull || ActiveDirtyCoverageRects.IsEmpty())
	{
		MarkCoverageDirty();
		return;
	}
	bCoverageDirty = true;
	if (bCoverageDirtyFull) return;
	DirtyCoverageRects.Append(ActiveDirtyCoverageRects);
}

FGuid FVoxelViewManager::BeginPublishGroup(const FVoxelPublishGroupKey& InKey)
{
	if (const FGuid* Existing = OpenPublishGroups.Find(InKey))
		return *Existing;
	const FGuid Group = Publisher->BeginGroup(InKey);
	OpenPublishGroups.Add(InKey, Group);
	return Group;
}

void FVoxelViewManager::ResolveTransitionVisibility()
{
	auto Steps = ResolveTransitionVisibilitySteps();
	while (!Steps.IsComplete())
		Steps.Resume();
}

bool FVoxelViewManager::CommitTerrainOwnership(const FVoxelPublishGroupKey& InKey, const TConstArrayView<FVoxelViewKey> InTarget)
{
	if (InKey.Representation == 1)
		if (auto* Metrics = GeometryMetrics.Find({EVoxelTerrainRepresentation::VoxelProxy, InKey.Coordinate, InKey.Level}))
			Metrics->bPresented = !InTarget.IsEmpty();
	// 每代提交替换此 owner 当前的完整节点集，包括前代在快照形成后才提交的节点。
	TArray<FVoxelViewKey>& Current = VisibleTerrainNodesByGroup.FindOrAdd(InKey);
	bool bChanged = false;
	for (const FVoxelViewKey& Node : Current)
		if (!InTarget.Contains(Node))
			bChanged |= VisibleTerrainNodes.Remove(Node) > 0;
	for (const FVoxelViewKey& Node : InTarget)
		if (!VisibleTerrainNodes.Contains(Node))
		{
			VisibleTerrainNodes.Add(Node);
			bChanged = true;
		}
	for (const auto& Node : Current)
		IndexCoverage(Node);
	for (const auto& Node : InTarget)
		IndexCoverage(Node);
	Current = TArray<FVoxelViewKey>(InTarget);
	if (Current.IsEmpty())
		VisibleTerrainNodesByGroup.Remove(InKey);
	return bChanged;
}

void FVoxelViewManager::RebuildHeightfieldTransitions(const FCoveragePassSnapshot& InSnapshot)
{
	auto Work = RebuildHeightfieldTransitionSteps(InSnapshot);
	while (!Work.IsComplete())
	{
		Work.Resume();
	}
}

void FVoxelViewManager::PumpHeightfieldTransitions()
{
	if (UnsubmittedTransitionOwners.IsEmpty())
		return;
	const auto Config = Module.GetGenerationConfig();
	const auto Registry = Module.GetRegistry();
	if (!Config || !Config->Recipe || !Registry)
		return;
	const double Deadline = TerrainDeadline(Module.GetViewSettings().Budget.AdmissionMilliseconds);
	for (int32 Submitted = 0; Submitted < 4 && !UnsubmittedTransitionOwners.IsEmpty() && FPlatformTime::Seconds() < Deadline;)
	{
		const FVoxelHeightfieldNodeKey Owner = UnsubmittedTransitionOwners.Last();
		const uint64* Signature = DesiredTransitionSignatures.Find(Owner);
		const TArray<FVoxelHeightfieldTransitionEdge>* Edges = DesiredTransitionEdges.Find(Owner);
		const uint64* Built = HeightfieldTransitionSignatures.Find(Owner);
		const uint64* Pending = PendingTransitionSignatures.Find(Owner);
		const uint64* Prepared = PreparedTransitionSignatures.Find(Owner);
		if (!Signature || !Edges || (Built && *Built == *Signature) || (Pending && *Pending == *Signature) || (Prepared && *Prepared == *Signature))
		{
			UnsubmittedTransitionOwners.Pop();
			continue;
		}
		auto FindSource = [this](const FVoxelHeightfieldNodeKey& Key)
		{
			FHeightfieldTransitionTileSource Source;
			if (Key.Representation == EVoxelHeightfieldRepresentation::Surface)
			{
				Source.Surface = SurfaceData.FindRef({Key.Coordinate, Key.Level});
			}
			else if (Key.Representation == EVoxelHeightfieldRepresentation::Macro)
			{
				Source.Macro = MacroData.FindRef({Key.Coordinate, Key.Level});
			}
			return Source;
		};
		TMap<FVoxelHeightfieldNodeKey, FHeightfieldTransitionTileSource> Sources;
		Sources.Add(Owner, FindSource(Owner));
		for (const FVoxelHeightfieldTransitionEdge& Edge : *Edges)
		{
			if (!Edge.bPending)
				Sources.Add(Edge.Neighbor, FindSource(Edge.Neighbor));
		}
		bool bValid = true;
		for (const auto& Pair : Sources)
		{
			bValid &= Pair.Value.Surface.IsValid() || Pair.Value.Macro.IsValid();
		}
		if (!bValid)
		{
			UnsubmittedTransitionOwners.Pop();
			MarkCoverageDirty();
			continue;
		}
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildViewTransition;
		Request.TerrainStage = Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? 2 : 3;
		Request.WorkClass = EVoxelWorkClass::Boundary;
		if (Owner.Representation == EVoxelHeightfieldRepresentation::Surface)
		{
			if (const auto* Priority = GetViewInterest().SurfacePriorities.Find({Owner.Coordinate, Owner.Level}))
				Request.SourcePriority = *Priority;
		}
		else if (const auto* Priority = GetViewInterest().MacroPriorities.Find({Owner.Coordinate, Owner.Level}))
			Request.SourcePriority = *Priority;
		Request.DistanceScore = Owner.Representation == EVoxelHeightfieldRepresentation::Surface
									? MinimumObserverDistanceCells(SurfaceWorldCenter({Owner.Coordinate, Owner.Level}))
									: MinimumObserverDistanceCells(MacroWorldCenter({Owner.Coordinate, Owner.Level}));
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.Token = *Signature;
		Request.Stamp.Section = FIntVector(Owner.Coordinate.X, Owner.Coordinate.Y, static_cast<int32>(Owner.Representation));
		Request.Stamp.ViewKey.Level = Owner.Level;
		Request.ReservedBytes = 4ull * 1024ull * 1024ull;
		Request.Execute = [Owner, Sources = MoveTemp(Sources), OwnerEdges = *Edges, Config, Registry,
							  TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells,
							  Lod = Module.GetViewSettings().Lod](const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			if (Cancel.Load())
				return Result;
			TMap<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView> Views;
			for (const auto& Pair : Sources)
				Views.Add(Pair.Key, Pair.Value.View());
			const FVoxelHeightfieldTileView& OwnerView = Views.FindChecked(Owner);
			auto Payload = MakeShared<FHeightfieldTransitionTaskPayload, ESPMode::ThreadSafe>();
			Payload->Mesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
			Result.bSuccess = FVoxelHeightfieldTransitionBuilder::BuildMesh(
				OwnerView, OwnerEdges, Views, *Config, *Registry, Config->Recipe->Settings.MinZ, TextureStretch, *Payload->Mesh, Result.Error, &Cancel);
			if (Result.bSuccess)
				Result.CustomPayload = Payload;
			return Result;
		};
		FVoxelCoverageLocalSnapshot Guard;
		const auto Rect = Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Owner.Coordinate, Owner.Level})
																						   : MacroCoverageRect({Owner.Coordinate, Owner.Level});
		HeightfieldCoverageIndex.Capture(FIntVector(Rect.Min.X - 1, Rect.Min.Y - 1, Module.GetManifest().Settings.MinZ),
			FIntVector(Rect.Max.X + 1, Rect.Max.Y + 1, Module.GetManifest().Settings.MaxZ), Guard, false);
		Request.Apply = [this, Owner, TaskSignature = *Signature, Guard = MoveTemp(Guard)](FVoxelTaskResult&& Result)
		{
			if (!HeightfieldCoverageIndex.IsCurrent(Guard))
			{
				Result.bSuccess = false;
				Result.bCanceled = true;
			}
			ApplyHeightfieldTransition(Owner, TaskSignature, MoveTemp(Result));
		};
		if (Scheduler.IsTerminal({Request.Kind, Request.Stamp}))
		{
			UnsubmittedTransitionOwners.Pop();
			continue;
		}
		if (!EnqueueViewTask(MoveTemp(Request)))
			break;
		PendingTransitionSignatures.Add(Owner, *Signature);
		UnsubmittedTransitionOwners.Pop();
		++Submitted;
	}
}

void FVoxelViewManager::ApplyHeightfieldTransition(const FVoxelHeightfieldNodeKey& InOwner, const uint64 InSignature, FVoxelTaskResult&& InResult)
{
	if (const uint64* Pending = PendingTransitionSignatures.Find(InOwner); Pending && *Pending == InSignature)
	{
		PendingTransitionSignatures.Remove(InOwner);
	}
	const uint64* Desired = DesiredTransitionSignatures.Find(InOwner);
	if (InResult.Stamp.WorldEpoch != WorldEpoch || !Desired || *Desired != InSignature)
		return;
	if (InResult.bCanceled)
	{
		UnsubmittedTransitionOwners.AddUnique(InOwner);
		return;
	}
	if (!InResult.bSuccess || !InResult.CustomPayload)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heightfield transition build failed: %s"), *InResult.Error);
		Scheduler.ObserveDemand(EVoxelTaskKind::BuildViewTransition, InResult.Stamp, EVoxelTaskBlockReason::TerminalFailure);
		return;
	}
	const auto Payload = StaticCastSharedPtr<const FHeightfieldTransitionTaskPayload>(InResult.CustomPayload);
	if (!Payload->Mesh)
	{
		UnsubmittedTransitionOwners.AddUnique(InOwner);
		return;
	}
	PreparedTransitionSignatures.Add(InOwner, InSignature);
	PreparedTransitionMeshes.Add(InOwner, Payload->Mesh);
	MarkCoverageDirty(InOwner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({InOwner.Coordinate, InOwner.Level})
																						 : MacroCoverageRect({InOwner.Coordinate, InOwner.Level}));
}

bool FVoxelViewManager::VolumeHandoffAffectsHeightfield(const FVoxelHeightfieldNodeKey& InKey) const
{
	FVoxelHeightfieldTileView View;
	if (InKey.Representation == EVoxelHeightfieldRepresentation::Surface)
	{
		const auto Data = SurfaceData.FindRef({InKey.Coordinate, InKey.Level});
		if (!Data)
			return false;
		View = FVoxelHeightfieldTransitionBuilder::MakeView(*Data);
	}
	else
	{
		const auto Data = MacroData.FindRef({InKey.Coordinate, InKey.Level});
		if (!Data)
			return false;
		View = FVoxelHeightfieldTransitionBuilder::MakeView(*Data);
	}
	int32 MinimumHeight = MAX_int32;
	int32 MaximumHeight = MIN_int32;
	for (const int32 Height : View.Ground)
	{
		if (Height == MIN_int32)
			continue;
		MinimumHeight = FMath::Min(MinimumHeight, Height);
		MaximumHeight = FMath::Max(MaximumHeight, Height);
	}
	for (const int32 Height : View.Water)
	{
		if (Height == MIN_int32)
			continue;
		MinimumHeight = FMath::Min(MinimumHeight, Height);
		MaximumHeight = FMath::Max(MaximumHeight, Height);
	}
	if (MinimumHeight > MaximumHeight)
		return false;
	const FIntPoint Maximum = View.Origin + FIntPoint(View.CellSide() * View.Step);
	const FVoxelCoverageBox Bounds{FIntVector(View.Origin.X, View.Origin.Y, MinimumHeight - 1), FIntVector(Maximum.X, Maximum.Y, MaximumHeight + 2)};
	return VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes);
}

void FVoxelViewManager::RebuildVolumeTransitions(const TSet<FVoxelViewKey>& InTargetNodes)
{
	auto Work = RebuildVolumeTransitionSteps(InTargetNodes);
	while (!Work.IsComplete())
	{
		Work.Resume();
	}
}

void FVoxelViewManager::PumpVolumeTransitions()
{
	if (UnsubmittedVolumeOwners.IsEmpty())
		return;
	const auto Registry = Module.GetRegistry();
	const auto Shapes = Module.GetShapes();
	if (!Registry || !Shapes)
		return;
	const double Deadline = TerrainDeadline(Module.GetViewSettings().Budget.AdmissionMilliseconds);
	for (int32 Submitted = 0; Submitted < 4 && !UnsubmittedVolumeOwners.IsEmpty() && FPlatformTime::Seconds() < Deadline;)
	{
		const FVoxelViewKey Owner = UnsubmittedVolumeOwners.Last();
		const uint64* Signature = DesiredVolumeSignatures.Find(Owner);
		const auto Data = VoxelProxyData.FindRef(Owner);
		const auto Context = DesiredVolumeContexts.FindRef(Owner);
		if (!Signature || !Data || !Context || VolumeTransitionSignatures.FindRef(Owner) == *Signature ||
			PendingVolumeSignatures.FindRef(Owner) == *Signature || PreparedVolumeSignatures.FindRef(Owner) == *Signature)
		{
			UnsubmittedVolumeOwners.Pop();
			continue;
		}
		FString Error;
		const auto PageMesh = MakeShared<FVoxelProxyPageMesh, ESPMode::ThreadSafe>();
		if (!PageMesh->Initialize(Data->Geometry, Error))
		{
			UnsubmittedVolumeOwners.Pop();
			continue;
		}
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildVolumeTransition;
		Request.WorkClass = VolumeTransitionWorkClass(Owner);
		if (const auto* Priority = GetViewInterest().VoxelProxyPriorities.Find(Owner))
			Request.SourcePriority = *Priority;
		Request.DistanceScore = MinimumObserverDistanceCells(Owner.GetBounds());
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.ViewKey = Owner;
		Request.Stamp.Revision = Data->Revision;
		Request.Stamp.GeometryRevision = GetTypeHash(Data->Geometry);
		Request.bResumable = true;
		Request.Stamp.Token = *Signature;
		Request.ReservedBytes = 48ull * 1024ull * 1024ull;
		Request.InputBytes = 12ull * 1024ull * 1024ull + Data->Cells.GetAllocatedSize();
		for (const auto& Halo : Data->Halo)
			Request.InputBytes += Halo.GetAllocatedSize();
		for (const auto& Patch : Context->Patches)
			Request.InputBytes += Patch.Neighbor.States.GetAllocatedSize();
		Request.Execute = [Data, PageMesh, Context, Registry, Shapes, RecipeHash = Module.GetManifest().RecipeHash,
							  TextureRepeats = Data->Geometry.SampleStepCells / VoxelViewLod::TexturePeriodCells(Data->Geometry.SampleStepCells,
																					Module.GetViewSettings().MaximumTextureStretchCells)](
							  const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			Result.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
			bool bComplete = false;
			Result.bSuccess =
				PageMesh->Advance(*Data, *Registry, *Shapes, Context.Get(), TextureRepeats, *Result.VoxelProxyMesh, bComplete, Result.Error, &Cancel);
			if (Result.bSuccess && !bComplete)
			{
				Result.VoxelProxyMesh.Reset();
				Result.bContinue = true;
				return Result;
			}
			if (!Result.bSuccess)
				return Result;
			FVoxelSectionSnapshot Snapshot;
			Result.VolumeBoundaryMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.bSuccess =
				BuildVoxelProxySnapshot(*Data, RecipeHash, Snapshot, Result.Error) &&
				FVoxelSectionMesher::Build(Snapshot, *Registry, *Shapes, *Result.VolumeBoundaryMesh, &Cancel, TextureRepeats, Context.Get(), true);
			if (!Result.bSuccess && Result.Error.IsEmpty())
				Result.Error = TEXT("Failed to mesh volume boundary");
			return Result;
		};
		FVoxelCoverageLocalSnapshot Guard;
		const auto Bounds = Owner.GetBounds();
		CoverageIndex.Capture(Bounds.Min - FIntVector(1), Bounds.Max + FIntVector(1), Guard, false);
		Request.Apply = [this, Owner, TaskSignature = *Signature, Guard = MoveTemp(Guard)](FVoxelTaskResult&& Result)
		{
			if (!CoverageIndex.IsCurrent(Guard))
			{
				Result.bSuccess = false;
				Result.bCanceled = true;
			}
			ApplyVolumeTransition(Owner, TaskSignature, MoveTemp(Result));
		};
		if (Scheduler.IsTerminal({Request.Kind, Request.Stamp}))
		{
			UnsubmittedVolumeOwners.Pop();
			continue;
		}
		if (!EnqueueViewTask(MoveTemp(Request)))
			break;
		PendingVolumeSignatures.Add(Owner, *Signature);
		UnsubmittedVolumeOwners.Pop();
		++Submitted;
	}
}

void FVoxelViewManager::ApplyVolumeTransition(const FVoxelViewKey& InOwner, const uint64 InSignature, FVoxelTaskResult&& InResult)
{
	if (PendingVolumeSignatures.FindRef(InOwner) == InSignature)
	{
		PendingVolumeSignatures.Remove(InOwner);
	}
	const uint64* Desired = DesiredVolumeSignatures.Find(InOwner);
	const auto* Geometry = GetViewInterest().FindGeometry(InOwner);
	if (!Geometry || GetTypeHash(*Geometry) != InResult.Stamp.GeometryRevision || InResult.Stamp.WorldEpoch != WorldEpoch || !Desired ||
		*Desired != InSignature)
		return;
	if (InResult.bCanceled)
	{
		UnsubmittedVolumeOwners.AddUnique(InOwner);
		return;
	}
	if (!InResult.bSuccess || !InResult.VoxelProxyMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Volume transition build failed: %s"), *InResult.Error);
		Scheduler.ObserveDemand(EVoxelTaskKind::BuildVolumeTransition, InResult.Stamp, EVoxelTaskBlockReason::TerminalFailure);
		MarkCoverageDirty(VoxelProxyCoverageRect(InOwner));
		return;
	}
	PreparedVolumeMeshes.Add(InOwner, InResult.VoxelProxyMesh);
	PreparedVolumeBoundaryMeshes.Add(InOwner, InResult.VolumeBoundaryMesh);
	if (const auto Context = DesiredVolumeContexts.FindRef(InOwner))
		for (const auto& Patch : Context->Patches)
			if (Patch.Face.Neighbor.Level == 0)
				if (const auto* Page = FinePagePlan.SectionOwners.Find(Patch.Face.Neighbor.Coordinate))
					MarkFinePageDirty(*Page);
	bFinePresentationDirty |= !DirtyFinePages.IsEmpty();
	PreparedVolumeSignatures.Add(InOwner, InSignature);
	MarkCoverageDirty(VoxelProxyCoverageRect(InOwner));
}

void FVoxelViewManager::QueueRetirement(const FVoxelPublishGroupKey& InKey)
{
	if (!RetirementIndices.Contains(InKey))
		RetirementIndices.Add(InKey, RetirementQueue.Add(InKey));
}

void FVoxelViewManager::RemoveRetirement(const FVoxelPublishGroupKey& InKey)
{
	const auto* Found = RetirementIndices.Find(InKey);
	if (!Found)
		return;
	const int32 Index = *Found;
	RetirementIndices.Remove(InKey);
	RetirementQueue.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	if (RetirementQueue.IsValidIndex(Index))
		RetirementIndices.FindChecked(RetirementQueue[Index]) = Index;
	RetirementCursor = RetirementQueue.IsEmpty() ? 0 : RetirementCursor % RetirementQueue.Num();
}

void FVoxelViewManager::CleanupRetiredRepresentations(const double InNow)
{
	if (CoverageContinuation.IsValid() || RetirementQueue.IsEmpty())
		return;
	CoverageInterestSnapshot = AppliedInterestSnapshot ? AppliedInterestSnapshot : Module.GetCurrentInterestSnapshot();
	CoverageStartedSeconds = FPlatformTime::Seconds();
	CoverageContinuation = CleanupRetiredRepresentationSteps(InNow);
}

bool FVoxelViewManager::OnTask(FVoxelTaskResult&& InResult)
{
	if (InResult.bCanceled)
		++TerrainWorkMetrics.Canceled;
	if (InResult.Stamp.WorldEpoch != WorldEpoch)
		++TerrainWorkMetrics.Stale;
	if (CoverageContinuation.IsValid() && InResult.bSuccess && !InResult.bCanceled && InResult.Stamp.WorldEpoch == WorldEpoch &&
		(InResult.Kind == EVoxelTaskKind::BuildFineMesh || InResult.Kind == EVoxelTaskKind::BuildVoxelProxy || InResult.Kind == EVoxelTaskKind::BuildSurface ||
			InResult.Kind == EVoxelTaskKind::BuildMacro))
	{
		DeferredCoverageResults.Add(MoveTemp(InResult));
		return true;
	}
	if (InResult.Stamp.WorldEpoch != WorldEpoch || InResult.bCanceled || !InResult.bSuccess)
	{
		if (InResult.Stamp.WorldEpoch == WorldEpoch && !InResult.bCanceled && !InResult.bSuccess)
		{
			Scheduler.ObserveDemand(InResult.Kind, InResult.Stamp, EVoxelTaskBlockReason::TerminalFailure);
			UE_LOG(LogTemp, Warning, TEXT("Voxel representation task failed: kind=%d error=%s"), static_cast<int32>(InResult.Kind), *InResult.Error);
		}
		return InResult.Kind == EVoxelTaskKind::BuildFineMesh || InResult.Kind == EVoxelTaskKind::BuildVoxelProxy ||
			   InResult.Kind == EVoxelTaskKind::BuildSurface || InResult.Kind == EVoxelTaskKind::BuildWater || InResult.Kind == EVoxelTaskKind::BuildMacro;
	}

	switch (InResult.Kind)
	{
	case EVoxelTaskKind::BuildFineMesh:
		if (PublishFine(InResult))
			MarkCoverageDirty(FineCoverageRect(InResult.Stamp.Section));
		return true;
	case EVoxelTaskKind::BuildVoxelProxy:
		if (PublishVoxelProxy(InResult))
			MarkCoverageDirty(VoxelProxyCoverageRect(InResult.Stamp.ViewKey));
		return true;
	case EVoxelTaskKind::BuildSurface:
		if (InResult.Surface && SurfaceWanted.Contains(InResult.Stamp.SurfaceKey) && PublishSurface(InResult))
		{
			const FVoxelSurfaceTileKey Key = InResult.Stamp.SurfaceKey;
			MarkCoverageDirty(SurfaceCoverageRect(Key));
			if (InResult.Water)
				WaterData.Add(Key, InResult.Water);
			else
				WaterData.Remove(Key);
		}
		return true;
	case EVoxelTaskKind::BuildWater:
		// 升级过程中已排队的旧 Water 任务只做丢弃。
		return true;
	case EVoxelTaskKind::BuildMacro:
		if (InResult.Macro && MacroWanted.Contains(InResult.Stamp.MacroKey) && PublishMacro(InResult))
		{
			MarkCoverageDirty(MacroCoverageRect(InResult.Stamp.MacroKey));
		}
		return true;
	default:
		return false;
	}
}

bool FVoxelViewManager::ApplyRemoteRepresentation(const FVoxelRepresentationReply& InReply, FString& OutError)
{
	if (InReply.Kind == EVoxelRepresentationReplyKind::Retry || InReply.Kind == EVoxelRepresentationReplyKind::Unchanged)
	{
		OutError.Reset();
		return true;
	}
	const FVoxelTerrainGeometry* Desired = nullptr;
	FVoxelTaskRequest Request;
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.Revision = InReply.Revision;
	Request.WorkClass = EVoxelWorkClass::Visible;
	if (InReply.Type == EVoxelRepresentationWireType::VoxelProxy)
	{
		Request.Kind = EVoxelTaskKind::BuildVoxelProxy;
		Request.Stamp.ViewKey = {InReply.Key.Coordinate, InReply.Key.Level};
		Desired = GetViewInterest().FindGeometry(Request.Stamp.ViewKey);
	}
	else if (InReply.Type == EVoxelRepresentationWireType::SurfaceProxy)
	{
		Request.Kind = EVoxelTaskKind::BuildSurface;
		Request.Stamp.SurfaceKey = {FIntPoint(InReply.Key.Coordinate.X, InReply.Key.Coordinate.Y), InReply.Key.Level};
		Desired = GetViewInterest().FindGeometry(Request.Stamp.SurfaceKey);
	}
	else if (InReply.Type == EVoxelRepresentationWireType::MacroTerrain)
	{
		Request.Kind = EVoxelTaskKind::BuildMacro;
		Request.Stamp.MacroKey = {FIntPoint(InReply.Key.Coordinate.X, InReply.Key.Coordinate.Y), InReply.Key.Level};
		Desired = GetViewInterest().FindGeometry(Request.Stamp.MacroKey);
	}
	if (InReply.Kind == EVoxelRepresentationReplyKind::Rejected)
	{
		if (!Desired)
		{
			OutError = TEXT("Rejected remote geometry is obsolete");
			return false;
		}
		Request.Stamp.GeometryRevision = GetTypeHash(*Desired);
		const uint32 KeyHash = InReply.Type == EVoxelRepresentationWireType::VoxelProxy		? GetTypeHash(Request.Stamp.ViewKey)
							   : InReply.Type == EVoxelRepresentationWireType::SurfaceProxy ? GetTypeHash(Request.Stamp.SurfaceKey)
																							: GetTypeHash(Request.Stamp.MacroKey);
		Request.Stamp.Token = HashCombineFast(KeyHash, GetTypeHash(InReply.Revision));
		Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::TerminalFailure);
		OutError.Reset();
		return true;
	}
	Request.Kind = InReply.Type == EVoxelRepresentationWireType::VoxelProxy		? EVoxelTaskKind::GenerateVoxelProxy
				   : InReply.Type == EVoxelRepresentationWireType::SurfaceProxy ? EVoxelTaskKind::GenerateSurface
																				: EVoxelTaskKind::GenerateMacro;
	FVoxelGeometryWireHeader Header;
	if (!Desired || !VoxelGeometryWire::Decode(InReply.Data, Header, OutError) ||
		!VoxelGeometryWire::ValidateExpected(Header, {*Desired, InReply.Serial, InReply.Revision}, OutError))
		return false;
	if (InReply.Type == EVoxelRepresentationWireType::VoxelProxy)
		Module.GetRuntime()->GetChangeHierarchy().SetVoxelProxyRevision(Request.Stamp.ViewKey, InReply.Revision);
	else if (InReply.Type == EVoxelRepresentationWireType::SurfaceProxy)
		Module.GetRuntime()->GetChangeHierarchy().SetSurfaceRevision(Request.Stamp.SurfaceKey, InReply.Revision);
	else
		Module.GetRuntime()->GetChangeHierarchy().SetMacroRevision(Request.Stamp.MacroKey, InReply.Revision);
	const auto Registry = Module.GetRegistry();
	const auto Shapes = Module.GetShapes();
	const auto Config = Module.GetGenerationConfig();
	if (!Registry || !Shapes || !Config)
	{
		OutError = TEXT("Remote terrain dependencies unavailable");
		return false;
	}
	struct FRemotePage
	{
		TArray<uint8> Bytes;
		FVoxelTaskResult Data;
		FVoxelProxyPageMesh ProxyMesh;
		FVoxelHeightfieldPageMesh HeightfieldMesh;
		bool bDecoded = false;
	};
	auto Page = MakeShared<FRemotePage, ESPMode::ThreadSafe>();
	Page->Bytes = InReply.Data;
	Request.bResumable = true;
	Request.InputBytes = Page->Bytes.GetAllocatedSize() + 24ull * 1024ull * 1024ull;
	Request.ReservedBytes = 48ull * 1024ull * 1024ull;
	Request.Stamp.GeometryRevision = GetTypeHash(*Desired);
	Request.Stamp.Token = HashCombineFast(InReply.Type == EVoxelRepresentationWireType::VoxelProxy	   ? GetTypeHash(Request.Stamp.ViewKey)
										  : InReply.Type == EVoxelRepresentationWireType::SurfaceProxy ? GetTypeHash(Request.Stamp.SurfaceKey)
																									   : GetTypeHash(Request.Stamp.MacroKey),
		GetTypeHash(InReply.Revision));
	Request.Execute = [Page, Type = InReply.Type, Geometry = *Desired, Registry, Shapes, Config,
						  TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		if (!Page->bDecoded)
		{
			bool bDecoded = false;
			if (Type == EVoxelRepresentationWireType::VoxelProxy)
			{
				Page->Data.VoxelProxy = MakeShared<FVoxelVoxelProxyData>();
				bDecoded = FVoxelRepresentationSync::DecodeVoxelProxy(Page->Bytes, *Page->Data.VoxelProxy, Result.Error) &&
						   Page->ProxyMesh.Initialize(Geometry, Result.Error, 0, true);
			}
			else if (Type == EVoxelRepresentationWireType::SurfaceProxy)
			{
				Page->Data.Surface = MakeShared<FVoxelSurfaceTileData>();
				Page->Data.Water = MakeShared<FVoxelWaterSurfaceTileData>();
				bDecoded = FVoxelRepresentationSync::DecodeSurface(Page->Bytes, *Page->Data.Surface, Result.Error) &&
						   FVoxelWaterViewBuilder().Build(*Page->Data.Surface, *Page->Data.Water, Result.Error) &&
						   Page->HeightfieldMesh.Initialize(Geometry, Result.Error, 0, true);
			}
			else
			{
				Page->Data.Macro = MakeShared<FVoxelMacroTileData>();
				bDecoded = FVoxelRepresentationSync::DecodeMacro(Page->Bytes, *Page->Data.Macro, Result.Error) &&
						   Page->HeightfieldMesh.Initialize(Geometry, Result.Error, 0, true);
			}
			if (!bDecoded)
			{
				Result.bSuccess = false;
				return Result;
			}
			Page->Bytes.Empty();
			Page->bDecoded = true;
			Result.bContinue = true;
			return Result;
		}
		bool bComplete = false;
		if (Page->Data.VoxelProxy)
		{
			Result.VoxelProxy = Page->Data.VoxelProxy;
			Result.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.bSuccess = Page->ProxyMesh.Advance(*Result.VoxelProxy, *Registry, *Shapes, nullptr,
				Geometry.SampleStepCells / VoxelViewLod::TexturePeriodCells(Geometry.SampleStepCells, TextureStretch), *Result.VoxelProxyMesh, bComplete,
				Result.Error, &Cancel);
		}
		else if (Page->Data.Surface)
		{
			Result.Surface = Page->Data.Surface;
			Result.Water = Page->Data.Water;
			Result.SurfaceMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.bSuccess = Page->HeightfieldMesh.Advance(
				*Result.Surface, *Result.Water, *Config, *Registry, TextureStretch, *Result.SurfaceMesh, bComplete, Result.Error, &Cancel);
		}
		else
		{
			Result.Macro = Page->Data.Macro;
			Result.MacroMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.bSuccess =
				Page->HeightfieldMesh.Advance(*Result.Macro, *Config, *Registry, TextureStretch, *Result.MacroMesh, bComplete, Result.Error, &Cancel);
		}
		if (Result.bSuccess && bComplete)
		{
			if (Result.VoxelProxy)
			{
				Result.VoxelProxy->EstimatedMeshBytes = Page->ProxyMesh.GetEstimatedMeshBytes();
				Result.bKnownEmptyMesh = Result.VoxelProxy->EstimatedMeshBytes == 0;
			}
			if (Result.Surface)
				Result.Surface->EstimatedMeshBytes = Page->HeightfieldMesh.GetEstimatedMeshBytes();
			if (Result.Macro)
				Result.Macro->EstimatedMeshBytes = Page->HeightfieldMesh.GetEstimatedMeshBytes();
		}
		Result.VoxelProxyMesh.Reset();
		Result.SurfaceMesh.Reset();
		Result.MacroMesh.Reset();
		Result.bContinue = Result.bSuccess && !bComplete;
		return Result;
	};
	if (!EnqueuePreparedData(MoveTemp(Request)))
	{
		OutError = TEXT("Remote terrain admission deferred");
		return false;
	}
	OutError.Reset();
	return true;
}

void FVoxelViewManager::InvalidateRemoteRepresentations(const FVoxelRepresentationInvalidate& InInvalidate)
{
	if (CoverageContinuation.IsValid())
	{
		++CoverageContentRevision;
		DeferredCoverageCallbacks.Enqueue(
			[this, Invalidate = InInvalidate]
			{
				InvalidateRemoteRepresentations(Invalidate);
			});
		return;
	}
	bTerrainBuildStageDirty = true;
	Scheduler.SetObservedTerrainStage(0);
	AppliedInterestRevision = 0;
	MarkCoverageDirty();
	for (const FVoxelRepresentationWireKey& WireKey : InInvalidate.Keys)
	{
		const FVoxelViewKey VoxelKey{WireKey.Coordinate, WireKey.Level};
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(VoxelKey) > 0;
		VoxelProxyData.Remove(VoxelKey);
		AirVoxelProxyNodes.Remove(VoxelKey);
		ProxySurfaceCoverage.Remove(VoxelKey);
		VoxelProxyRevisions.Remove(VoxelKey);
		VolumeTransitionSignatures.Remove(VoxelKey);
		PreparedVolumeSignatures.Remove(VoxelKey);
		PreparedVolumeMeshes.Remove(VoxelKey);
		Module.GetRuntime()->GetChangeHierarchy().SetVoxelProxyRevision(VoxelKey, InInvalidate.Revision);

		const FIntPoint Coordinate(WireKey.Coordinate.X, WireKey.Coordinate.Y);
		const FVoxelSurfaceTileKey SurfaceKey{Coordinate, WireKey.Level};
		SurfaceReady.Remove(SurfaceKey);
		SurfaceData.Remove(SurfaceKey);
		WaterData.Remove(SurfaceKey);
		SurfaceRevisions.Remove(SurfaceKey);
		Module.GetRuntime()->GetChangeHierarchy().SetSurfaceRevision(SurfaceKey, InInvalidate.Revision);

		const FVoxelMacroTileKey MacroKey{Coordinate, WireKey.Level};
		MacroReady.Remove(MacroKey);
		MacroData.Remove(MacroKey);
		MacroRevisions.Remove(MacroKey);
		Module.GetRuntime()->GetChangeHierarchy().SetMacroRevision(MacroKey, InInvalidate.Revision);
	}
	PrunePreparedData();
}

void FVoxelViewManager::InvalidateSection(const FIntVector& InKey)
{
	if (CoverageContinuation.IsValid())
	{
		++CoverageContentRevision;
		DeferredCoverageCallbacks.Enqueue(
			[this, InKey]
			{
				InvalidateSection(InKey);
			});
		return;
	}
	bTerrainBuildStageDirty = true;
	Scheduler.SetObservedTerrainStage(0);
	PrunePreparedData();
	InvalidateNeighbors(InKey);
	InvalidateFinePresentation(InKey);
	Scheduler.CancelSection(InKey);
	const FVoxelAffectedRepresentationKeys Affected = Module.GetRuntime()->GetChangeHierarchy().GatherAffectedKeys(InKey);
	InvalidateRepresentationKeys(Affected);
}

void FVoxelViewManager::InvalidateRepresentationKeys(const FVoxelAffectedRepresentationKeys& InAffected)
{
	if (CoverageContinuation.IsValid())
	{
		++CoverageContentRevision;
		DeferredCoverageCallbacks.Enqueue(
			[this, Affected = InAffected]
			{
				InvalidateRepresentationKeys(Affected);
			});
		return;
	}
	for (const FVoxelViewKey& Key : InAffected.Proxy)
	{
		const bool bRelevant = VoxelProxyWanted.Contains(Key) || VoxelProxyReady.Contains(Key) || VoxelProxyActors.Contains(Key) ||
							   VisibleTerrainNodes.Contains(Key) || VoxelProxyData.Contains(Key);
		VoxelProxyRevisions.Remove(Key);
		if (bRelevant)
			MarkCoverageDirty(VoxelProxyCoverageRect(Key));
		bHeightfieldTopologyDirty |= ProxySurfaceCoverage.Remove(Key) > 0;
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(Key) > 0;
		VoxelProxyData.Remove(Key);
		AirVoxelProxyNodes.Remove(Key);
		VolumeTransitionSignatures.Remove(Key);
		PreparedVolumeSignatures.Remove(Key);
		PreparedVolumeMeshes.Remove(Key);
	}

	for (const FVoxelSurfaceTileKey& SurfaceKey : InAffected.Surface)
	{
		const bool bRelevant =
			SurfaceWanted.Contains(SurfaceKey) || SurfaceReady.Contains(SurfaceKey) || SurfaceActors.Contains(SurfaceKey) || SurfaceData.Contains(SurfaceKey);
		SurfaceRevisions.Remove(SurfaceKey);
		bHeightfieldTopologyDirty |= SurfaceReady.Remove(SurfaceKey) > 0;
		if (bRelevant)
			MarkCoverageDirty(SurfaceCoverageRect(SurfaceKey));
		SurfaceData.Remove(SurfaceKey);
		WaterData.Remove(SurfaceKey);
	}
	for (const FVoxelMacroTileKey& MacroKey : InAffected.Macro)
	{
		const bool bRelevant =
			MacroWanted.Contains(MacroKey) || MacroReady.Contains(MacroKey) || MacroActors.Contains(MacroKey) || MacroData.Contains(MacroKey);
		MacroRevisions.Remove(MacroKey);
		bHeightfieldTopologyDirty |= MacroReady.Remove(MacroKey) > 0;
		if (bRelevant)
			MarkCoverageDirty(MacroCoverageRect(MacroKey));
		MacroData.Remove(MacroKey);
	}
}

void FVoxelViewManager::InvalidateNeighbors(const FIntVector& InKey)
{
	if (CoverageContinuation.IsValid())
	{
		++CoverageContentRevision;
		DeferredCoverageCallbacks.Enqueue(
			[this, InKey]
			{
				InvalidateNeighbors(InKey);
			});
		return;
	}
	bTerrainBuildStageDirty = true;
	Scheduler.SetObservedTerrainStage(0);
	TSet<FIntVector> Neighbors;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (const int32 Sign : {-1, 1})
		{
			FIntVector Key = InKey;
			Key[Axis] += Sign;
			if (!FineWanted.Contains(Key) && !FineReady.Contains(Key) && !FineSectionMeshes.Contains(Key))
				continue;
			const bool bRemoved = FineReady.Remove(Key) > 0;
			bReadyTerrainBranchesDirty |= bRemoved;
			if (bRemoved)
				MarkCoverageDirty(FineCoverageRect(Key));
			InvalidateFinePresentation(Key);
			Neighbors.Add(Key);
		}
	}
	if (Neighbors.IsEmpty())
		return;
	Scheduler.CancelMatching(
		[&Neighbors](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildFineMesh && Neighbors.Contains(Stamp.Section);
		});
}

void FVoxelViewManager::Reset()
{
	CoverageContinuation.Reset();
	CoverageInterestSnapshot.Reset();
	DeferredCoverageResults.Reset();
	DeferredCoverageCallbacks.Empty();
	GeometryMetrics.Reset();
	PresentedGeometryMetrics.Reset();
	TerrainWorkMetrics = {};
	bTerrainBuildStageDirty = true;
	Scheduler.SetObservedTerrainStage(0);
	++VolumePlanSerial;
	++HeightfieldPlanSerial;
	HeightfieldCoveragePlans.Reset();
	PendingHeightfieldPlanOwners.Reset();
	VolumeCoveragePlans.Reset();
	VolumePlanIndex.Reset();
	PendingVolumeHandoffBoxes.Reset();
	for (const auto& Pair : FinePageActors)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	for (const TPair<FVoxelViewKey, TObjectPtr<AActor>>& Pair : VoxelProxyActors)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	for (const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>>& Pair : SurfaceActors)
	{
		if (Pair.Value)
			Pair.Value->Destroy();
	}
	for (const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>>& Pair : WaterActors)
	{
		if (Pair.Value)
			Pair.Value->Destroy();
	}
	for (const TPair<FVoxelMacroTileKey, TObjectPtr<AActor>>& Pair : MacroActors)
	{
		if (Pair.Value)
			Pair.Value->Destroy();
	}
	for (const TPair<FVoxelHeightfieldNodeKey, TObjectPtr<AActor>>& Pair : HeightfieldTransitionActors)
	{
		if (Pair.Value)
			Pair.Value->Destroy();
	}
	PreparedData.Reset();
	PreparedEnvironmentRetentions.Reset();
	PreparedDataBytes = 0;
	PendingDataBytes = 0;
	SkippedProxyMeshes = 0;
	SkippedVolumeMeshes = 0;
	for (auto& Scans : DataScanIndices)
		Scans.Reset();
	for (auto& Scans : DataFrontierScanIndices)
		Scans.Reset();
	FineSectionMeshes.Reset();
	PreparedFinePageMeshes.Reset();
	PendingFinePageBuilds.Reset();
	FinePageFailures.Reset();
	FinePageCaptures.Reset();
	FinePageVersions.Reset();
	FinePageContentRevisions.Reset();
	NextFinePreparationIndex = 0;
	FinePageActors.Reset();
	FinePagePlan = {};
	FinePageReady.Reset();
	EmptyFinePages.Reset();
	PresentedEmptyFinePages.Reset();
	PresentedFineSectionOwners.Reset();
	DirtyFinePages.Reset();
	PendingFinePages.Reset();
	bFinePresentationDirty = true;
	bHeightfieldTopologyDirty = true;
	FineRevisions.Reset();
	VoxelProxyActors.Reset();
	VoxelProxyRevisions.Reset();
	SurfaceActors.Reset();
	WaterActors.Reset();
	MacroActors.Reset();
	HeightfieldTransitionActors.Reset();
	HeightfieldTransitionSignatures.Reset();
	DesiredTransitionSignatures.Reset();
	PendingTransitionSignatures.Reset();
	PreparedTransitionSignatures.Reset();
	PreparedTransitionMeshes.Reset();
	DesiredTransitionEdges.Reset();
	UnsubmittedTransitionOwners.Reset();
	PendingHeightfieldHandoffRects.Reset();
	HeightfieldUnbalancedEdgeCount = 0;
	VolumeTransitionSignatures.Reset();
	DesiredVolumeSignatures.Reset();
	PendingVolumeSignatures.Reset();
	PreparedVolumeSignatures.Reset();
	PreparedVolumeMeshes.Reset();
	PreparedVolumeBoundaryMeshes.Reset();
	for (const auto& Pair : VolumeBoundaryActors)
		if (Pair.Value)
		{
			Publisher->Forget(Pair.Value);
			Pair.Value->Destroy();
		}
	VolumeBoundaryActors.Reset();
	DesiredVolumeContexts.Reset();
	UnsubmittedVolumeOwners.Reset();
	VolumeUnbalancedFaceCount = 0;
	SurfaceRevisions.Reset();
	MacroRevisions.Reset();
	VisibleTerrainNodes.Reset();
	VisibleTerrainNodesByGroup.Reset();
	CoverageIndex.Reset();
	TerrainRootIndex.Reset();
	IndexedTerrainRoots.Reset();
	HeightfieldCoverageIndex.Reset();
	FineWanted.Reset();
	VoxelProxyWanted.Reset();
	SurfaceWanted.Reset();
	MacroWanted.Reset();
	FineReady.Reset();
	VoxelProxyReady.Reset();
	ReadyTerrainBranches.Reset();
	bReadyTerrainBranchesDirty = true;
	SurfaceReady.Reset();
	MacroReady.Reset();
	VoxelProxyData.Reset();
	AirVoxelProxyNodes.Reset();
	ProxySurfaceCoverage.Reset();
	SurfaceData.Reset();
	WaterData.Reset();
	MacroData.Reset();
	FineLastWanted.Reset();
	VoxelProxyLastWanted.Reset();
	SurfaceLastWanted.Reset();
	MacroLastWanted.Reset();
	Admissions = {};
	PendingPublicationInputs.Reset();
	PendingInterestSnapshot.Reset();
	QueuedInterestSnapshot.Reset();
	AppliedInterestSnapshot.Reset();
	PendingInterestRevision = 0;
	bTaskPrioritiesDirty = false;
	InterestInputPhase = InterestInputCursor = 0;
	PendingFinePagePlan = {};
	PendingLayoutInvalidations.Reset();
	PriorityObservers.Reset();
	for (auto& Scans : AdmissionScanIndices)
		Scans.Reset();
	Publisher->Reset();
	OpenPublishGroups.Reset();
	AppliedInterestRevision = 0;
	LastActiveAdmissionKind = 4;
	LastActiveAdmissionPriority = MAX_int32;
	LastActiveDataKind = 4;
	AdmissionPriorities = {};
	LastResolvedFrontier = 0.0;
	MarkCoverageDirty();
	ActiveDirtyCoverageRects.Reset();
	bActiveCoverageFull = true;
	RetirementQueue.Reset();
	RetirementIndices.Reset();
	RetirementCursor = 0;
	NextRetireCheck = 0.0;
	LastRetireMilliseconds = 0.0;
	NextRepresentationDebugLog = 0.0;
}

bool FVoxelViewManager::RequiresSectionData(const FIntVector& InKey) const
{
	return FineReady.Contains(InKey) || FineSectionMeshes.Contains(InKey) || VisibleTerrainNodes.Contains({InKey, 0});
}

bool FVoxelViewManager::HasPrimaryRepresentation() const
{
	return !FinePageActors.IsEmpty() || !VoxelProxyActors.IsEmpty() || !SurfaceActors.IsEmpty() || !MacroActors.IsEmpty();
}

FVoxelRepresentationReadiness FVoxelViewManager::GetRepresentationReadiness() const
{
	FVoxelRepresentationReadiness Result;
	for (const auto& Pair : FinePageActors)
	{
		if (Pair.Value && !Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value))
			++Result.PresentedFine;
	}
	for (const auto& Pair : VoxelProxyActors)
	{
		if (Pair.Value && !Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value))
			++Result.PresentedVoxelProxy;
	}
	for (const auto& Pair : SurfaceActors)
	{
		if (Pair.Value && !Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value))
			++Result.PresentedSurface;
	}
	for (const auto& Pair : MacroActors)
	{
		if (Pair.Value && !Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value))
			++Result.PresentedMacro;
	}
	return Result;
}

FVoxelTerrainReadinessSnapshot FVoxelViewManager::CaptureTerrainReadiness() const
{
	const FVoxelInterestSet& Interest = GetViewInterest();
	FVoxelTerrainReadinessSnapshot Snapshot;
	Snapshot.WorldEpoch = WorldEpoch;
	Snapshot.PublicationRevision = Interest.PublicationRevision;
	Snapshot.FineLayoutRevision = FinePageLayoutRevision;
	Snapshot.bAdoptingInputs = bConsumingPublicationInputs;
	for (const auto& Pair : Interest.Sources)
	{
		Snapshot.Sources.Add(Pair.Key, Pair.Value.Source);
	}
	Snapshot.Targets.Reserve(Interest.Geometry.Num());
	for (const auto& Pair : Interest.Geometry)
	{
		FVoxelTerrainTargetReadiness& Target = Snapshot.Targets.AddDefaulted_GetRef();
		Target.Geometry = Pair.Value;
		const FVoxelTerrainSpatialKey& Key = Pair.Key;
		AActor* Actor = nullptr;
		if (Key.Representation == EVoxelTerrainRepresentation::Fine)
		{
			const FVoxelFinePageKey Page{Key.Coordinate, Key.Level};
			Actor = FinePageActors.FindRef(Page);
			Target.bDirty = DirtyFinePages.Contains(Page);
			Target.bMeshReady = FinePagePlan.Pages.Contains(Page) && FinePageReady.Contains(Page) && !Target.bDirty;
			Target.bFailed = FinePageFailures.Contains(Page);
			Target.bKnownEmpty = Target.bMeshReady && EmptyFinePages.Contains(Page);
			Target.bPublishing = PendingFinePages.Contains(Page) || Publisher->IsGroupBusy({0, Key.Coordinate, Key.Level});
			TArray<FIntVector> Sections;
			FVoxelFinePagePlanner::GatherSections(Page, Sections);
			Target.RequiredSections = Sections.Num();
			for (const FIntVector& Section : Sections)
			{
				const FVoxelSection* Data = Module.GetRuntime()->FindSection(Section);
				Target.ReadySections += Data && Data->Status == EVoxelSectionStatus::DataReady ? 1 : 0;
				Target.bFailed |= Data && Data->Status == EVoxelSectionStatus::Failed;
				const FVoxelFinePageKey* Owner = PresentedFineSectionOwners.Find(Section);
				Target.PresentedSections += Owner && *Owner == Page && FineReady.Contains(Section) && IsFinePresented(Section) ? 1 : 0;
			}
			Target.bDataReady = Target.RequiredSections > 0 && Target.ReadySections == Target.RequiredSections;
			Target.bPresented = Target.bMeshReady && Target.PresentedSections == Target.RequiredSections;
		}
		else
		{
			FVoxelViewAdmission Admission;
			uint8 GroupKind = 0;
			switch (Key.Representation)
			{
			case EVoxelTerrainRepresentation::VoxelProxy:
				Admission.Kind = EVoxelViewAdmissionKind::VoxelProxy;
				Admission.ProxyKey = {Key.Coordinate, Key.Level};
				Actor = VoxelProxyActors.FindRef(Admission.ProxyKey);
				GroupKind = 1;
				break;
			case EVoxelTerrainRepresentation::Surface:
				Admission.Kind = EVoxelViewAdmissionKind::Surface;
				Admission.SurfaceKey = {FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level};
				Actor = SurfaceActors.FindRef(Admission.SurfaceKey);
				GroupKind = 2;
				break;
			case EVoxelTerrainRepresentation::Macro:
				Admission.Kind = EVoxelViewAdmissionKind::Macro;
				Admission.MacroKey = {FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level};
				Actor = MacroActors.FindRef(Admission.MacroKey);
				GroupKind = 3;
				break;
			default:
				continue;
			}
			Target.bDataReady = IsAdmissionDataReady(Admission);
			Target.bMeshReady = IsAdmissionMeshReady(Admission);
			Target.bPresented = IsAdmissionPresented(Admission);
			Target.bFailed = IsAdmissionTerminalFailure(Admission);
			Target.bPublishing = Publisher->IsGroupBusy({GroupKind, Key.Coordinate, Key.Level});
			if (const FVoxelTerrainGeometryMetrics* Metrics = GeometryMetrics.Find(Key))
			{
				Target.bKnownEmpty = Target.bMeshReady && Metrics->Geometry == Target.Geometry && Metrics->bGenerated && Metrics->Triangles == 0;
			}
		}
		Target.bVisible = Target.bMeshReady && Actor && !Actor->IsHidden() && Publisher->IsPresented(Actor);
	}
	return Snapshot;
}

FVoxelPrimaryFineReadiness FVoxelViewManager::GetPrimaryFineReadiness(const TMap<FIntVector, FVoxelExactDemand>& InExact) const
{
	FVoxelPrimaryFineReadiness Result;
	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : InExact)
	{
		const FVoxelExactDemand& Demand = Pair.Value;
		if (!Demand.bWarmupData || !Demand.bFineRender)
		{
			continue;
		}

		++Result.Required;
		if (const auto* Page = FinePagePlan.SectionOwners.Find(Pair.Key))
		{
			Result.bFailed |= FinePageFailures.Contains(*Page);
		}
		if (FineReady.Contains(Pair.Key))
		{
			++Result.Ready;
			AActor* Actor = FineActor(Pair.Key);
			// 空结果也必须取得区域所有权；已准备但仍隐藏的网格不能放行出生。
			const bool bOwnsRegion = GetViewInterest().TerrainPlan.Roots.IsEmpty() || VisibleTerrainNodes.Contains({Pair.Key, 0});
			Result.Owned += bOwnsRegion ? 1 : 0;
			Result.Hidden += Actor && Actor->IsHidden() ? 1 : 0;
			Result.PublicationPending += Actor && !Publisher->IsCommitted(Actor) ? 1 : 0;
			if (bOwnsRegion && IsFinePresented(Pair.Key))
			{
				++Result.Presented;
			}
		}
		AActor* Actor = FineActor(Pair.Key);
		if (Actor && !Actor->IsHidden() && Publisher->IsPresented(Actor))
		{
			++Result.Renderable;
		}
	}
	return Result;
}

FVoxelPrimaryFineReadiness FVoxelViewManager::GetFineRadiusReadiness(const TConstArrayView<FIntVector> InFineKeys) const
{
	FVoxelPrimaryFineReadiness Result;
#if !UE_BUILD_SHIPPING
	const bool bLogIncomplete = FPlatformTime::Seconds() >= NextIncompleteFineLog;
	if (bLogIncomplete)
		NextIncompleteFineLog = FPlatformTime::Seconds() + 5.0;
#endif
	for (const FIntVector& Key : InFineKeys)
	{
		++Result.Required;
		if (const auto* Page = FinePagePlan.SectionOwners.Find(Key))
		{
			Result.bFailed |= FinePageFailures.Contains(*Page);
		}
#if !UE_BUILD_SHIPPING
		if (bLogIncomplete && !FineReady.Contains(Key))
			UE_LOG(LogTemp, Display, TEXT("Voxel playable Fine dependency: key=%s wanted=%d exactFine=%d page=%d data=%d hash=%u"), *Key.ToString(),
				FineWanted.Contains(Key), GetViewInterest().Exact.Find(Key) && GetViewInterest().Exact.FindChecked(Key).bFineRender,
				FinePagePlan.SectionOwners.Contains(Key),
				Module.GetRuntime()->FindSection(Key) ? static_cast<int32>(Module.GetRuntime()->FindSection(Key)->Status) : -1, GetTypeHash(Key));
#endif
		if (FineReady.Contains(Key))
		{
			++Result.Ready;
			AActor* Actor = FineActor(Key);
			const bool bOwnsRegion = GetViewInterest().TerrainPlan.Roots.IsEmpty() || VisibleTerrainNodes.Contains({Key, 0});
			Result.Owned += bOwnsRegion ? 1 : 0;
			Result.Hidden += Actor && Actor->IsHidden() ? 1 : 0;
			Result.PublicationPending += Actor && !Publisher->IsCommitted(Actor) ? 1 : 0;
			if (bOwnsRegion && IsFinePresented(Key))
			{
				++Result.Presented;
			}
		}
	}
	return Result;
}

void FVoxelViewManager::AdvanceInterestInputs(const uint64 InRevision, TConstArrayView<FVector> InObservers)
{
	if (!PendingPublicationInputs && AppliedInterestSnapshot && AppliedInterestRevision == Module.GetCurrentInterest().PublicationRevision)
		return;
	const auto Latest = Module.GetCurrentInterestSnapshot();
	if (!Latest || Latest->PublicationRevision == 0)
	{
		return;
	}
	if (Latest && (!QueuedInterestSnapshot || QueuedInterestSnapshot->PublicationRevision != Latest->PublicationRevision))
	{
		QueuedInterestSnapshot = Latest;
	}
	if (!PendingPublicationInputs)
	{
		const double Start = FPlatformTime::Seconds();
		if (!TerrainFrameBudget.TryStart(EVoxelPublishPhase::Plan, Start, InterestMicrostepEstimateMilliseconds))
			return;
		PendingInterestSnapshot = QueuedInterestSnapshot;
		QueuedInterestSnapshot.Reset();
		if (!PendingInterestSnapshot)
		{
			return;
		}
		PendingPublicationInputs = PendingInterestSnapshot->PublicationInputs;
		checkf(PendingPublicationInputs, TEXT("Accepted terrain interest must carry prepared publication inputs"));
		PendingInterestRevision = PendingInterestSnapshot->PublicationRevision;
		InterestInputPhase = InterestInputCursor = 0;
		PendingFinePagePlan = {};
		PendingLayoutInvalidations.Reset();
		bPendingPageLayoutChanged = false;
		PriorityObservers = TArray<FVector>(InObservers);
		MaximumTerrainRootLevel = 0;
		const double Finished = FPlatformTime::Seconds();
		InterestMicrostepEstimateMilliseconds = FMath::Clamp(InterestMicrostepEstimateMilliseconds * 0.8 + (Finished - Start) * 200.0, 0.001,
			double(Module.GetViewSettings().Budget.TerrainPublishMilliseconds) * 0.125);
		TerrainFrameBudget.Finish(EVoxelPublishPhase::Plan, Start, Finished);
	}
	TGuardValue<bool> ConsumingGuard(bConsumingPublicationInputs, true);
	const FVoxelInterestSet& Interest = *PendingInterestSnapshot;
	const auto& Inputs = *PendingPublicationInputs;
	const auto NextPhase = [this]
	{
		++InterestInputPhase;
		InterestInputCursor = 0;
	};
	const auto InvalidateLayoutOnce = [this](const FVoxelFinePageKey& Page)
	{
		bool bAlreadyInvalidated = false;
		PendingLayoutInvalidations.Add(Page, &bAlreadyInvalidated);
		if (!bAlreadyInvalidated)
		{
			InvalidateFinePageLayout(Page);
		}
	};
	const auto RemoveOne = [this, &NextPhase](auto& Wanted, const auto& Desired, auto&& Removed)
	{
		if (InterestInputCursor >= Wanted.GetMaxIndex())
		{
			NextPhase();
			return;
		}
		const auto Id = FSetElementId::FromInteger(InterestInputCursor);
		if (!Wanted.IsValidId(Id))
		{
			++InterestInputCursor;
			return;
		}
		const auto Key = Wanted[Id];
		if (Desired.Contains(Key))
		{
			++InterestInputCursor;
			return;
		}
		Wanted.Remove(Id);
		Removed(Key);
	};
	const auto AddOne = [this, &NextPhase](const auto& Keys, auto&& Added)
	{
		if (!Keys.IsValidIndex(InterestInputCursor))
		{
			NextPhase();
			return;
		}
		Added(Keys[InterestInputCursor++]);
	};
	const double InputDeadline = TerrainDeadline(1.0);
	for (int32 Operations = 0; Operations < 8192 && FPlatformTime::Seconds() < InputDeadline; ++Operations)
	{
		const double Start = FPlatformTime::Seconds();
		if (!TerrainFrameBudget.TryStart(EVoxelPublishPhase::Plan, Start, InterestMicrostepEstimateMilliseconds))
			return;
		switch (InterestInputPhase)
		{
		case 0:
			RemoveOne(IndexedTerrainRoots, Interest.TerrainPlan.Roots,
				[this](const auto& Key)
				{
					TerrainRootIndex.Remove({EVoxelTerrainRepresentation::VoxelProxy, Key.Coordinate, Key.Level});
				});
			break;
		case 1:
			AddOne(Inputs.Roots,
				[this](const auto& Key)
				{
					MaximumTerrainRootLevel = FMath::Max(MaximumTerrainRootLevel, Key.Level);
					if (IndexedTerrainRoots.Contains(Key))
						return;
					auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
					Record->Geometry.Key = {EVoxelTerrainRepresentation::VoxelProxy, Key.Coordinate, Key.Level};
					const auto Bounds = Key.GetBounds();
					Record->Min = Bounds.Min;
					Record->Max = Bounds.Max;
					FString Error;
					checkf(TerrainRootIndex.Upsert(Record, Error), TEXT("%s"), *Error);
					IndexedTerrainRoots.Add(Key);
				});
			break;
		case 2:
			RemoveOne(FineWanted, *Interest.FineSections,
				[this](const auto& Key)
				{
					FineLastWanted.Add(Key, FPlatformTime::Seconds());
					IndexCoverage(FVoxelViewKey{Key, 0});
					QueueRetirement({4, Key, 0});
					if (const auto* Page = FinePagePlan.SectionOwners.Find(Key))
						MarkFinePageDirty(*Page);
					MarkCoverageDirty(FineCoverageRect(Key));
					bPendingPageLayoutChanged = true;
				});
			break;
		case 3:
			AddOne(Inputs.Fine,
				[this, &Interest, &InvalidateLayoutOnce](const auto& Key)
				{
					const bool bChanged = !FineWanted.Contains(Key);
					FineWanted.Add(Key);
					if (bChanged)
						IndexCoverage(FVoxelViewKey{Key, 0});
					const auto* Old = FinePagePlan.SectionOwners.Find(Key);
					const auto* Next = Interest.FinePagePlan->SectionOwners.Find(Key);
					if (Next && (!Old || *Old != *Next))
					{
						InvalidateLayoutOnce(*Next);
						if (Old)
							InvalidateLayoutOnce(*Old);
						MarkCoverageDirty(FineCoverageRect(Key));
						bPendingPageLayoutChanged = true;
					}
				});
			break;
		case 4:
			RemoveOne(VoxelProxyWanted, Interest.VoxelProxy,
				[this](const auto& Key)
				{
					VoxelProxyLastWanted.Add(Key, FPlatformTime::Seconds());
					IndexCoverage(Key);
					QueueRetirement({1, Key.Coordinate, Key.Level});
					MarkCoverageDirty(VoxelProxyCoverageRect(Key));
				});
			break;
		case 5:
			AddOne(Inputs.Proxy,
				[this](const auto& Key)
				{
					const bool Changed = !VoxelProxyWanted.Contains(Key);
					VoxelProxyWanted.Add(Key);
					if (Changed)
					{
						IndexCoverage(Key);
						MarkCoverageDirty(VoxelProxyCoverageRect(Key));
					}
				});
			break;
		case 6:
			RemoveOne(SurfaceWanted, Interest.Surface,
				[this](const auto& Key)
				{
					SurfaceLastWanted.Add(Key, FPlatformTime::Seconds());
					IndexCoverage(Key);
					QueueRetirement({2, Key.Coordinate, Key.Level});
					MarkCoverageDirty(SurfaceCoverageRect(Key));
				});
			break;
		case 7:
			AddOne(Inputs.Surface,
				[this](const auto& Key)
				{
					const bool Changed = !SurfaceWanted.Contains(Key);
					SurfaceWanted.Add(Key);
					if (Changed)
					{
						IndexCoverage(Key);
						MarkCoverageDirty(SurfaceCoverageRect(Key));
					}
				});
			break;
		case 8:
			RemoveOne(MacroWanted, Interest.Macro,
				[this](const auto& Key)
				{
					MacroLastWanted.Add(Key, FPlatformTime::Seconds());
					IndexCoverage(Key);
					QueueRetirement({3, Key.Coordinate, Key.Level});
					MarkCoverageDirty(MacroCoverageRect(Key));
				});
			break;
		case 9:
			AddOne(Inputs.Macro,
				[this](const auto& Key)
				{
					const bool Changed = !MacroWanted.Contains(Key);
					MacroWanted.Add(Key);
					if (Changed)
					{
						IndexCoverage(Key);
						MarkCoverageDirty(MacroCoverageRect(Key));
					}
				});
			break;
		case 10:
			AddOne(Inputs.FinePages,
				[this](const auto& Page)
				{
					PendingFinePagePlan.Pages.Add(Page);
				});
			break;
		case 11:
			AddOne(Inputs.FineOwners,
				[this](const auto& Pair)
				{
					PendingFinePagePlan.SectionOwners.Add(Pair.Key, Pair.Value);
				});
			break;
		case 12:
		{
			if (InterestInputCursor >= FinePagePlan.Pages.GetMaxIndex())
			{
				NextPhase();
				break;
			}
			const auto Id = FSetElementId::FromInteger(InterestInputCursor++);
			if (!FinePagePlan.Pages.IsValidId(Id))
				break;
			const auto Page = FinePagePlan.Pages[Id];
			if (!PendingFinePagePlan.Pages.Contains(Page))
			{
				RetireFinePageBuildState(Page, true);
				QueueRetirement({0, Page.Coordinate, Page.Level});
			}
			break;
		}
		case 13:
			NextPhase();
			break;
		case 14:
			AddOne(Inputs.Geometry,
				[this](const auto& Geometry)
				{
					if (!Module.GetRuntime()->GetChangeHierarchy().RegisterGeometry(Geometry))
						return;
					FIntVector Min, Max;
					Geometry.Key.TryBounds(Geometry.WorldMinZ, Geometry.WorldMaxZ, Min, Max);
					MarkCoverageDirty({FIntPoint(Min.X, Min.Y), FIntPoint(Max.X, Max.Y)});
					bHeightfieldTopologyDirty |= Geometry.Key.IsColumn();
				});
			break;
		case 15:
			FinePagePlan = MoveTemp(PendingFinePagePlan);
			if (bPendingPageLayoutChanged)
				++FinePageLayoutRevision;
			bFinePresentationDirty |= !DirtyFinePages.IsEmpty();
			Admissions = PendingInterestSnapshot->Admissions;
			AdmissionPriorities = PendingInterestSnapshot->AdmissionPriorities;
			NextPhase();
			break;
		case 16:
			CancelStaleViewTasks();
			NextPhase();
			break;
		case 17:
			UpdateTaskPriorities();
			NextPhase();
			break;
		case 18:
			Publisher->InvalidatePriorities();
			AppliedInterestRevision = PendingInterestRevision;
			AppliedInterestSnapshot = PendingInterestSnapshot;
			PendingInterestSnapshot.Reset();
			PendingPublicationInputs.Reset();
			PendingLayoutInvalidations.Reset();
			TerrainFrameBudget.Finish(EVoxelPublishPhase::Plan, Start, FPlatformTime::Seconds());
			return;
		}
		const double Finished = FPlatformTime::Seconds();
		InterestMicrostepEstimateMilliseconds = FMath::Clamp(InterestMicrostepEstimateMilliseconds * 0.8 + (Finished - Start) * 200.0, 0.001,
			double(Module.GetViewSettings().Budget.TerrainPublishMilliseconds) * 0.125);
		TerrainFrameBudget.Finish(EVoxelPublishPhase::Plan, Start, Finished);
	}
}

void FVoxelViewManager::UpdateFineAndVoxelProxy(const TConstArrayView<FVector> InObservers)
{
	const FVoxelInterestSet& Interest = GetViewInterest();
	const auto& NewRoots = Interest.TerrainPlan.Roots;
	MaximumTerrainRootLevel = 0;
	for (const auto& Root : IndexedTerrainRoots)
		if (!NewRoots.Contains(Root))
			TerrainRootIndex.Remove({EVoxelTerrainRepresentation::VoxelProxy, Root.Coordinate, Root.Level});
	for (const auto& Root : NewRoots)
	{
		MaximumTerrainRootLevel = FMath::Max(MaximumTerrainRootLevel, Root.Level);
		if (IndexedTerrainRoots.Contains(Root))
			continue;
		auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
		Record->Geometry.Key = {EVoxelTerrainRepresentation::VoxelProxy, Root.Coordinate, Root.Level};
		const auto Bounds = Root.GetBounds();
		Record->Min = Bounds.Min;
		Record->Max = Bounds.Max;
		FString Error;
		checkf(TerrainRootIndex.Upsert(Record, Error), TEXT("%s"), *Error);
	}
	IndexedTerrainRoots = NewRoots;
	const TSet<FIntVector> PreviousFine = FineWanted;
	const TSet<FVoxelViewKey> PreviousProxy = VoxelProxyWanted;
	FineWanted = Interest.FineSections ? *Interest.FineSections : TSet<FIntVector>();
	VoxelProxyWanted = Interest.VoxelProxy;
	for (const auto& Key : PreviousFine)
		if (!FineWanted.Contains(Key))
			IndexCoverage(FVoxelViewKey{Key, 0});
	for (const auto& Key : FineWanted)
		if (!PreviousFine.Contains(Key))
			IndexCoverage(FVoxelViewKey{Key, 0});
	for (const auto& Key : PreviousProxy)
		if (!VoxelProxyWanted.Contains(Key))
			IndexCoverage(Key);
	for (const auto& Key : VoxelProxyWanted)
		if (!PreviousProxy.Contains(Key))
			IndexCoverage(Key);
	RebuildFinePagePlan();
}

void FVoxelViewManager::UpdateSurface(const TConstArrayView<FVector> InObservers)
{
	const auto Previous = SurfaceWanted;
	SurfaceWanted = GetViewInterest().Surface;
	for (const auto& Key : Previous)
		if (!SurfaceWanted.Contains(Key))
			IndexCoverage(Key);
	for (const auto& Key : SurfaceWanted)
		if (!Previous.Contains(Key))
			IndexCoverage(Key);
}

void FVoxelViewManager::UpdateMacro(const TConstArrayView<FVector> InObservers)
{
	const auto Previous = MacroWanted;
	MacroWanted = GetViewInterest().Macro;
	for (const auto& Key : Previous)
		if (!MacroWanted.Contains(Key))
			IndexCoverage(Key);
	for (const auto& Key : MacroWanted)
		if (!Previous.Contains(Key))
			IndexCoverage(Key);
}

bool FVoxelViewManager::TryResolveFineWithoutMesh(const FIntVector& InSection, const uint64 InRevision)
{
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(InSection);
	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();
	if (!Section || Section->Status != EVoxelSectionStatus::DataReady || Section->CommittedRevision != InRevision ||
		Section->Blocks.Num() != ViewSectionSide * ViewSectionSide * ViewSectionSide || !Registry)
	{
		return false;
	}

	auto IsOccludingFullCube = [&Registry](const FVoxelBlockState& Block)
	{
		if (Block.IsAir())
		{
			return false;
		}
		const FVoxelRuntimeDefinition* Definition = Registry->Find(Block.TypeId);
		return Definition && Definition->Shape == EVoxelShapeKind::FullCube && Definition->bOccludes;
	};

	bool bAllAir = true;
	bool bFullyOccluding = true;
	for (const FVoxelBlockState& Block : Section->Blocks)
	{
		if (Block.IsAir())
		{
			bFullyOccluding = false;
			continue;
		}
		bAllAir = false;
		if (!IsOccludingFullCube(Block))
		{
			bFullyOccluding = false;
			break;
		}
	}

	if (!bAllAir && !bFullyOccluding)
	{
		return false;
	}

	if (bFullyOccluding)
	{
		for (int32 Face = 0; Face < 6; ++Face)
		{
			const int32 Axis = Face / 2;
			const int32 UAxis = (Axis + 1) % 3;
			const int32 VAxis = (Axis + 2) % 3;
			FIntVector NeighborKey = InSection;
			NeighborKey[Axis] += Face % 2 == 0 ? 1 : -1;
			const FVoxelSection* Neighbor = Module.GetRuntime()->FindSection(NeighborKey);
			if (!Neighbor || Neighbor->Status != EVoxelSectionStatus::DataReady ||
				Neighbor->Blocks.Num() != ViewSectionSide * ViewSectionSide * ViewSectionSide)
			{
				return false;
			}

			for (int32 V = 0; V < ViewSectionSide; ++V)
			{
				for (int32 U = 0; U < ViewSectionSide; ++U)
				{
					FIntVector Local = FIntVector::ZeroValue;
					Local[Axis] = Face % 2 == 0 ? 0 : ViewSectionSide - 1;
					Local[UAxis] = U;
					Local[VAxis] = V;
					const int32 Index = Local.X + ViewSectionSide * (Local.Y + ViewSectionSide * Local.Z);
					if (!IsOccludingFullCube(Neighbor->Blocks[Index]))
					{
						return false;
					}
				}
			}
		}
	}

	FVoxelTaskResult Result;
	Result.Kind = EVoxelTaskKind::BuildFineMesh;
	Result.Stamp.WorldEpoch = WorldEpoch;
	Result.Stamp.Token = Section->Stamp.Token;
	Result.Stamp.Revision = InRevision;
	Result.Stamp.Section = InSection;
	Result.bSuccess = true;
	Result.FineMesh = MakeShared<FVoxelSectionMeshResult>();
	const bool bPublished = PublishFine(Result);
	if (bPublished)
		MarkCoverageDirty(FineCoverageRect(InSection));
	return bPublished;
}
bool FVoxelViewManager::RequestFine(const FIntVector& InSection, const uint64 InRevision)
{
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(InSection);

	if (!Section || Section->Status != EVoxelSectionStatus::DataReady)
	{
		return false;
	}

	FVoxelTaskStamp Stamp;
	Stamp.WorldEpoch = WorldEpoch;
	Stamp.Token = Section->Stamp.Token;
	Stamp.Revision = InRevision;
	Stamp.Section = InSection;

	if (Scheduler.Has(Stamp, EVoxelTaskKind::BuildFineMesh))
	{
		return false;
	}

	for (int32 Face = 0; Face < 6; ++Face)
	{
		FIntVector Neighbor = InSection;
		Neighbor[Face / 2] += (Face & 1) ? -1 : 1;
		const FVoxelSection* Data = Module.GetRuntime()->FindSection(Neighbor);
		if (GetViewInterest().Exact.Contains(Neighbor) && (!Data || Data->Status != EVoxelSectionStatus::DataReady))
			return false;
	}
	FVoxelSectionSnapshot Snapshot;
	if (!Module.GetRuntime()->CaptureSnapshot(InSection, Snapshot))
		return false;

	FVoxelTaskRequest Request;

	Request.Kind = EVoxelTaskKind::BuildFineMesh;

	const FVoxelExactDemand* Demand = GetViewInterest().Exact.Find(InSection);
	Request.WorkClass = Demand && (Demand->bWarmupData || Demand->bWarmupCollision || Demand->bMovementCriticalData || Demand->bMovementCriticalCollision ||
									  Demand->bMovementCriticalFine)
							? EVoxelWorkClass::Critical
							: EVoxelWorkClass::Interactive;

	Request.Stamp = Stamp;
	Request.DistanceScore = MinimumObserverDistanceCells(FVector(InSection * ViewSectionSide + FIntVector(ViewSectionSide / 2)) * Module.BlockSize());
	Request.SourcePriority = Demand ? Demand->Priority : 0;
	Request.ForwardScore = Demand ? Demand->ForwardScore : 0.0;

	Request.InputBytes = Snapshot.Bytes();

	Request.ReservedBytes = 48ull * 1024ull * 1024ull;

	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();

	const TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes = Module.GetShapes();

	Request.Execute = [Snapshot = MoveTemp(Snapshot), Registry, Shapes](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;

		Result.FineMesh = MakeShared<FVoxelSectionMeshResult>();

		Result.bSuccess = Registry && Shapes && FVoxelSectionMesher::Build(Snapshot, *Registry, *Shapes, *Result.FineMesh, &InCancel);

		return Result;
	};

	return EnqueueViewTask(MoveTemp(Request));
}

bool FVoxelViewManager::BuildVoxelProxySnapshot(
	const FVoxelVoxelProxyData& InData, const uint64 InRecipeHash, FVoxelSectionSnapshot& OutSnapshot, FString& OutError)
{
	if (InData.Geometry.SampleStepCells <= 0 || InData.GridSide != InData.Key.GetSide() / InData.Geometry.SampleStepCells ||
		InData.Cells.Num() != InData.GridSide * InData.GridSide * InData.GridSide)
	{
		OutError = TEXT("Voxel proxy snapshot has invalid cell data");
		return false;
	}

	FVoxelSectionSnapshot Snapshot;
	Snapshot.GridSide = InData.GridSide;
	Snapshot.Section = InData.Key.Coordinate;
	Snapshot.Stamp.RecipeHash = InRecipeHash;
	Snapshot.Revision = InData.Revision;
	Snapshot.Blocks.Reserve(InData.Cells.Num());
	for (const FVoxelBlockState State : InData.Cells)
		Snapshot.Blocks.Add(State.Pack());

	for (int32 Face = 0; Face < 6; ++Face)
	{
		Snapshot.Known[Face] = InData.Known[Face];
		if (!Snapshot.Known[Face])
			continue;
		const TArray<FVoxelBlockState>& Source = InData.Halo[Face];
		if (Source.Num() != InData.GridSide * InData.GridSide)
		{
			OutError = TEXT("Voxel proxy snapshot has invalid halo data");
			return false;
		}
		Snapshot.Halo[Face].Reserve(Source.Num());
		for (const FVoxelBlockState State : Source)
			Snapshot.Halo[Face].Add(State.Pack());
	}

	OutSnapshot = MoveTemp(Snapshot);
	OutError.Reset();
	return true;
}

bool FVoxelViewManager::RequestVoxelProxy(const FVoxelViewKey& InKey, const int32 InPriority, const bool bDataOnly)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InKey);
	if (!DesiredGeometry)
		return false;
	const FVoxelTerrainGeometry Geometry = *DesiredGeometry;
	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();
	if (!Config || !Config->Recipe)
		return false;
	const int32 Step = Geometry.SampleStepCells;
	const FVoxelTreeGenerationSettings& Tree = Config->Recipe->Settings.Ecology.Tree;
	const int32 Crown = Tree.bEnabled ? Tree.CrownRadius : 0;
	const int32 TreeHeight = Tree.bEnabled ? Tree.MaxHeight + Crown : 0;
	const int32 SampleStep = Geometry.SampleStepCells;
	const int32 TreePadding = Tree.bEnabled ? (FMath::Max(1, (Crown - 1) / SampleStep) + 1) * SampleStep : 0;
	const int32 HorizontalMargin = FMath::Max(Step + Crown, TreePadding);
	FVoxelGenerationBounds OverlayBounds = InKey.GetBounds();
	OverlayBounds.Min -= FIntVector(HorizontalMargin, HorizontalMargin, Step + TreeHeight);
	OverlayBounds.Max += FIntVector(HorizontalMargin, HorizontalMargin, Step);
	if (!Module.IsAuthority())
	{
		TArray<FIntVector> Modified;

		Module.GetRuntime()->GetChangeIndex().Enumerate(OverlayBounds, Modified);

		FVoxelTaskStamp RemoteDataStamp;
		RemoteDataStamp.WorldEpoch = WorldEpoch;
		RemoteDataStamp.GeometryRevision = GetTypeHash(Geometry);
		RemoteDataStamp.ViewKey = InKey;
		RemoteDataStamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(InKey);
		RemoteDataStamp.Token = HashCombineFast(GetTypeHash(InKey), GetTypeHash(RemoteDataStamp.Revision));
		if (!Modified.IsEmpty() && !PreparedData.Contains({EVoxelTaskKind::GenerateVoxelProxy, RemoteDataStamp}))
		{
			if (bDataOnly)
				return false;
			const uint64* Revision = VoxelProxyRevisions.Find(InKey);

			return Module.RequestRemoteRepresentation(EVoxelRepresentationWireType::VoxelProxy, {InKey.Coordinate, InKey.Level}, Revision ? *Revision : 0);
		}
	}

	const TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache = Module.GetGenerationCache();
	const TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator = Module.GetGenerator();

	if (!Config || !Cache)
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.SourcePriority = InPriority;

	Request.Kind = EVoxelTaskKind::BuildVoxelProxy;

	Request.TerrainStage = VoxelProxyTerrainStage(InKey);
	Request.WorkClass = Request.TerrainStage == 0 ? EVoxelWorkClass::Interactive : EVoxelWorkClass::Boundary;

	const bool bCriticalDependency =
		IsCriticalDependency(FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::VoxelProxy, FIntVector(InKey.Coordinate), InKey.Level});
	if (bCriticalDependency)
	{
		Request.WorkClass = EVoxelWorkClass::Critical;
		Request.TerrainStage = 0;
	}
	Request.Stamp.GeometryRevision = GetTypeHash(Geometry);
	Request.Stamp.WorldEpoch = WorldEpoch;

	Request.Stamp.ViewKey = InKey;

	Request.Stamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(InKey);

	Request.Stamp.Token = HashCombineFast(GetTypeHash(InKey), GetTypeHash(Request.Stamp.Revision));

	Request.ReservedBytes = 128ull * 1024ull * 1024ull;
	const FVoxelGenerationBounds ProxyBounds = InKey.GetBounds();
	Request.DistanceScore = GetViewInterest().TerrainPlan.FineDependencies.Contains(InKey) ? 0.0 : MinimumObserverDistanceCells(ProxyBounds);
	Request.ForwardScore = 0.0;

	if (Scheduler.Has(Request.Stamp, Request.Kind))
	{
		return false;
	}

	const FVoxelTaskKey DataKey{EVoxelTaskKind::GenerateVoxelProxy, Request.Stamp};
	TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> Prepared;
	if (bDataOnly)
	{
		if (PreparedData.Contains(DataKey) || Scheduler.Has(Request.Stamp, DataKey.Kind))
			return false;
		Request.Kind = DataKey.Kind;
		const uint64 Side = InKey.GetSide() / Geometry.SampleStepCells;
		Request.ReservedBytes = 64ull * 1024ull + 2ull * (Side * Side * Side + 6ull * Side * Side) * sizeof(FVoxelBlockState);
		Request.WorkClass = bCriticalDependency ? EVoxelWorkClass::Critical : EVoxelWorkClass::Prefetch;
	}
	else
	{
		Prepared = PreparedData.FindRef(DataKey);
		if (!Prepared || !Prepared->VoxelProxy || Prepared->VoxelProxy->Geometry != Geometry)
			return false;
		if (Prepared->bKnownEmptyMesh)
		{
			FVoxelTaskResult Empty;
			Empty.Stamp = Request.Stamp;
			Empty.VoxelProxy = Prepared->VoxelProxy;
			Empty.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
			Empty.VoxelProxyMesh->ValidateAndCache();
			if (!PublishVoxelProxy(Empty))
				return false;
			++SkippedProxyMeshes;
			MarkCoverageDirty(VoxelProxyCoverageRect(InKey));
			RemovePreparedData(DataKey);
			return true;
		}
	}

	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();

	const TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes = Module.GetShapes();

	const uint64 RecipeHash = Module.GetManifest().RecipeHash;
	FVoxelOverlaySnapshotSet Overlays;
	FString OverlayError;
	Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Dependency, Request.InputBytes, Request.ReservedBytes);
	const auto EnvironmentRetention = bDataOnly ? RetainPreparedEnvironment(DataKey, *Generator, OverlayBounds) : nullptr;
	if (bDataOnly && !Module.EnsureEnvironmentBounds(OverlayBounds, Request.WorkClass, InPriority, Request.DistanceScore, Request.ForwardScore).bReady)
		return false;
	if (bDataOnly && !Generator->EnsureCoarseOverlayBoundsReady(OverlayBounds, Request.WorkClass, InPriority).bReady)
		return false;
	if (bDataOnly && !Module.CaptureOverlays(OverlayBounds, Overlays, OverlayError))
		return false;
	Request.InputBytes = Prepared ? Prepared->ResultBytes() : Overlays.GetAllocatedBytes();

	TSharedPtr<FProxyMeshEstimate, ESPMode::ThreadSafe> MeshEstimate;
	TSharedPtr<FVoxelProxyPageBuild, ESPMode::ThreadSafe> PageBuild;
	if (bDataOnly)
	{
		PageBuild = MakeShared<FVoxelProxyPageBuild, ESPMode::ThreadSafe>();
		FString Error;
		MeshEstimate = MakeShared<FProxyMeshEstimate, ESPMode::ThreadSafe>();
		if (!PageBuild->Initialize(Geometry, Error) || !MeshEstimate->Counter.Initialize(Geometry, Error, 0, true))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			UE_LOG(LogTemp, Warning, TEXT("Proxy page retained after capacity failure: %s"), *Error);
			return false;
		}
		Request.bResumable = true;
		Request.InputBytes += PageBuild->MaximumRetainedBytes();
		Request.ReservedBytes += PageBuild->MaximumRetainedBytes();
	}
	auto PageMesh = MakeShared<FVoxelProxyPageMesh, ESPMode::ThreadSafe>();
	if (!bDataOnly)
	{
		FString Error;
		const uint64 RetainedBytes = Prepared->VoxelProxy->EstimatedMeshBytes + 64ull * 1024ull;
		if (!PageMesh->Initialize(Geometry, Error, RetainedBytes))
			return false;
		Request.bResumable = true;
		Request.InputBytes += RetainedBytes + RetainedBytes / 2;
		Request.ReservedBytes = RetainedBytes + Prepared->ResultBytes();
	}
	Request.Execute = [bDataOnly, PageBuild, MeshEstimate, PageMesh, Prepared, Config, Cache, Generator, EnvironmentRetention, Registry, Shapes, RecipeHash,
						  Overlays = MoveTemp(Overlays), InKey, Geometry, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells,
						  Lod = Module.GetViewSettings().Lod](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;

		if (bDataOnly)
		{
			if (MeshEstimate->Data)
			{
				FVoxelSectionMeshResult CountedMesh;
				bool bComplete = false;
				Result.bSuccess = MeshEstimate->Counter.Advance(*MeshEstimate->Data, *Registry, *Shapes, nullptr,
					Geometry.SampleStepCells / VoxelViewLod::TexturePeriodCells(Geometry.SampleStepCells, TextureStretch), CountedMesh, bComplete, Result.Error,
					&InCancel);
				if (!Result.bSuccess)
					return Result;
				if (!bComplete)
				{
					Result.bContinue = true;
					return Result;
				}
				MeshEstimate->Data->EstimatedMeshBytes = MeshEstimate->Counter.GetEstimatedMeshBytes();
				Result.VoxelProxy = MeshEstimate->Data;
				Result.bKnownEmptyMesh = Result.VoxelProxy->EstimatedMeshBytes == 0;
				return Result;
			}
			Result.VoxelProxy = MakeShared<FVoxelVoxelProxyData>();

			const FVoxelVoxelProxyBuilder Builder(Config.ToSharedRef(), Cache.ToSharedRef(), Generator);

			bool bComplete = false;
			Result.bSuccess = PageBuild->Advance(Builder, Overlays, *Result.VoxelProxy, bComplete, Result.Error, &InCancel);
			if (Result.bSuccess && !bComplete)
			{
				Result.VoxelProxy.Reset();
				Result.bContinue = true;
				return Result;
			}
			if (Result.bSuccess && Registry)
			{
				FVoxelSectionSnapshot Snapshot;
				Result.bSuccess = BuildVoxelProxySnapshot(*Result.VoxelProxy, RecipeHash, Snapshot, Result.Error);
				Result.bKnownEmptyMesh = Result.bSuccess && FVoxelSectionMesher::IsKnownEmpty(Snapshot, *Registry);
			}

			if (Result.bSuccess && !Result.bKnownEmptyMesh)
			{
				MeshEstimate->Data = Result.VoxelProxy;
				Result.VoxelProxy.Reset();
				Result.bContinue = true;
			}
			return Result;
		}
		Result.bSuccess = true;
		Result.VoxelProxy = Prepared->VoxelProxy;

		if (!Result.bSuccess || !Registry || !Shapes)
		{
			return Result;
		}

		Result.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
		bool bComplete = false;
		Result.bSuccess = PageMesh->Advance(*Result.VoxelProxy, *Registry, *Shapes, nullptr,
			Geometry.SampleStepCells / VoxelViewLod::TexturePeriodCells(Geometry.SampleStepCells, TextureStretch), *Result.VoxelProxyMesh, bComplete,
			Result.Error, &InCancel);
		if (Result.bSuccess && !bComplete)
		{
			Result.VoxelProxy.Reset();
			Result.VoxelProxyMesh.Reset();
			Result.bContinue = true;
		}

		return Result;
	};

	if (bDataOnly)
		return EnqueuePreparedData(MoveTemp(Request));
	if (!EnqueueViewTask(MoveTemp(Request)))
		return false;
	RemovePreparedData(DataKey);
	return true;
}

bool FVoxelViewManager::RequestSurface(const FVoxelSurfaceTileKey& InKey, const int32 InPriority, const bool bDataOnly)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InKey);
	if (!DesiredGeometry)
		return false;
	const FVoxelTerrainGeometry Geometry = *DesiredGeometry;
	if (!Module.IsAuthority())
	{
		const int32 Side = VoxelViewLod::SurfaceSide(Module.GetViewSettings().Lod, InKey.Level);

		const FIntPoint Min = InKey.Coordinate * Side;

		const FVoxelGenerationSettings& Settings = Module.GetManifest().Settings;

		TArray<FIntVector> Modified;

		Module.GetRuntime()->GetChangeIndex().Enumerate(
			{FIntVector(Min.X, Min.Y, Settings.MinZ), FIntVector(Min.X + Side, Min.Y + Side, Settings.MaxZ)}, Modified);

		FVoxelTaskStamp RemoteDataStamp;
		RemoteDataStamp.WorldEpoch = WorldEpoch;
		RemoteDataStamp.GeometryRevision = GetTypeHash(Geometry);
		RemoteDataStamp.SurfaceKey = InKey;
		RemoteDataStamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(InKey);
		RemoteDataStamp.Token = HashCombineFast(GetTypeHash(InKey), GetTypeHash(RemoteDataStamp.Revision));
		if (!Modified.IsEmpty() && !PreparedData.Contains({EVoxelTaskKind::GenerateSurface, RemoteDataStamp}))
		{
			if (bDataOnly)
				return false;
			const uint64* Revision = SurfaceRevisions.Find(InKey);

			return Module.RequestRemoteRepresentation(
				EVoxelRepresentationWireType::SurfaceProxy, {FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level}, Revision ? *Revision : 0);
		}
	}

	const TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator = Module.GetGenerator();

	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();

	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();

	if (!Generator || !Config || !Registry)
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.SourcePriority = InPriority;

	Request.Kind = EVoxelTaskKind::BuildSurface;

	Request.WorkClass = EVoxelWorkClass::Exploration;

	const bool bCriticalDependency =
		IsCriticalDependency(FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Surface, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level});
	if (bCriticalDependency)
	{
		Request.WorkClass = EVoxelWorkClass::Critical;
		Request.TerrainStage = 0;
	}
	Request.Stamp.GeometryRevision = GetTypeHash(Geometry);
	Request.Stamp.WorldEpoch = WorldEpoch;

	Request.Stamp.SurfaceKey = InKey;

	Request.Stamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(InKey);

	Request.Stamp.Token = HashCombineFast(GetTypeHash(InKey), GetTypeHash(Request.Stamp.Revision));

	Request.ReservedBytes = 40ull * 1024ull * 1024ull;
	Request.DistanceScore = MinimumObserverDistanceCells(SurfaceWorldCenter(InKey));
	Request.ForwardScore = 0.0;

	if (Scheduler.Has(Request.Stamp, Request.Kind))
	{
		return false;
	}

	const FVoxelTaskKey DataKey{EVoxelTaskKind::GenerateSurface, Request.Stamp};
	TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> Prepared;
	if (bDataOnly)
	{
		if (PreparedData.Contains(DataKey) || Scheduler.Has(Request.Stamp, DataKey.Kind))
			return false;
		Request.Kind = DataKey.Kind;
		Request.WorkClass = bCriticalDependency ? EVoxelWorkClass::Critical : EVoxelWorkClass::Prefetch;
	}
	else
	{
		Prepared = PreparedData.FindRef(DataKey);
		if (!Prepared || !Prepared->Surface || Prepared->Surface->Geometry != Geometry)
			return false;
	}

	const FVoxelGenerationSettings Settings = Module.GetManifest().Settings;
	FVoxelOverlaySnapshotSet Overlays;
	const int32 Step = Geometry.SampleStepCells;
	const int32 Side = VoxelViewLod::SurfaceSide(Module.GetViewSettings().Lod, InKey.Level);
	const int32 Margin = FMath::Max(Step, Settings.Ecology.Tree.bEnabled ? Settings.Ecology.Tree.CrownRadius : 0);
	const FVoxelGenerationBounds OverlayBounds{FIntVector(InKey.Coordinate.X * Side - Margin, InKey.Coordinate.Y * Side - Margin, Settings.MinZ),
		FIntVector((InKey.Coordinate.X + 1) * Side + Margin, (InKey.Coordinate.Y + 1) * Side + Margin, Settings.MaxZ)};
	FString OverlayError;
	Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Dependency, Request.InputBytes, Request.ReservedBytes);
	const auto EnvironmentRetention = bDataOnly ? RetainPreparedEnvironment(DataKey, *Generator, OverlayBounds) : nullptr;
	if (bDataOnly && !Module.EnsureEnvironmentBounds(OverlayBounds, Request.WorkClass, InPriority, Request.DistanceScore, Request.ForwardScore).bReady)
		return false;
	if (bDataOnly && !Generator->EnsureCoarseOverlayBoundsReady(OverlayBounds, Request.WorkClass, InPriority).bReady)
		return false;
	if (bDataOnly && !Module.CaptureOverlays(OverlayBounds, Overlays, OverlayError))
		return false;
	Request.InputBytes = Prepared ? Prepared->ResultBytes() : Overlays.GetAllocatedBytes();

	TSharedPtr<FHeightfieldMeshEstimate, ESPMode::ThreadSafe> MeshEstimate;
	TSharedPtr<FVoxelHeightfieldPageBuild, ESPMode::ThreadSafe> PageBuild;
	if (bDataOnly)
	{
		PageBuild = MakeShared<FVoxelHeightfieldPageBuild, ESPMode::ThreadSafe>();
		FString Error;
		MeshEstimate = MakeShared<FHeightfieldMeshEstimate, ESPMode::ThreadSafe>();
		if (!PageBuild->Initialize(Geometry, Error) || !MeshEstimate->Counter.Initialize(Geometry, Error, 0, true))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			return false;
		}
		Request.bResumable = true;
		Request.InputBytes += PageBuild->MaximumRetainedBytes();
		Request.ReservedBytes += PageBuild->MaximumRetainedBytes();
	}
	TSharedPtr<FVoxelHeightfieldPageMesh, ESPMode::ThreadSafe> PageMesh;
	if (!bDataOnly)
	{
		PageMesh = MakeShared<FVoxelHeightfieldPageMesh, ESPMode::ThreadSafe>();
		FString Error;
		const uint64 RetainedBytes = Prepared->Surface->EstimatedMeshBytes + 64ull * 1024ull;
		if (!PageMesh->Initialize(Geometry, Error, RetainedBytes))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			return false;
		}
		Request.bResumable = true;
		Request.InputBytes += RetainedBytes + RetainedBytes / 2;
		Request.ReservedBytes = RetainedBytes + Prepared->ResultBytes();
	}
	Request.Execute = [bDataOnly, PageBuild, MeshEstimate, PageMesh, Prepared, Overlays = MoveTemp(Overlays), Generator, EnvironmentRetention, Config, Registry,
						  Settings, InKey, Geometry, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells,
						  Lod = Module.GetViewSettings().Lod](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;

		if (bDataOnly)
		{
			if (MeshEstimate->Surface)
			{
				FVoxelSectionMeshResult CountedMesh;
				bool bComplete = false;
				Result.bSuccess = MeshEstimate->Counter.Advance(
					*MeshEstimate->Surface, *MeshEstimate->Water, *Config, *Registry, TextureStretch, CountedMesh, bComplete, Result.Error, &InCancel);
				if (!Result.bSuccess)
					return Result;
				if (!bComplete)
				{
					Result.bContinue = true;
					return Result;
				}
				MeshEstimate->Surface->EstimatedMeshBytes = MeshEstimate->Counter.GetEstimatedMeshBytes();
				Result.Surface = MeshEstimate->Surface;
				Result.Water = MeshEstimate->Water;
				return Result;
			}
			Result.Surface = MakeShared<FVoxelSurfaceTileData>();

			const FVoxelSurfaceProxyBuilder Builder(Generator.ToSharedRef(), Config.ToSharedRef(), Settings, Overlays, Registry.ToSharedRef(), Lod);

			bool bComplete = false;
			Result.bSuccess = PageBuild->Advance(Builder, *Result.Surface, bComplete, Result.Error, &InCancel);
			if (Result.bSuccess && !bComplete)
			{
				Result.Surface.Reset();
				Result.bContinue = true;
				return Result;
			}

			if (!Result.bSuccess)
				return Result;
			Result.Water = MakeShared<FVoxelWaterSurfaceTileData>();
			FVoxelWaterViewBuilder WaterBuilder;
			Result.bSuccess = WaterBuilder.Build(*Result.Surface, *Result.Water, Result.Error);
			if (!Result.bSuccess)
			{
				return Result;
			}
			MeshEstimate->Surface = Result.Surface;
			MeshEstimate->Water = Result.Water;
			Result.Surface.Reset();
			Result.Water.Reset();
			Result.bContinue = true;
			return Result;
		}
		Result.SurfaceMesh = MakeShared<FVoxelSectionMeshResult>();
		bool bComplete = false;
		Result.bSuccess = PageMesh->Advance(
			*Prepared->Surface, *Prepared->Water, *Config, *Registry, TextureStretch, *Result.SurfaceMesh, bComplete, Result.Error, &InCancel);
		if (Result.bSuccess && !bComplete)
		{
			Result.SurfaceMesh.Reset();
			Result.bContinue = true;
			return Result;
		}
		Result.Surface = Prepared->Surface;
		Result.Water = Prepared->Water;

		return Result;
	};

	if (bDataOnly)
		return EnqueuePreparedData(MoveTemp(Request));
	if (!EnqueueViewTask(MoveTemp(Request)))
		return false;
	RemovePreparedData(DataKey);
	return true;
}

bool FVoxelViewManager::RequestMacro(const FVoxelMacroTileKey& InKey, const int32 InPriority, const bool bDataOnly)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InKey);
	if (!DesiredGeometry)
		return false;
	const FVoxelTerrainGeometry Geometry = *DesiredGeometry;
	if (!Module.IsAuthority())
	{
		const int32 Side = VoxelViewLod::MacroSide(Module.GetViewSettings().Lod, InKey.Level);

		const FIntPoint Min = InKey.Coordinate * Side;

		const FVoxelGenerationSettings& Settings = Module.GetManifest().Settings;

		TArray<FIntVector> Modified;

		Module.GetRuntime()->GetChangeIndex().Enumerate(
			{FIntVector(Min.X, Min.Y, Settings.MinZ), FIntVector(Min.X + Side, Min.Y + Side, Settings.MaxZ)}, Modified);

		FVoxelTaskStamp RemoteDataStamp;
		RemoteDataStamp.WorldEpoch = WorldEpoch;
		RemoteDataStamp.GeometryRevision = GetTypeHash(Geometry);
		RemoteDataStamp.MacroKey = InKey;
		RemoteDataStamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetMacroRevision(InKey);
		RemoteDataStamp.Token = HashCombineFast(GetTypeHash(InKey), GetTypeHash(RemoteDataStamp.Revision));
		if (!Modified.IsEmpty() && !PreparedData.Contains({EVoxelTaskKind::GenerateMacro, RemoteDataStamp}))
		{
			if (bDataOnly)
				return false;
			const uint64* Revision = MacroRevisions.Find(InKey);

			return Module.RequestRemoteRepresentation(
				EVoxelRepresentationWireType::MacroTerrain, {FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level}, Revision ? *Revision : 0);
		}
	}

	const TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator = Module.GetGenerator();

	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();

	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();

	if (!Generator || !Config || !Registry)
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.SourcePriority = InPriority;

	Request.Kind = EVoxelTaskKind::BuildMacro;

	Request.WorkClass = EVoxelWorkClass::Background;

	const bool bCriticalDependency =
		IsCriticalDependency(FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Macro, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level});
	if (bCriticalDependency)
	{
		Request.WorkClass = EVoxelWorkClass::Critical;
		Request.TerrainStage = 0;
	}
	Request.Stamp.GeometryRevision = GetTypeHash(Geometry);
	Request.Stamp.WorldEpoch = WorldEpoch;

	Request.Stamp.MacroKey = InKey;

	Request.Stamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetMacroRevision(InKey);

	Request.Stamp.Token = HashCombineFast(GetTypeHash(InKey), GetTypeHash(Request.Stamp.Revision));

	Request.ReservedBytes = 32ull * 1024ull * 1024ull;
	Request.DistanceScore = MinimumObserverDistanceCells(MacroWorldCenter(InKey));
	Request.ForwardScore = 0.0;

	if (Scheduler.Has(Request.Stamp, Request.Kind))
	{
		return false;
	}

	const FVoxelTaskKey DataKey{EVoxelTaskKind::GenerateMacro, Request.Stamp};
	TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> Prepared;
	if (bDataOnly)
	{
		if (PreparedData.Contains(DataKey) || Scheduler.Has(Request.Stamp, DataKey.Kind))
			return false;
		Request.Kind = DataKey.Kind;
		Request.WorkClass = bCriticalDependency ? EVoxelWorkClass::Critical : EVoxelWorkClass::Prefetch;
	}
	else
	{
		Prepared = PreparedData.FindRef(DataKey);
		if (!Prepared || !Prepared->Macro || Prepared->Macro->Geometry != Geometry)
			return false;
	}

	const FVoxelGenerationSettings Settings = Module.GetManifest().Settings;
	const int32 Step = Geometry.SampleStepCells;
	const int32 Side = VoxelViewLod::MacroSide(Module.GetViewSettings().Lod, InKey.Level);
	const int32 Margin = FMath::Max(Step, Settings.Ecology.Tree.bEnabled ? Settings.Ecology.Tree.CrownRadius : 0);
	FVoxelOverlaySnapshotSet Overlays;
	FString OverlayError;
	const FVoxelGenerationBounds Bounds{FIntVector(InKey.Coordinate.X * Side - Margin, InKey.Coordinate.Y * Side - Margin, Settings.MinZ),
		FIntVector((InKey.Coordinate.X + 1) * Side + Margin, (InKey.Coordinate.Y + 1) * Side + Margin, Settings.MaxZ)};
	Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Dependency, Request.InputBytes, Request.ReservedBytes);
	const auto EnvironmentRetention = bDataOnly ? RetainPreparedEnvironment(DataKey, *Generator, Bounds) : nullptr;
	if (bDataOnly && !Module.EnsureEnvironmentBounds(Bounds, Request.WorkClass, InPriority, Request.DistanceScore, Request.ForwardScore).bReady)
		return false;
	if (bDataOnly && !Generator->EnsureCoarseOverlayBoundsReady(Bounds, Request.WorkClass, InPriority).bReady)
		return false;
	if (bDataOnly && !Module.CaptureOverlays(Bounds, Overlays, OverlayError))
	{
		return false;
	}
	Request.InputBytes = Prepared ? Prepared->ResultBytes() : Overlays.GetAllocatedBytes();
	TSharedPtr<FHeightfieldMeshEstimate, ESPMode::ThreadSafe> MeshEstimate;
	TSharedPtr<FVoxelHeightfieldPageBuild, ESPMode::ThreadSafe> PageBuild;
	if (bDataOnly)
	{
		PageBuild = MakeShared<FVoxelHeightfieldPageBuild, ESPMode::ThreadSafe>();
		FString Error;
		MeshEstimate = MakeShared<FHeightfieldMeshEstimate, ESPMode::ThreadSafe>();
		if (!PageBuild->Initialize(Geometry, Error) || !MeshEstimate->Counter.Initialize(Geometry, Error, 0, true))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			return false;
		}
		Request.bResumable = true;
		Request.InputBytes += PageBuild->MaximumRetainedBytes();
		Request.ReservedBytes += PageBuild->MaximumRetainedBytes();
	}
	TSharedPtr<FVoxelHeightfieldPageMesh, ESPMode::ThreadSafe> PageMesh;
	if (!bDataOnly)
	{
		PageMesh = MakeShared<FVoxelHeightfieldPageMesh, ESPMode::ThreadSafe>();
		FString Error;
		const uint64 RetainedBytes = Prepared->Macro->EstimatedMeshBytes + 64ull * 1024ull;
		if (!PageMesh->Initialize(Geometry, Error, RetainedBytes))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			return false;
		}
		Request.bResumable = true;
		Request.InputBytes += RetainedBytes + RetainedBytes / 2;
		Request.ReservedBytes = RetainedBytes + Prepared->ResultBytes();
	}
	Request.Execute = [bDataOnly, PageBuild, MeshEstimate, PageMesh, Prepared, Overlays = MoveTemp(Overlays), Settings, Generator, EnvironmentRetention, Config,
						  Registry, InKey, Geometry, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells,
						  Lod = Module.GetViewSettings().Lod](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;

		if (bDataOnly)
		{
			if (MeshEstimate->Macro)
			{
				FVoxelSectionMeshResult CountedMesh;
				bool bComplete = false;
				Result.bSuccess =
					MeshEstimate->Counter.Advance(*MeshEstimate->Macro, *Config, *Registry, TextureStretch, CountedMesh, bComplete, Result.Error, &InCancel);
				if (!Result.bSuccess)
					return Result;
				if (!bComplete)
				{
					Result.bContinue = true;
					return Result;
				}
				MeshEstimate->Macro->EstimatedMeshBytes = MeshEstimate->Counter.GetEstimatedMeshBytes();
				Result.Macro = MeshEstimate->Macro;
				return Result;
			}
			Result.Macro = MakeShared<FVoxelMacroTileData>();

			const FVoxelMacroTerrainBuilder Builder(Generator.ToSharedRef(), Config.ToSharedRef(), Settings, Overlays, Registry.ToSharedRef(), Lod);

			bool bComplete = false;
			Result.bSuccess = PageBuild->Advance(Builder, *Result.Macro, bComplete, Result.Error, &InCancel);
			if (Result.bSuccess && !bComplete)
			{
				Result.Macro.Reset();
				Result.bContinue = true;
				return Result;
			}

			if (!Result.bSuccess)
				return Result;
			MeshEstimate->Macro = Result.Macro;
			Result.Macro.Reset();
			Result.bContinue = true;
			return Result;
		}
		Result.MacroMesh = MakeShared<FVoxelSectionMeshResult>();
		bool bComplete = false;
		Result.bSuccess = PageMesh->Advance(*Prepared->Macro, *Config, *Registry, TextureStretch, *Result.MacroMesh, bComplete, Result.Error, &InCancel);
		if (Result.bSuccess && !bComplete)
		{
			Result.MacroMesh.Reset();
			Result.bContinue = true;
			return Result;
		}
		Result.Macro = Prepared->Macro;

		return Result;
	};

	if (bDataOnly)
		return EnqueuePreparedData(MoveTemp(Request));
	if (!EnqueueViewTask(MoveTemp(Request)))
		return false;
	RemovePreparedData(DataKey);
	return true;
}

bool FVoxelViewManager::HasRenderableMesh(const FVoxelSectionMeshResult& InMesh)
{
	for (const FVoxelRenderBatch& Batch : InMesh.Batches)
	{
		if (!Batch.Mesh.Vertices.IsEmpty() && !Batch.Mesh.Triangles.IsEmpty())
			return true;
	}
	return false;
}

bool FVoxelViewManager::PublishFine(const FVoxelTaskResult& InResult)
{
	const FIntVector& Key = InResult.Stamp.Section;
	if (!InResult.FineMesh || !FineWanted.Contains(Key))
		return false;
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(Key);
	if (!Section || Section->Stamp.Epoch != InResult.Stamp.WorldEpoch || Section->Stamp.Token != InResult.Stamp.Token ||
		Section->CommittedRevision != InResult.Stamp.Revision)
		return false;
	FineReady.Add(Key);
	FineRevisions.Add(Key, InResult.Stamp.Revision);
	FineSectionMeshes.Add(Key, MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>(*InResult.FineMesh));
	IndexCoverage(FVoxelViewKey{Key, 0});
	FineLastWanted.Add(Key, FPlatformTime::Seconds());
	TrackReadyTerrainNode({Key, 0});
	if (const FVoxelFinePageKey* Page = FinePagePlan.SectionOwners.Find(Key))
		MarkFinePageDirty(*Page);
	bFinePresentationDirty = true;
	bTerrainBuildStageDirty = true;
	MarkCoverageDirty(FineCoverageRect(Key));
	return true;
}

bool FVoxelViewManager::PublishVoxelProxy(const FVoxelTaskResult& InResult)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InResult.Stamp.ViewKey);
	if (!InResult.VoxelProxy || !DesiredGeometry || InResult.VoxelProxy->Geometry != *DesiredGeometry)
		return false;
	const FVoxelViewKey& Key = InResult.Stamp.ViewKey;
	if (!InResult.VoxelProxy || !InResult.VoxelProxyMesh || !VoxelProxyWanted.Contains(Key) ||
		InResult.Stamp.Token != HashCombineFast(GetTypeHash(Key), GetTypeHash(InResult.Stamp.Revision)) ||
		Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(Key) != InResult.Stamp.Revision)
	{
		return false;
	}

	VoxelProxyData.Add(Key, InResult.VoxelProxy);
	if (!InResult.VoxelProxy->Cells.IsEmpty() && !InResult.VoxelProxy->Cells.ContainsByPredicate(
													 [](const FVoxelBlockState& State)
													 {
														 return !State.IsAir();
													 }))
		AirVoxelProxyNodes.Add(Key);
	else
		AirVoxelProxyNodes.Remove(Key);
	PreparedVolumeSignatures.Remove(Key);
	PreparedVolumeMeshes.Remove(Key);
	FVoxelBoundaryTransitionContext BaseContext;
	BaseContext.Owner = Key;
	VolumeTransitionSignatures.Add(Key, BaseContext.Signature(InResult.VoxelProxy->Revision));
	UpdateProxySurfaceCoverage(*InResult.VoxelProxy);
	bTerrainBuildStageDirty = true;
	VoxelProxyReady.Add(Key);
	TrackReadyTerrainNode(Key);
	VoxelProxyLastWanted.Add(Key, FPlatformTime::Seconds());
	VoxelProxyRevisions.Add(Key, InResult.Stamp.Revision);
	IndexCoverage(Key);
	const int32 TerrainStage = VoxelProxyTerrainStage(Key);
	const FGuid Group = BeginPublishGroup({1, Key.Coordinate, Key.Level});
	Publisher->SetTerrainStage(Group, TerrainStage);
	if (!HasRenderableMesh(*InResult.VoxelProxyMesh) && !VoxelProxyActors.Contains(Key))
	{
		RecordTerrainMesh({EVoxelTerrainRepresentation::VoxelProxy, Key.Coordinate, Key.Level}, *InResult.VoxelProxyMesh, InResult.ExecuteMilliseconds);
		return true;
	}

	AActor* Host = VoxelProxyActors.FindRef(Key);
	const auto Mesh = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>(MoveTemp(*InResult.VoxelProxyMesh));
	if (!Publisher->StageImmutable(Group, Host, FVector(Key.GetBounds().Min) * Module.BlockSize(),
			Module.BlockSize() * InResult.VoxelProxy->Geometry.SampleStepCells, Mesh, TerrainStage))
	{
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(Key) > 0;
		VoxelProxyData.Remove(Key);
		AirVoxelProxyNodes.Remove(Key);
		ProxySurfaceCoverage.Remove(Key);
		VoxelProxyRevisions.Remove(Key);
		VolumeTransitionSignatures.Remove(Key);
		return false;
	}
	VoxelProxyActors.Add(Key, Host);
	RecordTerrainMesh({EVoxelTerrainRepresentation::VoxelProxy, Key.Coordinate, Key.Level}, *Mesh, InResult.ExecuteMilliseconds);
	return true;
}

bool FVoxelViewManager::PublishSurface(const FVoxelTaskResult& InResult)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InResult.Stamp.SurfaceKey);
	if (!InResult.Surface || !DesiredGeometry || InResult.Surface->Geometry != *DesiredGeometry)
		return false;
	const FVoxelSurfaceTileKey& Key = InResult.Stamp.SurfaceKey;
	if (!InResult.Surface || !InResult.SurfaceMesh || !SurfaceWanted.Contains(Key) ||
		InResult.Stamp.Token != HashCombineFast(GetTypeHash(Key), GetTypeHash(InResult.Stamp.Revision)) ||
		Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(Key) != InResult.Stamp.Revision)
	{
		return false;
	}

	AActor* Host = SurfaceActors.FindRef(Key);
	const int32 TileSide = InResult.Surface->GetTileSide();
	const auto Mesh = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>(MoveTemp(*InResult.SurfaceMesh));
	if (!Publisher->StageImmutable(BeginPublishGroup({2, Key.Coordinate, Key.Level}), Host,
			FVector(Key.Coordinate.X * TileSide, Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(), Module.BlockSize(), Mesh, 2))
	{
		SurfaceReady.Remove(Key);
		return false;
	}
	SurfaceActors.Add(Key, Host);
	bTerrainBuildStageDirty = true;
	SurfaceReady.Add(Key);
	bHeightfieldTopologyDirty = true;
	SurfaceLastWanted.Add(Key, FPlatformTime::Seconds());
	SurfaceRevisions.Add(Key, InResult.Stamp.Revision);
	SurfaceData.Add(Key, InResult.Surface);
	IndexCoverage(Key);
	RecordTerrainMesh(
		{EVoxelTerrainRepresentation::Surface, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0), Key.Level}, *Mesh, InResult.ExecuteMilliseconds);
	return true;
}

void FVoxelViewManager::PublishWater(const FVoxelTaskResult& InResult)
{
	const FVoxelSurfaceTileKey& Key = InResult.Stamp.SurfaceKey;

	if (!InResult.Water || !InResult.WaterMesh || !SurfaceWanted.Contains(Key) ||
		InResult.Stamp.Token != HashCombineFast(GetTypeHash(Key), GetTypeHash(InResult.Stamp.Revision)) ||
		Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(Key) != InResult.Stamp.Revision)
	{
		return;
	}

	AActor* Host = WaterActors.FindRef(Key);

	const int32 TileSide = (InResult.Water->Side - 1) * InResult.Water->Step;

	if (Publisher->Stage(BeginPublishGroup({2, Key.Coordinate, Key.Level}), Host,
			FVector(Key.Coordinate.X * TileSide, Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(), Module.BlockSize(), MoveTemp(*InResult.WaterMesh), 2))
	{
		WaterActors.Add(Key, Host);
	}
}

bool FVoxelViewManager::PublishMacro(const FVoxelTaskResult& InResult)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InResult.Stamp.MacroKey);
	if (!InResult.Macro || !DesiredGeometry || InResult.Macro->Geometry != *DesiredGeometry)
	{
		return false;
	}

	const FVoxelMacroTileKey& Key = InResult.Stamp.MacroKey;

	if (!InResult.MacroMesh || !MacroWanted.Contains(Key) || InResult.Stamp.Token != HashCombineFast(GetTypeHash(Key), GetTypeHash(InResult.Stamp.Revision)) ||
		Module.GetRuntime()->GetChangeHierarchy().GetMacroRevision(Key) != InResult.Stamp.Revision)
	{
		return false;
	}

	AActor* Host = MacroActors.FindRef(Key);
	const int32 TileSide = InResult.Macro->GetTileSide();
	const auto Mesh = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>(MoveTemp(*InResult.MacroMesh));

	if (Publisher->StageImmutable(BeginPublishGroup({3, Key.Coordinate, Key.Level}), Host,
			FVector(Key.Coordinate.X * TileSide, Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(), Module.BlockSize(), Mesh, 3))
	{
		MacroActors.Add(Key, Host);
		MacroData.Add(Key, InResult.Macro);
		MacroReady.Add(Key);
		MacroLastWanted.Add(Key, FPlatformTime::Seconds());
		MacroRevisions.Add(Key, InResult.Stamp.Revision);

		bTerrainBuildStageDirty = true;
		bHeightfieldTopologyDirty = true;
		IndexCoverage(Key);
		RecordTerrainMesh(
			{EVoxelTerrainRepresentation::Macro, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0), Key.Level}, *Mesh, InResult.ExecuteMilliseconds);
		return true;
	}

	MacroReady.Remove(Key);
	return false;
}

AActor* FVoxelViewManager::FineActor(const FIntVector& InSection) const
{
	const FVoxelFinePageKey* Page = PresentedFineSectionOwners.Find(InSection);
	if (!Page)
		Page = FinePagePlan.SectionOwners.Find(InSection);
	return Page ? FinePageActors.FindRef(*Page) : nullptr;
}

bool FVoxelViewManager::IsFinePresented(const FIntVector& InSection) const
{
	const FVoxelFinePageKey* Page = PresentedFineSectionOwners.Find(InSection);
	if (!Page)
		return false;
	AActor* Actor = FinePageActors.FindRef(*Page);
	return (Actor && !Actor->IsHidden() && Publisher->IsCommitted(Actor)) || PresentedEmptyFinePages.Contains(*Page);
}

void FVoxelViewManager::RebuildFinePagePlan()
{
	const auto& Planned = GetViewInterest().FinePagePlan;
	FVoxelFinePagePlan Next;
	if (Planned)
	{
		Next = *Planned;
	}
	else
	{
		TArray<FVoxelStreamingSource> Sources;
		for (const auto& Pair : GetViewInterest().Sources)
			Sources.Add(Pair.Value.Source);
		FString Error;
		if (!FVoxelFinePagePlanner::Build(FineWanted, Sources, Module.GetViewSettings(), Next, Error))
		{
			if (LastFinePagePlanningError != Error)
			{
				UE_LOG(LogTemp, Error, TEXT("Fine page plan rejected; committed layout retained: %s"), *Error);
				LastFinePagePlanningError = Error;
			}
			return;
		}
	}
	LastFinePagePlanningError.Reset();
	for (const auto& Pair : Next.SectionOwners)
	{
		const FVoxelFinePageKey* Previous = FinePagePlan.SectionOwners.Find(Pair.Key);
		if (!Previous || *Previous != Pair.Value)
		{
			InvalidateFinePageLayout(Pair.Value);
			if (Previous)
			{
				InvalidateFinePageLayout(*Previous);
			}
			MarkCoverageDirty(FineCoverageRect(Pair.Key));
		}
	}
	for (const auto& Pair : FinePagePlan.SectionOwners)
	{
		if (!Next.SectionOwners.Contains(Pair.Key))
		{
			InvalidateFinePageLayout(Pair.Value);
			MarkCoverageDirty(FineCoverageRect(Pair.Key));
		}
	}
	if (Next.Pages.Num() != FinePagePlan.Pages.Num() || !Next.Pages.Difference(FinePagePlan.Pages).IsEmpty())
	{
		++FinePageLayoutRevision;
	}
	for (const auto& Page : FinePagePlan.Pages)
		if (!Next.Pages.Contains(Page))
		{
			RetireFinePageBuildState(Page, true);
			QueueRetirement({0, Page.Coordinate, Page.Level});
		}
	FinePagePlan = MoveTemp(Next);
	bFinePresentationDirty |= !DirtyFinePages.IsEmpty();
}

bool FVoxelViewManager::CaptureFinePageInputs(const FVoxelFinePageKey& InPage, TArray<FFinePageInput>& OutInputs, uint64& OutSignature) const
{
	TArray<FIntVector> Sections;
	FVoxelFinePagePlanner::GatherSections(InPage, Sections);
	uint64 Signature = (1469598103934665603ull ^ GetTypeHash(InPage)) * 1099511628211ull;
	for (const FIntVector& Section : Sections)
	{
		FFinePageInput Input;
		if (!CaptureFinePageInput(Section, Input, Signature))
		{
			return false;
		}
		OutInputs.Add(MoveTemp(Input));
	}
	OutSignature = Signature;
	return true;
}

void FVoxelViewManager::PrepareFinePages()
{
	if (!bFinePresentationDirty)
		return;
	const bool bCriticalPending = HasCriticalNearFieldPending();
	const double Deadline = TerrainDeadline(Module.GetViewSettings().Budget.ResultApplyMilliseconds);
	int32 Prepared = 0;
	const int32 MaximumAttempts = FMath::Min(64, DirtyFinePages.GetMaxIndex());
	for (int32 Attempt = 0; Attempt < MaximumAttempts && FPlatformTime::Seconds() < Deadline; ++Attempt)
	{
		if (DirtyFinePages.IsEmpty())
		{
			break;
		}
		NextFinePreparationIndex %= DirtyFinePages.GetMaxIndex();
		const auto Id = FSetElementId::FromInteger(NextFinePreparationIndex++);
		if (!DirtyFinePages.IsValidId(Id))
		{
			continue;
		}
		const FVoxelFinePageKey Page = DirtyFinePages[Id];
		if (FinePageFailures.Contains(Page))
		{
			DirtyFinePages.Remove(Page);
			continue;
		}
		if (!FinePagePlan.Pages.Contains(Page))
		{
			RetireFinePageBuildState(Page, true);
			continue;
		}
		if (bCriticalPending && !GetViewInterest().CriticalFinePages.Contains(Page))
		{
			continue;
		}
		const uint64 PageVersion = FinePageVersions.FindRef(Page);
		const uint64 ContentRevision = FinePageContentRevisions.FindRef(Page);
		if (const FFinePageAttempt* Pending = PendingFinePageBuilds.Find(Page))
		{
			if (Pending->PageVersion == PageVersion && Pending->ContentRevision == ContentRevision)
			{
				continue;
			}
			const FVoxelTaskStamp Expected = Pending->Stamp;
			Scheduler.CancelMatching(
				[Expected](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
				{
					return Kind == EVoxelTaskKind::BuildFineMesh && Stamp == Expected;
				});
			PendingFinePageBuilds.Remove(Page);
		}
		TArray<FFinePageInput> Inputs;
		uint64 Signature = 0;
		const auto* Cached = PreparedFinePageMeshes.Find(Page);
		if (Cached && Cached->PageVersion == PageVersion && Cached->ContentRevision == ContentRevision)
		{
			Signature = Cached->Signature;
		}
		else if (!AdvanceFinePageCapture(Page, Inputs, Signature, Deadline))
		{
			continue;
		}
		const auto* PreparedPage = PreparedFinePageMeshes.Find(Page);
		if (!PreparedPage || PreparedPage->Signature != Signature)
		{
			const uint64 AttemptId = ++NextFinePageAttemptId;
			FVoxelTaskRequest Request;
			Request.Kind = EVoxelTaskKind::BuildFineMesh;
			Request.WorkClass = EVoxelWorkClass::Interactive;
			Request.Stamp.WorldEpoch = WorldEpoch;
			Request.Stamp.Section = FIntVector(MIN_int32, 1, 0);
			Request.Stamp.ViewKey = {Page.Coordinate, Page.Level};
			Request.Stamp.Token = AttemptId;
			Request.Stamp.Revision = ContentRevision;
			Request.Stamp.GeometryRevision = GetTypeHash(Page);
			Request.DistanceScore = MinimumObserverDistanceCells(Page.GetBounds());
			Request.InputBytes = Inputs.GetAllocatedSize();
			Request.SourcePriority = MAX_int32;
			for (const auto& Input : Inputs)
			{
				Request.InputBytes += Input.Mesh->Bytes() * 2;
				if (const auto* Demand = GetViewInterest().Exact.Find(Input.Section))
				{
					Request.SourcePriority = FMath::Min(Request.SourcePriority, Demand->Priority);
					if (Demand->bWarmupData || Demand->bWarmupCollision || Demand->bMovementCriticalData || Demand->bMovementCriticalCollision ||
						Demand->bMovementCriticalFine)
						Request.WorkClass = EVoxelWorkClass::Critical;
				}
			}
			Request.ReservedBytes = 64ull * 1024ull + Request.InputBytes * 2;
			struct FFinePageBuildState
			{
				TArray<FFinePageInput> Inputs;
				TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
				int32 Next = 0;
			};
			const auto Build = MakeShared<FFinePageBuildState, ESPMode::ThreadSafe>();
			Build->Inputs = MoveTemp(Inputs);
			Request.bResumable = true;
			Request.Execute = [Page, Build](const TAtomic<bool>& Cancel)
			{
				FVoxelTaskResult Result;
				const auto Mesh = Build->Mesh;
				const FIntVector Origin = Page.GetBounds().Min;
				const auto AppendWindow = [&Mesh, Origin](const FVoxelSectionMeshResult& Source, const FIntVector& Section)
				{
					for (const auto& Batch : Source.Batches)
					{
						if (Batch.Mesh.Triangles.IsEmpty())
							continue;
						auto Copy = Batch;
						for (auto& Vertex : Copy.Mesh.Vertices)
							Vertex += FVector(Section * 16 - Origin);
						Mesh->Batches.Add(MoveTemp(Copy));
					}
				};
				if (Build->Inputs.IsValidIndex(Build->Next))
				{
					const auto& Input = Build->Inputs[Build->Next++];
					if (Cancel.Load())
						return Result;
					if (!Input.SuppressedFaces)
					{
						AppendWindow(*Input.Mesh, Input.Section);
						Result.bSuccess = true;
						Result.bContinue = true;
						return Result;
					}
					FVoxelSectionMeshResult Filtered(*Input.Mesh);
					for (auto& Batch : Filtered.Batches)
					{
						TArray<int32> Triangles;
						for (int32 Index = 0; Index + 2 < Batch.Mesh.Triangles.Num(); Index += 3)
						{
							bool bBoundary = false;
							for (uint8 Face = 0; Face < 6 && !bBoundary; ++Face)
							{
								if (!(Input.SuppressedFaces & (1 << Face)))
									continue;
								const double Plane = (Face & 1) ? 0.0 : 16.0;
								bBoundary = true;
								for (int32 Vertex = 0; Vertex < 3; ++Vertex)
									bBoundary &= FMath::IsNearlyEqual(Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index + Vertex]][Face / 2], Plane, 1.e-8);
							}
							if (!bBoundary)
								for (int32 Vertex = 0; Vertex < 3; ++Vertex)
									Triangles.Add(Batch.Mesh.Triangles[Index + Vertex]);
						}
						Batch.Mesh.Triangles = MoveTemp(Triangles);
					}
					AppendWindow(Filtered, Input.Section);
					Result.bSuccess = true;
					Result.bContinue = true;
					return Result;
				}
				auto Payload = MakeShared<FVoxelFinePageBuildResult, ESPMode::ThreadSafe>();
				if (!Mesh->ValidateAndCache())
				{
					Result.Error = TEXT("Fine page mesh validation failed");
					return Result;
				}
				Payload->Mesh = Mesh;
				Result.CustomPayload = Payload;
				Result.bSuccess = !Cancel.Load();
				return Result;
			};
			const FVoxelTaskStamp AttemptStamp = Request.Stamp;
			Request.Apply = [this, Page, Signature, PageVersion, ContentRevision, AttemptStamp](FVoxelTaskResult&& Result)
			{
				ApplyFinePageBuild(Page, Signature, PageVersion, ContentRevision, AttemptStamp, MoveTemp(Result));
			};
			if (EnqueueViewTask(MoveTemp(Request)))
			{
				PendingFinePageBuilds.Add(Page, {AttemptStamp, Signature, PageVersion, ContentRevision});
			}
			else if (Scheduler.IsTerminal({EVoxelTaskKind::BuildFineMesh, AttemptStamp}))
			{
				FinePageFailures.Add(Page, TEXT("Fine page build exceeds task budget or has an invalid request"));
				DirtyFinePages.Remove(Page);
				++TerrainWorkMetrics.Failed;
				UE_LOG(LogTemp, Error, TEXT("Fine page build blocked: page=%s level=%u attempt=%llu error=%s"), *Page.Coordinate.ToString(), Page.Level,
					AttemptStamp.Token, *FinePageFailures.FindChecked(Page));
			}
			continue;
		}
		const auto Mesh = PreparedPage->Mesh;
		if (!Mesh || !Mesh->bValidated)
			continue;
		AActor* Host = FinePageActors.FindRef(Page);
		const bool bEmpty = !HasRenderableMesh(*Mesh);
		if (bEmpty && !Host && FinePageReady.Contains(Page) && PresentedEmptyFinePages.Contains(Page))
		{
			DirtyFinePages.Remove(Page);
			continue;
		}
		if (Publisher->IsGroupBusy({0, Page.Coordinate, Page.Level}))
			continue;
		const FGuid Group = BeginPublishGroup({0, Page.Coordinate, Page.Level});
		if (!bEmpty || Host)
		{
			if (!Publisher->StageImmutable(Group, Host, FVector(Page.GetBounds().Min) * Module.BlockSize(), Module.BlockSize(), Mesh, 0))
				continue;
			FinePageActors.Add(Page, Host);
			Publisher->SetCoverage(Group, Host, {});
			if (!Publisher->HasPresentation(Host))
				Publisher->SetHidden(Group, Host, true);
		}
		if (!bEmpty)
			EmptyFinePages.Remove(Page);
		else
			EmptyFinePages.Add(Page);
		FinePageReady.Add(Page);
		PendingFinePages.Add(Page);
		const FVoxelGenerationBounds PageBounds = Page.GetBounds();
		MarkCoverageDirty({FIntPoint(PageBounds.Min.X, PageBounds.Min.Y), FIntPoint(PageBounds.Max.X, PageBounds.Max.Y)});
		DirtyFinePages.Remove(Page);
		++Prepared;
		if (Prepared >= Module.GetViewSettings().Budget.FineApplyPerFrame || FPlatformTime::Seconds() >= Deadline)
			break;
	}
	bFinePresentationDirty = !DirtyFinePages.IsEmpty();
}

void FVoxelViewManager::GatherFinePageCandidates(const TSet<FVoxelViewKey>& InCandidates, TSet<FVoxelFinePageKey>& OutPages) const
{
	OutPages.Reset();
	const auto AddScopedPages = [this, &OutPages](const TSet<FVoxelFinePageKey>& Pages)
	{
		for (const FVoxelFinePageKey& Page : Pages)
		{
			const FVoxelGenerationBounds Bounds = Page.GetBounds();
			if (CoverageAffects({FIntPoint(Bounds.Min.X, Bounds.Min.Y), FIntPoint(Bounds.Max.X, Bounds.Max.Y)}))
				OutPages.Add(Page);
		}
	};
	AddScopedPages(DirtyFinePages);
	AddScopedPages(PendingFinePages);
	for (const FVoxelViewKey& Node : InCandidates)
	{
		if (Node.Level != 0)
			continue;
		if (const FVoxelFinePageKey* Page = PresentedFineSectionOwners.Find(Node.Coordinate))
			OutPages.Add(*Page);
		if (const FVoxelFinePageKey* Page = FinePagePlan.SectionOwners.Find(Node.Coordinate))
			OutPages.Add(*Page);
	}
	for (const FVoxelFinePageKey& Page : OutPages.Array())
	{
		TArray<FIntVector> Sections;
		FVoxelFinePagePlanner::GatherSections(Page, Sections);
		for (const FIntVector& Section : Sections)
		{
			if (const FVoxelFinePageKey* Desired = FinePagePlan.SectionOwners.Find(Section))
				OutPages.Add(*Desired);
		}
	}
}

TOptional<bool> FVoxelViewManager::UpdateFinePagePresentation(
	const FVoxelFinePageKey& InPage, const TSet<FVoxelViewKey>& InTarget, const FCoveragePassSnapshot& InSnapshot)
{
	TArray<FIntVector> Sections;
	FVoxelFinePagePlanner::GatherSections(InPage, Sections);
	bool bVisible = FinePagePlan.Pages.Contains(InPage) && FinePageReady.Contains(InPage);
	for (const FIntVector& Section : Sections)
	{
		bVisible &= InTarget.Contains({Section, 0});
	}
	const FVoxelGenerationBounds Bounds = InPage.GetBounds();
	if (VoxelCoverage::IntersectsAny3D({Bounds.Min, Bounds.Max}, PendingVolumeHandoffBoxes))
	{
		return {};
	}
	AActor* Actor = FinePageActors.FindRef(InPage);
	bool bOwnsAny = false;
	bool bOwnershipMatches = true;
	for (const FIntVector& Section : Sections)
	{
		const FVoxelFinePageKey* Owner = PresentedFineSectionOwners.Find(Section);
		const bool bOwnedByThisPage = Owner && *Owner == InPage;
		bOwnsAny |= bOwnedByThisPage;
		bOwnershipMatches &= bVisible ? bOwnedByThisPage && VisibleTerrainNodes.Contains({Section, 0}) : !bOwnedByThisPage;
	}
	if (!bVisible && !Actor && !bOwnsAny && !PendingFinePages.Contains(InPage))
		return {};
	if (bVisible)
	{
		for (const FIntVector& Section : Sections)
		{
			const FVoxelFinePageKey* Old = PresentedFineSectionOwners.Find(Section);
			if (!Old || *Old == InPage)
			{
				continue;
			}
			TArray<FIntVector> OldSections;
			FVoxelFinePagePlanner::GatherSections(*Old, OldSections);
			for (const FIntVector& OldSection : OldSections)
			{
				const FVoxelFinePageKey* Desired = FinePagePlan.SectionOwners.Find(OldSection);
				if (!Desired || !FinePageReady.Contains(*Desired) || !InTarget.Contains({OldSection, 0}))
				{
					return {};
				}
			}
		}
	}
	else if ((Actor && !Actor->IsHidden()) || bOwnsAny)
	{
		bool bInsideVolumeDomain = false;
		for (const FIntVector& Section : Sections)
		{
			if (FineWanted.Contains(Section))
			{
				bInsideVolumeDomain = true;
				break;
			}
			for (FVoxelViewKey Node{Section, 0}; Node.Level <= 8; Node = Node.GetParent())
			{
				if (VoxelProxyWanted.Contains(Node))
				{
					bInsideVolumeDomain = true;
					break;
				}
			}
			if (bInsideVolumeDomain)
			{
				break;
			}
		}
		const bool bInsideDomain =
			bInsideVolumeDomain ||
			VoxelCoverage::IntersectsAny2D({FIntPoint(Bounds.Min.X, Bounds.Min.Y), FIntPoint(Bounds.Max.X, Bounds.Max.Y)}, InSnapshot.WantedHeightfield);
		bool bReplacement = !bInsideDomain || IsFinePageReplacementReady(InPage, InSnapshot);
		if (!bReplacement)
		{
			bReplacement = true;
			for (const FIntVector& Section : Sections)
			{
				const FVoxelFinePageKey* Desired = FinePagePlan.SectionOwners.Find(Section);
				if (!Desired || *Desired == InPage || !FinePageReady.Contains(*Desired) || !InTarget.Contains({Section, 0}))
				{
					bReplacement = false;
					break;
				}
			}
		}
		if (!bReplacement)
		{
			return {};
		}
	}
	const FVoxelPublishGroupKey GroupKey{0, InPage.Coordinate, InPage.Level};
	if (!FinePagePlan.Pages.Contains(InPage))
	{
		DirtyFinePages.Remove(InPage);
	}
	if (bVisible && !Actor && !EmptyFinePages.Contains(InPage))
	{
		MarkFinePageDirty(InPage);
		bFinePresentationDirty = true;
		return {};
	}
	const bool bPresentationMatches = (!Actor && EmptyFinePages.Contains(InPage)) || (Actor && Actor->IsHidden() == !bVisible && Publisher->IsCommitted(Actor));
	if (bOwnershipMatches && bPresentationMatches && !PendingFinePages.Contains(InPage) && !Publisher->IsGroupBusy(GroupKey))
	{
		return bVisible;
	}
	const FGuid Group = BeginPublishGroup(GroupKey);
	if (Actor)
	{
		Publisher->SetCoverage(Group, Actor, {});
		Publisher->SetHidden(Group, Actor, !bVisible);
	}
	return bVisible;
}

void FVoxelViewManager::InvalidateFinePresentation(const FIntVector& InSection)
{
	FineSectionMeshes.Remove(InSection);
	bReadyTerrainBranchesDirty |= FineReady.Remove(InSection) > 0;
	FineRevisions.Remove(InSection);
	if (const FVoxelFinePageKey* Page = FinePagePlan.SectionOwners.Find(InSection))
	{
		MarkFinePageDirty(*Page);
		FinePageReady.Remove(*Page);
	}
	if (const FVoxelFinePageKey* Page = PresentedFineSectionOwners.Find(InSection))
		MarkFinePageDirty(*Page);
	bFinePresentationDirty = true;
	MarkCoverageDirty(FineCoverageRect(InSection));
}

bool FVoxelViewManager::IsFinePageReplacementReady(const FVoxelFinePageKey& InPage, const FCoveragePassSnapshot& InSnapshot) const
{
	TArray<FIntVector> Sections;
	FVoxelFinePagePlanner::GatherSections(InPage, Sections);
	for (const FIntVector& Section : Sections)
	{
		const FVoxelFinePageKey* Owner = InSnapshot.PresentedFineOwners.Find(Section);
		if (Owner && *Owner != InPage)
			continue;
		if (!IsTerrainVolumeCovered({Section, 0}, InSnapshot, false) && !VoxelCoverage::IsFullyCovered2D(FineCoverageRect(Section), InSnapshot.Heightfield))
			return false;
	}
	return true;
}

FVoxelCoverageRect FVoxelViewManager::TerrainCoverageRect(const FVoxelViewKey& InKey) const
{
	return InKey.Level == 0 ? FineCoverageRect(InKey.Coordinate) : VoxelProxyCoverageRect(InKey);
}

void FVoxelViewManager::BuildCoveragePassSnapshot(FCoveragePassSnapshot& OutSnapshot) const
{
	++CoverageSnapshotBuildCount;
	OutSnapshot = {};
	TSet<FIntVector> FineCandidates;
	TSet<FVoxelViewKey> ProxyCandidates;
	TSet<FVoxelSurfaceTileKey> SurfaceCandidates;
	TSet<FVoxelMacroTileKey> MacroCandidates;
	if (bActiveCoverageFull)
	{
		FineCandidates = FineReady;
		ProxyCandidates = VoxelProxyReady;
		SurfaceCandidates = SurfaceWanted;
		MacroCandidates = MacroWanted;
	}
	else
		CaptureCoverageCandidates(FineCandidates, ProxyCandidates, SurfaceCandidates, MacroCandidates);
	for (const FIntVector& Section : FineCandidates)
	{
		if (!FineReady.Contains(Section) || !FineWanted.Contains(Section) || !VisibleTerrainNodes.Contains({Section, 0}) || !IsFinePresented(Section))
			continue;
		OutSnapshot.PresentedFineSections.Add(Section);
		OutSnapshot.PresentedFineOwners.Add(Section, PresentedFineSectionOwners.FindChecked(Section));
		OutSnapshot.Fine.Add(FineCoverageBox(Section));
		OutSnapshot.FineNodes.Add({Section, 0});
	}
	for (const FVoxelViewKey& Node : ProxyCandidates)
	{
		if (!VoxelProxyReady.Contains(Node) || !VoxelProxyWanted.Contains(Node) || !VisibleTerrainNodes.Contains(Node))
			continue;
		AActor* Actor = VoxelProxyActors.FindRef(Node);
		if (Actor && (Actor->IsHidden() || !Publisher->IsCommitted(Actor)))
			continue;
		OutSnapshot.ProxyNodes.Add(Node);
		OutSnapshot.Proxy.Add(VoxelProxyCoverageBox(Node));
		if (const auto* Rects = ProxySurfaceCoverage.Find(Node))
			OutSnapshot.ProxySurface.Append(*Rects);
	}
	for (const auto& Key : SurfaceCandidates)
	{
		AActor* Actor = SurfaceActors.FindRef(Key);
		if (SurfaceWanted.Contains(Key) && SurfaceReady.Contains(Key) && Actor && !Actor->IsHidden() && Publisher->IsCommitted(Actor))
			OutSnapshot.Surface.Add(SurfaceCoverageRect(Key));
	}
	for (const auto& Key : MacroCandidates)
	{
		AActor* Actor = MacroActors.FindRef(Key);
		if (MacroWanted.Contains(Key) && MacroReady.Contains(Key) && Actor && !Actor->IsHidden() && Publisher->IsCommitted(Actor))
			OutSnapshot.Macro.Add(MacroCoverageRect(Key));
	}
	OutSnapshot.Heightfield = OutSnapshot.Surface;
	OutSnapshot.Heightfield.Append(OutSnapshot.Macro);
	for (const auto& Key : SurfaceCandidates)
		OutSnapshot.WantedHeightfield.Add(SurfaceCoverageRect(Key));
	for (const auto& Key : MacroCandidates)
		OutSnapshot.WantedHeightfield.Add(MacroCoverageRect(Key));
	const auto BuildBranches = [](const TSet<FVoxelViewKey>& Nodes, TSet<FVoxelViewKey>& Branches)
	{
		for (FVoxelViewKey Node : Nodes)
		{
			while (Node.Level <= 20)
			{
				bool bExists = false;
				Branches.Add(Node, &bExists);
				if (bExists)
					break;
				Node = Node.GetParent();
			}
		}
	};
	BuildBranches(OutSnapshot.FineNodes, OutSnapshot.FineBranches);
	BuildBranches(OutSnapshot.ProxyNodes, OutSnapshot.ProxyBranches);
}

bool FVoxelViewManager::IsTerrainVolumeCovered(const FVoxelViewKey& InNode, const FCoveragePassSnapshot& InSnapshot, const bool bByFine) const
{
	const auto& Nodes = bByFine ? InSnapshot.FineNodes : InSnapshot.ProxyNodes;
	const auto& Branches = bByFine ? InSnapshot.FineBranches : InSnapshot.ProxyBranches;
	FVoxelViewKey Ancestor = InNode;
	while (Ancestor.Level <= 20)
	{
		if (Nodes.Contains(Ancestor))
			return true;
		Ancestor = Ancestor.GetParent();
	}
	if (InNode.Level == 0 || !Branches.Contains(InNode))
		return false;
	TArray<FVoxelViewKey> Children;
	InNode.GetChildren(Children);
	for (const FVoxelViewKey& Child : Children)
	{
		if (!IsTerrainVolumeCovered(Child, InSnapshot, bByFine))
			return false;
	}
	return true;
}

bool FVoxelViewManager::IsFineReplacementReady(const FIntVector& InKey, const FCoveragePassSnapshot& InSnapshot) const
{
	return IsTerrainVolumeCovered({InKey, 0}, InSnapshot, false) || VoxelCoverage::IsFullyCovered2D(FineCoverageRect(InKey), InSnapshot.Heightfield);
}

bool FVoxelViewManager::IsProxyReplacementReady(const FVoxelViewKey& InKey, const FCoveragePassSnapshot& InSnapshot) const
{
	return IsTerrainVolumeCovered(InKey, InSnapshot, true) || VoxelCoverage::IsFullyCovered2D(VoxelProxyCoverageRect(InKey), InSnapshot.Heightfield);
}

bool FVoxelViewManager::IsSurfaceReplacementReady(const FVoxelSurfaceTileKey& InKey, const FCoveragePassSnapshot& InSnapshot) const
{
	return VoxelCoverage::IsFullyCovered2D(SurfaceCoverageRect(InKey), InSnapshot.Macro) ||
		   VoxelCoverage::IsFullyCovered2D(SurfaceCoverageRect(InKey), InSnapshot.ProxySurface);
}

bool FVoxelViewManager::IsMacroReplacementReady(const FVoxelMacroTileKey& InKey, const FCoveragePassSnapshot& InSnapshot) const
{
	return VoxelCoverage::IsFullyCovered2D(MacroCoverageRect(InKey), InSnapshot.Surface);
}

bool FVoxelViewManager::CommitFinePageOwnership(
	const FVoxelFinePageKey& InPage, TConstArrayView<FVoxelViewKey> InDesired, const TSet<FIntVector>& InRetainedFine, const TOptional<bool>& InVisible)
{
	const bool bPublishedMesh = PendingFinePages.Remove(InPage) > 0;
	AActor* Actor = FinePageActors.FindRef(InPage);
	const bool bPresented = InVisible.IsSet() && InVisible.GetValue() && FinePageReady.Contains(InPage) && !InDesired.IsEmpty() &&
							((Actor && !Actor->IsHidden() && Publisher->IsCommitted(Actor)) || (!Actor && EmptyFinePages.Contains(InPage)));
	if (InVisible.IsSet())
	{
		if (bPresented && EmptyFinePages.Contains(InPage))
			PresentedEmptyFinePages.Add(InPage);
		else
			PresentedEmptyFinePages.Remove(InPage);
	}
	if (auto* Metrics = GeometryMetrics.Find({EVoxelTerrainRepresentation::Fine, InPage.Coordinate, InPage.Level}); Metrics && InVisible.IsSet())
		Metrics->bPresented = bPresented;
	bool bAnyChanged = bPublishedMesh;
	TArray<FIntVector> Sections;
	FVoxelFinePagePlanner::GatherSections(InPage, Sections);
	for (const FIntVector& Section : Sections)
	{
		const FVoxelViewKey Node{Section, 0};
		const FVoxelFinePageKey* Previous = PresentedFineSectionOwners.Find(Section);
		bool bChanged = false;
		if (bPresented && InDesired.Contains(Node))
		{
			bChanged = !Previous || *Previous != InPage;
			if (bChanged)
			{
				PresentedFineSectionOwners.Add(Section, InPage);
			}
			bChanged |= CommitTerrainOwnership({0, Section, 0}, MakeArrayView(&Node, 1));
		}
		else if (Previous && *Previous == InPage && InVisible.IsSet() && !InVisible.GetValue())
		{
			PresentedFineSectionOwners.Remove(Section);
			bChanged = true;
			if (!InRetainedFine.Contains(Section))
			{
				bChanged |= CommitTerrainOwnership({0, Section, 0}, {});
			}
		}
		if (bChanged || bPublishedMesh)
		{
			MarkCoverageDirty(FineCoverageRect(Section));
		}
		bAnyChanged |= bChanged;
	}
	bTerrainBuildStageDirty |= bAnyChanged;
	return bAnyChanged;
}

double FVoxelViewManager::TerrainDeadline(const double InMaximumMilliseconds) const
{
	const double Now = FPlatformTime::Seconds();
	const double Remaining = bTerrainFrameBudgetActive ? TerrainFrameBudget.RemainingMilliseconds(Now) : InMaximumMilliseconds;
	return Now + FMath::Min(Remaining, InMaximumMilliseconds) / 1000.0;
}

void FVoxelViewManager::IndexCoverage(const FVoxelViewKey& InKey)
{
	auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	Record->Geometry.Key = {InKey.Level == 0 ? EVoxelTerrainRepresentation::Fine : EVoxelTerrainRepresentation::VoxelProxy, InKey.Coordinate, InKey.Level};
	if (InKey.Level > 0)
	{
		if (const auto Data = VoxelProxyData.FindRef(InKey))
			Record->Geometry = Data->Geometry;
		else if (const auto* Geometry = GetViewInterest().FindGeometry(InKey))
			Record->Geometry = *Geometry;
	}
	Record->Stamp = {WorldEpoch, 0, InKey.Level == 0 ? FineRevisions.FindRef(InKey.Coordinate) : VoxelProxyRevisions.FindRef(InKey), 0};
	Record->bCommitted = VisibleTerrainNodes.Contains(InKey);
	Record->bWanted = InKey.Level == 0 ? FineWanted.Contains(InKey.Coordinate) : VoxelProxyWanted.Contains(InKey);
	Record->bReady = InKey.Level == 0 ? FineReady.Contains(InKey.Coordinate) : VoxelProxyReady.Contains(InKey);
	Record->bHasGeometry =
		Record->bReady && (InKey.Level > 0 ? !AirVoxelProxyNodes.Contains(InKey)
										   : (FineSectionMeshes.FindRef(InKey.Coordinate) && HasRenderableMesh(*FineSectionMeshes.FindRef(InKey.Coordinate))));
	if (!Record->bWanted && !Record->bReady && !Record->bCommitted)
	{
		CoverageIndex.Remove(Record->Geometry.Key);
		return;
	}
	const auto Bounds = InKey.GetBounds();
	Record->Min = Bounds.Min;
	Record->Max = Bounds.Max;
	FString Error;
	checkf(CoverageIndex.Upsert(Record, Error), TEXT("%s"), *Error);
}

void FVoxelViewManager::IndexCoverage(const FVoxelSurfaceTileKey& InKey)
{
	auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	Record->Geometry.Key = {EVoxelTerrainRepresentation::Surface, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level};
	if (const auto Data = SurfaceData.FindRef(InKey))
		Record->Geometry = Data->Geometry;
	else if (const auto* Geometry = GetViewInterest().FindGeometry(InKey))
		Record->Geometry = *Geometry;
	Record->Geometry.WorldMinZ = Module.GetManifest().Settings.MinZ;
	Record->Geometry.WorldMaxZ = Module.GetManifest().Settings.MaxZ;
	Record->Stamp = {WorldEpoch, 0, SurfaceRevisions.FindRef(InKey), 0};
	Record->bWanted = SurfaceWanted.Contains(InKey);
	Record->bReady = SurfaceReady.Contains(InKey);
	Record->bHasGeometry = Record->bReady;
	Record->bCommitted = SurfaceActors.FindRef(InKey) && Publisher->IsCommitted(SurfaceActors.FindRef(InKey));
	if (!Record->bWanted && !Record->bReady && !Record->bCommitted)
	{
		CoverageIndex.Remove(Record->Geometry.Key);
		HeightfieldCoverageIndex.Remove(Record->Geometry.Key);
		return;
	}
	Record->Geometry.Key.TryBounds(Record->Geometry.WorldMinZ, Record->Geometry.WorldMaxZ, Record->Min, Record->Max);
	FString Error;
	checkf(CoverageIndex.Upsert(Record, Error), TEXT("%s"), *Error);
	checkf(HeightfieldCoverageIndex.Upsert(Record, Error), TEXT("%s"), *Error);
}

void FVoxelViewManager::IndexCoverage(const FVoxelMacroTileKey& InKey)
{
	auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	Record->Geometry.Key = {EVoxelTerrainRepresentation::Macro, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level};
	if (const auto Data = MacroData.FindRef(InKey))
		Record->Geometry = Data->Geometry;
	else if (const auto* Geometry = GetViewInterest().FindGeometry(InKey))
		Record->Geometry = *Geometry;
	Record->Geometry.WorldMinZ = Module.GetManifest().Settings.MinZ;
	Record->Geometry.WorldMaxZ = Module.GetManifest().Settings.MaxZ;
	Record->Stamp = {WorldEpoch, 0, MacroRevisions.FindRef(InKey), 0};
	Record->bWanted = MacroWanted.Contains(InKey);
	Record->bReady = MacroReady.Contains(InKey);
	Record->bHasGeometry = Record->bReady;
	Record->bCommitted = MacroActors.FindRef(InKey) && Publisher->IsCommitted(MacroActors.FindRef(InKey));
	if (!Record->bWanted && !Record->bReady && !Record->bCommitted)
	{
		CoverageIndex.Remove(Record->Geometry.Key);
		HeightfieldCoverageIndex.Remove(Record->Geometry.Key);
		return;
	}
	Record->Geometry.Key.TryBounds(Record->Geometry.WorldMinZ, Record->Geometry.WorldMaxZ, Record->Min, Record->Max);
	FString Error;
	checkf(CoverageIndex.Upsert(Record, Error), TEXT("%s"), *Error);
	checkf(HeightfieldCoverageIndex.Upsert(Record, Error), TEXT("%s"), *Error);
}

void FVoxelViewManager::CaptureCoverageCandidates(
	TSet<FIntVector>& OutFine, TSet<FVoxelViewKey>& OutProxy, TSet<FVoxelSurfaceTileKey>& OutSurface, TSet<FVoxelMacroTileKey>& OutMacro) const
{
	for (const FVoxelCoverageRect& Rect : ActiveDirtyCoverageRects)
	{
		FVoxelCoverageLocalSnapshot Snapshot;
		CoverageIndex.Capture(FIntVector(Rect.Min.X - 1, Rect.Min.Y - 1, MIN_int32 + 1), FIntVector(Rect.Max.X + 1, Rect.Max.Y + 1, MAX_int32), Snapshot);
		CoverageIndexNodesVisited += Snapshot.VisitedIndexNodes;
		CoverageRecordsExamined += Snapshot.ExaminedRecords;
		CoverageRecordsCaptured += Snapshot.Records.Num();
		CoverageReadGuards += Snapshot.Guards.Num();
		for (const auto& Record : Snapshot.Records)
		{
			const auto& Key = Record->Geometry.Key;
			switch (Key.Representation)
			{
			case EVoxelTerrainRepresentation::Fine:
				OutFine.Add(Key.Coordinate);
				break;
			case EVoxelTerrainRepresentation::VoxelProxy:
				OutProxy.Add({Key.Coordinate, Key.Level});
				break;
			case EVoxelTerrainRepresentation::Surface:
				OutSurface.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				break;
			case EVoxelTerrainRepresentation::Macro:
				OutMacro.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				break;
			default:
				break;
			}
		}
	}
}

void FVoxelViewManager::RefreshIndexedCoverage(const FVoxelCoverageRect& InBounds)
{
	FVoxelCoverageLocalSnapshot Snapshot;
	CoverageIndex.Capture(FIntVector(InBounds.Min.X, InBounds.Min.Y, MIN_int32 + 1), FIntVector(InBounds.Max.X, InBounds.Max.Y, MAX_int32), Snapshot);
	for (const auto& Record : Snapshot.Records)
	{
		const auto& Key = Record->Geometry.Key;
		switch (Key.Representation)
		{
		case EVoxelTerrainRepresentation::Fine:
		case EVoxelTerrainRepresentation::VoxelProxy:
			IndexCoverage(FVoxelViewKey{Key.Coordinate, Key.Level});
			break;
		case EVoxelTerrainRepresentation::Surface:
			IndexCoverage(FVoxelSurfaceTileKey{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			break;
		case EVoxelTerrainRepresentation::Macro:
			IndexCoverage(FVoxelMacroTileKey{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			break;
		default:
			break;
		}
	}
}

const FVoxelInterestSet& FVoxelViewManager::GetViewInterest() const
{
	if (bConsumingPublicationInputs && PendingInterestSnapshot)
	{
		return *PendingInterestSnapshot;
	}
	if (CoverageInterestSnapshot)
		return *CoverageInterestSnapshot;
	return AppliedInterestSnapshot ? *AppliedInterestSnapshot : Module.GetCurrentInterest();
}

void FVoxelViewManager::MarkFinePageDirty(const FVoxelFinePageKey& InPage)
{
	FinePageFailures.Remove(InPage);
	++FinePageContentRevisions.FindOrAdd(InPage);
	DirtyFinePages.Add(InPage);
	bFinePresentationDirty = true;
}

void FVoxelViewManager::InvalidateFinePageLayout(const FVoxelFinePageKey& InPage)
{
	FinePageVersions.Add(InPage, ++NextFinePageVersion);
	PreparedFinePageMeshes.Remove(InPage);
	FinePageCaptures.Remove(InPage);
	MarkFinePageDirty(InPage);
}

void FVoxelViewManager::RetireFinePageBuildState(const FVoxelFinePageKey& InPage, const bool bCancelTask)
{
	if (const FFinePageAttempt* Pending = PendingFinePageBuilds.Find(InPage); Pending && bCancelTask)
	{
		const FVoxelTaskStamp Expected = Pending->Stamp;
		Scheduler.CancelMatching(
			[Expected](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
			{
				return Kind == EVoxelTaskKind::BuildFineMesh && Stamp == Expected;
			});
	}
	DirtyFinePages.Remove(InPage);
	PreparedFinePageMeshes.Remove(InPage);
	PendingFinePageBuilds.Remove(InPage);
	FinePageFailures.Remove(InPage);
	FinePageCaptures.Remove(InPage);
	FinePageVersions.Remove(InPage);
	FinePageContentRevisions.Remove(InPage);
}

bool FVoxelViewManager::CaptureFinePageInput(const FIntVector& InSection, FFinePageInput& OutInput, uint64& InOutSignature) const
{
	const auto Mix = [&InOutSignature](const uint64 Value)
	{
		InOutSignature = (InOutSignature ^ Value) * 1099511628211ull;
	};
	const auto Mesh = FineSectionMeshes.FindRef(InSection);
	if (!FineWanted.Contains(InSection) || !FineReady.Contains(InSection) || !Mesh)
		return false;
	FFinePageInput Input{InSection, Mesh, 0};
	for (uint8 Face = 0; Face < 6; ++Face)
	{
		const FIntVector Neighbor = InSection + VoxelCoord::Direction(Face);
		if (FineWanted.Contains(Neighbor))
			continue;
		FVoxelViewKey Owner{Neighbor, 0};
		for (uint8 Level = 1; Level <= Module.GetViewSettings().Lod.MaximumVoxelProxyLevel; ++Level)
		{
			Owner = Owner.GetParent();
			const auto Context = DesiredVolumeContexts.FindRef(Owner);
			if (!Context)
				continue;
			const uint64 ContextSignature = Context->Signature(VoxelProxyRevisions.FindRef(Owner));
			if (PreparedVolumeSignatures.FindRef(Owner) != ContextSignature && VolumeTransitionSignatures.FindRef(Owner) != ContextSignature)
				continue;
			if (Context->Patches.ContainsByPredicate(
					[InSection, Face](const auto& Patch)
					{
						return Patch.Face.Neighbor == FVoxelViewKey{InSection, 0} && (static_cast<uint8>(Patch.Face.Direction) ^ 1) == Face;
					}))
			{
				Input.SuppressedFaces |= 1 << Face;
				Mix(ContextSignature);
				break;
			}
		}
	}
	Mix(GetTypeHash(InSection));
	Mix(FineRevisions.FindRef(InSection));
	Mix(Input.SuppressedFaces);
	OutInput = MoveTemp(Input);
	return true;
}

bool FVoxelViewManager::AdvanceFinePageCapture(
	const FVoxelFinePageKey& InPage, TArray<FFinePageInput>& OutInputs, uint64& OutSignature, const double InDeadline)
{
	FFinePageCapture& Capture = FinePageCaptures.FindOrAdd(InPage);
	const uint64 PageVersion = FinePageVersions.FindRef(InPage);
	const uint64 ContentRevision = FinePageContentRevisions.FindRef(InPage);
	if (Capture.NextSection == 0 || Capture.PageVersion != PageVersion || Capture.ContentRevision != ContentRevision)
	{
		Capture = {};
		Capture.PageVersion = PageVersion;
		Capture.ContentRevision = ContentRevision;
		Capture.Signature = (Capture.Signature ^ GetTypeHash(InPage)) * 1099511628211ull;
	}
	const int32 Side = 1 << InPage.Level;
	const int32 Count = Side * Side * Side;
	const FIntVector Min = InPage.GetBounds().Min / 16;
	for (int32 Operations = 0; Operations < 16 && Capture.NextSection < Count && FPlatformTime::Seconds() < InDeadline; ++Operations)
	{
		const int32 Index = Capture.NextSection;
		const FIntVector Section = Min + FIntVector(Index % Side, (Index / Side) % Side, Index / (Side * Side));
		FFinePageInput Input;
		uint64 Signature = Capture.Signature;
		if (!CaptureFinePageInput(Section, Input, Signature))
		{
			return false;
		}
		Capture.Signature = Signature;
		Capture.Inputs.Add(MoveTemp(Input));
		++Capture.NextSection;
	}
	if (Capture.NextSection < Count)
	{
		return false;
	}
	OutInputs = MoveTemp(Capture.Inputs);
	OutSignature = Capture.Signature;
	FinePageCaptures.Remove(InPage);
	return true;
}

bool FVoxelViewManager::HasCriticalNearFieldPending() const
{
	const auto Pending = [this](const FIntVector& Key, const FVoxelSection* Section, const TCHAR* Reason)
	{
#if !UE_BUILD_SHIPPING
		const double Now = FPlatformTime::Seconds();
		if (Now >= NextCriticalPendingLog)
		{
			NextCriticalPendingLog = Now + 5.0;
			UE_LOG(LogTemp, Display,
				TEXT("Voxel critical view blocker: epoch=%llu section=%s reason=%s status=%d fineWanted=%d fineReady=%d revision=%llu cachedRevision=%llu"),
				WorldEpoch, *Key.ToString(), Reason, Section ? static_cast<int32>(Section->Status) : -1, FineWanted.Contains(Key), FineReady.Contains(Key),
				Section ? Section->CommittedRevision : 0, FineRevisions.FindRef(Key));
			if (const auto* Page = FinePagePlan.SectionOwners.Find(Key))
			{
				AActor* Actor = FinePageActors.FindRef(*Page);
				const FVoxelGenerationBounds Bounds = Page->GetBounds();
				UE_LOG(LogTemp, Display,
					TEXT("Voxel critical Fine Page: page=%s level=%u ready=%d dirty=%d pending=%d prepared=%d attempt=%d empty=%d presentedEmpty=%d owned=%d "
						 "actor=%d hidden=%d committed=%d busy=%d volumeHandoff=%d"),
					*Page->Coordinate.ToString(), Page->Level, FinePageReady.Contains(*Page), DirtyFinePages.Contains(*Page), PendingFinePages.Contains(*Page),
					PreparedFinePageMeshes.Contains(*Page), PendingFinePageBuilds.Contains(*Page), EmptyFinePages.Contains(*Page),
					PresentedEmptyFinePages.Contains(*Page), PresentedFineSectionOwners.Contains(Key), Actor != nullptr, Actor && Actor->IsHidden(),
					Actor && Publisher->IsCommitted(Actor), Publisher->IsGroupBusy({0, Page->Coordinate, Page->Level}),
					VoxelCoverage::IntersectsAny3D({Bounds.Min, Bounds.Max}, PendingVolumeHandoffBoxes));
			}
		}
#endif
		return true;
	};
	for (const auto& Pair : Module.GetCurrentInterest().Exact)
	{
		const FVoxelExactDemand& Demand = Pair.Value;
		const bool bCritical =
			Demand.bWarmupData || Demand.bWarmupCollision || Demand.bMovementCriticalData || Demand.bMovementCriticalFine || Demand.bMovementCriticalCollision;
		if (!bCritical)
		{
			continue;
		}

		const FVoxelSection* Section = Module.GetRuntime() ? Module.GetRuntime()->FindSection(Pair.Key) : nullptr;
		if (!Section)
		{
			return Pending(Pair.Key, Section, TEXT("SectionMissing"));
		}

		if (Section->Status == EVoxelSectionStatus::Failed)
		{
			continue;
		}

		if (Section->Status != EVoxelSectionStatus::DataReady)
		{
			return Pending(Pair.Key, Section, TEXT("DataReady"));
		}

		if ((Demand.bWarmupCollision || Demand.bMovementCriticalCollision) && !Module.IsCollisionReady(Pair.Key))
		{
			return Pending(Pair.Key, Section, TEXT("CollisionReady"));
		}

		if (Demand.bMovementCriticalFine && !FineReady.Contains(Pair.Key))
		{
			return Pending(Pair.Key, Section, TEXT("FineReady"));
		}
		if (Demand.bMovementCriticalFine && !IsFinePresented(Pair.Key))
		{
			return Pending(Pair.Key, Section, TEXT("FinePresented"));
		}
	}

	return false;
}

bool FVoxelViewManager::IsCriticalDependency(const FVoxelTerrainSpatialKey& InKey) const
{
	if (Module.GetCurrentInterest().CriticalDependencies.Contains(InKey))
		return true;
	if (InKey.Representation == EVoxelTerrainRepresentation::VoxelProxy)
	{
		return VolumeTransitionWorkClass({InKey.Coordinate, InKey.Level}) == EVoxelWorkClass::Critical;
	}
	return false;
}

bool FVoxelViewManager::IsCriticalDependency(const FVoxelViewAdmission& InAdmission) const
{
	if (InAdmission.Kind == EVoxelViewAdmissionKind::Fine)
	{
		const auto& Interest = Module.GetCurrentInterest();
		if (Interest.FinePagePlan)
		{
			const auto* Page = Interest.FinePagePlan->SectionOwners.Find(InAdmission.FineKey);
			if (Page && Interest.CriticalFinePages.Contains(*Page))
			{
				return true;
			}
		}
		const auto* Demand = Interest.Exact.Find(InAdmission.FineKey);
		return Demand && (Demand->bWarmupData || Demand->bWarmupCollision || Demand->bMovementCriticalData || Demand->bMovementCriticalCollision ||
							 Demand->bMovementCriticalFine);
	}
	const FVoxelTerrainSpatialKey Key =
		InAdmission.Kind == EVoxelViewAdmissionKind::VoxelProxy
			? FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::VoxelProxy, InAdmission.ProxyKey.Coordinate, InAdmission.ProxyKey.Level}
		: InAdmission.Kind == EVoxelViewAdmissionKind::Surface
			? FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Surface,
				  FIntVector(InAdmission.SurfaceKey.Coordinate.X, InAdmission.SurfaceKey.Coordinate.Y, 0), InAdmission.SurfaceKey.Level}
			: FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Macro, FIntVector(InAdmission.MacroKey.Coordinate.X, InAdmission.MacroKey.Coordinate.Y, 0),
				  InAdmission.MacroKey.Level};
	return IsCriticalDependency(Key);
}

FVoxelGameThreadContinuation FVoxelViewManager::CaptureCoverageCandidateSteps(
	TSet<FIntVector>& OutFine, TSet<FVoxelViewKey>& OutProxy, TSet<FVoxelSurfaceTileKey>& OutSurface, TSet<FVoxelMacroTileKey>& OutMacro) const
{
	if (bActiveCoverageFull)
	{
		FVoxelCoverageLocalSnapshot Snapshot;
		co_await CoverageIndex.CaptureAllSteps(Snapshot);
		for (const auto& Record : Snapshot.Records)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto& Key = Record->Geometry.Key;
			switch (Key.Representation)
			{
			case EVoxelTerrainRepresentation::Fine:
				OutFine.Add(Key.Coordinate);
				break;
			case EVoxelTerrainRepresentation::VoxelProxy:
				OutProxy.Add({Key.Coordinate, Key.Level});
				break;
			case EVoxelTerrainRepresentation::Surface:
				OutSurface.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				break;
			case EVoxelTerrainRepresentation::Macro:
				OutMacro.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				break;
			default:
				break;
			}
		}
		co_return;
	}
	for (const FVoxelCoverageRect& Rect : ActiveDirtyCoverageRects)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		FVoxelCoverageLocalSnapshot Snapshot;
		co_await CoverageIndex.CaptureSteps(
			FIntVector(Rect.Min.X - 1, Rect.Min.Y - 1, MIN_int32 + 1), FIntVector(Rect.Max.X + 1, Rect.Max.Y + 1, MAX_int32), Snapshot);
		CoverageIndexNodesVisited += Snapshot.VisitedIndexNodes;
		CoverageRecordsExamined += Snapshot.ExaminedRecords;
		CoverageRecordsCaptured += Snapshot.Records.Num();
		CoverageReadGuards += Snapshot.Guards.Num();
		for (const auto& Record : Snapshot.Records)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto& Key = Record->Geometry.Key;
			switch (Key.Representation)
			{
			case EVoxelTerrainRepresentation::Fine:
				OutFine.Add(Key.Coordinate);
				break;
			case EVoxelTerrainRepresentation::VoxelProxy:
				OutProxy.Add({Key.Coordinate, Key.Level});
				break;
			case EVoxelTerrainRepresentation::Surface:
				OutSurface.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				break;
			case EVoxelTerrainRepresentation::Macro:
				OutMacro.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				break;
			default:
				break;
			}
		}
	}
}

FVoxelGameThreadContinuation FVoxelViewManager::BuildCoverageSnapshotSteps(FCoveragePassSnapshot& OutSnapshot) const
{
	++CoverageSnapshotBuildCount;
	OutSnapshot = {};
	TSet<FIntVector> FineCandidates;
	TSet<FVoxelViewKey> ProxyCandidates;
	TSet<FVoxelSurfaceTileKey> SurfaceCandidates;
	TSet<FVoxelMacroTileKey> MacroCandidates;
	co_await CaptureCoverageCandidateSteps(FineCandidates, ProxyCandidates, SurfaceCandidates, MacroCandidates);
	for (const FIntVector& Section : FineCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!FineReady.Contains(Section) || !FineWanted.Contains(Section) || !VisibleTerrainNodes.Contains({Section, 0}) || !IsFinePresented(Section))
			continue;
		OutSnapshot.PresentedFineSections.Add(Section);
		OutSnapshot.PresentedFineOwners.Add(Section, PresentedFineSectionOwners.FindChecked(Section));
		OutSnapshot.Fine.Add(FineCoverageBox(Section));
		OutSnapshot.FineNodes.Add({Section, 0});
	}
	for (const FVoxelViewKey& Node : ProxyCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!VoxelProxyReady.Contains(Node) || !VoxelProxyWanted.Contains(Node) || !VisibleTerrainNodes.Contains(Node))
			continue;
		AActor* Actor = VoxelProxyActors.FindRef(Node);
		if (Actor && (Actor->IsHidden() || !Publisher->IsCommitted(Actor)))
			continue;
		OutSnapshot.ProxyNodes.Add(Node);
		OutSnapshot.Proxy.Add(VoxelProxyCoverageBox(Node));
		if (const auto* Rects = ProxySurfaceCoverage.Find(Node))
		{
			co_await VoxelGameThread::AppendSteps(OutSnapshot.ProxySurface, *Rects);
		}
	}
	for (const auto& Key : SurfaceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		AActor* Actor = SurfaceActors.FindRef(Key);
		if (SurfaceWanted.Contains(Key) && SurfaceReady.Contains(Key) && Actor && !Actor->IsHidden() && Publisher->IsCommitted(Actor))
			OutSnapshot.Surface.Add(SurfaceCoverageRect(Key));
	}
	for (const auto& Key : MacroCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		AActor* Actor = MacroActors.FindRef(Key);
		if (MacroWanted.Contains(Key) && MacroReady.Contains(Key) && Actor && !Actor->IsHidden() && Publisher->IsCommitted(Actor))
			OutSnapshot.Macro.Add(MacroCoverageRect(Key));
	}
	co_await VoxelGameThread::AppendSteps(OutSnapshot.Heightfield, OutSnapshot.Surface);
	co_await VoxelGameThread::AppendSteps(OutSnapshot.Heightfield, OutSnapshot.Macro);
	for (const auto& Key : SurfaceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		OutSnapshot.WantedHeightfield.Add(SurfaceCoverageRect(Key));
	}
	for (const auto& Key : MacroCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		OutSnapshot.WantedHeightfield.Add(MacroCoverageRect(Key));
	}
	const auto BuildBranches = [](const TSet<FVoxelViewKey>& Nodes, TSet<FVoxelViewKey>& Branches) -> FVoxelGameThreadContinuation
	{
		for (FVoxelViewKey Node : Nodes)
		{
			while (Node.Level <= 20)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				bool bExists = false;
				Branches.Add(Node, &bExists);
				if (bExists)
					break;
				Node = Node.GetParent();
			}
		}
	};
	co_await BuildBranches(OutSnapshot.FineNodes, OutSnapshot.FineBranches);
	co_await BuildBranches(OutSnapshot.ProxyNodes, OutSnapshot.ProxyBranches);
}

FVoxelGameThreadContinuation FVoxelViewManager::RebuildHeightfieldTransitionSteps(const FCoveragePassSnapshot& CoverageSnapshot)
{
	TArray<FVoxelHeightfieldTileView> Views;
	TArray<FHeightfieldTransitionTileSource> ViewPins;
	TSet<FIntVector> FineCandidates;
	TSet<FVoxelViewKey> ProxyCandidates;
	TSet<FVoxelSurfaceTileKey> SurfaceCandidates;
	TSet<FVoxelMacroTileKey> MacroCandidates;
	co_await CaptureCoverageCandidateSteps(FineCandidates, ProxyCandidates, SurfaceCandidates, MacroCandidates);
	TSet<FVoxelHeightfieldNodeKey> LocalOwners;
	for (const auto& Key : SurfaceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (CoverageAffects(SurfaceCoverageRect(Key)))
			LocalOwners.Add({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level});
	}
	for (const auto& Key : MacroCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (CoverageAffects(MacroCoverageRect(Key)))
			LocalOwners.Add({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level});
	}
	if (!bActiveCoverageFull)
		for (const auto& Owner : LocalOwners)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto Rect = Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Owner.Coordinate, Owner.Level})
																							   : MacroCoverageRect({Owner.Coordinate, Owner.Level});
			FVoxelCoverageLocalSnapshot Neighbors;
			co_await HeightfieldCoverageIndex.CaptureSteps(
				FIntVector(Rect.Min.X - 1, Rect.Min.Y - 1, MIN_int32 + 1), FIntVector(Rect.Max.X + 1, Rect.Max.Y + 1, MAX_int32), Neighbors);
			for (const auto& Record : Neighbors.Records)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				const auto& Key = Record->Geometry.Key;
				if (Key.Representation == EVoxelTerrainRepresentation::Surface)
					SurfaceCandidates.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				else if (Key.Representation == EVoxelTerrainRepresentation::Macro)
					MacroCandidates.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			}
		}
	Views.Reserve(SurfaceCandidates.Num() + MacroCandidates.Num());
	for (const auto& Key : SurfaceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelSurfaceTileKey, TSharedPtr<const FVoxelSurfaceTileData>> Pair(Key, SurfaceData.FindRef(Key));
		if (!Pair.Value || !SurfaceReady.Contains(Pair.Key) || !SurfaceActors.FindRef(Pair.Key))
		{
			continue;
		}
		if (!SurfaceWanted.Contains(Pair.Key))
		{
			bool bReplaced = false;
			co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Pair.Key), CoverageSnapshot.Macro, bReplaced);
			if (!bReplaced)
			{
				co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Pair.Key), CoverageSnapshot.ProxySurface, bReplaced);
			}
			if (bReplaced)
			{
				continue;
			}
		}
		Views.Add(FVoxelHeightfieldTransitionBuilder::MakeView(*Pair.Value));
		FHeightfieldTransitionTileSource Pin;
		Pin.Surface = Pair.Value;
		ViewPins.Add(MoveTemp(Pin));
	}
	for (const auto& Key : MacroCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelMacroTileKey, TSharedPtr<const FVoxelMacroTileData>> Pair(Key, MacroData.FindRef(Key));
		if (!Pair.Value || !MacroReady.Contains(Pair.Key) || !MacroActors.FindRef(Pair.Key))
		{
			continue;
		}
		if (!MacroWanted.Contains(Pair.Key))
		{
			bool bReplaced = false;
			co_await VoxelCoverage::IsFullyCovered2DSteps(MacroCoverageRect(Pair.Key), CoverageSnapshot.Surface, bReplaced);
			if (bReplaced)
			{
				continue;
			}
		}
		Views.Add(FVoxelHeightfieldTransitionBuilder::MakeView(*Pair.Value));
		FHeightfieldTransitionTileSource Pin;
		Pin.Macro = Pair.Value;
		ViewPins.Add(MoveTemp(Pin));
	}
	TArray<FVoxelHeightfieldTileFootprint> PendingTiles;
	PendingTiles.Reserve(SurfaceCandidates.Num() + MacroCandidates.Num());
	for (const FVoxelSurfaceTileKey& Key : SurfaceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!SurfaceWanted.Contains(Key))
			continue;
		if (SurfaceReady.Contains(Key) && SurfaceData.Contains(Key) && SurfaceActors.FindRef(Key))
			continue;
		const auto* DesiredGeometry = GetViewInterest().FindGeometry(Key);
		if (!DesiredGeometry)
			continue;
		const int32 Step = DesiredGeometry->SampleStepCells;
		const int32 Side = VoxelViewLod::SurfaceSide(Module.GetViewSettings().Lod, Key.Level);
		PendingTiles.Add({{EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level}, Key.Coordinate * Side, Side, Step});
	}
	for (const FVoxelMacroTileKey& Key : MacroCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!MacroWanted.Contains(Key))
			continue;
		if (MacroReady.Contains(Key) && MacroData.Contains(Key) && MacroActors.FindRef(Key))
			continue;
		const auto* DesiredGeometry = GetViewInterest().FindGeometry(Key);
		if (!DesiredGeometry)
			continue;
		const int32 Step = DesiredGeometry->SampleStepCells;
		const int32 Side = VoxelViewLod::MacroSide(Module.GetViewSettings().Lod, Key.Level);
		PendingTiles.Add({{EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level}, Key.Coordinate * Side, Side, Step});
	}
	TMap<FVoxelHeightfieldNodeKey, int32> ViewIndices;
	TMap<FVoxelHeightfieldNodeKey, int32> PendingIndices;
	for (int32 Index = 0; Index < Views.Num(); ++Index)
		ViewIndices.Add(Views[Index].Key, Index);
	for (int32 Index = 0; Index < PendingTiles.Num(); ++Index)
		PendingIndices.Add(PendingTiles[Index].Key, Index);
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>> Dependencies;
	co_await FVoxelHeightfieldTransitionBuilder::GatherOwnerDependenciesSteps(Views, PendingTiles, Dependencies);

	if (bActiveCoverageFull)
		PendingHeightfieldPlanOwners.Reset();
	else
		for (const auto& Key : LocalOwners)
			PendingHeightfieldPlanOwners.Remove(Key);
	TArray<FVoxelHeightfieldTransitionEdge> Edges;
	for (const auto& OwnerView : Views)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelHeightfieldNodeKey Owner = OwnerView.Key;
		if (!LocalOwners.Contains(Owner))
			continue;
		TArray<FVoxelHeightfieldTileView> LocalViews;
		TArray<FVoxelHeightfieldTileFootprint> LocalPending;
		TArray<FHeightfieldTransitionTileSource> Pins;
		uint64 Signature = 1469598103934665603ull;
		const auto Mix = [&Signature](const uint64 Value)
		{
			Signature = (Signature ^ Value) * 1099511628211ull;
		};
		const auto& Keys = Dependencies.FindChecked(Owner);
		for (const auto& Key : Keys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			Mix(GetTypeHash(Key));
			if (const int32* Index = ViewIndices.Find(Key))
			{
				LocalViews.Add(Views[*Index]);
				Mix(Views[*Index].Revision);
				Mix(Views[*Index].GeometryRevision);
				FHeightfieldTransitionTileSource Pin;
				if (Key.Representation == EVoxelHeightfieldRepresentation::Surface)
					Pin.Surface = SurfaceData.FindRef({Key.Coordinate, Key.Level});
				else
					Pin.Macro = MacroData.FindRef({Key.Coordinate, Key.Level});
				Pins.Add(MoveTemp(Pin));
			}
			else if (const int32* PendingIndex = PendingIndices.Find(Key))
			{
				Mix(0);
				LocalPending.Add(PendingTiles[*PendingIndex]);
			}
		}
		auto& State = HeightfieldCoveragePlans.FindOrAdd(Owner);
		State.DesiredSignature = Signature;
		if (State.Plan)
		{
			const auto PlanSnapshot = State.Plan;
			co_await VoxelGameThread::AppendSteps(Edges, PlanSnapshot->Edges);
		}
		if (!State.Plan || State.Plan->Signature != Signature)
		{
			for (const auto& Key : Keys)
				PendingHeightfieldPlanOwners.Add(Key);
			// 高度场拓扑使用不可变输入完成一次规划；Fine 的移动裁剪由发布器单独处理。
			if (State.PendingSignature != 0)
				continue;
			FVoxelTaskRequest Request;
			Request.Kind = EVoxelTaskKind::BuildViewCoverage;
			Request.WorkClass = EVoxelWorkClass::Boundary;
			Request.TerrainStage = Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? 2 : 3;
			if (Owner.Representation == EVoxelHeightfieldRepresentation::Surface)
			{
				if (const auto* Priority = GetViewInterest().SurfacePriorities.Find({Owner.Coordinate, Owner.Level}))
					Request.SourcePriority = *Priority;
			}
			else if (const auto* Priority = GetViewInterest().MacroPriorities.Find({Owner.Coordinate, Owner.Level}))
				Request.SourcePriority = *Priority;
			Request.Stamp.WorldEpoch = WorldEpoch;
			Request.Stamp.Section = FIntVector(Owner.Coordinate.X, Owner.Coordinate.Y, static_cast<int32>(Owner.Representation));
			Request.Stamp.ViewKey.Level = Owner.Level;
			Request.Stamp.Token = Signature;
			Request.ReservedBytes = 1024ull * 1024ull;
			Request.InputBytes = LocalViews.GetAllocatedSize() + LocalPending.GetAllocatedSize();
			Request.DistanceScore = Owner.Representation == EVoxelHeightfieldRepresentation::Surface
										? MinimumObserverDistanceCells(SurfaceWorldCenter({Owner.Coordinate, Owner.Level}))
										: MinimumObserverDistanceCells(MacroWorldCenter({Owner.Coordinate, Owner.Level}));
			Request.Execute = [Owner, LocalViews = MoveTemp(LocalViews), LocalPending = MoveTemp(LocalPending), Pins = MoveTemp(Pins), Signature](
								  const TAtomic<bool>& Cancel)
			{
				FVoxelTaskResult Result;
				if (Cancel.Load())
					return Result;
				auto Payload = MakeShared<FVoxelHeightfieldCoveragePlan, ESPMode::ThreadSafe>();
				Payload->Signature = Signature;
				if (!FVoxelHeightfieldTransitionBuilder::BuildEdges(LocalViews, Payload->Edges, Result.Error))
					return Result;
				FVoxelHeightfieldTransitionBuilder::AppendPendingEdges(LocalViews, LocalPending, Payload->Edges);
				Payload->Edges.RemoveAll(
					[Owner](const auto& Edge)
					{
						return !(Edge.Owner == Owner);
					});
				Result.CustomPayload = Payload;
				Result.bSuccess = !Cancel.Load();
				return Result;
			};
			const FVoxelCoverageRect OwnerRect = Owner.Representation == EVoxelHeightfieldRepresentation::Surface
													 ? SurfaceCoverageRect({Owner.Coordinate, Owner.Level})
													 : MacroCoverageRect({Owner.Coordinate, Owner.Level});
			FVoxelCoverageLocalSnapshot Guard;
			co_await HeightfieldCoverageIndex.CaptureSteps(FIntVector(OwnerRect.Min.X - 1, OwnerRect.Min.Y - 1, Module.GetManifest().Settings.MinZ),
				FIntVector(OwnerRect.Max.X + 1, OwnerRect.Max.Y + 1, Module.GetManifest().Settings.MaxZ), Guard, false);
			auto& ResumedState = HeightfieldCoveragePlans.FindChecked(Owner);
			const uint64 Serial = HeightfieldPlanSerial;
			Request.Apply = [this, Serial, Owner, Signature, Guard = MoveTemp(Guard)](FVoxelTaskResult&& Result)
			{
				if (Serial != HeightfieldPlanSerial || Result.Stamp.WorldEpoch != WorldEpoch)
					return;
				auto* Current = HeightfieldCoveragePlans.Find(Owner);
				if (!Current || Current->PendingSignature != Signature)
					return;
				Current->PendingSignature = 0;
				if (!HeightfieldCoverageIndex.IsCurrent(Guard))
				{
					MarkCoverageDirty(Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Owner.Coordinate, Owner.Level})
																									   : MacroCoverageRect({Owner.Coordinate, Owner.Level}));
					return;
				}
				if (Result.bSuccess && !Result.bCanceled &&
					(!Current->Plan || Current->Plan->Signature != Current->DesiredSignature || Signature == Current->DesiredSignature))
				{
					Current->Plan = StaticCastSharedPtr<const FVoxelHeightfieldCoveragePlan>(Result.CustomPayload);
				}
				MarkCoverageDirty(Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Owner.Coordinate, Owner.Level})
																								   : MacroCoverageRect({Owner.Coordinate, Owner.Level}));
			};
			if (EnqueueViewTask(MoveTemp(Request)))
				ResumedState.PendingSignature = Signature;
			continue;
		}
	}
	if (bActiveCoverageFull)
	{
		for (auto It = HeightfieldCoveragePlans.CreateIterator(); It; ++It)
			if (!ViewIndices.Contains(It.Key()))
				It.RemoveCurrent();
	}
	else
		for (const auto& Key : LocalOwners)
			if (!ViewIndices.Contains(Key))
				HeightfieldCoveragePlans.Remove(Key);
	HeightfieldUnbalancedEdgeCount = 0;
	for (const FVoxelHeightfieldTransitionEdge& Edge : Edges)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		HeightfieldUnbalancedEdgeCount += Edge.bUnbalanced ? 1 : 0;
	}
	TMap<FVoxelHeightfieldNodeKey, uint64> Revisions;
	for (const FVoxelHeightfieldTileView& View : Views)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		Revisions.Add(View.Key, View.Revision);
	}
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldTransitionEdge>> NewEdges;
	for (const FVoxelHeightfieldTransitionEdge& Edge : Edges)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		NewEdges.FindOrAdd(Edge.Owner).Add(Edge);
	}
	TMap<FVoxelHeightfieldNodeKey, uint64> NewSignatures;
	TArray<FVoxelHeightfieldNodeKey> NewUnsubmitted;
	for (const auto& Pair : NewEdges)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const uint64 Signature = FVoxelHeightfieldTransitionBuilder::BuildSignature(Pair.Key, Revisions.FindChecked(Pair.Key), Pair.Value);
		NewSignatures.Add(Pair.Key, Signature);
		const uint64* Built = HeightfieldTransitionSignatures.Find(Pair.Key);
		const uint64* Pending = PendingTransitionSignatures.Find(Pair.Key);
		const uint64* Prepared = PreparedTransitionSignatures.Find(Pair.Key);
		if ((!Built || *Built != Signature) && (!Pending || *Pending != Signature) && (!Prepared || *Prepared != Signature))
		{
			NewUnsubmitted.Add(Pair.Key);
		}
	}
	if (bActiveCoverageFull)
	{
		for (const auto& Pair : DesiredTransitionSignatures)
			LocalOwners.Add(Pair.Key);
		DesiredTransitionSignatures.Reset();
		DesiredTransitionEdges.Reset();
		UnsubmittedTransitionOwners.Reset();
	}
	for (const auto& Key : LocalOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Desired = NewSignatures.Find(Key);
		if (!Desired || PreparedTransitionSignatures.FindRef(Key) != *Desired)
		{
			PreparedTransitionMeshes.Remove(Key);
			PreparedTransitionSignatures.Remove(Key);
		}
		DesiredTransitionSignatures.Remove(Key);
		DesiredTransitionEdges.Remove(Key);
		UnsubmittedTransitionOwners.RemoveSingleSwap(Key, EAllowShrinking::No);
	}
	for (const auto& Pair : NewSignatures)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		DesiredTransitionSignatures.Add(Pair.Key, Pair.Value);
	}
	for (auto& Pair : NewEdges)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		DesiredTransitionEdges.Add(Pair.Key, MoveTemp(Pair.Value));
	}
	for (const auto& Key : NewUnsubmitted)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		UnsubmittedTransitionOwners.AddUnique(Key);
	}
	co_await VoxelGameThread::SortSteps(UnsubmittedTransitionOwners,
		[this](const FVoxelHeightfieldNodeKey& A, const FVoxelHeightfieldNodeKey& B)
		{
			const bool bASurface = A.Representation == EVoxelHeightfieldRepresentation::Surface;
			const bool bBSurface = B.Representation == EVoxelHeightfieldRepresentation::Surface;
			if (bASurface != bBSurface)
				return !bASurface;
			const double DistanceA = bASurface ? MinimumObserverDistanceCells(SurfaceWorldCenter({A.Coordinate, A.Level}))
											   : MinimumObserverDistanceCells(MacroWorldCenter({A.Coordinate, A.Level}));
			const double DistanceB = bBSurface ? MinimumObserverDistanceCells(SurfaceWorldCenter({B.Coordinate, B.Level}))
											   : MinimumObserverDistanceCells(MacroWorldCenter({B.Coordinate, B.Level}));
			return DistanceA > DistanceB;
		});
	Scheduler.CancelMatching(
		[this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			if (Kind != EVoxelTaskKind::BuildViewTransition)
				return false;
			const FVoxelHeightfieldNodeKey Owner{
				static_cast<EVoxelHeightfieldRepresentation>(Stamp.Section.Z), FIntPoint(Stamp.Section.X, Stamp.Section.Y), Stamp.ViewKey.Level};
			const uint64* WantedSignature = DesiredTransitionSignatures.Find(Owner);
			return !WantedSignature || *WantedSignature != Stamp.Token;
		});
	co_return;
}

FVoxelGameThreadContinuation FVoxelViewManager::RebuildVolumeTransitionSteps(const TSet<FVoxelViewKey>& InTargetNodes)
{
	TMap<FVoxelViewKey, TSet<FVoxelViewKey>> Regions;
	co_await FVoxelVolumeTransitionPlanner::GatherCoverageRegionsSteps(InTargetNodes, Regions);
	if (bActiveCoverageFull)
		PendingVolumeHandoffBoxes.Reset();
	else
		PendingVolumeHandoffBoxes.RemoveAll(
			[this](const FVoxelCoverageBox& Box)
			{
				return CoverageAffects({FIntPoint(Box.Min.X, Box.Min.Y), FIntPoint(Box.Max.X, Box.Max.Y)});
			});
	TSet<FVoxelViewKey> ChangedTopologyNodes;
	TArray<FVoxelVolumeTransitionFace> Faces;
	for (const auto& Pair : Regions)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelViewKey Region = Pair.Key;
		if (!CoverageAffects(VoxelProxyCoverageRect(Region)))
			continue;
		auto& State = VolumeCoveragePlans.FindOrAdd(Region);
		auto RegionRecord = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
		const auto RegionBounds = Region.GetBounds();
		RegionRecord->Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, Region.Coordinate, Region.Level}, 1, Module.GetManifest().Settings.MinZ,
			Module.GetManifest().Settings.MaxZ, 0, 0};
		RegionRecord->Min = RegionBounds.Min;
		RegionRecord->Max = RegionBounds.Max;
		FString RegionIndexError;
		VolumePlanIndex.Upsert(RegionRecord, RegionIndexError);
		uint64 Signature = 0;
		for (const auto& Node : Pair.Value)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			Signature ^= VoxelGeneration::Mix(GetTypeHash(Node));
			if (Node.Level > 0)
				if (const auto* Geometry = GetViewInterest().FindGeometry(Node))
					Signature ^= VoxelGeneration::Mix(GetTypeHash(*Geometry));
		}
		Signature = VoxelGeneration::Mix(Signature ^ Pair.Value.Num());
		State.DesiredSignature = Signature;
		if (State.Plan)
		{
			const auto PlanSnapshot = State.Plan;
			co_await VoxelGameThread::AppendSteps(Faces, PlanSnapshot->Faces);
		}
		if (State.Plan && State.Plan->Signature == Signature && State.Plan->Nodes.Includes(Pair.Value) && State.Plan->Nodes.Num() == Pair.Value.Num())
		{
			continue;
		}
		// 同精度 Fine 使用 section 邻接网格，不存在 LOD 过渡面；规划快照更新不阻止其取得所有权。
		auto NeedsTopologyHandoff = [this, Region](const FVoxelViewKey& Node, const TSet<FVoxelViewKey>& Nodes)
		{
			if (Node.Level > 0)
				return true;
			const FVoxelGenerationBounds Bounds = Node.GetBounds();
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				for (const int32 Sign : {-1, 1})
				{
					FIntVector SameLevel = Node.Coordinate;
					SameLevel[Axis] += Sign;
					if (Nodes.Contains({SameLevel, 0}))
						continue;
					FIntVector Point = Bounds.Min + FIntVector(8);
					Point[Axis] = Sign > 0 ? Bounds.Max[Axis] : Bounds.Min[Axis] - 1;
					for (uint8 Level = 1; Level <= Region.Level; ++Level)
					{
						const int32 Side = 16 << Level;
						if (Nodes.Contains({FIntVector(VoxelGeneration::FloorDivide(Point.X, Side), VoxelGeneration::FloorDivide(Point.Y, Side),
												VoxelGeneration::FloorDivide(Point.Z, Side)),
								Level}))
							return true;
					}
				}
			}
			return false;
		};
		bool bHasProxy = false;
		for (const auto& Node : Pair.Value)
			if (Node.Level > 0)
			{
				bHasProxy = true;
				break;
			}
		bool bHadProxy = false;
		if (State.Plan)
			for (const auto& Node : State.Plan->Nodes)
				if (Node.Level > 0)
				{
					bHadProxy = true;
					break;
				}
		if (bHasProxy || bHadProxy)
			for (const FVoxelViewKey& Node : Pair.Value)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if ((!State.Plan || !State.Plan->Nodes.Contains(Node)) &&
					(NeedsTopologyHandoff(Node, Pair.Value) || (State.Plan && NeedsTopologyHandoff(Node, State.Plan->Nodes))))
					ChangedTopologyNodes.Add(Node);
			}
		if (State.Plan && (bHasProxy || bHadProxy))
		{
			for (const FVoxelViewKey& Node : State.Plan->Nodes)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (!Pair.Value.Contains(Node) && (NeedsTopologyHandoff(Node, Pair.Value) || NeedsTopologyHandoff(Node, State.Plan->Nodes)))
					ChangedTopologyNodes.Add(Node);
			}
		}
		// 保留正在计算的不可变快照；完成后只为最新差异补充任务，避免移动时持续取消。
		if (State.PendingSignature != 0)
			continue;
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildViewCoverage;
		Request.WorkClass = EVoxelWorkClass::Visible;
		Request.TerrainStage = 1;
		Request.SourcePriority = MAX_int32;
		for (const auto& Node : Pair.Value)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (Node.Level == 0)
				Request.TerrainStage = 0;
			if (const auto* Demand = Node.Level == 0 ? GetViewInterest().Exact.Find(Node.Coordinate) : nullptr)
			{
				Request.SourcePriority = FMath::Min(Request.SourcePriority, Demand->Priority);
				if (Demand->bWarmupData || Demand->bWarmupCollision || Demand->bMovementCriticalData || Demand->bMovementCriticalCollision ||
					Demand->bMovementCriticalFine)
					Request.WorkClass = EVoxelWorkClass::Critical;
			}
			else if (const auto* Priority = GetViewInterest().VoxelProxyPriorities.Find(Node))
				Request.SourcePriority = FMath::Min(Request.SourcePriority, *Priority);
		}
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.Section = FIntVector(Region.Coordinate.X, Region.Coordinate.Y, -1);
		Request.Stamp.ViewKey = Region;
		Request.Stamp.Token = Signature;
		Request.ReservedBytes = 4ull * 1024ull * 1024ull;
		Request.InputBytes = Pair.Value.GetAllocatedSize();
		Request.DistanceScore = MinimumObserverDistanceCells(Region.GetBounds());
		TMap<FVoxelViewKey, int32> SampleSteps;
		for (const auto& Node : Pair.Value)
			if (Node.Level == 0)
				SampleSteps.Add(Node, 1);
			else if (const auto* Geometry = GetViewInterest().FindGeometry(Node))
				SampleSteps.Add(Node, Geometry->SampleStepCells);
		Request.Execute = [Region, Nodes = Pair.Value, Signature, SampleSteps = MoveTemp(SampleSteps)](const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			if (Cancel.Load())
				return Result;
			auto Payload = MakeShared<FVoxelVolumeCoveragePlan, ESPMode::ThreadSafe>();
			Payload->Signature = Signature;
			Payload->Nodes = Nodes;
			uint8 MaximumLevel = 0;
			for (const FVoxelViewKey& Key : Nodes)
				MaximumLevel = FMath::Max(MaximumLevel, Key.Level);
			FVoxelVolumeTransitionPlanner::Build(Nodes, MaximumLevel, Payload->Faces, &SampleSteps);
			Payload->Faces.RemoveAll(
				[Region](const auto& Face)
				{
					FVoxelViewKey Owner = Face.Owner;
					while (Owner.Level < Region.Level)
						Owner = Owner.GetParent();
					return !(Owner == Region);
				});
			Result.CustomPayload = Payload;
			Result.bSuccess = !Cancel.Load();
			return Result;
		};
		const auto Bounds = Region.GetBounds();
		FVoxelCoverageLocalSnapshot Guard;
		CoverageIndex.Capture(Bounds.Min - FIntVector(1), Bounds.Max + FIntVector(1), Guard, false);
		const uint64 Serial = VolumePlanSerial;
		Request.Apply = [this, Serial, Region, Signature, Guard = MoveTemp(Guard)](FVoxelTaskResult&& Result)
		{
			if (Serial != VolumePlanSerial || Result.Stamp.WorldEpoch != WorldEpoch)
				return;
			auto* Current = VolumeCoveragePlans.Find(Region);
			if (!Current || Current->PendingSignature != Signature)
				return;
			Current->PendingSignature = 0;
			if (!CoverageIndex.IsCurrent(Guard))
			{
				MarkCoverageDirty(VoxelProxyCoverageRect(Region));
				return;
			}
			if (Result.bSuccess && !Result.bCanceled &&
				(!Current->Plan || Current->Plan->Signature != Current->DesiredSignature || Signature == Current->DesiredSignature))
			{
				Current->Plan = StaticCastSharedPtr<const FVoxelVolumeCoveragePlan>(Result.CustomPayload);
			}
			MarkCoverageDirty(VoxelProxyCoverageRect(Region));
		};
		if (EnqueueViewTask(MoveTemp(Request)))
			State.PendingSignature = Signature;
	}
	TSet<FVoxelViewKey> PreviousRegions;
	if (bActiveCoverageFull)
		for (const auto& Pair : VolumeCoveragePlans)
			PreviousRegions.Add(Pair.Key);
	else
		for (const auto& Rect : ActiveDirtyCoverageRects)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			FVoxelCoverageLocalSnapshot LocalPlans;
			VolumePlanIndex.Capture(FIntVector(Rect.Min.X, Rect.Min.Y, MIN_int32), FIntVector(Rect.Max.X, Rect.Max.Y, MAX_int32), LocalPlans);
			for (const auto& Record : LocalPlans.Records)
				PreviousRegions.Add({Record->Geometry.Key.Coordinate, Record->Geometry.Key.Level});
		}
	for (const auto& Region : PreviousRegions)
		if (!Regions.Contains(Region))
		{
			VolumeCoveragePlans.Remove(Region);
			VolumePlanIndex.Remove({EVoxelTerrainRepresentation::VoxelProxy, Region.Coordinate, Region.Level});
		}
	for (const FVoxelViewKey& Node : ChangedTopologyNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		FVoxelCoverageBox Bounds = Node.Level == 0 ? FineCoverageBox(Node.Coordinate) : VoxelProxyCoverageBox(Node);
		Bounds.Min -= FIntVector(1, 1, 1);
		Bounds.Max += FIntVector(1, 1, 1);
		PendingVolumeHandoffBoxes.Add(Bounds);
	}
	VolumeUnbalancedFaceCount = 0;
	MissingFineBoundaryCount = 0;
	MissingProxyBoundaryCount = 0;
	InvalidVolumeContextCount = 0;
	TMap<FVoxelViewKey, TSharedPtr<FVoxelBoundaryTransitionContext>> Contexts;
	TSet<FVoxelViewKey> BlockedOwners;
	// 拓扑依赖不足与网格尚未发布不同；后者只阻止交接，不阻止关联网格继续准备。
	const TArray<FVoxelCoverageBox> PendingTopologyBoxes = PendingVolumeHandoffBoxes;
	for (const FVoxelVolumeTransitionFace& Face : Faces)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!InTargetNodes.Contains(Face.Owner) || !InTargetNodes.Contains(Face.Neighbor))
			continue;
		if (VoxelCoverage::IntersectsAny3D(
				Face.Neighbor.Level == 0 ? FineCoverageBox(Face.Neighbor.Coordinate) : VoxelProxyCoverageBox(Face.Neighbor), PendingTopologyBoxes))
		{
			BlockedOwners.Add(Face.Owner);
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Face.Owner));
			continue;
		}
		VolumeUnbalancedFaceCount += Face.Ratio > 2 ? 1 : 0;
		FVoxelBoundaryFaceSnapshot Boundary;
		const uint8 NeighborFace = static_cast<uint8>(Face.Direction) ^ 1;
		bool bCaptured = false;
		if (Face.Neighbor.Level == 0)
		{
			if (const FVoxelSection* Section = Module.GetRuntime()->FindSection(Face.Neighbor.Coordinate))
			{
				bCaptured = FVoxelBoundaryFaceSnapshot::CaptureFine(Face.Neighbor, NeighborFace, *Section, Boundary);
			}
		}
		else if (const auto Data = VoxelProxyData.FindRef(Face.Neighbor))
		{
			bCaptured = FVoxelBoundaryFaceSnapshot::CaptureProxy(*Data, NeighborFace, Boundary);
		}
		if (!bCaptured)
		{
			if (Face.Neighbor.Level == 0)
				++MissingFineBoundaryCount;
			else
				++MissingProxyBoundaryCount;
			BlockedOwners.Add(Face.Owner);
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Face.Owner));
			continue;
		}
		TSharedPtr<FVoxelBoundaryTransitionContext>& Context = Contexts.FindOrAdd(Face.Owner);
		if (!Context)
		{
			Context = MakeShared<FVoxelBoundaryTransitionContext>();
			Context->Owner = Face.Owner;
			if (const auto OwnerData = VoxelProxyData.FindRef(Face.Owner))
				Context->SampleStepCells = OwnerData->Geometry.SampleStepCells;
		}
		Context->Patches.Add({Face, MoveTemp(Boundary)});
		if (Face.Neighbor.Level > 0)
		{
			auto& Opposite = Contexts.FindOrAdd(Face.Neighbor);
			if (!Opposite)
			{
				Opposite = MakeShared<FVoxelBoundaryTransitionContext>();
				Opposite->Owner = Face.Neighbor;
			}
			if (const auto OtherData = VoxelProxyData.FindRef(Face.Neighbor))
				Opposite->SampleStepCells = OtherData->Geometry.SampleStepCells;
			auto Suppression = Face;
			Suppression.Owner = Face.Neighbor;
			Suppression.Neighbor = Face.Owner;
			Suppression.Direction = static_cast<EVoxelVolumeFaceDirection>(static_cast<uint8>(Face.Direction) ^ 1);
			const int32 Axis = static_cast<uint8>(Face.Direction) / 2;
			const int32 Plane = (static_cast<uint8>(Face.Direction) & 1) ? Face.Max[Axis] : Face.Min[Axis];
			Suppression.Min[Axis] = Plane;
			Suppression.Max[Axis] = Plane;
			Opposite->SuppressedFaces.Add(Suppression);
			if (const auto OwnerData = VoxelProxyData.FindRef(Face.Owner))
				Opposite->SuppressionRevision = HashCombineFast(
					GetTypeHash(Opposite->SuppressionRevision), HashCombineFast(GetTypeHash(OwnerData->Revision), GetTypeHash(OwnerData->Geometry)));
		}
	}
	TMap<FVoxelViewKey, uint64> NewSignatures;
	TMap<FVoxelViewKey, TSharedPtr<const FVoxelBoundaryTransitionContext>> NewContexts;
	TArray<FVoxelViewKey> NewUnsubmitted;
	for (const FVoxelViewKey& Key : InTargetNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0)
			continue;
		if (!CoverageAffects(VoxelProxyCoverageRect(Key)))
			continue;
		// 全空气节点保留空间所有权和邻接数据，但不会产生接缝几何或网格交接。
		if (BlockedOwners.Contains(Key) || VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Key), PendingTopologyBoxes))
			continue;
		const auto Data = VoxelProxyData.FindRef(Key);
		if (!Data)
		{
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Key));
			continue;
		}
		TSharedPtr<FVoxelBoundaryTransitionContext> Context = Contexts.FindRef(Key);
		if (!Context)
		{
			Context = MakeShared<FVoxelBoundaryTransitionContext>();
			Context->Owner = Key;
			Context->SampleStepCells = Data->Geometry.SampleStepCells;
		}
		Context->SampleStepCells = Data->Geometry.SampleStepCells;
		if (!Context->Validate())
		{
			++InvalidVolumeContextCount;
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Key));
			continue;
		}
		const uint64 Signature = Context->Signature(Data->Revision);
		NewSignatures.Add(Key, Signature);
		NewContexts.Add(Key, Context);
		const uint64* Built = VolumeTransitionSignatures.Find(Key);
		const uint64* Pending = PendingVolumeSignatures.Find(Key);
		const uint64* Prepared = PreparedVolumeSignatures.Find(Key);
		if ((!Built || *Built != Signature) && (!Prepared || *Prepared != Signature))
		{
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Key));
			if (!Pending || *Pending != Signature)
				NewUnsubmitted.Add(Key);
		}
	}
	TSet<FVoxelViewKey> LocalOwners;
	if (bActiveCoverageFull)
	{
		for (const auto& Pair : DesiredVolumeSignatures)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			LocalOwners.Add(Pair.Key);
		}
		DesiredVolumeSignatures.Reset();
		DesiredVolumeContexts.Reset();
		UnsubmittedVolumeOwners.Reset();
	}
	else
	{
		TSet<FIntVector> Fine;
		TSet<FVoxelSurfaceTileKey> Surface;
		TSet<FVoxelMacroTileKey> Macro;
		co_await CaptureCoverageCandidateSteps(Fine, LocalOwners, Surface, Macro);
	}
	co_await VoxelGameThread::AppendSteps(LocalOwners, InTargetNodes);
	for (const auto& Key : LocalOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0)
			continue;
		if (!CoverageAffects(VoxelProxyCoverageRect(Key)))
			continue;
		const auto* Desired = NewSignatures.Find(Key);
		if (!Desired || PreparedVolumeSignatures.FindRef(Key) != *Desired)
		{
			PreparedVolumeMeshes.Remove(Key);
			PreparedVolumeBoundaryMeshes.Remove(Key);
			PreparedVolumeSignatures.Remove(Key);
		}
		DesiredVolumeSignatures.Remove(Key);
		DesiredVolumeContexts.Remove(Key);
		UnsubmittedVolumeOwners.RemoveSingleSwap(Key, EAllowShrinking::No);
	}
	for (const auto& Pair : NewSignatures)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		DesiredVolumeSignatures.Add(Pair.Key, Pair.Value);
	}
	for (auto& Pair : NewContexts)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		DesiredVolumeContexts.Add(Pair.Key, MoveTemp(Pair.Value));
	}
	for (const auto& Key : NewUnsubmitted)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		UnsubmittedVolumeOwners.AddUnique(Key);
	}
	co_await VoxelGameThread::SortSteps(UnsubmittedVolumeOwners,
		[this](const FVoxelViewKey& A, const FVoxelViewKey& B)
		{
			const EVoxelWorkClass ClassA = VolumeTransitionWorkClass(A);
			const EVoxelWorkClass ClassB = VolumeTransitionWorkClass(B);
			if (ClassA != ClassB)
				return static_cast<uint8>(ClassA) > static_cast<uint8>(ClassB);
			return MinimumObserverDistanceCells(A.GetBounds()) > MinimumObserverDistanceCells(B.GetBounds());
		});

	Scheduler.CancelMatching(
		[this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			if (Kind != EVoxelTaskKind::BuildVolumeTransition)
				return false;
			const uint64* Desired = DesiredVolumeSignatures.Find(Stamp.ViewKey);
			return !Desired || *Desired != Stamp.Token;
		});
	co_return;
}

FVoxelGameThreadContinuation FVoxelViewManager::ResolveTransitionVisibilitySteps()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewHandoff);
	const double ResolveStart = FPlatformTime::Seconds();
	const uint64 ContentRevision = CoverageContentRevision;
	PrepareFinePages();
	const FVoxelTerrainViewPlan& Plan = GetViewInterest().TerrainPlan;
	const bool bUsesTerrainPlan = !Plan.Roots.IsEmpty();
	FCoveragePassSnapshot CoverageSnapshot;
	TSet<FVoxelViewKey> TerrainActorCandidates;
	TSet<FVoxelFinePageKey> FinePageCandidates;
	TSet<FVoxelViewKey> TargetTerrainNodes;
	TSet<FVoxelViewKey> PreviousTerrainNodes;
	TSet<FIntVector> LocalFineKeys;
	TSet<FVoxelViewKey> LocalProxyKeys;
	TSet<FVoxelSurfaceTileKey> LocalSurfaceKeys;
	TSet<FVoxelMacroTileKey> LocalMacroKeys;
	TSet<FVoxelViewKey> ResolveRoots;
	co_await CaptureCoverageCandidateSteps(LocalFineKeys, LocalProxyKeys, LocalSurfaceKeys, LocalMacroKeys);
	for (const auto& Key : LocalFineKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (VisibleTerrainNodes.Contains({Key, 0}))
			PreviousTerrainNodes.Add({Key, 0});
	}
	for (const auto& Key : LocalProxyKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (VisibleTerrainNodes.Contains(Key))
			PreviousTerrainNodes.Add(Key);
	}
	if (!bActiveCoverageFull)
	{
		for (const auto& Rect : ActiveDirtyCoverageRects)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			FVoxelCoverageLocalSnapshot Roots;
			co_await TerrainRootIndex.CaptureSteps(FIntVector(Rect.Min.X, Rect.Min.Y, MIN_int32 + 1), FIntVector(Rect.Max.X, Rect.Max.Y, MAX_int32), Roots);
			for (const auto& Record : Roots.Records)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				ResolveRoots.Add({Record->Geometry.Key.Coordinate, Record->Geometry.Key.Level});
			}
		}
	}
	if (bUsesTerrainPlan)
	{
		const TSet<FVoxelViewKey>& Previous = PreviousTerrainNodes;
		const uint8 RootLevel = MaximumTerrainRootLevel;
		TSet<FVoxelViewKey> PreviousScope;
		if (!bActiveCoverageFull)
		{
			for (const FVoxelViewKey& Key : Previous)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				FVoxelViewKey Root = Key;
				while (Root.Level < RootLevel && !Plan.Roots.Contains(Root))
					Root = Root.GetParent();
				if (ResolveRoots.Contains(Root) ||
					(!Plan.Roots.Contains(Root) && CoverageAffects(Key.Level == 0 ? FineCoverageRect(Key.Coordinate) : VoxelProxyCoverageRect(Key))))
					PreviousScope.Add(Key);
			}
		}
		TSet<FVoxelViewKey> LocalReadyBranches;
		const auto TrackBranch = [&LocalReadyBranches](FVoxelViewKey Key)
		{
			while (Key.Level <= 8)
			{
				bool bExists = false;
				LocalReadyBranches.Add(Key, &bExists);
				if (bExists)
					break;
				Key = Key.GetParent();
			}
		};
		for (const auto& Key : LocalFineKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (FineReady.Contains(Key))
				TrackBranch({Key, 0});
		}
		for (const auto& Key : LocalProxyKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (VoxelProxyReady.Contains(Key))
				TrackBranch(Key);
		}
		auto IsReady = [this, &Plan, &Previous](const FVoxelViewKey& Key)
		{
			if (!Plan.Leaves.Contains(Key) && !Previous.Contains(Key))
				return false;
			if (Key.Level > 0)
				return VoxelProxyReady.Contains(Key);
			const FVoxelFinePageKey* Page = FinePagePlan.SectionOwners.Find(Key.Coordinate);
			return FineReady.Contains(Key.Coordinate) && Page && FinePageReady.Contains(*Page);
		};
		if (bActiveCoverageFull)
		{
			co_await Plan.ResolveVisibleSteps(IsReady, TargetTerrainNodes, &Previous, &LocalReadyBranches, nullptr);
			co_await VoxelGameThread::AppendSteps(TerrainActorCandidates, Previous);
			co_await VoxelGameThread::AppendSteps(TerrainActorCandidates, TargetTerrainNodes);
		}
		else
		{
			co_await Plan.ResolveVisibleSteps(IsReady, TargetTerrainNodes, &PreviousScope, &LocalReadyBranches, &ResolveRoots);
			co_await VoxelGameThread::AppendSteps(TerrainActorCandidates, PreviousScope);
			co_await VoxelGameThread::AppendSteps(TerrainActorCandidates, TargetTerrainNodes);
			for (const FVoxelViewKey& Key : Previous)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (!PreviousScope.Contains(Key))
					TargetTerrainNodes.Add(Key);
			}
		}
		co_await BuildCoverageSnapshotSteps(CoverageSnapshot);
		const double TreeMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0;
		TArray<FVoxelCoverageRect> OutsideTerrainDomain;
		TArray<FVoxelCoverageRect> ReadyOutsideCoverage;
		TArray<FVoxelCoverageBox> ReadyFineCoverage;
		TArray<FVoxelCoverageBox> ReadyProxyCoverage;
		bool bOutsideTerrainDomainBuilt = false;
		for (const FVoxelViewKey& Key : Previous)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (!bActiveCoverageFull && !PreviousScope.Contains(Key))
				continue;
			FVoxelViewKey Root = Key;
			while (Root.Level < RootLevel && !Plan.Roots.Contains(Root))
			{
				co_await FVoxelGameThreadContinuation::Yield();
				Root = Root.GetParent();
			}
			if (Plan.Roots.Contains(Root))
			{
				continue;
			}
			if (!bOutsideTerrainDomainBuilt)
			{
				OutsideTerrainDomain.Reserve(SurfaceWanted.Num() + MacroWanted.Num());
				for (const FVoxelSurfaceTileKey& SurfaceKey : LocalSurfaceKeys)
				{
					co_await FVoxelGameThreadContinuation::Yield();
					OutsideTerrainDomain.Add(SurfaceCoverageRect(SurfaceKey));
				}
				for (const FVoxelMacroTileKey& MacroKey : LocalMacroKeys)
				{
					co_await FVoxelGameThreadContinuation::Yield();
					OutsideTerrainDomain.Add(MacroCoverageRect(MacroKey));
				}
				co_await VoxelGameThread::AppendSteps(ReadyOutsideCoverage, CoverageSnapshot.Heightfield);
				bOutsideTerrainDomainBuilt = true;
			}
			const FVoxelCoverageRect PreviousRect = Key.Level == 0 ? FineCoverageRect(Key.Coordinate) : VoxelProxyCoverageRect(Key);
			if (!VoxelCoverage::IntersectsAny2D(PreviousRect, OutsideTerrainDomain))
			{
				continue;
			}
			bool bReplacement = false;
			co_await IsTerrainVolumeCoveredSteps(Key, CoverageSnapshot, Key.Level > 0, bReplacement);
			if (!bReplacement)
				co_await VoxelCoverage::IsFullyCovered2DSteps(PreviousRect, ReadyOutsideCoverage, bReplacement);
			const bool bRetain = !bReplacement;
			if (bRetain)
			{
				TargetTerrainNodes.Add(Key);
			}
		}
		const double PlanNow = FPlatformTime::Seconds();
		if (CVarVoxelDebugRepresentation.GetValueOnGameThread() > 0 && PlanNow >= NextPlanTimingLog)
		{
			UE_LOG(LogTemp, Display, TEXT("Voxel plan phase elapsed ms: tree=%.3f retain=%.3f previous=%d visible=%d"), TreeMilliseconds,
				(PlanNow - ResolveStart) * 1000.0 - TreeMilliseconds, Previous.Num(), TargetTerrainNodes.Num());
			NextPlanTimingLog = PlanNow + 5.0;
		}
	}
	if (!bUsesTerrainPlan)
		co_await BuildCoverageSnapshotSteps(CoverageSnapshot);
	if (!bUsesTerrainPlan)
	{
		co_await VoxelGameThread::AppendSteps(TargetTerrainNodes, PreviousTerrainNodes);
		for (const FVoxelViewKey& Node : PreviousTerrainNodes)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const bool bWanted = Node.Level == 0 ? FineWanted.Contains(Node.Coordinate) : VoxelProxyWanted.Contains(Node);
			if (!bWanted)
			{
				bool bReplacement = Node.Level == 0 && !VoxelCoverage::IntersectsAny2D(FineCoverageRect(Node.Coordinate), CoverageSnapshot.WantedHeightfield);
				if (!bReplacement)
				{
					co_await IsTerrainVolumeCoveredSteps(Node, CoverageSnapshot, Node.Level > 0, bReplacement);
				}
				if (!bReplacement)
				{
					co_await VoxelCoverage::IsFullyCovered2DSteps(TerrainCoverageRect(Node), CoverageSnapshot.Heightfield, bReplacement);
				}
				if (bReplacement)
				{
					TargetTerrainNodes.Remove(Node);
				}
			}
			if (CoverageAffects(TerrainCoverageRect(Node)))
				TerrainActorCandidates.Add(Node);
		}
		for (const FIntVector& Section : LocalFineKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const FVoxelFinePageKey* Page = FinePagePlan.SectionOwners.Find(Section);
			if (FineWanted.Contains(Section) && Page && FinePageReady.Contains(*Page))
				TargetTerrainNodes.Add({Section, 0});
		}
		for (const auto& Node : LocalProxyKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (VoxelProxyWanted.Contains(Node) && VoxelProxyReady.Contains(Node))
				TargetTerrainNodes.Add(Node);
		}
		for (const auto& Node : TargetTerrainNodes)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (CoverageAffects(TerrainCoverageRect(Node)))
				TerrainActorCandidates.Add(Node);
		}
	}
#if !UE_BUILD_SHIPPING
	TArray<FVoxelViewKey, TInlineAllocator<2>> PointHandoffKeys;
	TSet<FVoxelViewKey> PointTreeTargets;
	if (FPlatformTime::Seconds() >= NextPointHandoffLog)
	{
		for (const auto& Pair : GetViewInterest().Sources)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto& Source = Pair.Value.Source;
			if (!Source.bLocalView || !Source.Has(EVoxelStreamingCapability::FineVisual) || Source.Has(EVoxelStreamingCapability::LocalRefinement))
				continue;
			const auto AddPoint = [&](const FIntVector& Cell)
			{
				PointHandoffKeys.AddUnique(
					{FIntVector(VoxelGeneration::FloorDivide(Cell.X, 16), VoxelGeneration::FloorDivide(Cell.Y, 16), VoxelGeneration::FloorDivide(Cell.Z, 16)),
						0});
			};
			AddPoint(Source.Center);
			FVoxelNaturalColumnEntryPtr Column;
			if (Module.GetGenerationCache() && Module.GetGenerationCache()->FindNaturalColumn(FIntPoint(Source.Center.X, Source.Center.Y), Column))
				AddPoint(FIntVector(Source.Center.X, Source.Center.Y, Column->Column.SurfaceZ));
			break;
		}
		for (const auto& Key : PointHandoffKeys)
			if (TargetTerrainNodes.Contains(Key))
				PointTreeTargets.Add(Key);
		NextPointHandoffLog = FPlatformTime::Seconds() + 5.0;
	}
#endif
	TSet<FVoxelHeightfieldNodeKey> LocalHeightfieldOwners;
	for (const auto& Key : LocalSurfaceKeys)
		LocalHeightfieldOwners.Add({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level});
	for (const auto& Key : LocalMacroKeys)
		LocalHeightfieldOwners.Add({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level});
	co_await RebuildVolumeTransitionSteps(TargetTerrainNodes);
	TMap<FVoxelViewKey, uint64> VolumeToCommit;
	if (!bActiveCoverageFull)
	{
		auto TerrainRect = [this](const FVoxelViewKey& Key)
		{
			return Key.Level == 0 ? FineCoverageRect(Key.Coordinate) : VoxelProxyCoverageRect(Key);
		};
		for (const FVoxelViewKey& Key : TargetTerrainNodes)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (!VisibleTerrainNodes.Contains(Key) && !CoverageAffects(TerrainRect(Key)))
			{
				ExpandActiveCoverage(TerrainRect(Key));
				TerrainActorCandidates.Add(Key);
			}
		}
		if (!bActiveCoverageFull)
		{
			for (const FVoxelViewKey& Key : PreviousTerrainNodes)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (!TargetTerrainNodes.Contains(Key) && !CoverageAffects(TerrainRect(Key)))
				{
					ExpandActiveCoverage(TerrainRect(Key));
					TerrainActorCandidates.Add(Key);
				}
			}
		}
	}
	const double PlanMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0;
	const bool bNeedsExclusions = !bUsesTerrainPlan || !SurfaceActors.IsEmpty() || !MacroActors.IsEmpty();
	TArray<FBox> FineBoxes;
	TArray<FBox> ProxyBoxes;
	TArray<FBox> ProxySurfaceBoxes;
	auto Box = [](const FVoxelCoverageBox& Bounds)
	{
		return FBox(FVector(Bounds.Min), FVector(Bounds.Max));
	};
	auto Column = [](const FVoxelCoverageRect& Bounds)
	{
		return FBox(FVector(Bounds.Min.X, Bounds.Min.Y, -1.e12), FVector(Bounds.Max.X, Bounds.Max.Y, 1.e12));
	};
	TArray<FBox> ScopeBoxes;
	for (const auto& Rect : ActiveDirtyCoverageRects)
		ScopeBoxes.Add(Column(Rect));
	const auto SetCoverage = [this, &ScopeBoxes](const FGuid& Group, AActor* Actor, TArray<FBox> Boxes,
								 TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> Common = nullptr) -> FVoxelGameThreadContinuation
	{
		bool bSuccess = false;
		if (bActiveCoverageFull)
		{
			co_await Publisher->SetCoverageSteps(Group, Actor, MoveTemp(Boxes), Common, bSuccess);
		}
		else
		{
			co_await Publisher->SetScopedCoverageSteps(Group, Actor, MoveTemp(Boxes), Common, ScopeBoxes, bSuccess);
		}
		if (!bSuccess)
		{
			MarkCoverageDirty();
		}
	};
	const auto SetSharedCoverage = [this, &ScopeBoxes](const FGuid& Group, AActor* Actor,
									   TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> Common) -> FVoxelGameThreadContinuation
	{
		bool bSuccess = false;
		if (bActiveCoverageFull)
		{
			co_await Publisher->SetSharedCoverageSteps(Group, Actor, Common, bSuccess);
		}
		else
		{
			co_await Publisher->SetScopedCoverageSteps(Group, Actor, {}, Common, ScopeBoxes, bSuccess, true);
		}
		if (!bSuccess)
		{
			MarkCoverageDirty();
		}
	};
	if (bHeightfieldTopologyDirty)
	{
		co_await RebuildHeightfieldTransitionSteps(CoverageSnapshot);
		bHeightfieldTopologyDirty = false;
		++HeightfieldTopologyRebuildCount;
	}

	TMap<FVoxelHeightfieldNodeKey, uint64> HeightfieldToCommit;
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Prepared = PreparedTransitionSignatures.Find(LocalOwner);
		if (!Prepared)
			continue;
		const TPair<FVoxelHeightfieldNodeKey, uint64> Pair(LocalOwner, *Prepared);
		const uint64* Desired = DesiredTransitionSignatures.Find(Pair.Key);
		const auto Mesh = PreparedTransitionMeshes.FindRef(Pair.Key);
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
											? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
											: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (!Desired || *Desired != Pair.Value || !Mesh || PendingHeightfieldPlanOwners.Contains(Pair.Key) || VolumeHandoffAffectsHeightfield(Pair.Key))
			continue;
		const int32 TileSide = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
								   ? VoxelViewLod::SurfaceSide(Module.GetViewSettings().Lod, Pair.Key.Level)
								   : VoxelViewLod::MacroSide(Module.GetViewSettings().Lod, Pair.Key.Level);
		AActor* Host = HeightfieldTransitionActors.FindRef(Pair.Key);
		if (!Publisher->StageImmutable(BeginPublishGroup({static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5),
										   Pair.Key.Coordinate, Pair.Key.Level}),
				Host, FVector(Pair.Key.Coordinate.X * TileSide, Pair.Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(), Module.BlockSize(), Mesh,
				Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 2 : 3, true))
		{
			MarkCoverageDirty();
			continue;
		}
		HeightfieldTransitionActors.Add(Pair.Key, Host);
		co_await SetCoverage(BeginPublishGroup({static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5),
								 Pair.Key.Coordinate, Pair.Key.Level}),
			Host, {});
		HeightfieldToCommit.Add(Pair.Key, Pair.Value);
	}
	TSet<FVoxelHeightfieldNodeKey> BlockedHeightfieldNodes;
	for (const auto& Owner : LocalHeightfieldOwners)
		if (PendingHeightfieldPlanOwners.Contains(Owner))
			BlockedHeightfieldNodes.Add(Owner);
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelHeightfieldNodeKey, int32> Pair(LocalOwner, 0);
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
											? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
											: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (VolumeHandoffAffectsHeightfield(Pair.Key))
			BlockedHeightfieldNodes.Add(Pair.Key);
	}
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Edges = DesiredTransitionEdges.Find(LocalOwner);
		if (!Edges)
			continue;
		const TPair<FVoxelHeightfieldNodeKey, const TArray<FVoxelHeightfieldTransitionEdge>&> Pair(LocalOwner, *Edges);
		const uint64* Desired = DesiredTransitionSignatures.Find(Pair.Key);
		const uint64* Built = HeightfieldTransitionSignatures.Find(Pair.Key);
		AActor* Host = HeightfieldTransitionActors.FindRef(Pair.Key);
		const uint64* Staged = HeightfieldToCommit.Find(Pair.Key);
		const bool bReady = Desired && ((Built && *Built == *Desired && (!Host || Publisher->HasPresentation(Host))) || (Staged && *Staged == *Desired));
		if (bReady)
			continue;
		BlockedHeightfieldNodes.Add(Pair.Key);
		for (const FVoxelHeightfieldTransitionEdge& Edge : Pair.Value)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (!Edge.bPending)
				BlockedHeightfieldNodes.Add(Edge.Neighbor);
		}
	}
	if (bActiveCoverageFull)
		PendingHeightfieldHandoffRects.Reset();
	else
		PendingHeightfieldHandoffRects.RemoveAll(
			[this](const FVoxelCoverageRect& Rect)
			{
				return CoverageAffects(Rect);
			});
	for (const FVoxelHeightfieldNodeKey& Key : BlockedHeightfieldNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Representation == EVoxelHeightfieldRepresentation::Surface)
		{
			const FVoxelSurfaceTileKey Tile{Key.Coordinate, Key.Level};
			if (SurfaceWanted.Contains(Tile))
				PendingHeightfieldHandoffRects.Add(SurfaceCoverageRect(Tile));
		}
		else
		{
			const FVoxelMacroTileKey Tile{Key.Coordinate, Key.Level};
			if (MacroWanted.Contains(Tile))
				PendingHeightfieldHandoffRects.Add(MacroCoverageRect(Tile));
		}
	}
	for (const auto& LocalOwner : LocalProxyKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Prepared = PreparedVolumeSignatures.Find(LocalOwner);
		if (!Prepared)
			continue;
		const TPair<FVoxelViewKey, uint64> Pair(LocalOwner, *Prepared);
		const uint64* Desired = DesiredVolumeSignatures.Find(Pair.Key);
		const auto Mesh = PreparedVolumeMeshes.FindRef(Pair.Key);
		if (!Desired || *Desired != Pair.Value || !Mesh || !TargetTerrainNodes.Contains(Pair.Key) ||
			VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Pair.Key), PendingVolumeHandoffBoxes))
			continue;
		// 空接缝结果也必须提交签名，释放准备结果并停止重复覆盖计算。
		const FGuid Group = BeginPublishGroup({1, Pair.Key.Coordinate, Pair.Key.Level});
		const int32 TerrainStage = VolumeTransitionWorkClass(Pair.Key) <= EVoxelWorkClass::Interactive ? 0 : 1;
		Publisher->SetTerrainStage(Group, TerrainStage);
		AActor* Host = VoxelProxyActors.FindRef(Pair.Key);
		if (HasRenderableMesh(*Mesh) || Host)
		{
			if (!Publisher->StageImmutable(Group, Host, FVector(Pair.Key.GetBounds().Min) * Module.BlockSize(),
					Module.BlockSize() * VoxelProxyData.FindChecked(Pair.Key)->Geometry.SampleStepCells, Mesh, TerrainStage))
			{
				PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Pair.Key));
				MarkCoverageDirty(VoxelProxyCoverageRect(Pair.Key));
				continue;
			}
			VoxelProxyActors.Add(Pair.Key, Host);
		}
		if (const auto BoundaryMesh = PreparedVolumeBoundaryMeshes.FindRef(Pair.Key))
		{
			AActor* BoundaryHost = VolumeBoundaryActors.FindRef(Pair.Key);
			if (HasRenderableMesh(*BoundaryMesh) || BoundaryHost)
			{
				if (!Publisher->StageImmutable(Group, BoundaryHost, FVector(Pair.Key.GetBounds().Min) * Module.BlockSize(),
						Module.BlockSize() * VoxelProxyData.FindChecked(Pair.Key)->Geometry.SampleStepCells, BoundaryMesh, TerrainStage, true))
					continue;
				VolumeBoundaryActors.Add(Pair.Key, BoundaryHost);
				co_await SetCoverage(Group, BoundaryHost, {});
				Publisher->SetHidden(Group, BoundaryHost, false);
			}
		}
		VolumeToCommit.Add(Pair.Key, Pair.Value);
	}
	for (auto It = TargetTerrainNodes.CreateIterator(); It; ++It)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelCoverageBox Bounds = It->Level == 0 ? FineCoverageBox(It->Coordinate) : VoxelProxyCoverageBox(*It);
		if (VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes))
			It.RemoveCurrent();
	}
	for (const auto& Key : PreviousTerrainNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelCoverageBox Bounds = Key.Level == 0 ? FineCoverageBox(Key.Coordinate) : VoxelProxyCoverageBox(Key);
		if (VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes))
			TargetTerrainNodes.Add(Key);
	}
#if !UE_BUILD_SHIPPING
	for (const auto& Key : PointHandoffKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		AActor* Actor = FineActor(Key.Coordinate);
		UE_LOG(LogTemp, Display,
			TEXT(
				"Voxel point handoff: section=%s ready=%d desired=%d tree=%d volumeBlocked=%d target=%d owned=%d actor=%d hidden=%d committed=%d groupBusy=%d"),
			*Key.Coordinate.ToString(), FineReady.Contains(Key.Coordinate), Plan.Leaves.Contains(Key), PointTreeTargets.Contains(Key),
			VoxelCoverage::IntersectsAny3D(FineCoverageBox(Key.Coordinate), PendingVolumeHandoffBoxes), TargetTerrainNodes.Contains(Key),
			VisibleTerrainNodes.Contains(Key), Actor != nullptr, Actor && Actor->IsHidden(), Actor && Publisher->IsCommitted(Actor),
			Publisher->IsGroupBusy({0, Key.Coordinate, 0}));
	}
#endif
	auto HandoffPending = [this](const FVoxelCoverageRect& Rect)
	{
		return VoxelCoverage::IntersectsAny2D(Rect, PendingHeightfieldHandoffRects);
	};
	TArray<FVoxelCoverageRect> RecheckCoverageRects;
	for (const auto& Pair : VolumeToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		RecheckCoverageRects.Add(VoxelProxyCoverageRect(Pair.Key));
	}
	for (const auto& Pair : HeightfieldToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		RecheckCoverageRects.Add(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
									 ? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
									 : MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level}));
	}
	// 裁剪与实际保留的体素分区一致，不能先裁掉仍在等待体素接缝的上一代覆盖。
	if (bUsesTerrainPlan && bNeedsExclusions)
	{
		for (const FVoxelViewKey& Key : TargetTerrainNodes)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (Key.Level == 0)
			{
				if (FineActor(Key.Coordinate))
					FineBoxes.Add(Box(FineCoverageBox(Key.Coordinate)));
			}
			else
			{
				if (!VoxelProxyActors.FindRef(Key))
					continue;
				ProxyBoxes.Add(Box(VoxelProxyCoverageBox(Key)));
				if (const TArray<FVoxelCoverageRect>* Rects = ProxySurfaceCoverage.Find(Key))
				{
					const FVoxelCoverageBox Bounds = VoxelProxyCoverageBox(Key);
					for (const FVoxelCoverageRect& Rect : *Rects)
						ProxySurfaceBoxes.Add(FBox(FVector(Rect.Min.X, Rect.Min.Y, Bounds.Min.Z), FVector(Rect.Max.X, Rect.Max.Y, Bounds.Max.Z)));
				}
			}
		}
	}
	else if (!bUsesTerrainPlan)
	{
		for (const FIntVector& Key : FineReady)
			if (FineWanted.Contains(Key))
				FineBoxes.Add(Box(FineCoverageBox(Key)));
	}
	co_await VoxelMeshClipper::NormalizeBoxesSteps(FineBoxes);
	co_await VoxelMeshClipper::NormalizeBoxesSteps(ProxyBoxes);
	co_await VoxelMeshClipper::NormalizeBoxesSteps(ProxySurfaceBoxes);
	const auto CommonHeightfieldBoxes = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>(FineBoxes);
	CommonHeightfieldBoxes->Append(ProxySurfaceBoxes);
	const auto CommonTransitionBoxes = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>();
	for (FBox Bounds : *CommonHeightfieldBoxes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		Bounds.Min.Z = -1.e12;
		Bounds.Max.Z = 1.e12;
		CommonTransitionBoxes->Add(Bounds);
	}
	co_await VoxelMeshClipper::NormalizeBoxesSteps(*CommonTransitionBoxes);
	const double BoxesMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0 - PlanMilliseconds;
	TArray<FVoxelCoverageRect> PresentedHeightfieldRects;
	for (const auto& Key : LocalSurfaceKeys)
		if (AActor* Actor = SurfaceActors.FindRef(Key); Actor && !Actor->IsHidden() && Publisher->IsPresented(Actor))
			PresentedHeightfieldRects.Add(SurfaceCoverageRect(Key));
	for (const auto& Key : LocalMacroKeys)
		if (AActor* Actor = MacroActors.FindRef(Key); Actor && !Actor->IsHidden() && Publisher->IsPresented(Actor))
			PresentedHeightfieldRects.Add(MacroCoverageRect(Key));

	co_await GatherFinePageCandidateSteps(TerrainActorCandidates, FinePageCandidates);
	TerrainCandidateCount = TerrainActorCandidates.Num();
	FinePageCandidateCount = FinePageCandidates.Num();
	TMap<FVoxelFinePageKey, bool> FineVisibility;
	for (const FVoxelFinePageKey& Page : FinePageCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		TOptional<bool> Visibility;
		co_await UpdateFinePagePresentationSteps(Page, TargetTerrainNodes, CoverageSnapshot, Visibility);
		if (Visibility.IsSet())
			FineVisibility.Add(Page, Visibility.GetValue());
	}
	for (const FVoxelViewKey& Key : TerrainActorCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0)
			continue;
		AActor* Actor = VoxelProxyActors.FindRef(Key);
		if (!Actor || !CoverageAffects(VoxelProxyCoverageRect(Key)))
			continue;
		const bool bVisible = TargetTerrainNodes.Contains(Key);
		const FVoxelPublishGroupKey GroupKey{1, Key.Coordinate, Key.Level};
		if (!bVisible && Actor->IsHidden() && Publisher->IsCommitted(Actor) && !Publisher->IsGroupBusy(GroupKey))
			continue;
		const FGuid Group = BeginPublishGroup(GroupKey);
		if (VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Key), PendingVolumeHandoffBoxes))
		{
			if (!Publisher->HasPresentation(Actor))
				Publisher->SetHidden(Group, Actor, true);
			continue;
		}
		if (bVisible)
		{
			TArray<FBox> Exclusions = FineBoxes;
			for (const auto& Other : TargetTerrainNodes)
				if (Other.Level > 0 && Other.Level < Key.Level)
					Exclusions.Add(Box(VoxelProxyCoverageBox(Other)));
			co_await SetCoverage(Group, Actor, MoveTemp(Exclusions));
		}
		Publisher->SetHidden(Group, Actor, !bVisible);
		if (AActor* Boundary = VolumeBoundaryActors.FindRef(Key))
			Publisher->SetHidden(Group, Boundary, !bVisible);
	}
	for (const FVoxelSurfaceTileKey& LocalKey : LocalSurfaceKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>> Pair(LocalKey, SurfaceActors.FindRef(LocalKey));
		if (!Pair.Value)
			continue;
		if (!CoverageAffects(SurfaceCoverageRect(Pair.Key)))
			continue;
		const FGuid Group = BeginPublishGroup({2, Pair.Key.Coordinate, Pair.Key.Level});
		const bool bHasPreviousCoverage =
			Publisher->HasPresentation(Pair.Value) || VoxelCoverage::IntersectsAny2D(SurfaceCoverageRect(Pair.Key), PresentedHeightfieldRects);
		if (bHasPreviousCoverage && (BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface, Pair.Key.Coordinate, Pair.Key.Level}) ||
										(!SurfaceWanted.Contains(Pair.Key) && HandoffPending(SurfaceCoverageRect(Pair.Key)))))
		{
			if (!Publisher->HasPresentation(Pair.Value))
				Publisher->SetHidden(Group, Pair.Value, true);
			else
				co_await SetSharedCoverage(Group, Pair.Value, CommonHeightfieldBoxes);
			if (AActor* Water = WaterActors.FindRef(Pair.Key))
				co_await SetSharedCoverage(Group, Water, CommonHeightfieldBoxes);
			continue;
		}
		const bool bWanted = SurfaceWanted.Contains(Pair.Key);
		bool bHidden = false;
		if (!bWanted)
			co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Pair.Key), CoverageSnapshot.Macro, bHidden);
		if (!bWanted && !bHidden)
			co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Pair.Key), CoverageSnapshot.ProxySurface, bHidden);
		const FVoxelCoverageRect ActorRect = SurfaceCoverageRect(Pair.Key);
		if (!CoverageAffects(ActorRect))
		{
			Publisher->SetHidden(Group, Pair.Value, bHidden);
			if (AActor* Water = WaterActors.FindRef(Pair.Key))
				Publisher->SetHidden(Group, Water, bHidden);
			continue;
		}
		TArray<FBox> Exclusions;
		for (const FVoxelSurfaceTileKey& Key : LocalSurfaceKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (SurfaceReady.Contains(Key) && SurfaceWanted.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level}) && Key != Pair.Key &&
				(!bWanted || Key.Level < Pair.Key.Level))
				if (ActorRect.Intersects(SurfaceCoverageRect(Key)))
					Exclusions.Add(Column(SurfaceCoverageRect(Key)));
		}
		if (!bWanted)
		{
			for (const FVoxelMacroTileKey& Key : LocalMacroKeys)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (MacroReady.Contains(Key) && MacroWanted.Contains(Key) &&
					!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level}))
					if (ActorRect.Intersects(MacroCoverageRect(Key)))
						Exclusions.Add(Column(MacroCoverageRect(Key)));
			}
		}
		if (!bHidden)
		{
			if (!Publisher->IsCommitted(Pair.Value))
				RecheckCoverageRects.Add(ActorRect);
			co_await SetCoverage(Group, Pair.Value, Exclusions, CommonHeightfieldBoxes);
			if (AActor* Water = WaterActors.FindRef(Pair.Key))
				co_await SetCoverage(Group, Water, MoveTemp(Exclusions), CommonHeightfieldBoxes);
		}
		Publisher->SetHidden(Group, Pair.Value, bHidden);
		if (AActor* Water = WaterActors.FindRef(Pair.Key))
			Publisher->SetHidden(Group, Water, bHidden);
	}
	for (const FVoxelMacroTileKey& LocalKey : LocalMacroKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelMacroTileKey, TObjectPtr<AActor>> Pair(LocalKey, MacroActors.FindRef(LocalKey));
		if (!Pair.Value)
			continue;
		if (!CoverageAffects(MacroCoverageRect(Pair.Key)))
			continue;
		const FGuid Group = BeginPublishGroup({3, Pair.Key.Coordinate, Pair.Key.Level});
		const bool bHasPreviousCoverage =
			Publisher->HasPresentation(Pair.Value) || VoxelCoverage::IntersectsAny2D(MacroCoverageRect(Pair.Key), PresentedHeightfieldRects);
		if (bHasPreviousCoverage && (BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro, Pair.Key.Coordinate, Pair.Key.Level}) ||
										(!MacroWanted.Contains(Pair.Key) && HandoffPending(MacroCoverageRect(Pair.Key)))))
		{
			if (!Publisher->HasPresentation(Pair.Value))
				Publisher->SetHidden(Group, Pair.Value, true);
			else
				co_await SetSharedCoverage(Group, Pair.Value, CommonHeightfieldBoxes);
			continue;
		}
		const bool bWanted = MacroWanted.Contains(Pair.Key);
		bool bHidden = false;
		if (!bWanted)
			co_await VoxelCoverage::IsFullyCovered2DSteps(MacroCoverageRect(Pair.Key), CoverageSnapshot.Surface, bHidden);
		const FVoxelCoverageRect ActorRect = MacroCoverageRect(Pair.Key);
		if (!CoverageAffects(ActorRect))
		{
			Publisher->SetHidden(Group, Pair.Value, bHidden);
			continue;
		}
		TArray<FBox> Exclusions;
		for (const FVoxelSurfaceTileKey& Key : LocalSurfaceKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (SurfaceReady.Contains(Key) && SurfaceWanted.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level}))
				if (ActorRect.Intersects(SurfaceCoverageRect(Key)))
					Exclusions.Add(Column(SurfaceCoverageRect(Key)));
		}
		for (const FVoxelMacroTileKey& Key : LocalMacroKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (MacroReady.Contains(Key) && MacroWanted.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level}) && Key != Pair.Key &&
				(!bWanted || Key.Level < Pair.Key.Level))
				if (ActorRect.Intersects(MacroCoverageRect(Key)))
					Exclusions.Add(Column(MacroCoverageRect(Key)));
		}
		if (!bHidden)
		{
			if (!Publisher->IsCommitted(Pair.Value))
				RecheckCoverageRects.Add(ActorRect);
			co_await SetCoverage(Group, Pair.Value, MoveTemp(Exclusions), CommonHeightfieldBoxes);
		}
		Publisher->SetHidden(Group, Pair.Value, bHidden);
	}
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelHeightfieldNodeKey, TObjectPtr<AActor>> Pair(LocalOwner, HeightfieldTransitionActors.FindRef(LocalOwner));
		if (!Pair.Value)
			continue;
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
											? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
											: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (!CoverageAffects(Rect))
			continue;
		const FGuid Group = BeginPublishGroup(
			{static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5), Pair.Key.Coordinate, Pair.Key.Level});
		if (Publisher->HasPresentation(Pair.Value) && (BlockedHeightfieldNodes.Contains(Pair.Key) || HandoffPending(Rect)))
		{
			co_await SetSharedCoverage(Group, Pair.Value, CommonTransitionBoxes);
			HeightfieldToCommit.Remove(Pair.Key);
			continue;
		}
		co_await SetCoverage(Group, Pair.Value, {}, CommonTransitionBoxes);
		Publisher->SetHidden(Group, Pair.Value, !DesiredTransitionSignatures.Contains(Pair.Key) || BlockedHeightfieldNodes.Contains(Pair.Key));
	}
	const double ActorsMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0 - PlanMilliseconds - BoxesMilliseconds;
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> PreviousNodesByGroup;
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> TargetNodesByGroup;
	auto TerrainGroupKey = [this](const FVoxelViewKey& Key, const bool bPresented = false)
	{
		if (Key.Level == 0)
		{
			const auto& Owners = bPresented ? PresentedFineSectionOwners : FinePagePlan.SectionOwners;
			if (const FVoxelFinePageKey* Page = Owners.Find(Key.Coordinate))
				return FVoxelPublishGroupKey{0, Page->Coordinate, Page->Level};
			return FVoxelPublishGroupKey{0, Key.Coordinate, 0};
		}
		return FVoxelPublishGroupKey{1, Key.Coordinate, Key.Level};
	};
	for (const FVoxelViewKey& Key : PreviousTerrainNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!CoverageAffects(TerrainCoverageRect(Key)))
			continue;
		if (!TargetTerrainNodes.Contains(Key))
			BeginPublishGroup(TerrainGroupKey(Key, true));
		if (OpenPublishGroups.Contains(TerrainGroupKey(Key, true)))
			PreviousNodesByGroup.FindOrAdd(TerrainGroupKey(Key, true)).Add(Key);
	}
	for (const FVoxelViewKey& Key : TargetTerrainNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		// 空 section 的空间所有权同样按局部分组交接，不能等待全局最终回调。
		if (!CoverageAffects(TerrainCoverageRect(Key)))
			continue;
		if (!VisibleTerrainNodes.Contains(Key))
			BeginPublishGroup(TerrainGroupKey(Key));
		if (OpenPublishGroups.Contains(TerrainGroupKey(Key)))
			TargetNodesByGroup.FindOrAdd(TerrainGroupKey(Key)).Add(Key);
	}
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> VolumeByGroup;
	for (const auto& Pair : VolumeToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		VolumeByGroup.FindOrAdd(TerrainGroupKey(Pair.Key)).Add(Pair.Key);
	}
	TMap<FVoxelPublishGroupKey, TArray<FVoxelHeightfieldNodeKey>> HeightfieldByGroup;
	for (const auto& Pair : HeightfieldToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelHeightfieldNodeKey& Key = Pair.Key;
		const uint8 Representation = Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5;
		HeightfieldByGroup.FindOrAdd({Representation, Key.Coordinate, Key.Level}).Add(Key);
	}
	TSet<FVoxelPublishGroupKey> OwnershipChanges;
	for (const auto& Pair : OpenPublishGroups)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Previous = PreviousNodesByGroup.Find(Pair.Key);
		const auto* Target = TargetNodesByGroup.Find(Pair.Key);
		bool bChanged = (Previous ? Previous->Num() : 0) != (Target ? Target->Num() : 0);
		if (!bChanged && Previous && Target)
			for (const auto& Node : *Previous)
				if (!Target->Contains(Node))
				{
					bChanged = true;
					break;
				}
		if (bChanged || VolumeByGroup.Contains(Pair.Key) || HeightfieldByGroup.Contains(Pair.Key))
			OwnershipChanges.Add(Pair.Key);
	}
	TFunction<void()> OnPublishCommitted = [this, LocalOwners = MoveTemp(LocalHeightfieldOwners), Recheck = MoveTemp(RecheckCoverageRects)]()
	{
		for (const auto& Key : LocalOwners)
		{
			if (DesiredTransitionSignatures.Contains(Key))
				continue;
			const FVoxelCoverageRect Rect = Key.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Key.Coordinate, Key.Level})
																										   : MacroCoverageRect({Key.Coordinate, Key.Level});
			if (VoxelCoverage::IntersectsAny2D(Rect, PendingHeightfieldHandoffRects))
				continue;
			if (AActor* Actor = HeightfieldTransitionActors.FindRef(Key))
			{
				Publisher->Forget(Actor);
				Actor->Destroy();
			}
			HeightfieldTransitionSignatures.Remove(Key);
			HeightfieldTransitionActors.Remove(Key);
		}
		for (const auto& Rect : Recheck)
			MarkCoverageDirty(Rect);
	};
	TMap<FVoxelFinePageKey, TSet<FIntVector>> RetainedFineByPage;
	for (const auto& Pair : OpenPublishGroups)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Pair.Key.Representation != 0)
			continue;
		const FVoxelFinePageKey Page{Pair.Key.Coordinate, Pair.Key.Level};
		TArray<FIntVector> Sections;
		co_await GatherFinePageSectionSteps(Page, Sections);
		for (const auto& Section : Sections)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (TargetTerrainNodes.Contains({Section, 0}))
				RetainedFineByPage.FindOrAdd(Page).Add(Section);
		}
	}
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometryMetrics> GeneratedForCommit;
	for (const auto& Group : OpenPublishGroups)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Group.Key.Representation > 3)
			continue;
		const auto Representation = static_cast<EVoxelTerrainRepresentation>(Group.Key.Representation + 1);
		const FVoxelTerrainSpatialKey Key{Representation, Group.Key.Coordinate, Group.Key.Level};
		if (const auto* Metrics = GeometryMetrics.Find(Key))
			GeneratedForCommit.Add(Key, *Metrics);
	}
	TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)> OnGroupCommitted =
		[this, Generated = MoveTemp(GeneratedForCommit), Target = MoveTemp(TargetNodesByGroup), RetainedFine = MoveTemp(RetainedFineByPage),
			FineVisibility = MoveTemp(FineVisibility), Volume = MoveTemp(VolumeByGroup), Heightfield = MoveTemp(HeightfieldByGroup),
			VolumeSignatures = MoveTemp(VolumeToCommit), HeightfieldSignatures = MoveTemp(HeightfieldToCommit)](TConstArrayView<FVoxelPublishGroupKey> Keys)
	{
		bTerrainBuildStageDirty = true;
		for (const FVoxelPublishGroupKey& Key : Keys)
		{
			const auto* Desired = Target.Find(Key);
			if (Key.Representation == 0)
			{
				const FVoxelFinePageKey Page{Key.Coordinate, Key.Level};
				const bool* Visibility = FineVisibility.Find(Page);
				const TSet<FIntVector>* Retained = RetainedFine.Find(Page);
				const TSet<FIntVector> Empty;
				CommitFinePageOwnership(Page, Desired ? MakeArrayView(*Desired) : TConstArrayView<FVoxelViewKey>(), Retained ? *Retained : Empty,
					Visibility ? TOptional<bool>(*Visibility) : TOptional<bool>());
			}
			else if (Key.Representation == 1 && CommitTerrainOwnership(Key, Desired ? MakeArrayView(*Desired) : TConstArrayView<FVoxelViewKey>()))
			{
				MarkCoverageDirty(VoxelProxyCoverageRect({Key.Coordinate, Key.Level}));
				bHeightfieldTopologyDirty = true;
			}
			if (Key.Representation <= 3)
			{
				const FVoxelTerrainSpatialKey Spatial{static_cast<EVoxelTerrainRepresentation>(Key.Representation + 1), Key.Coordinate, Key.Level};
				AActor* Actor = Key.Representation == 0	  ? FinePageActors.FindRef({Key.Coordinate, Key.Level})
								: Key.Representation == 1 ? VoxelProxyActors.FindRef({Key.Coordinate, Key.Level})
								: Key.Representation == 2 ? SurfaceActors.FindRef({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level})
														  : MacroActors.FindRef({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				if (const auto* Metrics = Generated.Find(Spatial))
				{
					auto Presented = *Metrics;
					Presented.bPresented = Publisher->GetPresentedMeshCost(Actor, Presented.Vertices, Presented.Triangles, Presented.MeshBytes);
					if (Key.Representation == 0 && PresentedEmptyFinePages.Contains({Key.Coordinate, Key.Level}))
					{
						Presented.bPresented = true;
						Presented.Vertices = 0;
						Presented.Triangles = 0;
						Presented.MeshBytes = 0;
					}
					if (Presented.bPresented)
						PresentedGeometryMetrics.Add(Spatial, Presented);
					else
						PresentedGeometryMetrics.Remove(Spatial);
				}
				else if (!Actor || Actor->IsHidden())
					PresentedGeometryMetrics.Remove(Spatial);
			}
			if (const TArray<FVoxelViewKey>* Nodes = Volume.Find(Key))
			{
				for (const FVoxelViewKey& Node : *Nodes)
				{
					if (const uint64* Signature = VolumeSignatures.Find(Node))
					{
						VolumeTransitionSignatures.Add(Node, *Signature);
						const uint64* Prepared = PreparedVolumeSignatures.Find(Node);
						if (Prepared && *Prepared == *Signature)
						{
							PreparedVolumeSignatures.Remove(Node);
							PreparedVolumeMeshes.Remove(Node);
							PreparedVolumeBoundaryMeshes.Remove(Node);
						}
						MarkCoverageDirty(VoxelProxyCoverageRect(Node));
					}
				}
			}
			if (const TArray<FVoxelHeightfieldNodeKey>* Nodes = Heightfield.Find(Key))
			{
				for (const FVoxelHeightfieldNodeKey& Node : *Nodes)
				{
					if (const uint64* Signature = HeightfieldSignatures.Find(Node))
					{
						HeightfieldTransitionSignatures.Add(Node, *Signature);
						const uint64* Prepared = PreparedTransitionSignatures.Find(Node);
						if (Prepared && *Prepared == *Signature)
						{
							PreparedTransitionSignatures.Remove(Node);
							PreparedTransitionMeshes.Remove(Node);
						}
						MarkCoverageDirty(Node.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Node.Coordinate, Node.Level})
																										  : MacroCoverageRect({Node.Coordinate, Node.Level}));
					}
				}
			}
		}
	};
	for (auto It = OpenPublishGroups.CreateIterator(); It; ++It)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		Publisher->SetOwnershipChanged(It.Value(), OwnershipChanges.Contains(It.Key()));
		if (It.Key().Representation == 0 && PendingFinePages.Contains({It.Key().Coordinate, It.Key().Level}))
			continue;
		if (Publisher->DiscardUnchangedGroup(It.Value()))
			It.RemoveCurrent();
	}
	if (OpenPublishGroups.IsEmpty())
	{
		OnPublishCommitted();
	}
	else
	{
		Publisher->SetCommitCallbacks(MoveTemp(OnPublishCommitted), MoveTemp(OnGroupCommitted));
		bool bAccepted = true;
		for (const auto& Pair : OpenPublishGroups)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Pair.Key, Module.BlockSize(), Module.GetViewSettings().Lod);
			FVoxelCoverageLocalSnapshot Guard;
			if (Bounds.IsValid)
			{
				const bool bVolume = Pair.Key.Representation <= 1;
				const int32 MinZ = bVolume ? FMath::FloorToInt(Bounds.Min.Z / Module.BlockSize()) - 1 : MIN_int32 + 1;
				const int32 MaxZ = bVolume ? FMath::CeilToInt(Bounds.Max.Z / Module.BlockSize()) + 1 : MAX_int32;
				co_await CoverageIndex.CaptureSteps(
					FIntVector(FMath::FloorToInt(Bounds.Min.X / Module.BlockSize()) - 1, FMath::FloorToInt(Bounds.Min.Y / Module.BlockSize()) - 1, MinZ),
					FIntVector(FMath::CeilToInt(Bounds.Max.X / Module.BlockSize()) + 1, FMath::CeilToInt(Bounds.Max.Y / Module.BlockSize()) + 1, MaxZ), Guard,
					false);
			}
			const uint64 Epoch = WorldEpoch;
			Publisher->SetValidation(Pair.Value,
				[this, Epoch, ContentRevision, Guard = MoveTemp(Guard)]
				{
					return Epoch == WorldEpoch && ContentRevision == CoverageContentRevision && CoverageIndex.IsCurrent(Guard);
				});
			bAccepted &= Publisher->CommitGroup(Pair.Value);
		}
		OpenPublishGroups.Reset();
		if (!bAccepted)
			MarkCoverageDirty();
	}
	const double Now = FPlatformTime::Seconds();
	if (CVarVoxelDebugRepresentation.GetValueOnGameThread() > 0 && Now >= NextTimingLog)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel coverage phase elapsed ms: plan=%.3f boxes=%.3f actors=%.3f commit=%.3f"), PlanMilliseconds, BoxesMilliseconds,
			ActorsMilliseconds, (Now - ResolveStart) * 1000.0 - PlanMilliseconds - BoxesMilliseconds - ActorsMilliseconds);
		NextTimingLog = Now + 5.0;
	}
}

bool FVoxelViewManager::EnqueueViewTask(FVoxelTaskRequest&& InRequest)
{
	if (InRequest.Apply)
	{
		const auto Apply = MakeShared<TFunction<void(FVoxelTaskResult&&)>>(MoveTemp(InRequest.Apply));
		InRequest.Apply = [this, Apply](FVoxelTaskResult&& InResult)
		{
			if (CoverageContinuation.IsValid())
			{
				const auto Result = MakeShared<FVoxelTaskResult>(MoveTemp(InResult));
				DeferredCoverageCallbacks.Enqueue(
					[Apply, Result]
					{
						(*Apply)(MoveTemp(*Result));
					});
				return;
			}
			(*Apply)(MoveTemp(InResult));
		};
	}
	return Scheduler.Enqueue(MoveTemp(InRequest));
}

void FVoxelViewManager::RecordTerrainMesh(const FVoxelTerrainSpatialKey& InKey, const FVoxelSectionMeshResult& InMesh, const double InBuildMilliseconds)
{
	auto& Metrics = GeometryMetrics.FindOrAdd(InKey);
	if (const auto* Geometry = GetViewInterest().Geometry.Find(InKey))
		Metrics.Geometry = *Geometry;
	else
		Metrics.Geometry = {InKey, 1, Module.GetManifest().Settings.MinZ, Module.GetManifest().Settings.MaxZ, 0, 0};
	Metrics.Vertices = Metrics.Triangles = 0;
	for (const auto& Batch : InMesh.Batches)
	{
		Metrics.Vertices += Batch.Mesh.Vertices.Num();
		Metrics.Triangles += Batch.Mesh.Triangles.Num() / 3;
	}
	Metrics.MeshBytes = InMesh.Bytes();
	Metrics.BuildMilliseconds = InBuildMilliseconds;
	Metrics.bGenerated = true;
	++TerrainWorkMetrics.Completed;
	TerrainWorkMetrics.BuildMilliseconds += InBuildMilliseconds;
}

FVoxelTerrainWorkMetrics FVoxelViewManager::GetTerrainWorkMetrics() const
{
	FVoxelTerrainWorkMetrics Result = TerrainWorkMetrics;
	Result.Canceled += CanceledFinePageResults;
	Result.Stale += StaleFinePageResults;
	return Result;
}

void FVoxelViewManager::LogTerrainGeometryMetrics() const
{
	struct FBucket
	{
		int32 Pages = 0;
		uint64 Vertices = 0;
		uint64 Triangles = 0;
		uint64 Bytes = 0;
		double AreaCells = 0.0;
		double BuildMilliseconds = 0.0;
		TSet<FIntPoint> Footprints;
	};
	const auto Log = [this](const TCHAR* Phase, const TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometryMetrics>& Values)
	{
		TMap<uint64, FBucket> Buckets;
		for (const auto& Pair : Values)
		{
			const auto& Metrics = Pair.Value;
			const auto& Geometry = Metrics.Geometry;
			auto& Bucket =
				Buckets.FindOrAdd((uint64(Geometry.Key.Representation) << 56) | (uint64(Geometry.Key.Level) << 48) | uint32(Geometry.SampleStepCells));
			++Bucket.Pages;
			Bucket.Vertices += Metrics.Vertices;
			Bucket.Triangles += Metrics.Triangles;
			Bucket.Bytes += Metrics.MeshBytes;
			bool bAlreadyCounted = false;
			Bucket.Footprints.Add(FIntPoint(Geometry.Key.Coordinate.X, Geometry.Key.Coordinate.Y), &bAlreadyCounted);
			if (!bAlreadyCounted)
			{
				Bucket.AreaCells += double(Geometry.Key.SideCells()) * Geometry.Key.SideCells();
			}
			Bucket.BuildMilliseconds += Metrics.BuildMilliseconds;
		}
		for (const auto& Pair : Buckets)
			UE_LOG(LogTemp, Display,
				TEXT("Voxel geometry actual: phase=%s epoch=%llu rep=%d level=%u step=%u pages=%d ownershipAreaCells=%.0f vertices=%llu triangles=%llu "
					 "bytes=%llu buildMs=%.3f"),
				Phase, WorldEpoch, int32(Pair.Key >> 56), uint8(Pair.Key >> 48), uint32(Pair.Key), Pair.Value.Pages, Pair.Value.AreaCells, Pair.Value.Vertices,
				Pair.Value.Triangles, Pair.Value.Bytes, Pair.Value.BuildMilliseconds);
	};
	Log(TEXT("generated"), GeometryMetrics);
	Log(TEXT("presented"), PresentedGeometryMetrics);
	const auto Work = GetTerrainWorkMetrics();
	UE_LOG(LogTemp, Display,
		TEXT("Voxel terrain work: epoch=%llu completed=%llu failed=%llu canceled=%llu stale=%llu retry=%llu buildMs=%.3f coverageSteps=%llu pendingCoverage=%d "
			 "ageMs=%.3f"),
		WorldEpoch, Work.Completed, Work.Failed, Work.Canceled, Work.Stale, Work.Retried, Work.BuildMilliseconds, CoverageMicrosteps,
		CoverageContinuation.IsValid(), CoverageContinuation.IsValid() ? (FPlatformTime::Seconds() - CoverageStartedSeconds) * 1000.0 : 0.0);
}

void FVoxelViewManager::ApplyFinePageBuild(const FVoxelFinePageKey& Page, const uint64 Signature, const uint64 PageVersion, const uint64 ContentRevision,
	const FVoxelTaskStamp& AttemptStamp, FVoxelTaskResult&& Result)
{
	const FFinePageAttempt* Pending = PendingFinePageBuilds.Find(Page);
	if (!Pending || !(Pending->Stamp == Result.Stamp) || !(AttemptStamp == Result.Stamp))
	{
		++StaleFinePageResults;
		return;
	}
	PendingFinePageBuilds.Remove(Page);
	if (Result.bCanceled)
	{
		++CanceledFinePageResults;
		TerrainWorkMetrics.Retried += FinePagePlan.Pages.Contains(Page);
		return;
	}
	if (Result.Stamp.WorldEpoch != WorldEpoch || !FinePagePlan.Pages.Contains(Page) || FinePageVersions.FindRef(Page) != PageVersion ||
		FinePageContentRevisions.FindRef(Page) != ContentRevision)
	{
		++StaleFinePageResults;
		TerrainWorkMetrics.Retried += FinePagePlan.Pages.Contains(Page);
		return;
	}
	if (!Result.bSuccess)
	{
		LastFinePagePlanningError = Result.Error;
		FinePageFailures.Add(Page, Result.Error);
		DirtyFinePages.Remove(Page);
		++TerrainWorkMetrics.Failed;
		UE_LOG(LogTemp, Error, TEXT("Fine page build failed: page=%s level=%u attempt=%llu error=%s"), *Page.Coordinate.ToString(), Page.Level,
			AttemptStamp.Token, *Result.Error);
		return;
	}
	const auto Payload = StaticCastSharedPtr<const FVoxelFinePageBuildResult>(Result.CustomPayload);
	check(Payload && Payload->Mesh);
	PreparedFinePageMeshes.Add(Page, {Signature, Payload->Mesh, PageVersion, ContentRevision});
	RecordTerrainMesh({EVoxelTerrainRepresentation::Fine, Page.Coordinate, Page.Level}, *Payload->Mesh, Result.ExecuteMilliseconds);
	DirtyFinePages.Add(Page);
	bFinePresentationDirty = true;
	const auto Bounds = Page.GetBounds();
	MarkCoverageDirty({FIntPoint(Bounds.Min.X, Bounds.Min.Y), FIntPoint(Bounds.Max.X, Bounds.Max.Y)});
}

FVoxelGameThreadContinuation FVoxelViewManager::IsTerrainVolumeCoveredSteps(
	const FVoxelViewKey InNode, const FCoveragePassSnapshot& InSnapshot, const bool bByFine, bool& OutCovered) const
{
	OutCovered = false;
	const auto& Nodes = bByFine ? InSnapshot.FineNodes : InSnapshot.ProxyNodes;
	const auto& Branches = bByFine ? InSnapshot.FineBranches : InSnapshot.ProxyBranches;
	for (FVoxelViewKey Ancestor = InNode; Ancestor.Level <= 20; Ancestor = Ancestor.GetParent())
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Nodes.Contains(Ancestor))
		{
			OutCovered = true;
			co_return;
		}
	}
	if (InNode.Level == 0 || !Branches.Contains(InNode))
		co_return;
	TArray<FVoxelViewKey> Children;
	InNode.GetChildren(Children);
	for (const auto& Child : Children)
	{
		bool bChildCovered = false;
		co_await IsTerrainVolumeCoveredSteps(Child, InSnapshot, bByFine, bChildCovered);
		if (!bChildCovered)
			co_return;
	}
	OutCovered = true;
}

namespace
{
FVoxelGameThreadContinuation GatherFinePageSectionSteps(FVoxelFinePageKey Page, TArray<FIntVector>& Out);
FVoxelGameThreadContinuation GatherFinePageSectionSteps(const FVoxelFinePageKey Page, TArray<FIntVector>& Out)
{
	Out.Reset();
	const auto Bounds = Page.GetBounds();
	for (int64 Z = Bounds.Min.Z; Z < Bounds.Max.Z; Z += 16)
		for (int64 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y += 16)
			for (int64 X = Bounds.Min.X; X < Bounds.Max.X; X += 16)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				Out.Add(FIntVector(X / 16, Y / 16, Z / 16));
			}
}
} // namespace

FVoxelGameThreadContinuation FVoxelViewManager::IsFinePageReplacementReadySteps(
	const FVoxelFinePageKey& InPage, const FCoveragePassSnapshot& InSnapshot, bool& OutReady) const
{
	OutReady = false;
	TArray<FIntVector> Sections;
	co_await GatherFinePageSectionSteps(InPage, Sections);
	for (const FIntVector& Section : Sections)
	{
		const FVoxelFinePageKey* Owner = InSnapshot.PresentedFineOwners.Find(Section);
		if (Owner && *Owner != InPage)
			continue;
		bool bCovered = false;
		co_await IsTerrainVolumeCoveredSteps({Section, 0}, InSnapshot, false, bCovered);
		if (!bCovered)
			co_await VoxelCoverage::IsFullyCovered2DSteps(FineCoverageRect(Section), InSnapshot.Heightfield, bCovered);
		if (!bCovered)
			co_return;
	}
	OutReady = true;
}

FVoxelGameThreadContinuation FVoxelViewManager::GatherFinePageCandidateSteps(const TSet<FVoxelViewKey>& InCandidates, TSet<FVoxelFinePageKey>& OutPages) const
{
	OutPages.Reset();
	const auto AddScopedPages = [this, &OutPages](const TSet<FVoxelFinePageKey>& Pages) -> FVoxelGameThreadContinuation
	{
		for (const FVoxelFinePageKey& Page : Pages)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const FVoxelGenerationBounds Bounds = Page.GetBounds();
			if (CoverageAffects({FIntPoint(Bounds.Min.X, Bounds.Min.Y), FIntPoint(Bounds.Max.X, Bounds.Max.Y)}))
				OutPages.Add(Page);
		}
	};
	co_await AddScopedPages(DirtyFinePages);
	co_await AddScopedPages(PendingFinePages);
	for (const FVoxelViewKey& Node : InCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Node.Level != 0) continue;
		if (const FVoxelFinePageKey* Page = PresentedFineSectionOwners.Find(Node.Coordinate)) OutPages.Add(*Page);
		if (const FVoxelFinePageKey* Page = FinePagePlan.SectionOwners.Find(Node.Coordinate)) OutPages.Add(*Page);
	}
	TArray<FVoxelFinePageKey> InitialPages;
	co_await VoxelGameThread::AppendSteps(InitialPages, OutPages);
	for (const FVoxelFinePageKey& Page : InitialPages)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		TArray<FIntVector> Sections;
		co_await GatherFinePageSectionSteps(Page, Sections);
		for (const FIntVector& Section : Sections)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (const FVoxelFinePageKey* Desired = FinePagePlan.SectionOwners.Find(Section)) OutPages.Add(*Desired);
		}
	}
}

FVoxelGameThreadContinuation FVoxelViewManager::UpdateFinePagePresentationSteps(
	const FVoxelFinePageKey& InPage, const TSet<FVoxelViewKey>& InTarget, const FCoveragePassSnapshot& InSnapshot, TOptional<bool>& OutVisible)
{
	OutVisible.Reset();
	TArray<FIntVector> Sections;
	co_await GatherFinePageSectionSteps(InPage, Sections);
	bool bVisible = FinePagePlan.Pages.Contains(InPage) && FinePageReady.Contains(InPage);
	for (const FIntVector& Section : Sections)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		bVisible &= InTarget.Contains({Section, 0});
	}
	const FVoxelGenerationBounds Bounds = InPage.GetBounds();
	if (VoxelCoverage::IntersectsAny3D({Bounds.Min, Bounds.Max}, PendingVolumeHandoffBoxes))
	{
		co_return;
	}
	AActor* Actor = FinePageActors.FindRef(InPage);
	bool bOwnsAny = false;
	bool bOwnershipMatches = true;
	for (const FIntVector& Section : Sections)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelFinePageKey* Owner = PresentedFineSectionOwners.Find(Section);
		const bool bOwnedByThisPage = Owner && *Owner == InPage;
		bOwnsAny |= bOwnedByThisPage;
		bOwnershipMatches &= bVisible
			? bOwnedByThisPage && VisibleTerrainNodes.Contains({Section, 0})
			: !bOwnedByThisPage;
	}
	if (!bVisible && !Actor && !bOwnsAny && !PendingFinePages.Contains(InPage))
		co_return;
	if (bVisible)
	{
		for (const FIntVector& Section : Sections)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const FVoxelFinePageKey* Old = PresentedFineSectionOwners.Find(Section);
			if (!Old || *Old == InPage)
			{
				continue;
			}
			TArray<FIntVector> OldSections;
			const FVoxelFinePageKey OldPage = *Old;
			co_await GatherFinePageSectionSteps(OldPage, OldSections);
			for (const FIntVector& OldSection : OldSections)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				const FVoxelFinePageKey* Desired = FinePagePlan.SectionOwners.Find(OldSection);
				if (!Desired || !FinePageReady.Contains(*Desired) || !InTarget.Contains({OldSection, 0}))
				{
					co_return;
				}
			}
		}
	}
	else if ((Actor && !Actor->IsHidden()) || bOwnsAny)
	{
		bool bInsideVolumeDomain = false;
		for (const FIntVector& Section : Sections)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (FineWanted.Contains(Section))
			{
				bInsideVolumeDomain = true;
				break;
			}
			for (FVoxelViewKey Node{Section, 0}; Node.Level <= 8; Node = Node.GetParent())
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (VoxelProxyWanted.Contains(Node))
				{
					bInsideVolumeDomain = true;
					break;
				}
			}
			if (bInsideVolumeDomain)
			{
				break;
			}
		}
		const bool bInsideDomain =
			bInsideVolumeDomain ||
			VoxelCoverage::IntersectsAny2D({FIntPoint(Bounds.Min.X, Bounds.Min.Y), FIntPoint(Bounds.Max.X, Bounds.Max.Y)}, InSnapshot.WantedHeightfield);
		bool bReplacement = !bInsideDomain;
		if (!bReplacement)
			co_await IsFinePageReplacementReadySteps(InPage, InSnapshot, bReplacement);
		if (!bReplacement)
		{
			bReplacement = true;
			for (const FIntVector& Section : Sections)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				const FVoxelFinePageKey* Desired = FinePagePlan.SectionOwners.Find(Section);
				if (!Desired || *Desired == InPage || !FinePageReady.Contains(*Desired) || !InTarget.Contains({Section, 0}))
				{
					bReplacement = false;
					break;
				}
			}
		}
		if (!bReplacement)
		{
			co_return;
		}
	}
	const FVoxelPublishGroupKey GroupKey{0, InPage.Coordinate, InPage.Level};
	if (!FinePagePlan.Pages.Contains(InPage))
	{
		DirtyFinePages.Remove(InPage);
	}
	if (bVisible && !Actor && !EmptyFinePages.Contains(InPage))
	{
		MarkFinePageDirty(InPage);
		bFinePresentationDirty = true;
		co_return;
	}
	const bool bPresentationMatches = (!Actor && EmptyFinePages.Contains(InPage)) || (Actor && Actor->IsHidden() == !bVisible && Publisher->IsCommitted(Actor));
	if (bOwnershipMatches && bPresentationMatches && !PendingFinePages.Contains(InPage) && !Publisher->IsGroupBusy(GroupKey))
	{
		OutVisible = bVisible;
		co_return;
	}
	const FGuid Group = BeginPublishGroup(GroupKey);
	if (Actor)
	{
		Publisher->SetCoverage(Group, Actor, {});
		Publisher->SetHidden(Group, Actor, !bVisible);
	}
	OutVisible = bVisible;
	co_return;
}

FVoxelGameThreadContinuation FVoxelViewManager::CleanupRetiredRepresentationSteps(const double InNow)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewRetire);

	auto CanRetire = [InNow](const double InLastWanted, const bool bReplacementReady, const bool bInsideDomain)
	{
		const double Age = InNow - InLastWanted;
		return Age >= RetireDelaySeconds && (bReplacementReady || (!bInsideDomain && Age >= OutsideDomainRetireDelaySeconds));
	};
	const bool bUsesTerrainPlan = !GetViewInterest().TerrainPlan.Roots.IsEmpty();

	if (RetirementQueue.IsEmpty())
		co_return;
	RetirementCursor %= RetirementQueue.Num();
	const FVoxelPublishGroupKey Candidate = RetirementQueue[RetirementCursor];
	RetirementCursor = (RetirementCursor + 1) % RetirementQueue.Num();
	TSet<FVoxelFinePageKey> Pages;
	TSet<FIntVector> Sections;
	TSet<FVoxelViewKey> ProxyRetireKeys;
	TSet<FVoxelSurfaceTileKey> SurfaceRetireKeys;
	TSet<FVoxelMacroTileKey> MacroRetireKeys;
	if (Candidate.Representation == 0)
		Pages.Add({Candidate.Coordinate, Candidate.Level});
	else if (Candidate.Representation == 4)
		Sections.Add(Candidate.Coordinate);
	else if (Candidate.Representation == 1)
		ProxyRetireKeys.Add({Candidate.Coordinate, Candidate.Level});
	else if (Candidate.Representation == 2)
		SurfaceRetireKeys.Add({FIntPoint(Candidate.Coordinate.X, Candidate.Coordinate.Y), Candidate.Level});
	else if (Candidate.Representation == 3)
		MacroRetireKeys.Add({FIntPoint(Candidate.Coordinate.X, Candidate.Coordinate.Y), Candidate.Level});
	const bool bWanted = Candidate.Representation == 0	 ? FinePagePlan.Pages.Contains({Candidate.Coordinate, Candidate.Level})
						 : Candidate.Representation == 4 ? FineWanted.Contains(Candidate.Coordinate)
						 : Candidate.Representation == 1 ? VoxelProxyWanted.Contains({Candidate.Coordinate, Candidate.Level})
						 : Candidate.Representation == 2 ? SurfaceWanted.Contains({FIntPoint(Candidate.Coordinate.X, Candidate.Coordinate.Y), Candidate.Level})
														 : MacroWanted.Contains({FIntPoint(Candidate.Coordinate.X, Candidate.Coordinate.Y), Candidate.Level});
	if (bWanted)
	{
		RemoveRetirement(Candidate);
		co_return;
	}
	const bool bWasFull = bActiveCoverageFull;
	TArray<FVoxelCoverageRect> PreviousScope = MoveTemp(ActiveDirtyCoverageRects);
	bActiveCoverageFull = false;
	const int32 Side = 16 << Candidate.Level;
	const FIntPoint Min(Candidate.Coordinate.X * Side, Candidate.Coordinate.Y * Side);
	ActiveDirtyCoverageRects.Add({Min, Min + FIntPoint(Side)});
	FCoveragePassSnapshot Snapshot;
	co_await BuildCoverageSnapshotSteps(Snapshot);
	ActiveDirtyCoverageRects = MoveTemp(PreviousScope);
	bActiveCoverageFull = bWasFull;
	for (const auto& Page : Pages)
	{
		if (FinePagePlan.Pages.Contains(Page) || Publisher->IsGroupBusy({0, Page.Coordinate, Page.Level}))
			continue;
		bool bReplacement = false;
		co_await IsFinePageReplacementReadySteps(Page, Snapshot, bReplacement);
		if (FinePageActors.FindRef(Page) && !FinePageActors.FindRef(Page)->IsHidden() && !bReplacement)
			continue;
		if (AActor* Actor = FinePageActors.FindRef(Page))
		{
			Publisher->Forget(Actor);
			Actor->Destroy();
		}
		FinePageReady.Remove(Page);
		PreparedFinePageMeshes.Remove(Page);
		FinePageActors.Remove(Page);
		EmptyFinePages.Remove(Page);
		PresentedEmptyFinePages.Remove(Page);
		RemoveRetirement(Candidate);
		if (Candidate.Representation <= 3)
		{
			const FVoxelTerrainSpatialKey Spatial{
				static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level};
			GeometryMetrics.Remove(Spatial);
			PresentedGeometryMetrics.Remove(Spatial);
		}
		if (Candidate.Representation >= 1 && Candidate.Representation <= 3)
			Module.GetRuntime()->GetChangeHierarchy().ReleaseGeometry(
				{static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level});
	}
	for (const FIntVector& Section : Sections)
	{
		if (FineWanted.Contains(Section) || VisibleTerrainNodes.Contains({Section, 0}) || PresentedFineSectionOwners.Contains(Section))
			continue;
		if (InNow - FineLastWanted.FindRef(Section) < RetireDelaySeconds)
			continue;
		FineSectionMeshes.Remove(Section);
		FineReady.Remove(Section);
		FineRevisions.Remove(Section);
		FineLastWanted.Remove(Section);
		IndexCoverage(FVoxelViewKey{Section, 0});
		RemoveRetirement(Candidate);
		if (Candidate.Representation <= 3)
		{
			const FVoxelTerrainSpatialKey Spatial{
				static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level};
			GeometryMetrics.Remove(Spatial);
			PresentedGeometryMetrics.Remove(Spatial);
		}
		if (Candidate.Representation >= 1 && Candidate.Representation <= 3)
			Module.GetRuntime()->GetChangeHierarchy().ReleaseGeometry(
				{static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level});
		bReadyTerrainBranchesDirty = true;
	}
	for (const FVoxelViewKey Key : ProxyRetireKeys)
	{
		if (VoxelProxyWanted.Contains(Key) || VisibleTerrainNodes.Contains(Key))
			continue;
		if (Publisher->IsGroupBusy({1, Key.Coordinate, Key.Level}))
			continue;
		const double LastWanted = VoxelProxyLastWanted.FindRef(Key);
		if (bUsesTerrainPlan)
		{
			if (InNow - LastWanted < RetireDelaySeconds)
				continue;
		}
		else
		{
			bool bReplacement = false;
			co_await IsTerrainVolumeCoveredSteps(Key, Snapshot, true, bReplacement);
			if (!bReplacement)
				co_await VoxelCoverage::IsFullyCovered2DSteps(VoxelProxyCoverageRect(Key), Snapshot.Heightfield, bReplacement);
			if (!CanRetire(LastWanted, bReplacement, IsProxyInsideRenderDomain(Key)))
			{
				continue;
			}
		}
		if (AActor* Actor = VoxelProxyActors.FindRef(Key))
		{
			Publisher->Forget(Actor);
			Actor->Destroy();
		}
		VoxelProxyActors.Remove(Key);
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(Key) > 0;
		VoxelProxyData.Remove(Key);
		AirVoxelProxyNodes.Remove(Key);
		ProxySurfaceCoverage.Remove(Key);
		VoxelProxyRevisions.Remove(Key);
		VolumeTransitionSignatures.Remove(Key);
		PreparedVolumeSignatures.Remove(Key);
		PreparedVolumeMeshes.Remove(Key);
		VoxelProxyLastWanted.Remove(Key);
		PreparedVolumeBoundaryMeshes.Remove(Key);
		if (AActor* Boundary = VolumeBoundaryActors.FindRef(Key))
		{
			Publisher->Forget(Boundary);
			Boundary->Destroy();
		}
		VolumeBoundaryActors.Remove(Key);
		IndexCoverage(Key);
		RemoveRetirement(Candidate);
		if (Candidate.Representation <= 3)
		{
			const FVoxelTerrainSpatialKey Spatial{
				static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level};
			GeometryMetrics.Remove(Spatial);
			PresentedGeometryMetrics.Remove(Spatial);
		}
		if (Candidate.Representation >= 1 && Candidate.Representation <= 3)
			Module.GetRuntime()->GetChangeHierarchy().ReleaseGeometry(
				{static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level});
	}
	for (const FVoxelSurfaceTileKey Key : SurfaceRetireKeys)
	{
		if (SurfaceWanted.Contains(Key) || InNow - SurfaceLastWanted.FindRef(Key) < RetireDelaySeconds)
			continue;
		if (Publisher->IsGroupBusy({2, Key.Coordinate, Key.Level}))
			continue;
		if (VoxelCoverage::IntersectsAny2D(SurfaceCoverageRect(Key), PendingHeightfieldHandoffRects))
			continue;
		bool bReplacement = false;
		co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Key), Snapshot.Macro, bReplacement);
		if (!bReplacement)
			co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Key), Snapshot.ProxySurface, bReplacement);
		if (SurfaceWanted.Contains(Key) || !CanRetire(SurfaceLastWanted.FindRef(Key), bReplacement, IsSurfaceInsideRenderDomain(Key)))
			continue;
		if (AActor* Actor = SurfaceActors.FindRef(Key))
		{
			Publisher->Forget(Actor);
			Actor->Destroy();
		}
		if (AActor* Water = WaterActors.FindRef(Key))
		{
			Publisher->Forget(Water);
			Water->Destroy();
		}
		SurfaceActors.Remove(Key);
		WaterActors.Remove(Key);
		bHeightfieldTopologyDirty |= SurfaceReady.Remove(Key) > 0;
		SurfaceData.Remove(Key);
		WaterData.Remove(Key);
		SurfaceRevisions.Remove(Key);
		SurfaceLastWanted.Remove(Key);
		IndexCoverage(Key);
		RemoveRetirement(Candidate);
		if (Candidate.Representation <= 3)
		{
			const FVoxelTerrainSpatialKey Spatial{
				static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level};
			GeometryMetrics.Remove(Spatial);
			PresentedGeometryMetrics.Remove(Spatial);
		}
		if (Candidate.Representation >= 1 && Candidate.Representation <= 3)
			Module.GetRuntime()->GetChangeHierarchy().ReleaseGeometry(
				{static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level});
		MarkCoverageDirty(SurfaceCoverageRect(Key));
	}
	for (const FVoxelMacroTileKey Key : MacroRetireKeys)
	{
		if (MacroWanted.Contains(Key) || InNow - MacroLastWanted.FindRef(Key) < RetireDelaySeconds)
			continue;
		if (Publisher->IsGroupBusy({3, Key.Coordinate, Key.Level}))
			continue;
		if (VoxelCoverage::IntersectsAny2D(MacroCoverageRect(Key), PendingHeightfieldHandoffRects))
			continue;
		bool bReplacement = false;
		co_await VoxelCoverage::IsFullyCovered2DSteps(MacroCoverageRect(Key), Snapshot.Surface, bReplacement);
		if (MacroWanted.Contains(Key) || !CanRetire(MacroLastWanted.FindRef(Key), bReplacement, IsMacroInsideRenderDomain(Key)))
			continue;
		if (AActor* Actor = MacroActors.FindRef(Key))
		{
			Publisher->Forget(Actor);
			Actor->Destroy();
		}
		MacroActors.Remove(Key);
		bHeightfieldTopologyDirty |= MacroReady.Remove(Key) > 0;
		MacroData.Remove(Key);
		MacroRevisions.Remove(Key);
		MacroLastWanted.Remove(Key);
		IndexCoverage(Key);
		RemoveRetirement(Candidate);
		if (Candidate.Representation <= 3)
		{
			const FVoxelTerrainSpatialKey Spatial{
				static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level};
			GeometryMetrics.Remove(Spatial);
			PresentedGeometryMetrics.Remove(Spatial);
		}
		if (Candidate.Representation >= 1 && Candidate.Representation <= 3)
			Module.GetRuntime()->GetChangeHierarchy().ReleaseGeometry(
				{static_cast<EVoxelTerrainRepresentation>(Candidate.Representation + 1), Candidate.Coordinate, Candidate.Level});
		MarkCoverageDirty(MacroCoverageRect(Key));
	}
}
