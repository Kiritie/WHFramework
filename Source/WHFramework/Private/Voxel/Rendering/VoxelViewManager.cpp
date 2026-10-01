#include "Voxel/Rendering/VoxelViewManager.h"

#include "Voxel/Streaming/VoxelInterestRuntime.h"

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
	Publisher->SetObservers(InObservers);
	Publisher->Tick();
	FrameTimings = Publisher->GetFrameTimings();
	const double AfterPublish = FPlatformTime::Seconds();
	if (Publisher->NeedsUpdate())
	{
		MarkCoverageDirty();
	}
	const double Now =
		FPlatformTime::Seconds();

	if (AppliedInterestRevision != InInterestRevision)
	{
		PriorityObservers = TArray<FVector>(InObservers);
		UpdateFineAndVoxelProxy(InObservers);
		UpdateSurface(InObservers);
		UpdateMacro(InObservers);
		CancelStaleViewTasks();
		UpdateWantedTimestamps(Now);
		RebuildAdmissions(InObservers);
		UpdateTaskPriorities();
		AppliedInterestRevision = InInterestRevision;
	}

	const double AfterPlan = FPlatformTime::Seconds();
	if (CVarVoxelDebugRepresentation.GetValueOnGameThread() > 0 &&
		Now >= NextRepresentationDebugLog)
	{
		LogRepresentationState(InObservers);
		NextRepresentationDebugLog = Now + 5.0;
	}

	if (bCoverageDirty)
	{
		const double CoverageStart = FPlatformTime::Seconds();
		bActiveCoverageFull = bCoverageDirtyFull;
		ActiveDirtyCoverageRects = MoveTemp(DirtyCoverageRects);
		bCoverageDirtyFull = false;
		bCoverageDirty = false;
		ResolveTransitionVisibility();
		ActiveDirtyCoverageRects.Reset();
		bActiveCoverageFull = true;
		LastCoverageMilliseconds = (FPlatformTime::Seconds() - CoverageStart) * 1000.0;
	}
	const double AfterCoverage = FPlatformTime::Seconds();
	FrameTimings.CoverageMilliseconds = (AfterCoverage - AfterPlan) * 1000.0;
	PumpHeightfieldTransitions();
	PumpVolumeTransitions();

	if (Now >= NextRetireCheck)
	{
		const double RetireStart = FPlatformTime::Seconds();
		CleanupRetiredRepresentations(Now);
		LastRetireMilliseconds = (FPlatformTime::Seconds() - RetireStart) * 1000.0;
		FrameTimings.RetireMilliseconds += LastRetireMilliseconds;
		NextRetireCheck = Now + RetireDelaySeconds;
	}
	const double AfterMaintenance = FPlatformTime::Seconds();
	ProcessAdmissions();
	ProcessDataAdmissions();
	FrameTimings.AdmissionMilliseconds = (FPlatformTime::Seconds() - AfterMaintenance) * 1000.0;
#if !UE_BUILD_SHIPPING
	const double TickEnd = FPlatformTime::Seconds();
	static double LastSlowViewLog = 0.0;
	if (TickEnd - TickStart > 0.008 && TickEnd - LastSlowViewLog > 1.0)
	{
		LastSlowViewLog = TickEnd;
		UE_LOG(LogTemp, Display, TEXT("Voxel view phases ms: publish=%.2f plan=%.2f coverage=%.2f maintenance=%.2f admission=%.2f sourcePriority=%d stage=%d"),
			(AfterPublish - TickStart) * 1000.0, (AfterPlan - AfterPublish) * 1000.0,
			(AfterCoverage - AfterPlan) * 1000.0, (AfterMaintenance - AfterCoverage) * 1000.0,
			(TickEnd - AfterMaintenance) * 1000.0, LastActiveAdmissionPriority, LastActiveAdmissionKind);
	}
#endif
}

bool FVoxelViewManager::IsAdmissionDataReady(const FVoxelViewAdmission& InAdmission) const
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
		break;
	case EVoxelViewAdmissionKind::Surface:
		Key.Kind = EVoxelTaskKind::GenerateSurface;
		MeshKind = EVoxelTaskKind::BuildSurface;
		Key.Stamp.SurfaceKey = InAdmission.SurfaceKey;
		Key.Stamp.Revision = Hierarchy.GetSurfaceRevision(InAdmission.SurfaceKey);
		Key.Stamp.Token = HashCombineFast(GetTypeHash(InAdmission.SurfaceKey), GetTypeHash(Key.Stamp.Revision));
		break;
	case EVoxelViewAdmissionKind::Macro:
		Key.Kind = EVoxelTaskKind::GenerateMacro;
		MeshKind = EVoxelTaskKind::BuildMacro;
		Key.Stamp.MacroKey = InAdmission.MacroKey;
		Key.Stamp.Revision = Hierarchy.GetMacroRevision(InAdmission.MacroKey);
		Key.Stamp.Token = HashCombineFast(GetTypeHash(InAdmission.MacroKey), GetTypeHash(Key.Stamp.Revision));
		break;
	default:
		return false;
	}
	return PreparedData.Contains(Key) || Scheduler.Has(Key.Stamp, MeshKind);
}

void FVoxelViewManager::ProcessDataAdmissions()
{
	const TMap<int32, uint8> Lanes = ResolveAdmissionLanes(Admissions,
		[this](const FVoxelViewAdmission& Admission) { return IsAdmissionDataReady(Admission); }, HasMovementCriticalFinePending());
	const int32 FirstKind = LastActiveDataKind < 4 ? LastActiveDataKind % 3 + 1 : 1;
	LastActiveDataKind = 4;
	const FVoxelViewSettings& Settings = Module.GetViewSettings();
	const int32 Limits[] = { 0, Settings.DataAdmissionPerFrame, Settings.DataAdmissionPerFrame, Settings.DataAdmissionPerFrame };
	int32 Submitted[4] = {};
	const double Deadline = FPlatformTime::Seconds() + Settings.AdmissionMilliseconds / 1000.0;
	for (const int32 Priority : AdmissionPriorities)
	{
		const uint8* LaneMask = Lanes.Find(Priority);
		if (!LaneMask) continue;
		for (int32 Offset = 0; Offset < 3; ++Offset)
		{
			const int32 Kind = (FirstKind - 1 + Offset) % 3 + 1;
			if ((*LaneMask & (1 << Kind)) == 0) continue;
			// 粗数据轮换先后顺序，避免单次预检超出软时间片后永久饿死后续表示。
			const double LaneDeadline = FPlatformTime::Seconds() +
				FMath::Max(0.0, Deadline - FPlatformTime::Seconds()) / (3 - Offset);
			if (LastActiveDataKind == 4) LastActiveDataKind = Kind;
			const TArray<int32>& Lane = Module.GetCurrentInterest().AdmissionLanes[Kind];
			const auto LanePriority = [this](const int32 Index) { return Admissions[Index].Priority; };
			const int32 First = Algo::LowerBoundBy(Lane, Priority, LanePriority);
			const int32 End = Algo::UpperBoundBy(Lane, Priority, LanePriority);
			if (First == End) continue;
			int32& Scan = DataScanIndices[Kind].FindOrAdd(Priority);
			if (Scan < First || Scan >= End) Scan = First;
			for (int32 Attempt = 0; Attempt < FMath::Min(FMath::Max(64, Limits[Kind] * 8), End - First) &&
				Submitted[Kind] < Limits[Kind] && FPlatformTime::Seconds() < LaneDeadline; ++Attempt)
			{
				const FVoxelViewAdmission& Admission = Admissions[Lane[Scan]];
				Scan = Scan + 1 < End ? Scan + 1 : First;
				if (IsAdmissionDataReady(Admission)) continue;
				switch (Admission.Kind)
				{
				case EVoxelViewAdmissionKind::VoxelProxy:
					Submitted[Kind] += RequestVoxelProxy(Admission.ProxyKey, Priority, true);
					break;
				case EVoxelViewAdmissionKind::Surface:
					Submitted[Kind] += RequestSurface(Admission.SurfaceKey, Priority, true);
					break;
				case EVoxelViewAdmissionKind::Macro:
					Submitted[Kind] += RequestMacro(Admission.MacroKey, Priority, true);
					break;
				default: break;
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
		return VoxelProxyWanted.Contains(InKey.Stamp.ViewKey) &&
			Hierarchy.GetVoxelProxyRevision(InKey.Stamp.ViewKey) == InKey.Stamp.Revision;
	case EVoxelTaskKind::GenerateSurface:
		return SurfaceWanted.Contains(InKey.Stamp.SurfaceKey) &&
			Hierarchy.GetSurfaceRevision(InKey.Stamp.SurfaceKey) == InKey.Stamp.Revision;
	case EVoxelTaskKind::GenerateMacro:
		return MacroWanted.Contains(InKey.Stamp.MacroKey) &&
			Hierarchy.GetMacroRevision(InKey.Stamp.MacroKey) == InKey.Stamp.Revision;
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
	const uint64 Limit = (InRequest.WorkClass == EVoxelWorkClass::Prefetch ? 128ull : 256ull) * 1024ull * 1024ull;
	const uint64 Reservation = InRequest.ReservedBytes + sizeof(FVoxelTaskResult) + sizeof(FVoxelTaskKey);
	if (PreparedDataBytes + PendingDataBytes + Reservation > Limit) return false;
	InRequest.Apply = [this, Reservation](FVoxelTaskResult&& Result)
	{
		PendingDataBytes -= Reservation;
		const FVoxelTaskKey Key { Result.Kind, Result.Stamp };
		if (Result.bCanceled || Result.Error.StartsWith(TEXT("DependencyNotReady")) || !IsPreparedDataCurrent(Key)) return;
		if (!Result.bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("Voxel data preparation failed: kind=%d error=%s"),
				static_cast<int32>(Result.Kind), *Result.Error);
			return;
		}
		RemovePreparedData(Key);
		if (Result.bKnownEmptyMesh && Result.VoxelProxy && !VoxelProxyActors.Contains(Result.Stamp.ViewKey))
		{
			Result.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
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
	if (!Scheduler.Enqueue(MoveTemp(InRequest))) return false;
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

EVoxelWorkClass FVoxelViewManager::VolumeTransitionWorkClass(const FVoxelViewKey& InOwner) const
{
	const auto Context = DesiredVolumeContexts.FindRef(InOwner);
	if (Context && Context->Patches.ContainsByPredicate([this](const FVoxelBoundaryTransitionPatch& Patch)
	{
		const auto* Demand = Patch.Face.Neighbor.Level == 0
			? Module.GetCurrentInterest().Exact.Find(Patch.Face.Neighbor.Coordinate) : nullptr;
		return Demand && Demand->bMovementCriticalFine;
	})) return EVoxelWorkClass::Critical;
	return Context && Context->Patches.ContainsByPredicate([](const FVoxelBoundaryTransitionPatch& Patch)
	{
		return Patch.Face.Neighbor.Level == 0;
	}) ? EVoxelWorkClass::Interactive : EVoxelWorkClass::Boundary;
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
			if (const FVoxelExactDemand* Demand = Module.GetCurrentInterest().Exact.Find(Stamp.Section))
			{
				WorkClass = Demand->bWarmupData || Demand->bMovementCriticalCollision ||
					Demand->bMovementCriticalFine
					? EVoxelWorkClass::Critical : EVoxelWorkClass::Interactive;
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
			Distance = MinimumObserverDistanceCells(Stamp.ViewKey.GetBounds());
			break;
		case EVoxelTaskKind::BuildSurface:
		case EVoxelTaskKind::GenerateSurface:
			if (const int32* SourcePriority = Module.GetCurrentInterest().SurfacePriorities.Find(Stamp.SurfaceKey))
				Priority = *SourcePriority;
			if (Kind == EVoxelTaskKind::GenerateSurface)
				WorkClass = EVoxelWorkClass::Prefetch;
			Distance = MinimumObserverDistanceCells(SurfaceWorldCenter(Stamp.SurfaceKey));
			break;
		case EVoxelTaskKind::BuildMacro:
		case EVoxelTaskKind::GenerateMacro:
			if (const int32* SourcePriority = Module.GetCurrentInterest().MacroPriorities.Find(Stamp.MacroKey))
				Priority = *SourcePriority;
			if (Kind == EVoxelTaskKind::GenerateMacro)
				WorkClass = EVoxelWorkClass::Prefetch;
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
}

void FVoxelViewManager::ProcessAdmissions()
{
	const FVoxelInterestSet& Interest = Module.GetCurrentInterest();
	const int32 FirstCoarseKind = LastActiveDataKind < 4 ? LastActiveDataKind % 3 + 1 : 1;
	const FVoxelViewSettings& Settings = Module.GetViewSettings();
	const int32 Limits[] = { Settings.FineAdmissionPerFrame, Settings.VoxelProxyAdmissionPerFrame,
		Settings.SurfaceAdmissionPerFrame, Settings.MacroAdmissionPerFrame };
	TMap<int32, TStaticArray<int32, 4>> FirstPendingAdmissions;
	const TMap<int32, uint8> Lanes = ResolveAdmissionLanes(Admissions,
		[this, &FirstPendingAdmissions](const FVoxelViewAdmission& Admission)
		{
			const bool bReady = IsAdmissionMeshReady(Admission) || IsAdmissionTerminalFailure(Admission);
			if (!bReady)
			{
				auto* First = FirstPendingAdmissions.Find(Admission.Priority);
				if (!First)
				{
					TStaticArray<int32, 4> Indices;
					for (int32& Index : Indices) Index = INDEX_NONE;
					First = &FirstPendingAdmissions.Add(Admission.Priority, Indices);
				}
				int32& Index = (*First)[static_cast<uint8>(Admission.Kind)];
				if (Index == INDEX_NONE) Index = &Admission - Admissions.GetData();
			}
			return bReady;
		}, HasMovementCriticalFinePending());
	LastActiveAdmissionKind = 4;
	LastActiveAdmissionPriority = MAX_int32;
	LastResolvedFrontier = MAX_dbl;
	int32 Submitted[4] = {};
	const double Deadline = FPlatformTime::Seconds() + Settings.AdmissionMilliseconds / 1000.0;
	for (const int32 Priority : AdmissionPriorities)
	{
		const uint8* LaneMask = Lanes.Find(Priority);
		if (!LaneMask) continue;
		for (int32 Offset = 0; Offset < 4; ++Offset)
		{
			const int32 Kind = Offset == 0 ? 0 : (FirstCoarseKind + Offset - 2) % 3 + 1;
			if ((*LaneMask & (1 << Kind)) == 0) continue;
			const TArray<int32>& Lane = Interest.AdmissionLanes[Kind];
			const int32 First = Algo::LowerBound(Lane, FirstPendingAdmissions.FindChecked(Priority)[Kind]);
			const int32 End = Algo::UpperBoundBy(Lane, Priority,
				[this](const int32 Index) { return Admissions[Index].Priority; });
			if (First == End) continue;
			int32 RemainingLanes = 0;
			for (int32 NextOffset = Offset; NextOffset < 4; ++NextOffset)
			{
				const int32 NextKind = NextOffset == 0 ? 0 : (FirstCoarseKind + NextOffset - 2) % 3 + 1;
				RemainingLanes += (*LaneMask & (1 << NextKind)) != 0;
			}
			const double LaneDeadline = FPlatformTime::Seconds() +
				FMath::Max(0.0, Deadline - FPlatformTime::Seconds()) / RemainingLanes;
			const double Frontier = Admissions[Lane[First]].DistanceCells + AdmissionBandWidthCells;
			if (LastActiveAdmissionKind == 4)
			{
				LastActiveAdmissionKind = Kind;
				LastActiveAdmissionPriority = Priority;
				LastResolvedFrontier = Frontier;
			}
			int32& Scan = AdmissionScanIndices[Kind].FindOrAdd(Priority);
			if (Scan < First || Scan >= End) Scan = First;
			const int32 Attempts = FMath::Min(End - Scan, FMath::Max(64, Limits[Kind] * 8));
			for (int32 Attempt = 0; Attempt < Attempts && Scan < End &&
				Submitted[Kind] < Limits[Kind] && FPlatformTime::Seconds() < LaneDeadline; ++Attempt)
			{
				const FVoxelViewAdmission& Admission = Admissions[Lane[Scan]];
				if (Admission.DistanceCells > Frontier)
				{
					Scan = First;
					break;
				}
				++Scan;
				if (!IsAdmissionMeshReady(Admission) && !IsAdmissionTerminalFailure(Admission))
					Submitted[Kind] += TrySubmitAdmission(Admission) ? 1 : 0;
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
	Admissions = Module.GetCurrentInterest().Admissions;
	AdmissionPriorities.Reset();
	for (const FVoxelViewAdmission& Admission : Admissions)
	{
		if (AdmissionPriorities.IsEmpty() || AdmissionPriorities.Last() != Admission.Priority)
			AdmissionPriorities.Add(Admission.Priority);
	}
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
		return VoxelProxyReady.Contains(A.ProxyKey);
	case EVoxelViewAdmissionKind::Surface:
		return SurfaceReady.Contains(A.SurfaceKey);
	case EVoxelViewAdmissionKind::Macro:
		return MacroReady.Contains(A.MacroKey);
	default:
		return true;
	}
}

bool FVoxelViewManager::IsAdmissionTerminalFailure(const FVoxelViewAdmission& A) const
{
	if (A.Kind != EVoxelViewAdmissionKind::Fine) return false;
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(A.FineKey);
	return Section && Section->Status == EVoxelSectionStatus::Failed;
}

TMap<int32, double> FVoxelViewManager::GetDataAdmissionLimits() const
{
	TMap<int32, double> Limits;
	for (const FVoxelViewAdmission& Admission : Module.GetCurrentInterest().Admissions)
	{
		if (Admission.Kind == EVoxelViewAdmissionKind::Fine && !Limits.Contains(Admission.Priority) &&
			!IsAdmissionDataReady(Admission) && !IsAdmissionTerminalFailure(Admission))
			Limits.Add(Admission.Priority, Admission.DistanceCells + AdmissionBandWidthCells + ViewSectionSide * 2.0);
	}
	return Limits;
}

void FVoxelViewManager::RefreshTaskPriorities()
{
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
	for (const auto& Pair : Module.GetCurrentInterest().Exact)
	{
		if (Pair.Value.bMovementCriticalFine && !FineReady.Contains(Pair.Key))
		{
			const FVoxelSection* Section = Module.GetRuntime()->FindSection(Pair.Key);
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
			if (Revision && *Revision == Section->CommittedRevision)
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
		if (const uint64* Revision = VoxelProxyRevisions.Find(A.ProxyKey);
			VoxelProxyReady.Contains(A.ProxyKey) && Revision &&
			*Revision == Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(A.ProxyKey))
		{
			return false;
		}
		return RequestVoxelProxy(A.ProxyKey, A.Priority);
	case EVoxelViewAdmissionKind::Surface:
		if (const uint64* Revision = SurfaceRevisions.Find(A.SurfaceKey);
			SurfaceReady.Contains(A.SurfaceKey) && Revision &&
			*Revision == Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(A.SurfaceKey))
		{
			return false;
		}
		return RequestSurface(A.SurfaceKey, A.Priority);
	case EVoxelViewAdmissionKind::Macro:
		if (const uint64* Revision = MacroRevisions.Find(A.MacroKey);
			MacroReady.Contains(A.MacroKey) && Revision &&
			*Revision == Module.GetRuntime()->GetChangeHierarchy().GetMacroRevision(A.MacroKey))
		{
			return false;
		}
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
		case EVoxelTaskKind::BuildFineMesh: return !FineWanted.Contains(Stamp.Section);
		case EVoxelTaskKind::BuildVoxelProxy: return !VoxelProxyWanted.Contains(Stamp.ViewKey);
		case EVoxelTaskKind::BuildSurface:
		case EVoxelTaskKind::BuildWater: return !SurfaceWanted.Contains(Stamp.SurfaceKey);
		case EVoxelTaskKind::BuildMacro: return !MacroWanted.Contains(Stamp.MacroKey);
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
		FineActors.Num(),
		CountVisible(FineActors),
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
			if (AActor* Actor = FineActors.FindRef(Key);
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
			if (!Module.GetCurrentInterest().Exact.Contains(Neighbor)) continue;
			const FVoxelSection* Data = Module.GetRuntime()->FindSection(Neighbor);
			if (!Data || Data->Status != EVoxelSectionStatus::DataReady)
			{
				if (FineWaitingNeighbors < 4)
				{
					const FVoxelExactDemand& Demand = Module.GetCurrentInterest().Exact.FindChecked(Neighbor);
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
	int32 PendingVolumePlans = 0;
	for (const auto& Pair : VolumeCoveragePlans) PendingVolumePlans += Pair.Value.PendingSignature != 0 ? 1 : 0;
	UE_LOG(LogTemp, Display, TEXT("Voxel handoff waits: volumePlans=%d pendingPlans=%d volumeRects=%d heightfieldPlans=%d pendingHeightfields=%d heightfieldRects=%d preparedVolume=%d preparedHeightfield=%d publisherBusy=%d"),
		VolumeCoveragePlans.Num(), PendingVolumePlans, PendingVolumeHandoffBoxes.Num(), HeightfieldCoveragePlans.Num(),
		PendingHeightfieldPlanOwners.Num(), PendingHeightfieldHandoffRects.Num(), PreparedVolumeSignatures.Num(),
		PreparedTransitionSignatures.Num(), Publisher->IsBusy() ? 1 : 0);
	const FVoxelTaskDiagnostics SchedulerStats = Scheduler.GetDiagnostics();
	const FVoxelTerrainViewPlan& Plan = Module.GetCurrentInterest().TerrainPlan;
	UE_LOG(LogTemp, Display, TEXT("Voxel terrain plan: roots=%d leaves=%d required=%d presented=%d budgetLimited=%d overBudget=%d volumeTransitions=%d pending=%d unbalanced=%d"),
		Plan.Roots.Num(), Plan.Leaves.Num(), Plan.Required.Num(), VisibleTerrainNodes.Num(), Plan.bBudgetLimited, Plan.OverBudgetLeaves,
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
	const int32 Side = FVoxelSurfaceTileData::CellSide * (1 << InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	return { Min, Min + FIntPoint(Side, Side) };
}

FVoxelCoverageRect FVoxelViewManager::MacroCoverageRect(const FVoxelMacroTileKey& InKey) const
{
	const int32 Side = FVoxelMacroTileData::CellSide * (FVoxelMacroTileData::BaseStep << InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	return { Min, Min + FIntPoint(Side, Side) };
}

void FVoxelViewManager::GatherReadyWantedFineBoxes(TArray<FVoxelCoverageBox>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(FineReady.Num());
	for (const FIntVector& Key : FineReady)
	{
		if (FineWanted.Contains(Key)) OutCoverage.Add(FineCoverageBox(Key));
	}
}

void FVoxelViewManager::GatherReadyWantedProxyBoxes(TArray<FVoxelCoverageBox>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(VoxelProxyReady.Num());
	for (const FVoxelViewKey& Key : VoxelProxyReady)
	{
		if (VoxelProxyWanted.Contains(Key)) OutCoverage.Add(VoxelProxyCoverageBox(Key));
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
		if (!VoxelProxyWanted.Contains(Key) || !VoxelProxyCoverageRect(Key).Intersects(InTarget))
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
	TArray<FVoxelCoverageRect>& Coverage = ProxySurfaceCoverage.FindOrAdd(InData.Key);
	Coverage.Reset();
	const int32 Step = InData.Key.GetSampleStep();
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
	TArray<FVoxelCoverageBox> Coverage;
	GatherReadyWantedProxyBoxes(Coverage);
	if (VoxelCoverage::IsFullyCovered3D(FineCoverageBox(InKey), Coverage)) return true;
	TArray<FVoxelCoverageRect> SurfaceCoverage;
	GatherReadyWantedSurfaceRects(SurfaceCoverage);
	TArray<FVoxelCoverageRect> MacroCoverage;
	GatherReadyWantedMacroRects(MacroCoverage);
	SurfaceCoverage.Append(MacroCoverage);
	return VoxelCoverage::IsFullyCovered2D(FineCoverageRect(InKey), SurfaceCoverage);
}

bool FVoxelViewManager::IsProxyReplacementReady(const FVoxelViewKey& InKey) const
{
	TArray<FVoxelCoverageBox> FineCoverage;
	GatherReadyWantedFineBoxes(FineCoverage);
	if (VoxelCoverage::IsFullyCovered3D(VoxelProxyCoverageBox(InKey), FineCoverage)) return true;
	TArray<FVoxelCoverageRect> SurfaceCoverage;
	GatherReadyWantedSurfaceRects(SurfaceCoverage);
	TArray<FVoxelCoverageRect> MacroCoverage;
	GatherReadyWantedMacroRects(MacroCoverage);
	SurfaceCoverage.Append(MacroCoverage);
	return VoxelCoverage::IsFullyCovered2D(VoxelProxyCoverageRect(InKey), SurfaceCoverage);
}

bool FVoxelViewManager::IsSurfaceReplacementReady(const FVoxelSurfaceTileKey& InKey) const
{
	const FVoxelCoverageRect Target = SurfaceCoverageRect(InKey);
	TArray<FVoxelCoverageRect> MacroCoverage;
	GatherReadyWantedMacroRects(MacroCoverage);
	if (VoxelCoverage::IsFullyCovered2D(Target, MacroCoverage)) return true;

	TArray<FVoxelCoverageRect> ProxyCoverage;
	GatherReadyWantedProxySurfaceRects(Target, ProxyCoverage);
	return VoxelCoverage::IsFullyCovered2D(Target, ProxyCoverage);
}

bool FVoxelViewManager::IsMacroReplacementReady(const FVoxelMacroTileKey& InKey) const
{
	TArray<FVoxelCoverageRect> SurfaceCoverage;
	GatherReadyWantedSurfaceRects(SurfaceCoverage);
	return VoxelCoverage::IsFullyCovered2D(MacroCoverageRect(InKey), SurfaceCoverage);
}

bool FVoxelViewManager::IsFineInsideRenderDomain(const FIntVector& InKey) const
{
	TArray<FVoxelCoverageRect> Domain;
	GatherCurrentRenderDomainRects(Domain);
	return VoxelCoverage::IntersectsAny2D(FineCoverageRect(InKey), Domain);
}

bool FVoxelViewManager::IsProxyInsideRenderDomain(const FVoxelViewKey& InKey) const
{
	TArray<FVoxelCoverageRect> Domain;
	GatherCurrentRenderDomainRects(Domain);
	return VoxelCoverage::IntersectsAny2D(VoxelProxyCoverageRect(InKey), Domain);
}

bool FVoxelViewManager::IsSurfaceInsideRenderDomain(const FVoxelSurfaceTileKey& InKey) const
{
	TArray<FVoxelCoverageRect> Domain;
	GatherCurrentRenderDomainRects(Domain);
	return VoxelCoverage::IntersectsAny2D(SurfaceCoverageRect(InKey), Domain);
}

bool FVoxelViewManager::IsMacroInsideRenderDomain(const FVoxelMacroTileKey& InKey) const
{
	TArray<FVoxelCoverageRect> Domain;
	GatherCurrentRenderDomainRects(Domain);
	return VoxelCoverage::IntersectsAny2D(MacroCoverageRect(InKey), Domain);
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
	MergeCoverageRect(DirtyCoverageRects, InBounds);
	if (DirtyCoverageRects.Num() > 128) MarkCoverageDirty();
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
	if (ActiveDirtyCoverageRects.Num() > 128) bActiveCoverageFull = true;
}

void FVoxelViewManager::ApplyInterestDelta(const FVoxelInterestDelta& InDelta)
{
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
	if (DirtyCoverageRects.Num() > 128) MarkCoverageDirty();
}

FGuid FVoxelViewManager::BeginPublishGroup(const FVoxelPublishGroupKey& InKey)
{
	if (const FGuid* Existing = OpenPublishGroups.Find(InKey))
	{
		return *Existing;
	}
	const FGuid Group = Publisher->BeginGroup(InKey);
	OpenPublishGroups.Add(InKey, Group);
	return Group;
}

void FVoxelViewManager::ResolveTransitionVisibility()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewHandoff);
	const double ResolveStart = FPlatformTime::Seconds();
	const FVoxelTerrainViewPlan& Plan = Module.GetCurrentInterest().TerrainPlan;
	const bool bUsesTerrainPlan = !Plan.Roots.IsEmpty();
	TSet<FVoxelViewKey> TargetTerrainNodes;
	if (bUsesTerrainPlan)
	{
		const TSet<FVoxelViewKey>& Previous = VisibleTerrainNodes;
		uint8 RootLevel = 0;
		for (const FVoxelViewKey& Root : Plan.Roots) RootLevel = FMath::Max(RootLevel, Root.Level);
		TSet<FVoxelViewKey> ResolveRoots;
		TSet<FVoxelViewKey> PreviousScope;
		if (!bActiveCoverageFull)
		{
			for (const FVoxelViewKey& Root : Plan.Roots)
				if (CoverageAffects(VoxelProxyCoverageRect(Root))) ResolveRoots.Add(Root);
			// 完整根分区一起解析，保留兄弟交接；其余根复用已提交的空间所有权。
			for (const FVoxelViewKey& Root : ResolveRoots)
				MergeCoverageRect(ActiveDirtyCoverageRects, VoxelProxyCoverageRect(Root));
			for (const FVoxelViewKey& Key : Previous)
			{
				FVoxelViewKey Root = Key;
				while (Root.Level < RootLevel && !Plan.Roots.Contains(Root)) Root = Root.GetParent();
				if (ResolveRoots.Contains(Root) || (!Plan.Roots.Contains(Root) &&
					CoverageAffects(Key.Level == 0 ? FineCoverageRect(Key.Coordinate) : VoxelProxyCoverageRect(Key))))
					PreviousScope.Add(Key);
			}
		}
		if (bReadyTerrainBranchesDirty) RebuildReadyTerrainBranches();
		auto IsReady = [this](const FVoxelViewKey& Key)
		{
			return Key.Level == 0 ? FineReady.Contains(Key.Coordinate) : VoxelProxyReady.Contains(Key);
		};
		if (bActiveCoverageFull)
		{
			Plan.ResolveVisible(IsReady, TargetTerrainNodes, &Previous, nullptr, &ReadyTerrainBranches);
		}
		else
		{
			Plan.ResolveVisible(IsReady, TargetTerrainNodes, &PreviousScope, nullptr, &ReadyTerrainBranches, &ResolveRoots);
			for (const FVoxelViewKey& Key : Previous)
				if (!PreviousScope.Contains(Key)) TargetTerrainNodes.Add(Key);
		}
		const double TreeMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0;
		TArray<FVoxelCoverageRect> OutsideTerrainDomain;
		TArray<FVoxelCoverageRect> ReadyOutsideCoverage;
		TArray<FVoxelCoverageBox> ReadyFineCoverage;
		TArray<FVoxelCoverageBox> ReadyProxyCoverage;
		bool bOutsideTerrainDomainBuilt = false;
		for (const FVoxelViewKey& Key : Previous)
		{
			if (!bActiveCoverageFull && !PreviousScope.Contains(Key)) continue;
			FVoxelViewKey Root = Key;
			while (Root.Level < RootLevel && !Plan.Roots.Contains(Root))
			{
				Root = Root.GetParent();
			}
			if (Plan.Roots.Contains(Root))
			{
				continue;
			}
			if (!bOutsideTerrainDomainBuilt)
			{
				OutsideTerrainDomain.Reserve(SurfaceWanted.Num() + MacroWanted.Num());
				for (const FVoxelSurfaceTileKey& SurfaceKey : SurfaceWanted)
				{
					OutsideTerrainDomain.Add(SurfaceCoverageRect(SurfaceKey));
				}
				for (const FVoxelMacroTileKey& MacroKey : MacroWanted)
				{
					OutsideTerrainDomain.Add(MacroCoverageRect(MacroKey));
				}
				GatherReadyWantedSurfaceRects(ReadyOutsideCoverage);
				TArray<FVoxelCoverageRect> ReadyMacroCoverage;
				GatherReadyWantedMacroRects(ReadyMacroCoverage);
				ReadyOutsideCoverage.Append(ReadyMacroCoverage);
				GatherReadyWantedFineBoxes(ReadyFineCoverage);
				GatherReadyWantedProxyBoxes(ReadyProxyCoverage);
				bOutsideTerrainDomainBuilt = true;
			}
			const FVoxelCoverageRect PreviousRect = Key.Level == 0
				? FineCoverageRect(Key.Coordinate) : VoxelProxyCoverageRect(Key);
			if (!VoxelCoverage::IntersectsAny2D(PreviousRect, OutsideTerrainDomain))
			{
				continue;
			}
			const bool bVolumeReplacementReady = Key.Level == 0
				? VoxelCoverage::IsFullyCovered3D(FineCoverageBox(Key.Coordinate), ReadyProxyCoverage)
				: VoxelCoverage::IsFullyCovered3D(VoxelProxyCoverageBox(Key), ReadyFineCoverage);
			const bool bRetain = !bVolumeReplacementReady &&
				!VoxelCoverage::IsFullyCovered2D(PreviousRect, ReadyOutsideCoverage);
			if (bRetain)
			{
				TargetTerrainNodes.Add(Key);
			}
		}
		static double NextPlanTimingLog = 0.0;
		const double PlanNow = FPlatformTime::Seconds();
		if (CVarVoxelDebugRepresentation.GetValueOnGameThread() > 0 && PlanNow >= NextPlanTimingLog)
		{
			UE_LOG(LogTemp, Display, TEXT("Voxel plan phases ms: tree=%.3f retain=%.3f previous=%d visible=%d"),
				TreeMilliseconds, (PlanNow - ResolveStart) * 1000.0 - TreeMilliseconds,
				Previous.Num(), TargetTerrainNodes.Num());
			NextPlanTimingLog = PlanNow + 5.0;
		}
	}
#if !UE_BUILD_SHIPPING
	static double NextPointHandoffLog = 0.0;
	TArray<FVoxelViewKey, TInlineAllocator<2>> PointHandoffKeys;
	TSet<FVoxelViewKey> PointTreeTargets;
	if (FPlatformTime::Seconds() >= NextPointHandoffLog)
	{
		for (const auto& Pair : Module.GetCurrentInterest().Sources)
		{
			const auto& Source = Pair.Value.Source;
			if (!Source.bLocalView || !Source.Has(EVoxelStreamingCapability::FineVisual) ||
				Source.Has(EVoxelStreamingCapability::LocalRefinement)) continue;
			const auto AddPoint = [&](const FIntVector& Cell)
			{
				PointHandoffKeys.AddUnique({FIntVector(VoxelGeneration::FloorDivide(Cell.X, 16),
					VoxelGeneration::FloorDivide(Cell.Y, 16), VoxelGeneration::FloorDivide(Cell.Z, 16)), 0});
			};
			AddPoint(Source.Center);
			FVoxelNaturalColumnEntryPtr Column;
			if (Module.GetGenerationCache() && Module.GetGenerationCache()->FindNaturalColumn(
				FIntPoint(Source.Center.X, Source.Center.Y), Column))
				AddPoint(FIntVector(Source.Center.X, Source.Center.Y, Column->Column.SurfaceZ));
			break;
		}
		for (const auto& Key : PointHandoffKeys)
			if (TargetTerrainNodes.Contains(Key)) PointTreeTargets.Add(Key);
		NextPointHandoffLog = FPlatformTime::Seconds() + 5.0;
	}
#endif
	if (bUsesTerrainPlan) RebuildVolumeTransitions(TargetTerrainNodes);
	else PendingVolumeHandoffBoxes.Reset();
	TMap<FVoxelViewKey, uint64> VolumeToCommit;
	if (!bActiveCoverageFull)
	{
		auto TerrainRect = [this](const FVoxelViewKey& Key)
		{
			return Key.Level == 0
				? FineCoverageRect(Key.Coordinate)
				: VoxelProxyCoverageRect(Key);
		};
		for (const FVoxelViewKey& Key : TargetTerrainNodes)
		{
			if (!VisibleTerrainNodes.Contains(Key) && !CoverageAffects(TerrainRect(Key)))
			{
				ExpandActiveCoverage(TerrainRect(Key));
			}
		}
		if (!bActiveCoverageFull)
		{
			for (const FVoxelViewKey& Key : VisibleTerrainNodes)
			{
				if (!TargetTerrainNodes.Contains(Key) && !CoverageAffects(TerrainRect(Key)))
				{
					ExpandActiveCoverage(TerrainRect(Key));
				}
			}
		}
		if (!bActiveCoverageFull)
		{
			for (const auto& Pair : DesiredVolumeSignatures)
			{
				const uint64* Built = VolumeTransitionSignatures.Find(Pair.Key);
				if ((!Built || *Built != Pair.Value) &&
					!CoverageAffects(VoxelProxyCoverageRect(Pair.Key)))
				{
					ExpandActiveCoverage(VoxelProxyCoverageRect(Pair.Key));
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
	RebuildHeightfieldTransitions();
	if (!bActiveCoverageFull)
	{
		for (const auto& Pair : DesiredTransitionSignatures)
		{
			const uint64* Built = HeightfieldTransitionSignatures.Find(Pair.Key);
			if (Built && *Built == Pair.Value) continue;
			const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
				? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
				: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
			if (!CoverageAffects(Rect))
			{
				ExpandActiveCoverage(Rect);
			}
		}
		if (!bActiveCoverageFull)
		{
			for (const auto& Pair : HeightfieldTransitionSignatures)
			{
				if (DesiredTransitionSignatures.Contains(Pair.Key)) continue;
				const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
					? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
					: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
				if (!CoverageAffects(Rect))
				{
					ExpandActiveCoverage(Rect);
				}
			}
		}
	}
	TMap<FVoxelHeightfieldNodeKey, uint64> HeightfieldToCommit;
	for (const auto& Pair : PreparedTransitionSignatures)
	{
		const uint64* Desired = DesiredTransitionSignatures.Find(Pair.Key);
		const auto Mesh = PreparedTransitionMeshes.FindRef(Pair.Key);
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
			? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level}) : MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (!Desired || *Desired != Pair.Value || !Mesh || PendingHeightfieldPlanOwners.Contains(Pair.Key) ||
			VolumeHandoffAffectsHeightfield(Pair.Key)) continue;
		const int32 TileSide = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
			? FVoxelSurfaceTileData::CellSide * (1 << Pair.Key.Level)
			: FVoxelMacroTileData::CellSide * (FVoxelMacroTileData::BaseStep << Pair.Key.Level);
		AActor* Host = HeightfieldTransitionActors.FindRef(Pair.Key);
		if (!Publisher->Stage(BeginPublishGroup(
			{static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5),
				Pair.Key.Coordinate, Pair.Key.Level}), Host,
			FVector(Pair.Key.Coordinate.X * TileSide, Pair.Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(),
			Module.BlockSize(), FVoxelSectionMeshResult(*Mesh),
			Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 2 : 3, true))
		{
			MarkCoverageDirty();
			continue;
		}
		HeightfieldTransitionActors.Add(Pair.Key, Host);
		Publisher->SetCoverage(BeginPublishGroup(
			{static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5),
				Pair.Key.Coordinate, Pair.Key.Level}), Host, {});
		HeightfieldToCommit.Add(Pair.Key, Pair.Value);
	}
	TSet<FVoxelHeightfieldNodeKey> BlockedHeightfieldNodes = PendingHeightfieldPlanOwners;
	for (const auto& Pair : HeightfieldCoveragePlans)
	{
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
			? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level}) : MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (VolumeHandoffAffectsHeightfield(Pair.Key)) BlockedHeightfieldNodes.Add(Pair.Key);
	}
	for (const auto& Pair : DesiredTransitionEdges)
	{
		const uint64* Desired = DesiredTransitionSignatures.Find(Pair.Key);
		const uint64* Built = HeightfieldTransitionSignatures.Find(Pair.Key);
		AActor* Host = HeightfieldTransitionActors.FindRef(Pair.Key);
		const uint64* Staged = HeightfieldToCommit.Find(Pair.Key);
		const bool bReady = Desired &&
			((Built && *Built == *Desired && (!Host || Publisher->HasPresentation(Host))) ||
				(Staged && *Staged == *Desired));
		if (bReady) continue;
		BlockedHeightfieldNodes.Add(Pair.Key);
		for (const FVoxelHeightfieldTransitionEdge& Edge : Pair.Value)
		{
			if (!Edge.bPending) BlockedHeightfieldNodes.Add(Edge.Neighbor);
		}
	}
	PendingHeightfieldHandoffRects.Reset();
	for (const FVoxelHeightfieldNodeKey& Key : BlockedHeightfieldNodes)
	{
		if (Key.Representation == EVoxelHeightfieldRepresentation::Surface)
		{
			const FVoxelSurfaceTileKey Tile{Key.Coordinate, Key.Level};
			if (SurfaceWanted.Contains(Tile)) PendingHeightfieldHandoffRects.Add(SurfaceCoverageRect(Tile));
		}
		else
		{
			const FVoxelMacroTileKey Tile{Key.Coordinate, Key.Level};
			if (MacroWanted.Contains(Tile)) PendingHeightfieldHandoffRects.Add(MacroCoverageRect(Tile));
		}
	}
	for (const auto& Pair : PreparedVolumeSignatures)
	{
		const uint64* Desired = DesiredVolumeSignatures.Find(Pair.Key);
		const auto Mesh = PreparedVolumeMeshes.FindRef(Pair.Key);
		if (!Desired || *Desired != Pair.Value || !Mesh || !TargetTerrainNodes.Contains(Pair.Key) ||
			VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Pair.Key), PendingVolumeHandoffBoxes)) continue;
		AActor* Host = VoxelProxyActors.FindRef(Pair.Key);
		if (HasRenderableMesh(*Mesh) || Host)
		{
			if (!Publisher->Stage(BeginPublishGroup({1, FIntPoint(Pair.Key.Coordinate.X, Pair.Key.Coordinate.Y), Pair.Key.Level}), Host,
				FVector(Pair.Key.GetBounds().Min) * Module.BlockSize(), Module.BlockSize() * Pair.Key.GetSampleStep(),
				FVoxelSectionMeshResult(*Mesh), VolumeTransitionWorkClass(Pair.Key) <= EVoxelWorkClass::Interactive ? 0 : 1))
			{
				PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Pair.Key));
				MarkCoverageDirty(VoxelProxyCoverageRect(Pair.Key));
				continue;
			}
			VoxelProxyActors.Add(Pair.Key, Host);
		}
		VolumeToCommit.Add(Pair.Key, Pair.Value);
	}
	for (auto It = TargetTerrainNodes.CreateIterator(); It; ++It)
	{
		const FVoxelCoverageBox Bounds = It->Level == 0 ? FineCoverageBox(It->Coordinate) : VoxelProxyCoverageBox(*It);
		if (VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes)) It.RemoveCurrent();
	}
	for (const auto& Key : VisibleTerrainNodes)
	{
		const FVoxelCoverageBox Bounds = Key.Level == 0 ? FineCoverageBox(Key.Coordinate) : VoxelProxyCoverageBox(Key);
		if (VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes)) TargetTerrainNodes.Add(Key);
	}
#if !UE_BUILD_SHIPPING
	for (const auto& Key : PointHandoffKeys)
	{
		AActor* Actor = FineActors.FindRef(Key.Coordinate);
		UE_LOG(LogTemp, Display,
			TEXT("Voxel point handoff: section=%s ready=%d desired=%d tree=%d volumeBlocked=%d target=%d owned=%d actor=%d hidden=%d committed=%d groupBusy=%d"),
			*Key.Coordinate.ToString(), FineReady.Contains(Key.Coordinate), Plan.Leaves.Contains(Key),
			PointTreeTargets.Contains(Key), VoxelCoverage::IntersectsAny3D(FineCoverageBox(Key.Coordinate), PendingVolumeHandoffBoxes),
			TargetTerrainNodes.Contains(Key), VisibleTerrainNodes.Contains(Key), Actor != nullptr,
			Actor && Actor->IsHidden(), Actor && Publisher->IsCommitted(Actor),
			Publisher->IsGroupBusy({0, FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), 0}));
	}
#endif
	auto HandoffPending = [this](const FVoxelCoverageRect& Rect)
	{
		return VoxelCoverage::IntersectsAny2D(Rect, PendingHeightfieldHandoffRects);
	};
	TArray<FVoxelCoverageRect> RecheckCoverageRects;
	for (const auto& Pair : VolumeToCommit)
	{
		RecheckCoverageRects.Add(VoxelProxyCoverageRect(Pair.Key));
	}
	for (const auto& Pair : HeightfieldToCommit)
	{
		RecheckCoverageRects.Add(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
			? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
			: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level}));
	}
	// 裁剪与实际保留的体素分区一致，不能先裁掉仍在等待体素接缝的上一代覆盖。
	if (bUsesTerrainPlan && bNeedsExclusions)
	{
		for (const FVoxelViewKey& Key : TargetTerrainNodes)
		{
			if (Key.Level == 0)
			{
				FineBoxes.Add(Box(FineCoverageBox(Key.Coordinate)));
			}
			else
			{
				ProxyBoxes.Add(Box(VoxelProxyCoverageBox(Key)));
				if (const TArray<FVoxelCoverageRect>* Rects = ProxySurfaceCoverage.Find(Key))
				{
					const FVoxelCoverageBox Bounds = VoxelProxyCoverageBox(Key);
					for (const FVoxelCoverageRect& Rect : *Rects)
						ProxySurfaceBoxes.Add(FBox(FVector(Rect.Min.X, Rect.Min.Y, Bounds.Min.Z),
							FVector(Rect.Max.X, Rect.Max.Y, Bounds.Max.Z)));
				}
			}
		}
	}
	else if (!bUsesTerrainPlan)
	{
		for (const FIntVector& Key : FineReady)
			if (FineWanted.Contains(Key)) FineBoxes.Add(Box(FineCoverageBox(Key)));
		for (const auto& Pair : FineActors)
			if (FineWanted.Contains(Pair.Key) && !FineReady.Contains(Pair.Key) && Publisher->IsPresented(Pair.Value))
				FineBoxes.Add(Box(FineCoverageBox(Pair.Key)));
	}
	VoxelMeshClipper::NormalizeBoxes(FineBoxes);
	VoxelMeshClipper::NormalizeBoxes(ProxyBoxes);
	VoxelMeshClipper::NormalizeBoxes(ProxySurfaceBoxes);
	const auto CommonHeightfieldBoxes = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>(FineBoxes);
	CommonHeightfieldBoxes->Append(ProxySurfaceBoxes);
	const auto CommonTransitionBoxes = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>();
	for (FBox Bounds : *CommonHeightfieldBoxes)
	{
		Bounds.Min.Z = -1.e12;
		Bounds.Max.Z = 1.e12;
		CommonTransitionBoxes->Add(Bounds);
	}
	VoxelMeshClipper::NormalizeBoxes(*CommonTransitionBoxes);
	const double BoxesMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0 - PlanMilliseconds;
	for (const TPair<FIntVector, TObjectPtr<AActor>>& Pair : FineActors)
	{
		if (!Pair.Value) continue;
		if (!CoverageAffects(FineCoverageRect(Pair.Key))) continue;
		if (bUsesTerrainPlan && Pair.Value->IsHidden() == !TargetTerrainNodes.Contains({Pair.Key, 0}) &&
			Publisher->IsCommitted(Pair.Value) &&
			!Publisher->IsGroupBusy({0, FIntPoint(Pair.Key.X, Pair.Key.Y), 0}))
			continue;
		const FGuid Group = BeginPublishGroup({0, FIntPoint(Pair.Key.X, Pair.Key.Y), 0});
		if (VoxelCoverage::IntersectsAny3D(FineCoverageBox(Pair.Key), PendingVolumeHandoffBoxes))
		{
			if (!Publisher->HasPresentation(Pair.Value)) Publisher->SetHidden(Group, Pair.Value, true);
			continue;
		}
		if (bUsesTerrainPlan)
		{
			const bool bVisible = TargetTerrainNodes.Contains({ Pair.Key, 0 });
			if (bVisible && CoverageAffects(FineCoverageRect(Pair.Key)))
			{
				Publisher->SetCoverage(Group, Pair.Value, {});
			}
			Publisher->SetHidden(Group, Pair.Value, !bVisible);
			continue;
		}
		const bool bWanted = FineWanted.Contains(Pair.Key);
		const bool bHidden = !bWanted && IsFineReplacementReady(Pair.Key);
		if (!CoverageAffects(FineCoverageRect(Pair.Key)))
		{
			Publisher->SetHidden(Group, Pair.Value, bHidden);
			continue;
		}
		TArray<FBox> Exclusions;
		if (!bWanted)
		{
			Exclusions = ProxyBoxes;
			for (const FVoxelSurfaceTileKey& Key : SurfaceReady)
			{
				if (SurfaceWanted.Contains(Key)) Exclusions.Add(Column(SurfaceCoverageRect(Key)));
			}
			for (const FVoxelMacroTileKey& Key : MacroReady)
			{
				if (MacroWanted.Contains(Key)) Exclusions.Add(Column(MacroCoverageRect(Key)));
			}
		}
		if (!bHidden)
			Publisher->SetCoverage(Group, Pair.Value, MoveTemp(Exclusions));
		Publisher->SetHidden(Group, Pair.Value, bHidden);
	}
	for (const TPair<FVoxelViewKey, TObjectPtr<AActor>>& Pair : VoxelProxyActors)
	{
		if (!Pair.Value) continue;
		if (!CoverageAffects(VoxelProxyCoverageRect(Pair.Key))) continue;
		if (bUsesTerrainPlan && Pair.Value->IsHidden() == !TargetTerrainNodes.Contains(Pair.Key) &&
			Publisher->IsCommitted(Pair.Value) &&
			!Publisher->IsGroupBusy({1, FIntPoint(Pair.Key.Coordinate.X, Pair.Key.Coordinate.Y), Pair.Key.Level}))
			continue;
		const FGuid Group = BeginPublishGroup(
			{1, FIntPoint(Pair.Key.Coordinate.X, Pair.Key.Coordinate.Y), Pair.Key.Level});
		if (VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Pair.Key), PendingVolumeHandoffBoxes))
		{
			if (!Publisher->HasPresentation(Pair.Value)) Publisher->SetHidden(Group, Pair.Value, true);
			continue;
		}
		if (bUsesTerrainPlan)
		{
			const bool bVisible = TargetTerrainNodes.Contains(Pair.Key);
			if (bVisible && CoverageAffects(VoxelProxyCoverageRect(Pair.Key)))
			{
				Publisher->SetCoverage(Group, Pair.Value, {});
			}
			Publisher->SetHidden(Group, Pair.Value, !bVisible);
			continue;
		}
		const bool bWanted = VoxelProxyWanted.Contains(Pair.Key);
		const bool bHidden = !bWanted && IsProxyReplacementReady(Pair.Key);
		if (!CoverageAffects(VoxelProxyCoverageRect(Pair.Key)))
		{
			Publisher->SetHidden(Group, Pair.Value, bHidden);
			continue;
		}
		TArray<FBox> Exclusions = FineBoxes;
		for (const FVoxelViewKey& Key : VoxelProxyReady)
		{
			if (VoxelProxyWanted.Contains(Key) && Key != Pair.Key && (!bWanted || Key.Level < Pair.Key.Level))
				Exclusions.Add(Box(VoxelProxyCoverageBox(Key)));
		}
		if (!bWanted)
		{
			for (const FVoxelSurfaceTileKey& Key : SurfaceReady)
			{
				if (SurfaceWanted.Contains(Key)) Exclusions.Add(Column(SurfaceCoverageRect(Key)));
			}
			for (const FVoxelMacroTileKey& Key : MacroReady)
			{
				if (MacroWanted.Contains(Key)) Exclusions.Add(Column(MacroCoverageRect(Key)));
			}
		}
		if (!bHidden)
			Publisher->SetCoverage(Group, Pair.Value, MoveTemp(Exclusions));
		Publisher->SetHidden(Group, Pair.Value, bHidden);
	}
	for (const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>>& Pair : SurfaceActors)
	{
		if (!Pair.Value) continue;
		if (!CoverageAffects(SurfaceCoverageRect(Pair.Key))) continue;
		const FGuid Group = BeginPublishGroup({2, Pair.Key.Coordinate, Pair.Key.Level});
		if (BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface,
			Pair.Key.Coordinate, Pair.Key.Level}) ||
			(!SurfaceWanted.Contains(Pair.Key) && HandoffPending(SurfaceCoverageRect(Pair.Key))))
		{
			if (!Publisher->HasPresentation(Pair.Value)) Publisher->SetHidden(Group, Pair.Value, true);
			else Publisher->SetSharedCoverage(Group, Pair.Value, CommonHeightfieldBoxes);
			if (AActor* Water = WaterActors.FindRef(Pair.Key))
				Publisher->SetSharedCoverage(Group, Water, CommonHeightfieldBoxes);
			continue;
		}
		const bool bWanted = SurfaceWanted.Contains(Pair.Key);
		const bool bHidden = !bWanted && IsSurfaceReplacementReady(Pair.Key);
		const FVoxelCoverageRect ActorRect = SurfaceCoverageRect(Pair.Key);
		if (!CoverageAffects(ActorRect))
		{
			Publisher->SetHidden(Group, Pair.Value, bHidden);
			if (AActor* Water = WaterActors.FindRef(Pair.Key)) Publisher->SetHidden(Group, Water, bHidden);
			continue;
		}
		TArray<FBox> Exclusions;
		for (const FVoxelSurfaceTileKey& Key : SurfaceReady)
		{
			if (SurfaceWanted.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface,
					Key.Coordinate, Key.Level}) &&
				Key != Pair.Key && (!bWanted || Key.Level < Pair.Key.Level))
				if (ActorRect.Intersects(SurfaceCoverageRect(Key))) Exclusions.Add(Column(SurfaceCoverageRect(Key)));
		}
		if (!bWanted)
		{
			for (const FVoxelMacroTileKey& Key : MacroReady)
			{
				if (MacroWanted.Contains(Key) &&
					!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro,
						Key.Coordinate, Key.Level})) if (ActorRect.Intersects(MacroCoverageRect(Key))) Exclusions.Add(Column(MacroCoverageRect(Key)));
			}
		}
		if (!bHidden)
		{
			if (!Publisher->IsCommitted(Pair.Value)) RecheckCoverageRects.Add(ActorRect);
			Publisher->SetCoverage(Group, Pair.Value, Exclusions, CommonHeightfieldBoxes);
			if (AActor* Water = WaterActors.FindRef(Pair.Key)) Publisher->SetCoverage(Group, Water, MoveTemp(Exclusions), CommonHeightfieldBoxes);
		}
		Publisher->SetHidden(Group, Pair.Value, bHidden);
		if (AActor* Water = WaterActors.FindRef(Pair.Key)) Publisher->SetHidden(Group, Water, bHidden);
	}
	for (const TPair<FVoxelMacroTileKey, TObjectPtr<AActor>>& Pair : MacroActors)
	{
		if (!Pair.Value) continue;
		if (!CoverageAffects(MacroCoverageRect(Pair.Key))) continue;
		const FGuid Group = BeginPublishGroup({3, Pair.Key.Coordinate, Pair.Key.Level});
		if (BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro,
			Pair.Key.Coordinate, Pair.Key.Level}) ||
			(!MacroWanted.Contains(Pair.Key) && HandoffPending(MacroCoverageRect(Pair.Key))))
		{
			if (!Publisher->HasPresentation(Pair.Value)) Publisher->SetHidden(Group, Pair.Value, true);
			else Publisher->SetSharedCoverage(Group, Pair.Value, CommonHeightfieldBoxes);
			continue;
		}
		const bool bWanted = MacroWanted.Contains(Pair.Key);
		const bool bHidden = !bWanted && IsMacroReplacementReady(Pair.Key);
		const FVoxelCoverageRect ActorRect = MacroCoverageRect(Pair.Key);
		if (!CoverageAffects(ActorRect))
		{
			Publisher->SetHidden(Group, Pair.Value, bHidden);
			continue;
		}
		TArray<FBox> Exclusions;
		for (const FVoxelSurfaceTileKey& Key : SurfaceReady)
		{
			if (SurfaceWanted.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface,
					Key.Coordinate, Key.Level})) if (ActorRect.Intersects(SurfaceCoverageRect(Key))) Exclusions.Add(Column(SurfaceCoverageRect(Key)));
		}
		for (const FVoxelMacroTileKey& Key : MacroReady)
		{
			if (MacroWanted.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro,
					Key.Coordinate, Key.Level}) &&
				Key != Pair.Key && (!bWanted || Key.Level < Pair.Key.Level))
				if (ActorRect.Intersects(MacroCoverageRect(Key))) Exclusions.Add(Column(MacroCoverageRect(Key)));
		}
		if (!bHidden)
		{
			if (!Publisher->IsCommitted(Pair.Value)) RecheckCoverageRects.Add(ActorRect);
			Publisher->SetCoverage(Group, Pair.Value, MoveTemp(Exclusions), CommonHeightfieldBoxes);
		}
		Publisher->SetHidden(Group, Pair.Value, bHidden);
	}
	for (const TPair<FVoxelHeightfieldNodeKey, TObjectPtr<AActor>>& Pair : HeightfieldTransitionActors)
	{
		if (!Pair.Value) continue;
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
			? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
			: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (!CoverageAffects(Rect)) continue;
		const FGuid Group = BeginPublishGroup(
			{static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5),
				Pair.Key.Coordinate, Pair.Key.Level});
		if (Publisher->HasPresentation(Pair.Value) &&
			(BlockedHeightfieldNodes.Contains(Pair.Key) || HandoffPending(Rect)))
		{
			Publisher->SetSharedCoverage(Group, Pair.Value, CommonTransitionBoxes);
			HeightfieldToCommit.Remove(Pair.Key);
			continue;
		}
		Publisher->SetCoverage(Group, Pair.Value, {}, CommonTransitionBoxes);
		Publisher->SetHidden(Group, Pair.Value, !DesiredTransitionSignatures.Contains(Pair.Key) ||
			BlockedHeightfieldNodes.Contains(Pair.Key));
	}
	const double ActorsMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0 - PlanMilliseconds - BoxesMilliseconds;
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> PreviousNodesByGroup;
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> TargetNodesByGroup;
	auto TerrainGroupKey = [](const FVoxelViewKey& Key)
	{
		return FVoxelPublishGroupKey{
			static_cast<uint8>(Key.Level == 0 ? 0 : 1),
			FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level};
	};
	for (const FVoxelViewKey& Key : VisibleTerrainNodes)
	{
		if (!TargetTerrainNodes.Contains(Key)) BeginPublishGroup(TerrainGroupKey(Key));
		if (OpenPublishGroups.Contains(TerrainGroupKey(Key))) PreviousNodesByGroup.FindOrAdd(TerrainGroupKey(Key)).Add(Key);
	}
	for (const FVoxelViewKey& Key : TargetTerrainNodes)
	{
		// 空 section 的空间所有权同样按局部分组交接，不能等待全局最终回调。
		if (!VisibleTerrainNodes.Contains(Key)) BeginPublishGroup(TerrainGroupKey(Key));
		if (OpenPublishGroups.Contains(TerrainGroupKey(Key))) TargetNodesByGroup.FindOrAdd(TerrainGroupKey(Key)).Add(Key);
	}
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> VolumeByGroup;
	for (const auto& Pair : VolumeToCommit)
	{
		VolumeByGroup.FindOrAdd(TerrainGroupKey(Pair.Key)).Add(Pair.Key);
	}
	TMap<FVoxelPublishGroupKey, TArray<FVoxelHeightfieldNodeKey>> HeightfieldByGroup;
	for (const auto& Pair : HeightfieldToCommit)
	{
		const FVoxelHeightfieldNodeKey& Key = Pair.Key;
		const uint8 Representation = Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5;
		HeightfieldByGroup.FindOrAdd({Representation, Key.Coordinate, Key.Level}).Add(Key);
	}
	TSet<FVoxelPublishGroupKey> OwnershipChanges;
	for (const auto& Pair : OpenPublishGroups)
	{
		const auto* Previous = PreviousNodesByGroup.Find(Pair.Key);
		const auto* Target = TargetNodesByGroup.Find(Pair.Key);
		bool bChanged = (Previous ? Previous->Num() : 0) != (Target ? Target->Num() : 0);
		if (!bChanged && Previous && Target)
			for (const auto& Node : *Previous) if (!Target->Contains(Node)) { bChanged = true; break; }
		if (bChanged || VolumeByGroup.Contains(Pair.Key) || HeightfieldByGroup.Contains(Pair.Key))
			OwnershipChanges.Add(Pair.Key);
	}
	TFunction<void()> OnPublishCommitted = [this, Recheck = MoveTemp(RecheckCoverageRects)]()
	{
		for (auto It = HeightfieldTransitionActors.CreateIterator(); It; ++It)
		{
			if (DesiredTransitionSignatures.Contains(It.Key())) continue;
			const FVoxelCoverageRect Rect = It.Key().Representation == EVoxelHeightfieldRepresentation::Surface
				? SurfaceCoverageRect({It.Key().Coordinate, It.Key().Level})
				: MacroCoverageRect({It.Key().Coordinate, It.Key().Level});
			if (VoxelCoverage::IntersectsAny2D(Rect,
				PendingHeightfieldHandoffRects)) continue;
			if (AActor* Actor = It.Value())
			{
				Publisher->Forget(Actor);
				Actor->Destroy();
			}
			HeightfieldTransitionSignatures.Remove(It.Key());
			It.RemoveCurrent();
		}
		for (const FVoxelCoverageRect& Rect : Recheck) MarkCoverageDirty(Rect);
	};
	TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)> OnGroupCommitted =
		[this, Target = MoveTemp(TargetNodesByGroup), Volume = MoveTemp(VolumeByGroup),
		Heightfield = MoveTemp(HeightfieldByGroup), VolumeSignatures = MoveTemp(VolumeToCommit),
		HeightfieldSignatures = MoveTemp(HeightfieldToCommit)](
		TConstArrayView<FVoxelPublishGroupKey> Keys)
	{
		for (const FVoxelPublishGroupKey& Key : Keys)
		{
			const auto* Desired = Target.Find(Key);
			const bool bOwnershipChanged = Key.Representation < 2 && CommitTerrainOwnership(Key,
				Desired ? MakeArrayView(*Desired) : TConstArrayView<FVoxelViewKey>());
			if (bOwnershipChanged && Key.Representation < 2)
			{
				const int32 Side = 16 << Key.Level;
				const FIntPoint Origin = Key.Coordinate * Side;
				MarkCoverageDirty({Origin, Origin + FIntPoint(Side)});
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
						MarkCoverageDirty(Node.Representation == EVoxelHeightfieldRepresentation::Surface
							? SurfaceCoverageRect({Node.Coordinate, Node.Level}) : MacroCoverageRect({Node.Coordinate, Node.Level}));
					}
				}
			}
		}
	};
	if (OpenPublishGroups.IsEmpty())
	{
		OnPublishCommitted();
	}
	else
	{
		for (const auto& Pair : OpenPublishGroups)
			Publisher->SetOwnershipChanged(Pair.Value, OwnershipChanges.Contains(Pair.Key));
		Publisher->SetCommitCallbacks(MoveTemp(OnPublishCommitted), MoveTemp(OnGroupCommitted));
		bool bAccepted = true;
		for (const auto& Pair : OpenPublishGroups)
		{
			bAccepted &= Publisher->CommitGroup(Pair.Value);
		}
		OpenPublishGroups.Reset();
		if (!bAccepted) MarkCoverageDirty();
	}
	static double NextTimingLog = 0.0;
	const double Now = FPlatformTime::Seconds();
	if (CVarVoxelDebugRepresentation.GetValueOnGameThread() > 0 && Now >= NextTimingLog)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel coverage phases ms: plan=%.3f boxes=%.3f actors=%.3f commit=%.3f"),
			PlanMilliseconds, BoxesMilliseconds, ActorsMilliseconds,
			(Now - ResolveStart) * 1000.0 - PlanMilliseconds - BoxesMilliseconds - ActorsMilliseconds);
		NextTimingLog = Now + 5.0;
	}
}

bool FVoxelViewManager::CommitTerrainOwnership(const FVoxelPublishGroupKey& InKey,
	const TConstArrayView<FVoxelViewKey> InTarget)
{
	// 每代提交替换此 owner 当前的完整节点集，包括前代在快照形成后才提交的节点。
	TArray<FVoxelViewKey>& Current = VisibleTerrainNodesByGroup.FindOrAdd(InKey);
	bool bChanged = false;
	for (const FVoxelViewKey& Node : Current)
		if (!InTarget.Contains(Node)) bChanged |= VisibleTerrainNodes.Remove(Node) > 0;
	for (const FVoxelViewKey& Node : InTarget)
		if (!VisibleTerrainNodes.Contains(Node)) { VisibleTerrainNodes.Add(Node); bChanged = true; }
	Current = TArray<FVoxelViewKey>(InTarget);
	if (Current.IsEmpty()) VisibleTerrainNodesByGroup.Remove(InKey);
	return bChanged;
}

bool FVoxelViewManager::RebuildHeightfieldTransitions()
{
	TArray<FVoxelHeightfieldTileView> Views;
	Views.Reserve(SurfaceData.Num() + MacroData.Num());
	for (const auto& Pair : SurfaceData)
	{
		if (!Pair.Value || !SurfaceReady.Contains(Pair.Key) ||
			!SurfaceActors.FindRef(Pair.Key) ||
			(!SurfaceWanted.Contains(Pair.Key) && IsSurfaceReplacementReady(Pair.Key)))
		{
			continue;
		}
		Views.Add(FVoxelHeightfieldTransitionBuilder::MakeView(*Pair.Value));
	}
	for (const auto& Pair : MacroData)
	{
		if (!Pair.Value || !MacroReady.Contains(Pair.Key) ||
			!MacroActors.FindRef(Pair.Key) ||
			(!MacroWanted.Contains(Pair.Key) && IsMacroReplacementReady(Pair.Key)))
		{
			continue;
		}
		Views.Add(FVoxelHeightfieldTransitionBuilder::MakeView(*Pair.Value));
	}
	TArray<FVoxelHeightfieldTileFootprint> PendingTiles;
	PendingTiles.Reserve(SurfaceWanted.Num() + MacroWanted.Num());
	for (const FVoxelSurfaceTileKey& Key : SurfaceWanted)
	{
		if (SurfaceReady.Contains(Key) && SurfaceData.Contains(Key) &&
			SurfaceActors.FindRef(Key)) continue;
		const int32 Step = 1 << Key.Level;
		const int32 Side = FVoxelSurfaceTileData::CellSide * Step;
		PendingTiles.Add({{EVoxelHeightfieldRepresentation::Surface,
			Key.Coordinate, Key.Level}, Key.Coordinate * Side, Side, Step});
	}
	for (const FVoxelMacroTileKey& Key : MacroWanted)
	{
		if (MacroReady.Contains(Key) && MacroData.Contains(Key) &&
			MacroActors.FindRef(Key)) continue;
		const int32 Step = FVoxelMacroTileData::BaseStep << Key.Level;
		const int32 Side = FVoxelMacroTileData::CellSide * Step;
		PendingTiles.Add({{EVoxelHeightfieldRepresentation::Macro,
			Key.Coordinate, Key.Level}, Key.Coordinate * Side, Side, Step});
	}
	TMap<FVoxelHeightfieldNodeKey, int32> ViewIndices;
	TMap<FVoxelHeightfieldNodeKey, int32> PendingIndices;
	for (int32 Index = 0; Index < Views.Num(); ++Index) ViewIndices.Add(Views[Index].Key, Index);
	for (int32 Index = 0; Index < PendingTiles.Num(); ++Index) PendingIndices.Add(PendingTiles[Index].Key, Index);
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>> Dependencies;
	FVoxelHeightfieldTransitionBuilder::GatherOwnerDependencies(Views, PendingTiles, Dependencies);
	PendingHeightfieldPlanOwners.Reset();
	TArray<FVoxelHeightfieldTransitionEdge> Edges;
	for (const auto& OwnerView : Views)
	{
		const FVoxelHeightfieldNodeKey Owner = OwnerView.Key;
		TArray<FVoxelHeightfieldTileView> LocalViews;
		TArray<FVoxelHeightfieldTileFootprint> LocalPending;
		TArray<FHeightfieldTransitionTileSource> Pins;
		uint64 Signature = 1469598103934665603ull;
		const auto Mix = [&Signature](const uint64 Value) { Signature = (Signature ^ Value) * 1099511628211ull; };
		const auto& Keys = Dependencies.FindChecked(Owner);
		for (const auto& Key : Keys)
		{
			Mix(GetTypeHash(Key));
			if (const int32* Index = ViewIndices.Find(Key))
			{
				LocalViews.Add(Views[*Index]);
				Mix(Views[*Index].Revision);
				FHeightfieldTransitionTileSource Pin;
				if (Key.Representation == EVoxelHeightfieldRepresentation::Surface) Pin.Surface = SurfaceData.FindRef({Key.Coordinate, Key.Level});
				else Pin.Macro = MacroData.FindRef({Key.Coordinate, Key.Level});
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
		if (State.Plan) Edges.Append(State.Plan->Edges);
		if (!State.Plan || State.Plan->Signature != Signature)
		{
			for (const auto& Key : Keys) PendingHeightfieldPlanOwners.Add(Key);
			// 高度场拓扑使用不可变输入完成一次规划；Fine 的移动裁剪由发布器单独处理。
			if (State.PendingSignature != 0) continue;
			FVoxelTaskRequest Request;
			Request.Kind = EVoxelTaskKind::BuildViewCoverage;
			Request.WorkClass = EVoxelWorkClass::Boundary;
			Request.TerrainStage = Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? 2 : 3;
			if (Owner.Representation == EVoxelHeightfieldRepresentation::Surface)
			{
				if (const auto* Priority = Module.GetCurrentInterest().SurfacePriorities.Find({Owner.Coordinate, Owner.Level}))
					Request.SourcePriority = *Priority;
			}
			else if (const auto* Priority = Module.GetCurrentInterest().MacroPriorities.Find({Owner.Coordinate, Owner.Level}))
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
			Request.Execute = [Owner, LocalViews = MoveTemp(LocalViews), LocalPending = MoveTemp(LocalPending),
				Pins = MoveTemp(Pins), Signature](const TAtomic<bool>& Cancel)
			{
				FVoxelTaskResult Result;
				if (Cancel.Load()) return Result;
				auto Payload = MakeShared<FVoxelHeightfieldCoveragePlan, ESPMode::ThreadSafe>();
				Payload->Signature = Signature;
				if (!FVoxelHeightfieldTransitionBuilder::BuildEdges(LocalViews, Payload->Edges, Result.Error)) return Result;
				FVoxelHeightfieldTransitionBuilder::AppendPendingEdges(LocalViews, LocalPending, Payload->Edges);
				Payload->Edges.RemoveAll([Owner](const auto& Edge) { return !(Edge.Owner == Owner); });
				Result.CustomPayload = Payload;
				Result.bSuccess = !Cancel.Load();
				return Result;
			};
			const uint64 Serial = HeightfieldPlanSerial;
			Request.Apply = [this, Serial, Owner, Signature](FVoxelTaskResult&& Result)
			{
				if (Serial != HeightfieldPlanSerial || Result.Stamp.WorldEpoch != WorldEpoch) return;
				auto* Current = HeightfieldCoveragePlans.Find(Owner);
				if (!Current || Current->PendingSignature != Signature) return;
				Current->PendingSignature = 0;
				if (Result.bSuccess && !Result.bCanceled &&
					(!Current->Plan || Current->Plan->Signature != Current->DesiredSignature || Signature == Current->DesiredSignature))
				{
					Current->Plan = StaticCastSharedPtr<const FVoxelHeightfieldCoveragePlan>(Result.CustomPayload);
				}
				MarkCoverageDirty(Owner.Representation == EVoxelHeightfieldRepresentation::Surface
					? SurfaceCoverageRect({Owner.Coordinate, Owner.Level}) : MacroCoverageRect({Owner.Coordinate, Owner.Level}));
			};
			if (Scheduler.Enqueue(MoveTemp(Request))) State.PendingSignature = Signature;
			continue;
		}
	}
	for (auto It = HeightfieldCoveragePlans.CreateIterator(); It; ++It)
	{
		if (!ViewIndices.Contains(It.Key())) It.RemoveCurrent();
	}
	HeightfieldUnbalancedEdgeCount = 0;
	for (const FVoxelHeightfieldTransitionEdge& Edge : Edges)
	{
		HeightfieldUnbalancedEdgeCount += Edge.bUnbalanced ? 1 : 0;
	}
	TMap<FVoxelHeightfieldNodeKey, uint64> Revisions;
	for (const FVoxelHeightfieldTileView& View : Views)
	{
		Revisions.Add(View.Key, View.Revision);
	}
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldTransitionEdge>> NewEdges;
	for (const FVoxelHeightfieldTransitionEdge& Edge : Edges)
	{
		NewEdges.FindOrAdd(Edge.Owner).Add(Edge);
	}
	TMap<FVoxelHeightfieldNodeKey, uint64> NewSignatures;
	TArray<FVoxelHeightfieldNodeKey> NewUnsubmitted;
	for (const auto& Pair : NewEdges)
	{
		const uint64 Signature = FVoxelHeightfieldTransitionBuilder::BuildSignature(
			Pair.Key, Revisions.FindChecked(Pair.Key), Pair.Value);
		NewSignatures.Add(Pair.Key, Signature);
		const uint64* Built = HeightfieldTransitionSignatures.Find(Pair.Key);
		const uint64* Pending = PendingTransitionSignatures.Find(Pair.Key);
		const uint64* Prepared = PreparedTransitionSignatures.Find(Pair.Key);
		if ((!Built || *Built != Signature) &&
			(!Pending || *Pending != Signature) &&
			(!Prepared || *Prepared != Signature))
		{
			NewUnsubmitted.Add(Pair.Key);
		}
	}
	for (auto It = PreparedTransitionSignatures.CreateIterator(); It; ++It)
	{
		const uint64* Desired = NewSignatures.Find(It.Key());
		if (Desired && *Desired == It.Value()) continue;
		PreparedTransitionMeshes.Remove(It.Key());
		It.RemoveCurrent();
	}
	DesiredTransitionSignatures = MoveTemp(NewSignatures);
	DesiredTransitionEdges = MoveTemp(NewEdges);
	UnsubmittedTransitionOwners = MoveTemp(NewUnsubmitted);
	UnsubmittedTransitionOwners.Sort([this](const FVoxelHeightfieldNodeKey& A, const FVoxelHeightfieldNodeKey& B)
	{
		const bool bASurface = A.Representation == EVoxelHeightfieldRepresentation::Surface;
		const bool bBSurface = B.Representation == EVoxelHeightfieldRepresentation::Surface;
		if (bASurface != bBSurface) return !bASurface;
		const double DistanceA = bASurface
			? MinimumObserverDistanceCells(SurfaceWorldCenter({A.Coordinate, A.Level}))
			: MinimumObserverDistanceCells(MacroWorldCenter({A.Coordinate, A.Level}));
		const double DistanceB = bBSurface
			? MinimumObserverDistanceCells(SurfaceWorldCenter({B.Coordinate, B.Level}))
			: MinimumObserverDistanceCells(MacroWorldCenter({B.Coordinate, B.Level}));
		return DistanceA > DistanceB;
	});
	Scheduler.CancelMatching([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		if (Kind != EVoxelTaskKind::BuildViewTransition) return false;
		const FVoxelHeightfieldNodeKey Owner{
			static_cast<EVoxelHeightfieldRepresentation>(Stamp.Section.Z),
			FIntPoint(Stamp.Section.X, Stamp.Section.Y), Stamp.ViewKey.Level};
		const uint64* WantedSignature = DesiredTransitionSignatures.Find(Owner);
		return !WantedSignature || *WantedSignature != Stamp.Token;
	});
	return true;
}

void FVoxelViewManager::PumpHeightfieldTransitions()
{
	if (UnsubmittedTransitionOwners.IsEmpty()) return;
	const auto Config = Module.GetGenerationConfig();
	const auto Registry = Module.GetRegistry();
	if (!Config || !Config->Recipe || !Registry) return;
	for (int32 Submitted = 0; Submitted < 4 && !UnsubmittedTransitionOwners.IsEmpty();)
	{
		const FVoxelHeightfieldNodeKey Owner = UnsubmittedTransitionOwners.Last();
		const uint64* Signature = DesiredTransitionSignatures.Find(Owner);
		const TArray<FVoxelHeightfieldTransitionEdge>* Edges = DesiredTransitionEdges.Find(Owner);
		const uint64* Built = HeightfieldTransitionSignatures.Find(Owner);
		const uint64* Pending = PendingTransitionSignatures.Find(Owner);
		const uint64* Prepared = PreparedTransitionSignatures.Find(Owner);
		if (!Signature || !Edges ||
			(Built && *Built == *Signature) ||
			(Pending && *Pending == *Signature) ||
			(Prepared && *Prepared == *Signature))
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
			if (!Edge.bPending) Sources.Add(Edge.Neighbor, FindSource(Edge.Neighbor));
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
			if (const auto* Priority = Module.GetCurrentInterest().SurfacePriorities.Find({Owner.Coordinate, Owner.Level}))
				Request.SourcePriority = *Priority;
		}
		else if (const auto* Priority = Module.GetCurrentInterest().MacroPriorities.Find({Owner.Coordinate, Owner.Level}))
			Request.SourcePriority = *Priority;
		Request.DistanceScore = Owner.Representation == EVoxelHeightfieldRepresentation::Surface
			? MinimumObserverDistanceCells(SurfaceWorldCenter({Owner.Coordinate, Owner.Level}))
			: MinimumObserverDistanceCells(MacroWorldCenter({Owner.Coordinate, Owner.Level}));
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.Token = *Signature;
		Request.Stamp.Section = FIntVector(Owner.Coordinate.X, Owner.Coordinate.Y,
			static_cast<int32>(Owner.Representation));
		Request.Stamp.ViewKey.Level = Owner.Level;
		Request.ReservedBytes = 4ull * 1024ull * 1024ull;
		Request.Execute = [Owner, Sources = MoveTemp(Sources), OwnerEdges = *Edges,
			Config, Registry, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells]
			(const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			if (Cancel.Load()) return Result;
			TMap<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView> Views;
			for (const auto& Pair : Sources) Views.Add(Pair.Key, Pair.Value.View());
			const FVoxelHeightfieldTileView& OwnerView = Views.FindChecked(Owner);
			auto Payload = MakeShared<FHeightfieldTransitionTaskPayload, ESPMode::ThreadSafe>();
			Payload->Mesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
			Result.bSuccess = FVoxelHeightfieldTransitionBuilder::BuildMesh(
				OwnerView, OwnerEdges, Views, *Config, *Registry,
				Config->Recipe->Settings.MinZ, TextureStretch,
				*Payload->Mesh, Result.Error, &Cancel);
			if (Result.bSuccess) Result.CustomPayload = Payload;
			return Result;
		};
		Request.Apply = [this, Owner, TaskSignature = *Signature](FVoxelTaskResult&& Result)
		{
			ApplyHeightfieldTransition(Owner, TaskSignature, MoveTemp(Result));
		};
		if (!Scheduler.Enqueue(MoveTemp(Request))) break;
		PendingTransitionSignatures.Add(Owner, *Signature);
		UnsubmittedTransitionOwners.Pop();
		++Submitted;
	}
}

void FVoxelViewManager::ApplyHeightfieldTransition(
	const FVoxelHeightfieldNodeKey& InOwner,
	const uint64 InSignature,
	FVoxelTaskResult&& InResult)
{
	if (const uint64* Pending = PendingTransitionSignatures.Find(InOwner);
		Pending && *Pending == InSignature)
	{
		PendingTransitionSignatures.Remove(InOwner);
	}
	const uint64* Desired = DesiredTransitionSignatures.Find(InOwner);
	if (InResult.Stamp.WorldEpoch != WorldEpoch || !Desired || *Desired != InSignature) return;
	if (InResult.bCanceled)
	{
		UnsubmittedTransitionOwners.AddUnique(InOwner);
		return;
	}
	if (!InResult.bSuccess || !InResult.CustomPayload)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heightfield transition build failed: %s"),
			*InResult.Error);
		UnsubmittedTransitionOwners.AddUnique(InOwner);
		return;
	}
	const auto Payload = StaticCastSharedPtr<const FHeightfieldTransitionTaskPayload>(
		InResult.CustomPayload);
	if (!Payload->Mesh)
	{
		UnsubmittedTransitionOwners.AddUnique(InOwner);
		return;
	}
	PreparedTransitionSignatures.Add(InOwner, InSignature);
	PreparedTransitionMeshes.Add(InOwner, Payload->Mesh);
	MarkCoverageDirty(InOwner.Representation == EVoxelHeightfieldRepresentation::Surface
		? SurfaceCoverageRect({InOwner.Coordinate, InOwner.Level})
		: MacroCoverageRect({InOwner.Coordinate, InOwner.Level}));
}

bool FVoxelViewManager::VolumeHandoffAffectsHeightfield(const FVoxelHeightfieldNodeKey& InKey) const
{
	FVoxelHeightfieldTileView View;
	if (InKey.Representation == EVoxelHeightfieldRepresentation::Surface)
	{
		const auto Data = SurfaceData.FindRef({InKey.Coordinate, InKey.Level});
		if (!Data) return false;
		View = FVoxelHeightfieldTransitionBuilder::MakeView(*Data);
	}
	else
	{
		const auto Data = MacroData.FindRef({InKey.Coordinate, InKey.Level});
		if (!Data) return false;
		View = FVoxelHeightfieldTransitionBuilder::MakeView(*Data);
	}
	int32 MinimumHeight = MAX_int32;
	int32 MaximumHeight = MIN_int32;
	for (const int32 Height : View.Ground)
	{
		if (Height == MIN_int32) continue;
		MinimumHeight = FMath::Min(MinimumHeight, Height);
		MaximumHeight = FMath::Max(MaximumHeight, Height);
	}
	for (const int32 Height : View.Water)
	{
		if (Height == MIN_int32) continue;
		MinimumHeight = FMath::Min(MinimumHeight, Height);
		MaximumHeight = FMath::Max(MaximumHeight, Height);
	}
	if (MinimumHeight > MaximumHeight) return false;
	const FIntPoint Maximum = View.Origin + FIntPoint(View.CellSide() * View.Step);
	const FVoxelCoverageBox Bounds{FIntVector(View.Origin.X, View.Origin.Y, MinimumHeight - 1),
		FIntVector(Maximum.X, Maximum.Y, MaximumHeight + 2)};
	return VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes);
}

bool FVoxelViewManager::RebuildVolumeTransitions(
	const TSet<FVoxelViewKey>& InTargetNodes)
{
	TMap<FVoxelViewKey, TSet<FVoxelViewKey>> Regions;
	FVoxelVolumeTransitionPlanner::GatherCoverageRegions(InTargetNodes, Regions);
	PendingVolumeHandoffBoxes.Reset();
	TSet<FVoxelViewKey> ChangedTopologyNodes;
	TArray<FVoxelVolumeTransitionFace> Faces;
	for (const auto& Pair : Regions)
	{
		const FVoxelViewKey Region = Pair.Key;
		auto& State = VolumeCoveragePlans.FindOrAdd(Region);
		uint64 Signature = 0;
		for (const auto& Node : Pair.Value) Signature ^= VoxelGeneration::Mix(GetTypeHash(Node));
		Signature = VoxelGeneration::Mix(Signature ^ Pair.Value.Num());
		State.DesiredSignature = Signature;
		if (State.Plan) Faces.Append(State.Plan->Faces);
		if (State.Plan && State.Plan->Nodes.Includes(Pair.Value) && State.Plan->Nodes.Num() == Pair.Value.Num())
		{
			continue;
		}
		// 同精度 Fine 使用 section 邻接网格，不存在 LOD 过渡面；规划快照更新不阻止其取得所有权。
		auto NeedsTopologyHandoff = [Region](const FVoxelViewKey& Node, const TSet<FVoxelViewKey>& Nodes)
		{
			if (Node.Level > 0) return true;
			const FVoxelGenerationBounds Bounds = Node.GetBounds();
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				for (const int32 Sign : {-1, 1})
				{
					FIntVector SameLevel = Node.Coordinate;
					SameLevel[Axis] += Sign;
					if (Nodes.Contains({SameLevel, 0})) continue;
					FIntVector Point = Bounds.Min + FIntVector(8);
					Point[Axis] = Sign > 0 ? Bounds.Max[Axis] : Bounds.Min[Axis] - 1;
					for (uint8 Level = 1; Level <= Region.Level; ++Level)
					{
						const int32 Side = 16 << Level;
						if (Nodes.Contains({FIntVector(VoxelGeneration::FloorDivide(Point.X, Side),
							VoxelGeneration::FloorDivide(Point.Y, Side), VoxelGeneration::FloorDivide(Point.Z, Side)), Level})) return true;
					}
				}
			}
			return false;
		};
		bool bHasProxy = false;
		for (const auto& Node : Pair.Value) if (Node.Level > 0) { bHasProxy = true; break; }
		bool bHadProxy = false;
		if (State.Plan) for (const auto& Node : State.Plan->Nodes)
			if (Node.Level > 0) { bHadProxy = true; break; }
		if (bHasProxy || bHadProxy) for (const FVoxelViewKey& Node : Pair.Value)
		{
			if ((!State.Plan || !State.Plan->Nodes.Contains(Node)) &&
				(NeedsTopologyHandoff(Node, Pair.Value) || (State.Plan && NeedsTopologyHandoff(Node, State.Plan->Nodes)))) ChangedTopologyNodes.Add(Node);
		}
		if (State.Plan && (bHasProxy || bHadProxy))
		{
			for (const FVoxelViewKey& Node : State.Plan->Nodes)
			{
				if (!Pair.Value.Contains(Node) &&
					(NeedsTopologyHandoff(Node, Pair.Value) || NeedsTopologyHandoff(Node, State.Plan->Nodes))) ChangedTopologyNodes.Add(Node);
			}
		}
		// 保留正在计算的不可变快照；完成后只为最新差异补充任务，避免移动时持续取消。
		if (State.PendingSignature != 0) continue;
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildViewCoverage;
		Request.WorkClass = EVoxelWorkClass::Visible;
		Request.SourcePriority = MAX_int32;
		for (const auto& Node : Pair.Value)
		{
			if (const auto* Demand = Node.Level == 0 ? Module.GetCurrentInterest().Exact.Find(Node.Coordinate) : nullptr)
			{
				Request.SourcePriority = FMath::Min(Request.SourcePriority, Demand->Priority);
				if (Demand->bMovementCriticalFine) Request.WorkClass = EVoxelWorkClass::Critical;
			}
			else if (const auto* Priority = Module.GetCurrentInterest().VoxelProxyPriorities.Find(Node))
				Request.SourcePriority = FMath::Min(Request.SourcePriority, *Priority);
		}
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.Section = FIntVector(Region.Coordinate.X, Region.Coordinate.Y, -1);
		Request.Stamp.ViewKey = Region;
		Request.Stamp.Token = Signature;
		Request.ReservedBytes = 4ull * 1024ull * 1024ull;
		Request.InputBytes = Pair.Value.GetAllocatedSize();
		Request.DistanceScore = MinimumObserverDistanceCells(Region.GetBounds());
		Request.Execute = [Region, Nodes = Pair.Value, Signature](const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			if (Cancel.Load()) return Result;
			auto Payload = MakeShared<FVoxelVolumeCoveragePlan, ESPMode::ThreadSafe>();
			Payload->Signature = Signature;
			Payload->Nodes = Nodes;
			uint8 MaximumLevel = 0;
			for (const FVoxelViewKey& Key : Nodes) MaximumLevel = FMath::Max(MaximumLevel, Key.Level);
			FVoxelVolumeTransitionPlanner::Build(Nodes, MaximumLevel, Payload->Faces);
			Payload->Faces.RemoveAll([Region](const auto& Face)
			{
				FVoxelViewKey Owner = Face.Owner;
				while (Owner.Level < Region.Level) Owner = Owner.GetParent();
				return !(Owner == Region);
			});
			Result.CustomPayload = Payload;
			Result.bSuccess = !Cancel.Load();
			return Result;
		};
		const uint64 Serial = VolumePlanSerial;
		Request.Apply = [this, Serial, Region, Signature](FVoxelTaskResult&& Result)
		{
			if (Serial != VolumePlanSerial || Result.Stamp.WorldEpoch != WorldEpoch) return;
			auto* Current = VolumeCoveragePlans.Find(Region);
			if (!Current || Current->PendingSignature != Signature) return;
			Current->PendingSignature = 0;
			if (Result.bSuccess && !Result.bCanceled &&
				(!Current->Plan || Current->Plan->Signature != Current->DesiredSignature || Signature == Current->DesiredSignature))
			{
				Current->Plan = StaticCastSharedPtr<const FVoxelVolumeCoveragePlan>(Result.CustomPayload);
			}
			MarkCoverageDirty(VoxelProxyCoverageRect(Region));
		};
		if (Scheduler.Enqueue(MoveTemp(Request))) State.PendingSignature = Signature;
	}
	for (auto It = VolumeCoveragePlans.CreateIterator(); It; ++It)
	{
		if (!Regions.Contains(It.Key())) It.RemoveCurrent();
	}
	for (const FVoxelViewKey& Node : ChangedTopologyNodes)
	{
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
	bool bSnapshotsComplete = true;
	for (const FVoxelVolumeTransitionFace& Face : Faces)
	{
		if (!InTargetNodes.Contains(Face.Owner) || !InTargetNodes.Contains(Face.Neighbor)) continue;
		if (VoxelCoverage::IntersectsAny3D(Face.Neighbor.Level == 0 ? FineCoverageBox(Face.Neighbor.Coordinate)
			: VoxelProxyCoverageBox(Face.Neighbor), PendingTopologyBoxes))
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
			if (const FVoxelSection* Section = Module.GetRuntime()->FindSection(
				Face.Neighbor.Coordinate))
			{
				bCaptured = FVoxelBoundaryFaceSnapshot::CaptureFine(
					Face.Neighbor, NeighborFace, *Section, Boundary);
			}
		}
		else if (const auto Data = VoxelProxyData.FindRef(Face.Neighbor))
		{
			bCaptured = FVoxelBoundaryFaceSnapshot::CaptureProxy(
				*Data, NeighborFace, Boundary);
		}
		if (!bCaptured)
		{
			if (Face.Neighbor.Level == 0) ++MissingFineBoundaryCount;
			else ++MissingProxyBoundaryCount;
			bSnapshotsComplete = false;
			BlockedOwners.Add(Face.Owner);
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Face.Owner));
			continue;
		}
		TSharedPtr<FVoxelBoundaryTransitionContext>& Context = Contexts.FindOrAdd(Face.Owner);
		if (!Context)
		{
			Context = MakeShared<FVoxelBoundaryTransitionContext>();
			Context->Owner = Face.Owner;
		}
		Context->Patches.Add({Face, MoveTemp(Boundary)});
	}
	TMap<FVoxelViewKey, uint64> NewSignatures;
	TMap<FVoxelViewKey, TSharedPtr<const FVoxelBoundaryTransitionContext>> NewContexts;
	TArray<FVoxelViewKey> NewUnsubmitted;
	bool bMeshesReady = bSnapshotsComplete;
	for (const FVoxelViewKey& Key : InTargetNodes)
	{
		if (Key.Level == 0) continue;
		if (BlockedOwners.Contains(Key) || VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Key), PendingTopologyBoxes)) continue;
		const auto Data = VoxelProxyData.FindRef(Key);
		if (!Data)
		{
			bMeshesReady = false;
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Key));
			continue;
		}
		TSharedPtr<FVoxelBoundaryTransitionContext> Context = Contexts.FindRef(Key);
		if (!Context)
		{
			Context = MakeShared<FVoxelBoundaryTransitionContext>();
			Context->Owner = Key;
		}
		if (!Context->Validate())
		{
			++InvalidVolumeContextCount;
			bMeshesReady = false;
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Key));
			continue;
		}
		const uint64 Signature = Context->Signature(Data->Revision);
		NewSignatures.Add(Key, Signature);
		NewContexts.Add(Key, Context);
		const uint64* Built = VolumeTransitionSignatures.Find(Key);
		const uint64* Pending = PendingVolumeSignatures.Find(Key);
		const uint64* Prepared = PreparedVolumeSignatures.Find(Key);
		if ((!Built || *Built != Signature) &&
			(!Prepared || *Prepared != Signature))
		{
			bMeshesReady = false;
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Key));
			if (!Pending || *Pending != Signature) NewUnsubmitted.Add(Key);
		}
	}
	DesiredVolumeSignatures = MoveTemp(NewSignatures);
	DesiredVolumeContexts = MoveTemp(NewContexts);
	UnsubmittedVolumeOwners = MoveTemp(NewUnsubmitted);
	UnsubmittedVolumeOwners.Sort([this](const FVoxelViewKey& A, const FVoxelViewKey& B)
	{
		const EVoxelWorkClass ClassA = VolumeTransitionWorkClass(A);
		const EVoxelWorkClass ClassB = VolumeTransitionWorkClass(B);
		if (ClassA != ClassB) return static_cast<uint8>(ClassA) > static_cast<uint8>(ClassB);
		return MinimumObserverDistanceCells(A.GetBounds()) > MinimumObserverDistanceCells(B.GetBounds());
	});
	for (auto It = PreparedVolumeSignatures.CreateIterator(); It; ++It)
	{
		const uint64* Desired = DesiredVolumeSignatures.Find(It.Key());
		if (Desired && *Desired == It.Value()) continue;
		PreparedVolumeMeshes.Remove(It.Key());
		It.RemoveCurrent();
	}
	Scheduler.CancelMatching([this](const EVoxelTaskKind Kind,
		const FVoxelTaskStamp& Stamp)
	{
		if (Kind != EVoxelTaskKind::BuildVolumeTransition) return false;
		const uint64* Desired = DesiredVolumeSignatures.Find(Stamp.ViewKey);
		return !Desired || *Desired != Stamp.Token;
	});
	return bMeshesReady;
}

void FVoxelViewManager::PumpVolumeTransitions()
{
	if (UnsubmittedVolumeOwners.IsEmpty()) return;
	const auto Registry = Module.GetRegistry();
	const auto Shapes = Module.GetShapes();
	if (!Registry || !Shapes) return;
	const double Deadline = FPlatformTime::Seconds() + Module.GetViewSettings().AdmissionMilliseconds / 1000.0;
	for (int32 Submitted = 0; Submitted < 4 && !UnsubmittedVolumeOwners.IsEmpty() &&
		FPlatformTime::Seconds() < Deadline;)
	{
		const FVoxelViewKey Owner = UnsubmittedVolumeOwners.Last();
		const uint64* Signature = DesiredVolumeSignatures.Find(Owner);
		const auto Data = VoxelProxyData.FindRef(Owner);
		const auto Context = DesiredVolumeContexts.FindRef(Owner);
		if (!Signature || !Data || !Context ||
			VolumeTransitionSignatures.FindRef(Owner) == *Signature ||
			PendingVolumeSignatures.FindRef(Owner) == *Signature ||
			PreparedVolumeSignatures.FindRef(Owner) == *Signature)
		{
			UnsubmittedVolumeOwners.Pop();
			continue;
		}
		FVoxelSectionSnapshot Snapshot;
		FString Error;
		if (!BuildVoxelProxySnapshot(*Data, Module.GetManifest().RecipeHash,
			Snapshot, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("Volume transition snapshot failed: %s"), *Error);
			UnsubmittedVolumeOwners.Pop();
			continue;
		}
		if (FVoxelSectionMesher::IsKnownEmpty(Snapshot, *Registry, Context.Get()))
		{
			++SkippedVolumeMeshes;
			PreparedVolumeMeshes.Add(Owner, MakeShared<FVoxelSectionMeshResult>());
			PreparedVolumeSignatures.Add(Owner, *Signature);
			UnsubmittedVolumeOwners.Pop();
			MarkCoverageDirty(VoxelProxyCoverageRect(Owner));
			continue;
		}
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildVolumeTransition;
		Request.WorkClass = VolumeTransitionWorkClass(Owner);
		if (const auto* Priority = Module.GetCurrentInterest().VoxelProxyPriorities.Find(Owner)) Request.SourcePriority = *Priority;
		Request.DistanceScore = MinimumObserverDistanceCells(Owner.GetBounds());
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.ViewKey = Owner;
		Request.Stamp.Revision = Data->Revision;
		Request.Stamp.Token = *Signature;
		Request.ReservedBytes = 48ull * 1024ull * 1024ull;
		Request.InputBytes = Snapshot.Bytes();
		for (const auto& Patch : Context->Patches) Request.InputBytes += Patch.Neighbor.States.GetAllocatedSize();
		Request.Execute = [Snapshot = MoveTemp(Snapshot), Context, Registry, Shapes,
			TextureRepeats = Owner.GetSampleStep() /
				VoxelViewLod::TexturePeriodCells(Owner.GetSampleStep(),
					Module.GetViewSettings().MaximumTextureStretchCells)]
			(const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			Result.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.bSuccess = FVoxelSectionMesher::Build(Snapshot, *Registry, *Shapes,
				*Result.VoxelProxyMesh, &Cancel, TextureRepeats, Context.Get());
			if (!Result.bSuccess) Result.Error = TEXT("Failed to mesh volume transition");
			return Result;
		};
		Request.Apply = [this, Owner, TaskSignature = *Signature](FVoxelTaskResult&& Result)
		{
			ApplyVolumeTransition(Owner, TaskSignature, MoveTemp(Result));
		};
		if (!Scheduler.Enqueue(MoveTemp(Request))) break;
		PendingVolumeSignatures.Add(Owner, *Signature);
		UnsubmittedVolumeOwners.Pop();
		++Submitted;
	}
}

void FVoxelViewManager::ApplyVolumeTransition(const FVoxelViewKey& InOwner,
	const uint64 InSignature, FVoxelTaskResult&& InResult)
{
	if (PendingVolumeSignatures.FindRef(InOwner) == InSignature)
	{
		PendingVolumeSignatures.Remove(InOwner);
	}
	const uint64* Desired = DesiredVolumeSignatures.Find(InOwner);
	if (InResult.Stamp.WorldEpoch != WorldEpoch || !Desired ||
		*Desired != InSignature) return;
	if (InResult.bCanceled)
	{
		UnsubmittedVolumeOwners.AddUnique(InOwner);
		return;
	}
	if (!InResult.bSuccess || !InResult.VoxelProxyMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Volume transition build failed: %s"),
			*InResult.Error);
		MarkCoverageDirty();
		return;
	}
	PreparedVolumeMeshes.Add(InOwner, InResult.VoxelProxyMesh);
	PreparedVolumeSignatures.Add(InOwner, InSignature);
	MarkCoverageDirty(VoxelProxyCoverageRect(InOwner));
}

void FVoxelViewManager::CleanupRetiredRepresentations(const double InNow)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewRetire);

	auto CanRetire = [InNow](const double InLastWanted, const bool bReplacementReady, const bool bInsideDomain)
	{
		const double Age = InNow - InLastWanted;
		return Age >= RetireDelaySeconds && (bReplacementReady || (!bInsideDomain && Age >= OutsideDomainRetireDelaySeconds));
	};
	const bool bUsesTerrainPlan = !Module.GetCurrentInterest().TerrainPlan.Roots.IsEmpty();

	TSet<FIntVector> FineRetireKeys = FineReady;
	for (const auto& Pair : FineActors) FineRetireKeys.Add(Pair.Key);
	for (const FIntVector Key : FineRetireKeys)
	{
		if (FineWanted.Contains(Key) || VisibleTerrainNodes.Contains({ Key, 0 })) continue;
		if (Publisher->IsGroupBusy({0, FIntPoint(Key.X, Key.Y), 0})) continue;
		const double LastWanted = FineLastWanted.FindRef(Key);
		if (bUsesTerrainPlan)
		{
			if (InNow - LastWanted < RetireDelaySeconds) continue;
		}
		else if (!CanRetire(LastWanted, IsFineReplacementReady(Key), IsFineInsideRenderDomain(Key)))
		{
			continue;
		}
		if (AActor* Actor = FineActors.FindRef(Key))
		{
			Publisher->Forget(Actor);
			Actor->Destroy();
		}
		FineActors.Remove(Key);
		bReadyTerrainBranchesDirty |= FineReady.Remove(Key) > 0;
		FineRevisions.Remove(Key);
		FineLastWanted.Remove(Key);
	}
	TSet<FVoxelViewKey> ProxyRetireKeys = VoxelProxyReady;
	for (const auto& Pair : VoxelProxyActors) ProxyRetireKeys.Add(Pair.Key);
	for (const FVoxelViewKey Key : ProxyRetireKeys)
	{
		if (VoxelProxyWanted.Contains(Key) || VisibleTerrainNodes.Contains(Key)) continue;
		if (Publisher->IsGroupBusy({1, FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level})) continue;
		const double LastWanted = VoxelProxyLastWanted.FindRef(Key);
		if (bUsesTerrainPlan)
		{
			if (InNow - LastWanted < RetireDelaySeconds) continue;
		}
		else if (!CanRetire(LastWanted, IsProxyReplacementReady(Key), IsProxyInsideRenderDomain(Key)))
		{
			continue;
		}
		if (AActor* Actor = VoxelProxyActors.FindRef(Key))
		{
			Publisher->Forget(Actor);
			Actor->Destroy();
		}
		VoxelProxyActors.Remove(Key);
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(Key) > 0;
		VoxelProxyData.Remove(Key);
		ProxySurfaceCoverage.Remove(Key);
		VoxelProxyRevisions.Remove(Key);
		VolumeTransitionSignatures.Remove(Key);
		PreparedVolumeSignatures.Remove(Key);
		PreparedVolumeMeshes.Remove(Key);
		VoxelProxyLastWanted.Remove(Key);
	}
	TSet<FVoxelSurfaceTileKey> SurfaceRetireKeys = SurfaceReady;
	for (const auto& Pair : SurfaceActors) SurfaceRetireKeys.Add(Pair.Key);
	for (const FVoxelSurfaceTileKey Key : SurfaceRetireKeys)
	{
		if (SurfaceWanted.Contains(Key) || InNow - SurfaceLastWanted.FindRef(Key) < RetireDelaySeconds) continue;
		if (Publisher->IsGroupBusy({2, Key.Coordinate, Key.Level})) continue;
		if (VoxelCoverage::IntersectsAny2D(SurfaceCoverageRect(Key),
			PendingHeightfieldHandoffRects)) continue;
		if (SurfaceWanted.Contains(Key) || !CanRetire(SurfaceLastWanted.FindRef(Key), IsSurfaceReplacementReady(Key), IsSurfaceInsideRenderDomain(Key))) continue;
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
		SurfaceReady.Remove(Key);
		SurfaceData.Remove(Key);
		WaterData.Remove(Key);
		SurfaceRevisions.Remove(Key);
		SurfaceLastWanted.Remove(Key);
		MarkCoverageDirty(SurfaceCoverageRect(Key));
	}
	TSet<FVoxelMacroTileKey> MacroRetireKeys = MacroReady;
	for (const auto& Pair : MacroActors) MacroRetireKeys.Add(Pair.Key);
	for (const FVoxelMacroTileKey Key : MacroRetireKeys)
	{
		if (MacroWanted.Contains(Key) || InNow - MacroLastWanted.FindRef(Key) < RetireDelaySeconds) continue;
		if (Publisher->IsGroupBusy({3, Key.Coordinate, Key.Level})) continue;
		if (VoxelCoverage::IntersectsAny2D(MacroCoverageRect(Key),
			PendingHeightfieldHandoffRects)) continue;
		if (MacroWanted.Contains(Key) || !CanRetire(MacroLastWanted.FindRef(Key), IsMacroReplacementReady(Key), IsMacroInsideRenderDomain(Key))) continue;
		if (AActor* Actor = MacroActors.FindRef(Key))
		{
			Publisher->Forget(Actor);
			Actor->Destroy();
		}
		MacroActors.Remove(Key);
		MacroReady.Remove(Key);
		MacroData.Remove(Key);
		MacroRevisions.Remove(Key);
		MacroLastWanted.Remove(Key);
		MarkCoverageDirty(MacroCoverageRect(Key));
	}
}

bool FVoxelViewManager::OnTask(FVoxelTaskResult&& InResult)
{
	if (InResult.Stamp.WorldEpoch != WorldEpoch || InResult.bCanceled || !InResult.bSuccess)
	{
		if (InResult.Stamp.WorldEpoch == WorldEpoch && !InResult.bCanceled && !InResult.bSuccess)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Voxel representation task failed: kind=%d error=%s"),
				static_cast<int32>(InResult.Kind),
				*InResult.Error);
		}
		return InResult.Kind == EVoxelTaskKind::BuildFineMesh ||
			InResult.Kind == EVoxelTaskKind::BuildVoxelProxy ||
			InResult.Kind == EVoxelTaskKind::BuildSurface ||
			InResult.Kind == EVoxelTaskKind::BuildWater ||
			InResult.Kind == EVoxelTaskKind::BuildMacro;
	}

	switch (InResult.Kind)
	{
	case EVoxelTaskKind::BuildFineMesh:
		if (PublishFine(InResult)) MarkCoverageDirty(FineCoverageRect(InResult.Stamp.Section));
		return true;
	case EVoxelTaskKind::BuildVoxelProxy:
		if (PublishVoxelProxy(InResult)) MarkCoverageDirty(VoxelProxyCoverageRect(InResult.Stamp.ViewKey));
		return true;
	case EVoxelTaskKind::BuildSurface:
		if (InResult.Surface && SurfaceWanted.Contains(InResult.Stamp.SurfaceKey) && PublishSurface(InResult))
		{
			const FVoxelSurfaceTileKey Key = InResult.Stamp.SurfaceKey;
			SurfaceData.Add(Key, InResult.Surface);
			MarkCoverageDirty(SurfaceCoverageRect(Key));
			if (InResult.Water) WaterData.Add(Key, InResult.Water);
			else WaterData.Remove(Key);
		}
		return true;
	case EVoxelTaskKind::BuildWater:
		// 升级过程中已排队的旧 Water 任务只做丢弃。
		return true;
	case EVoxelTaskKind::BuildMacro:
		if (InResult.Macro && MacroWanted.Contains(InResult.Stamp.MacroKey) && PublishMacro(InResult))
		{
			MacroData.Add(InResult.Stamp.MacroKey, MoveTemp(InResult.Macro));
			MarkCoverageDirty(MacroCoverageRect(InResult.Stamp.MacroKey));
		}
		return true;
	default:
		return false;
	}
}

bool FVoxelViewManager::ApplyRemoteRepresentation(
	const FVoxelRepresentationReply& InReply,
	FString& OutError)
{
	if (InReply.Kind == EVoxelRepresentationReplyKind::Retry)
	{
		OutError.Reset();
		return true;
	}
	if (InReply.Kind == EVoxelRepresentationReplyKind::Unchanged)
	{
		if (InReply.Type == EVoxelRepresentationWireType::VoxelProxy)
		{
			Module.GetRuntime()->GetChangeHierarchy().SetVoxelProxyRevision({ InReply.Key.Coordinate, InReply.Key.Level }, InReply.Revision);
		}
		else if (InReply.Type == EVoxelRepresentationWireType::SurfaceProxy)
		{
			Module.GetRuntime()->GetChangeHierarchy().SetSurfaceRevision(
				{ FIntPoint(InReply.Key.Coordinate.X, InReply.Key.Coordinate.Y), InReply.Key.Level }, InReply.Revision);
		}
		else if (InReply.Type == EVoxelRepresentationWireType::MacroTerrain)
		{
			Module.GetRuntime()->GetChangeHierarchy().SetMacroRevision(
				{ FIntPoint(InReply.Key.Coordinate.X, InReply.Key.Coordinate.Y), InReply.Key.Level }, InReply.Revision);
		}
		OutError.Reset();
		return true;
	}
	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();
	if (!Registry)
	{
		OutError = TEXT("Voxel runtime registry is unavailable");
		return false;
	}
	FVoxelTaskRequest Request;
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.Revision = InReply.Revision;
	Request.WorkClass = EVoxelWorkClass::Visible;
	Request.ReservedBytes = 48ull * 1024ull * 1024ull;
	if (InReply.Type == EVoxelRepresentationWireType::VoxelProxy)
	{
		FVoxelVoxelProxyData Data;
		if (!FVoxelRepresentationSync::DecodeVoxelProxy(InReply.Data, Data, OutError)) return false;
		Request.Kind = EVoxelTaskKind::BuildVoxelProxy;
		Request.Stamp.ViewKey = Data.Key;
		Request.Stamp.Token = HashCombineFast(GetTypeHash(Data.Key), GetTypeHash(InReply.Revision));
		Module.GetRuntime()->GetChangeHierarchy().SetVoxelProxyRevision(Data.Key, InReply.Revision);
		const TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes = Module.GetShapes();
		const uint64 RecipeHash = Module.GetManifest().RecipeHash;
		Request.Execute = [Data = MoveTemp(Data), Registry, Shapes, RecipeHash, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells](const TAtomic<bool>& InCancel) mutable
		{
			FVoxelTaskResult Result;
			Result.VoxelProxy = MakeShared<FVoxelVoxelProxyData>(MoveTemp(Data));
			FVoxelSectionSnapshot Snapshot;
			if (!BuildVoxelProxySnapshot(*Result.VoxelProxy, RecipeHash, Snapshot, Result.Error))
			{
				Result.bSuccess = false;
				return Result;
			}
			Result.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.bSuccess = Shapes && FVoxelSectionMesher::Build(
				Snapshot, *Registry, *Shapes, *Result.VoxelProxyMesh, &InCancel, Result.VoxelProxy->Key.GetSampleStep() / VoxelViewLod::TexturePeriodCells(Result.VoxelProxy->Key.GetSampleStep(), TextureStretch));
			return Result;
		};
	}
	else if (InReply.Type == EVoxelRepresentationWireType::SurfaceProxy)
	{
		FVoxelSurfaceTileData Data;
		if (!FVoxelRepresentationSync::DecodeSurface(InReply.Data, Data, OutError)) return false;
		Request.Kind = EVoxelTaskKind::BuildSurface;
		Request.Stamp.SurfaceKey = Data.Key;
		Request.Stamp.Token = HashCombineFast(GetTypeHash(Data.Key), GetTypeHash(InReply.Revision));
		Module.GetRuntime()->GetChangeHierarchy().SetSurfaceRevision(Data.Key, InReply.Revision);
		const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();
		Request.Execute = [Data = MoveTemp(Data), Registry, Config, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells](const TAtomic<bool>& InCancel) mutable
		{
			FVoxelTaskResult Result;
			Result.Surface = MakeShared<FVoxelSurfaceTileData>(MoveTemp(Data));
			Result.SurfaceMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.bSuccess = Config && FVoxelHeightfieldMesher::BuildBlockyTerrain(
				Result.Surface->Side,
				Result.Surface->Step,
				Result.Surface->GroundZ,
				Result.Surface->SurfaceMaterial,
				*Config,
				*Registry,
				*Result.SurfaceMesh,
				Result.Error,
				&InCancel,
				{},
				-0.02,
				TextureStretch);
			if (Result.bSuccess)
			{
				Result.bSuccess = FVoxelHeightfieldMesher::AppendDistantCells(
					Result.Surface->DistantCells,
					Result.Surface->Key.Coordinate * Result.Surface->GetTileSide(),
					Result.Surface->Step, *Registry, *Result.SurfaceMesh,
					Result.Error, &InCancel, TextureStretch);
			}
			if (!Result.bSuccess)
			{
				return Result;
			}
			Result.Water = MakeShared<FVoxelWaterSurfaceTileData>();
			FVoxelWaterViewBuilder WaterBuilder;
			Result.bSuccess = WaterBuilder.Build(*Result.Surface, *Result.Water, Result.Error);
			if (!Result.bSuccess)
			{
				return Result;
			}
			FVoxelSectionMeshResult WaterMesh;
			Result.bSuccess = FVoxelHeightfieldMesher::BuildWater(
				*Result.Water, *Registry, WaterMesh, Result.Error, &InCancel, TextureStretch);
			if (Result.bSuccess)
			{
				AppendNonEmptyBatches(*Result.SurfaceMesh, MoveTemp(WaterMesh));
			}
			return Result;
		};
	}
	else if (InReply.Type == EVoxelRepresentationWireType::MacroTerrain)
	{
		FVoxelMacroTileData Data;
		if (!FVoxelRepresentationSync::DecodeMacro(InReply.Data, Data, OutError)) return false;
		Request.Kind = EVoxelTaskKind::BuildMacro;
		Request.Stamp.MacroKey = Data.Key;
		Request.Stamp.Token = HashCombineFast(GetTypeHash(Data.Key), GetTypeHash(InReply.Revision));
		Module.GetRuntime()->GetChangeHierarchy().SetMacroRevision(Data.Key, InReply.Revision);
		const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();
		Request.Execute = [Data = MoveTemp(Data), Registry, Config, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells](const TAtomic<bool>& InCancel) mutable
		{
			FVoxelTaskResult Result;
			Result.Macro = MakeShared<FVoxelMacroTileData>(MoveTemp(Data));
			Result.MacroMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.bSuccess = Config && FVoxelHeightfieldMesher::BuildMacro(
				*Result.Macro,
				*Config,
				*Registry,
				*Result.MacroMesh,
				Result.Error,
				&InCancel, TextureStretch);
			return Result;
		};
	}
	else
	{
		OutError = TEXT("Voxel representation type is invalid");
		return false;
	}
	if (!Scheduler.Enqueue(MoveTemp(Request)))
	{
		OutError = TEXT("Voxel representation task could not be queued");
		return false;
	}
	OutError.Reset();
	return true;
}

void FVoxelViewManager::InvalidateRemoteRepresentations(
	const FVoxelRepresentationInvalidate& InInvalidate)
{
	AppliedInterestRevision = 0;
	MarkCoverageDirty();
	for (const FVoxelRepresentationWireKey& WireKey : InInvalidate.Keys)
	{
		const FVoxelViewKey VoxelKey { WireKey.Coordinate, WireKey.Level };
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(VoxelKey) > 0;
		VoxelProxyData.Remove(VoxelKey);
		ProxySurfaceCoverage.Remove(VoxelKey);
		VoxelProxyRevisions.Remove(VoxelKey);
		VolumeTransitionSignatures.Remove(VoxelKey);
		PreparedVolumeSignatures.Remove(VoxelKey);
		PreparedVolumeMeshes.Remove(VoxelKey);
		Module.GetRuntime()->GetChangeHierarchy().SetVoxelProxyRevision(VoxelKey, InInvalidate.Revision);

		const FIntPoint Coordinate(WireKey.Coordinate.X, WireKey.Coordinate.Y);
		const FVoxelSurfaceTileKey SurfaceKey { Coordinate, WireKey.Level };
		SurfaceReady.Remove(SurfaceKey);
		SurfaceData.Remove(SurfaceKey);
		WaterData.Remove(SurfaceKey);
		SurfaceRevisions.Remove(SurfaceKey);
		Module.GetRuntime()->GetChangeHierarchy().SetSurfaceRevision(SurfaceKey, InInvalidate.Revision);

		const FVoxelMacroTileKey MacroKey { Coordinate, WireKey.Level };
		MacroReady.Remove(MacroKey);
		MacroData.Remove(MacroKey);
		MacroRevisions.Remove(MacroKey);
		Module.GetRuntime()->GetChangeHierarchy().SetMacroRevision(MacroKey, InInvalidate.Revision);
	}
	PrunePreparedData();
}

void FVoxelViewManager::InvalidateSection(const FIntVector& InKey)
{
	PrunePreparedData();
	InvalidateNeighbors(InKey);
	MarkCoverageDirty();
	FineRevisions.Remove(InKey);
	bReadyTerrainBranchesDirty |= FineReady.Remove(InKey) > 0;
	Scheduler.CancelSection(InKey);
	const FVoxelChangeHierarchy& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	for (const FVoxelViewKey& Key : VoxelProxyReady.Array())
	{
		if (!Hierarchy.AffectsVoxelProxy(Key, InKey)) continue;
		VoxelProxyRevisions.Remove(Key);
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(Key) > 0;
		VoxelProxyData.Remove(Key);
		VolumeTransitionSignatures.Remove(Key);
		PreparedVolumeSignatures.Remove(Key);
		PreparedVolumeMeshes.Remove(Key);
	}

	for (const FVoxelSurfaceTileKey& SurfaceKey : SurfaceReady.Array())
	{
		if (!Hierarchy.AffectsSurface(SurfaceKey, InKey)) continue;
		SurfaceRevisions.Remove(SurfaceKey);
		SurfaceReady.Remove(SurfaceKey);
		SurfaceData.Remove(SurfaceKey);
		WaterData.Remove(SurfaceKey);
	}
	for (const FVoxelMacroTileKey& MacroKey : MacroReady.Array())
	{
		if (!Hierarchy.AffectsMacro(MacroKey, InKey)) continue;
		MacroRevisions.Remove(MacroKey);
		MacroReady.Remove(MacroKey);
		MacroData.Remove(MacroKey);
	}
}

void FVoxelViewManager::InvalidateNeighbors(const FIntVector& InKey)
{
	TSet<FIntVector> Neighbors;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (const int32 Sign : { -1, 1 })
		{
			FIntVector Key = InKey;
			Key[Axis] += Sign;
			if (!FineWanted.Contains(Key) && !FineReady.Contains(Key) && !FineActors.Contains(Key)) continue;
			const bool bRemoved = FineReady.Remove(Key) > 0;
			bReadyTerrainBranchesDirty |= bRemoved;
			if (bRemoved) MarkCoverageDirty(FineCoverageRect(Key));
			FineRevisions.Remove(Key);
			Neighbors.Add(Key);
		}
	}
	if (Neighbors.IsEmpty()) return;
	Scheduler.CancelMatching([&Neighbors](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildFineMesh && Neighbors.Contains(Stamp.Section);
	});
}

void FVoxelViewManager::Reset()
{
	++VolumePlanSerial;
	++HeightfieldPlanSerial;
	HeightfieldCoveragePlans.Reset();
	PendingHeightfieldPlanOwners.Reset();
	VolumeCoveragePlans.Reset();
	PendingVolumeHandoffBoxes.Reset();
	for (const TPair<FIntVector, TObjectPtr<AActor>>& Pair : FineActors)
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
		if (Pair.Value) Pair.Value->Destroy();
	}
	for (const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>>& Pair : WaterActors)
	{
		if (Pair.Value) Pair.Value->Destroy();
	}
	for (const TPair<FVoxelMacroTileKey, TObjectPtr<AActor>>& Pair : MacroActors)
	{
		if (Pair.Value) Pair.Value->Destroy();
	}
	for (const TPair<FVoxelHeightfieldNodeKey, TObjectPtr<AActor>>& Pair : HeightfieldTransitionActors)
	{
		if (Pair.Value) Pair.Value->Destroy();
	}
	PreparedData.Reset();
	PreparedEnvironmentRetentions.Reset();
	PreparedDataBytes = 0;
	PendingDataBytes = 0;
	SkippedProxyMeshes = 0;
	SkippedVolumeMeshes = 0;
	for (auto& Scans : DataScanIndices) Scans.Reset();
	FineActors.Reset();
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
	DesiredVolumeContexts.Reset();
	UnsubmittedVolumeOwners.Reset();
	VolumeUnbalancedFaceCount = 0;
	SurfaceRevisions.Reset();
	MacroRevisions.Reset();
	VisibleTerrainNodes.Reset();
	VisibleTerrainNodesByGroup.Reset();
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
	ProxySurfaceCoverage.Reset();
	SurfaceData.Reset();
	WaterData.Reset();
	MacroData.Reset();
	FineLastWanted.Reset();
	VoxelProxyLastWanted.Reset();
	SurfaceLastWanted.Reset();
	MacroLastWanted.Reset();
	Admissions = {};
	PriorityObservers.Reset();
	for (auto& Scans : AdmissionScanIndices) Scans.Reset();
	Publisher->Reset();
	OpenPublishGroups.Reset();
	AppliedInterestRevision = 0;
	LastActiveAdmissionKind = 4;
	LastActiveAdmissionPriority = MAX_int32;
	LastActiveDataKind = 4;
	AdmissionPriorities.Reset();
	LastResolvedFrontier = 0.0;
	MarkCoverageDirty();
	ActiveDirtyCoverageRects.Reset();
	bActiveCoverageFull = true;
	NextRetireCheck = 0.0;
	LastRetireMilliseconds = 0.0;
	NextRepresentationDebugLog = 0.0;
}

bool FVoxelViewManager::RequiresSectionData(const FIntVector& InKey) const
{
	return FineReady.Contains(InKey) || FineActors.Contains(InKey) || VisibleTerrainNodes.Contains({ InKey, 0 });
}

bool FVoxelViewManager::HasPrimaryRepresentation() const
{
	return !FineActors.IsEmpty() ||
		!VoxelProxyActors.IsEmpty() ||
		!SurfaceActors.IsEmpty() ||
		!MacroActors.IsEmpty();
}

FVoxelRepresentationReadiness FVoxelViewManager::GetRepresentationReadiness() const
{
	FVoxelRepresentationReadiness Result;
	for (const auto& Pair : FineActors)
	{
		if (Pair.Value && !Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value)) ++Result.PresentedFine;
	}
	for (const auto& Pair : VoxelProxyActors)
	{
		if (Pair.Value && !Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value)) ++Result.PresentedVoxelProxy;
	}
	for (const auto& Pair : SurfaceActors)
	{
		if (Pair.Value && !Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value)) ++Result.PresentedSurface;
	}
	for (const auto& Pair : MacroActors)
	{
		if (Pair.Value && !Pair.Value->IsHidden() && Publisher->IsPresented(Pair.Value)) ++Result.PresentedMacro;
	}
	return Result;
}

FVoxelPrimaryFineReadiness FVoxelViewManager::GetPrimaryFineReadiness(
	const TMap<FIntVector, FVoxelExactDemand>& InExact) const
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
		if (FineReady.Contains(Pair.Key))
		{
			++Result.Ready;
			AActor* Actor = FineActors.FindRef(Pair.Key);
			// 空结果也必须取得区域所有权；已准备但仍隐藏的网格不能放行出生。
			const bool bOwnsRegion = Module.GetCurrentInterest().TerrainPlan.Roots.IsEmpty() ||
				VisibleTerrainNodes.Contains({ Pair.Key, 0 });
			Result.Owned += bOwnsRegion ? 1 : 0;
			Result.Hidden += Actor && Actor->IsHidden() ? 1 : 0;
			Result.PublicationPending += Actor && !Publisher->IsCommitted(Actor) ? 1 : 0;
			if (bOwnsRegion && (!Actor || (!Actor->IsHidden() && Publisher->IsCommitted(Actor))))
			{
				++Result.Presented;
			}
		}
		AActor* Actor = FineActors.FindRef(Pair.Key);
		if (Actor && !Actor->IsHidden() && Publisher->IsPresented(Actor))
		{
			++Result.Renderable;
		}
	}
	return Result;
}

FVoxelPrimaryFineReadiness FVoxelViewManager::GetFineRadiusReadiness(
	const TConstArrayView<FIntVector> InFineKeys) const
{
	FVoxelPrimaryFineReadiness Result;
	for (const FIntVector& Key : InFineKeys)
	{
		++Result.Required;
		if (FineReady.Contains(Key))
		{
			++Result.Ready;
			AActor* Actor = FineActors.FindRef(Key);
			const bool bOwnsRegion = Module.GetCurrentInterest().TerrainPlan.Roots.IsEmpty() ||
				VisibleTerrainNodes.Contains({ Key, 0 });
			Result.Owned += bOwnsRegion ? 1 : 0;
			Result.Hidden += Actor && Actor->IsHidden() ? 1 : 0;
			Result.PublicationPending += Actor && !Publisher->IsCommitted(Actor) ? 1 : 0;
			if (bOwnsRegion && (!Actor || (!Actor->IsHidden() && Publisher->IsCommitted(Actor))))
			{
				++Result.Presented;
			}
		}
	}
	return Result;
}

void FVoxelViewManager::UpdateFineAndVoxelProxy(const TConstArrayView<FVector> InObservers)
{
	const FVoxelInterestSet& Interest = Module.GetCurrentInterest();
	FineWanted = Interest.FineSections ? *Interest.FineSections : TSet<FIntVector>();
	VoxelProxyWanted = Interest.VoxelProxy;
}

void FVoxelViewManager::UpdateSurface(const TConstArrayView<FVector> InObservers)
{
	SurfaceWanted = Module.GetCurrentInterest().Surface;
}

void FVoxelViewManager::UpdateMacro(const TConstArrayView<FVector> InObservers)
{
	MacroWanted = Module.GetCurrentInterest().Macro;
}

bool FVoxelViewManager::TryResolveFineWithoutMesh(
	const FIntVector& InSection,
	const uint64 InRevision)
{
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(InSection);
	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();
	if (!Section ||
		Section->Status != EVoxelSectionStatus::DataReady ||
		Section->CommittedRevision != InRevision ||
		Section->Blocks.Num() != ViewSectionSide * ViewSectionSide * ViewSectionSide ||
		!Registry)
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
		return Definition &&
			Definition->Shape == EVoxelShapeKind::FullCube &&
			Definition->bOccludes;
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
			if (!Neighbor ||
				Neighbor->Status != EVoxelSectionStatus::DataReady ||
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
	if (bPublished) MarkCoverageDirty(FineCoverageRect(InSection));
	return bPublished;
}
bool FVoxelViewManager::RequestFine(
	const FIntVector& InSection,
	const uint64 InRevision)
{
	const FVoxelSection* Section =
		Module.GetRuntime()->
			FindSection(
				InSection);

	if (!Section ||
		Section->Status !=
			EVoxelSectionStatus::DataReady)
	{
		return false;
	}

	FVoxelTaskStamp Stamp;
	Stamp.WorldEpoch =
		WorldEpoch;
	Stamp.Token =
		Section->Stamp.Token;
	Stamp.Revision =
		InRevision;
	Stamp.Section =
		InSection;


	if (Scheduler.Has(
		Stamp,
		EVoxelTaskKind::BuildFineMesh))
	{
		return false;
	}

	for (int32 Face = 0; Face < 6; ++Face)
	{
		FIntVector Neighbor = InSection;
		Neighbor[Face / 2] += (Face & 1) ? -1 : 1;
		const FVoxelSection* Data = Module.GetRuntime()->FindSection(Neighbor);
		if (Module.GetCurrentInterest().Exact.Contains(Neighbor) &&
			(!Data || Data->Status != EVoxelSectionStatus::DataReady)) return false;
	}
	FVoxelSectionSnapshot Snapshot;
	if (!Module.GetRuntime()->CaptureSnapshot(InSection, Snapshot)) return false;

	FVoxelTaskRequest Request;

	Request.Kind =
		EVoxelTaskKind::BuildFineMesh;

	const FVoxelExactDemand* Demand =
		Module.GetCurrentInterest().Exact.Find(InSection);
	Request.WorkClass =
		Demand && (Demand->bWarmupData || Demand->bMovementCriticalCollision ||
			Demand->bMovementCriticalFine)
			? EVoxelWorkClass::Critical
			: EVoxelWorkClass::Interactive;

	Request.Stamp =
		Stamp;
	Request.DistanceScore = MinimumObserverDistanceCells(
		FVector(InSection * ViewSectionSide + FIntVector(ViewSectionSide / 2)) * Module.BlockSize());
	Request.SourcePriority = Demand ? Demand->Priority : 0;
	Request.ForwardScore = Demand ? Demand->ForwardScore : 0.0;

	Request.InputBytes =
		Snapshot.Bytes();

	Request.ReservedBytes =
		48ull *
		1024ull *
		1024ull;

	const TSharedPtr<
		const FVoxelRegistrySnapshot,
		ESPMode::ThreadSafe> Registry =
			Module.GetRegistry();

	const TSharedPtr<
		const FVoxelShapeRegistry,
		ESPMode::ThreadSafe> Shapes =
			Module.GetShapes();

	Request.Execute =
		[
			Snapshot =
				MoveTemp(Snapshot),
			Registry,
			Shapes
		](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;

			Result.FineMesh =
				MakeShared<
					FVoxelSectionMeshResult>();

			Result.bSuccess =
				Registry &&
				Shapes &&
				FVoxelSectionMesher::Build(
					Snapshot,
					*Registry,
					*Shapes,
					*Result.FineMesh,
					&InCancel);

			return Result;
		};

	return Scheduler.Enqueue(MoveTemp(Request));
}

bool FVoxelViewManager::BuildVoxelProxySnapshot(
	const FVoxelVoxelProxyData& InData,
	const uint64 InRecipeHash,
	FVoxelSectionSnapshot& OutSnapshot,
	FString& OutError)
{
	if (InData.GridSide != InData.Key.GetGridSide() ||
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
	for (const FVoxelBlockState State : InData.Cells) Snapshot.Blocks.Add(State.Pack());

	for (int32 Face = 0; Face < 6; ++Face)
	{
		Snapshot.Known[Face] = InData.Known[Face];
		if (!Snapshot.Known[Face]) continue;
		const TArray<FVoxelBlockState>& Source = InData.Halo[Face];
		if (Source.Num() != InData.GridSide * InData.GridSide)
		{
			OutError = TEXT("Voxel proxy snapshot has invalid halo data");
			return false;
		}
		Snapshot.Halo[Face].Reserve(Source.Num());
		for (const FVoxelBlockState State : Source) Snapshot.Halo[Face].Add(State.Pack());
	}

	OutSnapshot = MoveTemp(Snapshot);
	OutError.Reset();
	return true;
}

bool FVoxelViewManager::RequestVoxelProxy(
	const FVoxelViewKey& InKey, const int32 InPriority, const bool bDataOnly)
{
	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		Module.GetGenerationConfig();
	if (!Config || !Config->Recipe) return false;
	const int32 Step = InKey.GetStep();
	const FVoxelTreeGenerationSettings& Tree = Config->Recipe->Settings.Ecology.Tree;
	const int32 Crown = Tree.bEnabled ? Tree.CrownRadius : 0;
	const int32 TreeHeight = Tree.bEnabled ? Tree.MaxHeight + Crown : 0;
	const int32 SampleStep = InKey.GetSampleStep();
	const int32 TreePadding = Tree.bEnabled ? (FMath::Max(1, (Crown - 1) / SampleStep) + 1) * SampleStep : 0;
	const int32 HorizontalMargin = FMath::Max(Step + Crown, TreePadding);
	FVoxelGenerationBounds OverlayBounds = InKey.GetBounds();
	OverlayBounds.Min -= FIntVector(HorizontalMargin, HorizontalMargin, Step + TreeHeight);
	OverlayBounds.Max += FIntVector(HorizontalMargin, HorizontalMargin, Step);
	if (!Module.IsAuthority())
	{
		TArray<FIntVector> Modified;

		Module.
			GetRuntime()->
			GetChangeIndex().
			Enumerate(
				OverlayBounds,
				Modified);

		if (!Modified.IsEmpty())
		{
			if (bDataOnly) return false;
			const uint64* Revision =
				VoxelProxyRevisions.
					Find(
						InKey);

			return Module.RequestRemoteRepresentation(
					EVoxelRepresentationWireType::
						VoxelProxy,
					{
						InKey.Coordinate,
						InKey.Level
					},
					Revision
						? *Revision
						: 0);

		}
	}

	const TSharedPtr<
		FVoxelGenerationPlanCache,
		ESPMode::ThreadSafe> Cache =
			Module.
			GetGenerationCache();
	const TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator =
		Module.GetGenerator();

	if (!Config ||
		!Cache)
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.SourcePriority = InPriority;

	Request.Kind =
		EVoxelTaskKind::
			BuildVoxelProxy;

	Request.WorkClass =
		EVoxelWorkClass::
			Boundary;

	Request.Stamp.WorldEpoch =
		WorldEpoch;

	Request.Stamp.ViewKey =
		InKey;

	Request.Stamp.Revision =
		Module.
			GetRuntime()->
			GetChangeHierarchy().
			GetVoxelProxyRevision(InKey);

	Request.Stamp.Token =
		HashCombineFast(
			GetTypeHash(
				InKey),
			GetTypeHash(
				Request.Stamp.
					Revision));

	Request.ReservedBytes =
		48ull *
		1024ull *
		1024ull;
	const FVoxelGenerationBounds ProxyBounds = InKey.GetBounds();
	Request.DistanceScore = Module.GetCurrentInterest().TerrainPlan.FineDependencies.Contains(InKey)
		? 0.0 : MinimumObserverDistanceCells(ProxyBounds);
	Request.ForwardScore = 0.0;

	if (Scheduler.Has(
			Request.Stamp,
			Request.Kind))
	{
		return false;
	}

	const FVoxelTaskKey DataKey { EVoxelTaskKind::GenerateVoxelProxy, Request.Stamp };
	TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> Prepared;
	if (bDataOnly)
	{
		if (PreparedData.Contains(DataKey) || Scheduler.Has(Request.Stamp, DataKey.Kind)) return false;
		Request.Kind = DataKey.Kind;
		const uint64 Side = InKey.GetGridSide();
		Request.ReservedBytes = 64ull * 1024ull +
			2ull * (Side * Side * Side + 6ull * Side * Side) * sizeof(FVoxelBlockState);
		Request.WorkClass = EVoxelWorkClass::Prefetch;
	}
	else
	{
		Prepared = PreparedData.FindRef(DataKey);
		if (!Prepared) return false;
		if (Prepared->bKnownEmptyMesh)
		{
			FVoxelTaskResult Empty;
			Empty.Stamp = Request.Stamp;
			Empty.VoxelProxy = Prepared->VoxelProxy;
			Empty.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
			if (!PublishVoxelProxy(Empty)) return false;
			++SkippedProxyMeshes;
			MarkCoverageDirty(VoxelProxyCoverageRect(InKey));
			RemovePreparedData(DataKey);
			return true;
		}
	}


	const TSharedPtr<
		const FVoxelRegistrySnapshot,
		ESPMode::ThreadSafe> Registry =
			Module.
				GetRegistry();

	const TSharedPtr<
		const FVoxelShapeRegistry,
		ESPMode::ThreadSafe> Shapes =
			Module.
				GetShapes();

	const uint64 RecipeHash =
		Module.
			GetManifest().
			RecipeHash;
	FVoxelOverlaySnapshotSet Overlays;
	FString OverlayError;
	const auto EnvironmentRetention = bDataOnly ? RetainPreparedEnvironment(DataKey, *Generator, OverlayBounds) : nullptr;
	if (bDataOnly && !Module.EnsureEnvironmentBounds(OverlayBounds, Request.WorkClass,
		InPriority, Request.DistanceScore, Request.ForwardScore).bReady) return false;
	if (bDataOnly && !Generator->EnsureCoarseOverlayBoundsReady(OverlayBounds, EVoxelWorkClass::Prefetch, InPriority).bReady) return false;
	if (bDataOnly && !Module.CaptureOverlays(OverlayBounds, Overlays, OverlayError)) return false;
	Request.InputBytes = Prepared ? Prepared->ResultBytes() : Overlays.GetAllocatedBytes();

	Request.Execute =
		[
			bDataOnly,
			Prepared,
			Config,
			Cache,
			Generator,
			EnvironmentRetention,
			Registry,
			Shapes,
			RecipeHash,
			Overlays = MoveTemp(Overlays),
			InKey,
			TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells
		](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;

			if (bDataOnly)
			{
				Result.VoxelProxy =
					MakeShared<
						FVoxelVoxelProxyData>();

				const FVoxelVoxelProxyBuilder Builder(
					Config.ToSharedRef(),
					Cache.ToSharedRef(),
					Generator);

				Result.bSuccess =
					Builder.Build(
						InKey,
						Overlays,
						*Result.
							VoxelProxy,
						Result.Error,
						&InCancel);
				if (Result.bSuccess && Registry)
				{
					FVoxelSectionSnapshot Snapshot;
					Result.bSuccess = BuildVoxelProxySnapshot(*Result.VoxelProxy, RecipeHash, Snapshot, Result.Error);
					Result.bKnownEmptyMesh = Result.bSuccess && FVoxelSectionMesher::IsKnownEmpty(Snapshot, *Registry);
				}

				return Result;
			}
			Result.bSuccess = true;
			Result.VoxelProxy = Prepared->VoxelProxy;

			if (!Result.bSuccess ||
				!Registry ||
				!Shapes)
			{
				return Result;
			}

			FVoxelSectionSnapshot Snapshot;
			if (!BuildVoxelProxySnapshot(*Result.VoxelProxy, RecipeHash, Snapshot, Result.Error))
			{
				Result.bSuccess = false;
				return Result;
			}
			Result.VoxelProxyMesh =
				MakeShared<
					FVoxelSectionMeshResult>();

			Result.bSuccess =
				FVoxelSectionMesher::
					Build(
						Snapshot,
						*Registry,
						*Shapes,
						*Result.
							VoxelProxyMesh,
						&InCancel, Result.VoxelProxy->Key.GetSampleStep() / VoxelViewLod::TexturePeriodCells(Result.VoxelProxy->Key.GetSampleStep(), TextureStretch));

			if (!Result.bSuccess &&
				Result.Error.IsEmpty())
			{
				Result.Error =
					TEXT(
						"Failed to mesh voxel proxy");
			}

			return Result;
		};

	if (bDataOnly) return EnqueuePreparedData(MoveTemp(Request));
	if (!Scheduler.Enqueue(MoveTemp(Request))) return false;
	RemovePreparedData(DataKey);
	return true;
}

bool FVoxelViewManager::RequestSurface(
	const FVoxelSurfaceTileKey& InKey, const int32 InPriority, const bool bDataOnly)
{
	if (!Module.IsAuthority())
	{
		const int32 Side =
			FVoxelSurfaceTileData::
				CellSide *
			(1 << InKey.Level);

		const FIntPoint Min =
			InKey.Coordinate *
			Side;

		const FVoxelGenerationSettings& Settings =
			Module.GetManifest().
				Settings;

		TArray<FIntVector> Modified;

		Module.GetRuntime()->
			GetChangeIndex().
			Enumerate(
				{
					FIntVector(
						Min.X,
						Min.Y,
						Settings.MinZ),
					FIntVector(
						Min.X + Side,
						Min.Y + Side,
						Settings.MaxZ)
				},
				Modified);

		if (!Modified.IsEmpty())
		{
			if (bDataOnly) return false;
			const uint64* Revision =
				SurfaceRevisions.Find(
					InKey);

			return Module.RequestRemoteRepresentation(
				EVoxelRepresentationWireType::
					SurfaceProxy,
				{
					FIntVector(
						InKey.Coordinate.X,
						InKey.Coordinate.Y,
						0),
					InKey.Level
				},
				Revision
					? *Revision
					: 0);

		}
	}

	const TSharedPtr<
		const FVoxelGenerationPipeline,
		ESPMode::ThreadSafe> Generator =
			Module.GetGenerator();

	const TSharedPtr<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> Config =
			Module.GetGenerationConfig();

	const TSharedPtr<
		const FVoxelRegistrySnapshot,
		ESPMode::ThreadSafe> Registry =
			Module.GetRegistry();

	if (!Generator ||
		!Config ||
		!Registry)
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.SourcePriority = InPriority;

	Request.Kind =
		EVoxelTaskKind::BuildSurface;

	Request.WorkClass =
		EVoxelWorkClass::Exploration;

	Request.Stamp.WorldEpoch =
		WorldEpoch;

	Request.Stamp.SurfaceKey =
		InKey;

	Request.Stamp.Revision =
		Module.GetRuntime()->
			GetChangeHierarchy().
			GetSurfaceRevision(InKey);

	Request.Stamp.Token =
		HashCombineFast(
			GetTypeHash(InKey),
			GetTypeHash(
				Request.Stamp.
					Revision));

	Request.ReservedBytes =
		40ull *
		1024ull *
		1024ull;
	Request.DistanceScore = MinimumObserverDistanceCells(SurfaceWorldCenter(InKey));
	Request.ForwardScore = 0.0;

	if (Scheduler.Has(
		Request.Stamp,
		Request.Kind))
	{
		return false;
	}

	const FVoxelTaskKey DataKey { EVoxelTaskKind::GenerateSurface, Request.Stamp };
	TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> Prepared;
	if (bDataOnly)
	{
		if (PreparedData.Contains(DataKey) || Scheduler.Has(Request.Stamp, DataKey.Kind)) return false;
		Request.Kind = DataKey.Kind;
		Request.WorkClass = EVoxelWorkClass::Prefetch;
	}
	else
	{
		Prepared = PreparedData.FindRef(DataKey);
		if (!Prepared) return false;
	}


	if (Prepared && Prepared->Surface)
	{
		const uint64 VertexBytes = sizeof(FVector) * 2 + sizeof(FVector2D) * 3 +
			sizeof(FLinearColor) + sizeof(FProcMeshTangent);
		const uint64 CellMeshBytes = 24ull * VertexBytes + 36ull * sizeof(int32);
		Request.ReservedBytes += Prepared->ResultBytes() +
			static_cast<uint64>(Prepared->Surface->DistantCells.Num()) * CellMeshBytes;
	}

	const FVoxelGenerationSettings Settings =
		Module.GetManifest().
			Settings;
	FVoxelOverlaySnapshotSet Overlays;
	const int32 Step = 1 << InKey.Level;
	const int32 Side = FVoxelSurfaceTileData::CellSide * Step;
	const int32 Margin = FMath::Max(Step, Settings.Ecology.Tree.bEnabled
		? Settings.Ecology.Tree.CrownRadius : 0);
	const FVoxelGenerationBounds OverlayBounds{
		FIntVector(InKey.Coordinate.X * Side - Margin, InKey.Coordinate.Y * Side - Margin, Settings.MinZ),
		FIntVector((InKey.Coordinate.X + 1) * Side + Margin, (InKey.Coordinate.Y + 1) * Side + Margin, Settings.MaxZ)
	};
	FString OverlayError;
	const auto EnvironmentRetention = bDataOnly ? RetainPreparedEnvironment(DataKey, *Generator, OverlayBounds) : nullptr;
	if (bDataOnly && !Module.EnsureEnvironmentBounds(OverlayBounds, Request.WorkClass,
		InPriority, Request.DistanceScore, Request.ForwardScore).bReady) return false;
	if (bDataOnly && !Generator->EnsureCoarseOverlayBoundsReady(OverlayBounds, EVoxelWorkClass::Prefetch, InPriority).bReady) return false;
	if (bDataOnly && !Module.CaptureOverlays(OverlayBounds, Overlays, OverlayError)) return false;
	Request.InputBytes = Prepared ? Prepared->ResultBytes() : Overlays.GetAllocatedBytes();

	Request.Execute =
		[
			bDataOnly,
			Prepared,
			Overlays = MoveTemp(Overlays),
			Generator,
			EnvironmentRetention,
			Config,
			Registry,
			Settings,
			InKey,
			TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells
		](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;

			if (bDataOnly)
			{
				Result.Surface =
					MakeShared<
						FVoxelSurfaceTileData>();

				const FVoxelSurfaceProxyBuilder Builder(
					Generator.ToSharedRef(),
					Config.ToSharedRef(),
					Settings,
					Overlays, Registry.ToSharedRef());
				FVoxelSurfaceBuildTiming Timing;

				Result.bSuccess =
					Builder.Build(
						InKey,
						*Result.Surface,
						Result.Error,
						&InCancel,
						&Timing);

				if (!Result.bSuccess) return Result;
				Result.Water = MakeShared<FVoxelWaterSurfaceTileData>();
				FVoxelWaterViewBuilder WaterBuilder;
				Result.bSuccess = WaterBuilder.Build(*Result.Surface, *Result.Water, Result.Error);
				if (!Result.bSuccess)
				{
					return Result;
				}
				return Result;
			}
			Result.bSuccess = true;
			Result.Surface = Prepared->Surface;

			if (Result.bSuccess)
			{
				Result.SurfaceMesh =
					MakeShared<
						FVoxelSectionMeshResult>();

				Result.bSuccess =
					FVoxelHeightfieldMesher::
						BuildBlockyTerrain(
							Result.Surface->Side,
							Result.Surface->Step,
							Result.Surface->GroundZ,
							Result.Surface->SurfaceMaterial,
							*Config,
							*Registry,
							*Result.SurfaceMesh,
							Result.Error,
							&InCancel,
							{},
							-0.02,
							TextureStretch);
				if (Result.bSuccess)
				{
				Result.bSuccess = FVoxelHeightfieldMesher::AppendDistantCells(
						Result.Surface->DistantCells,
						Result.Surface->Key.Coordinate * Result.Surface->GetTileSide(),
						Result.Surface->Step, *Registry, *Result.SurfaceMesh,
						Result.Error, &InCancel, TextureStretch);
				}
			}

			if (!Result.bSuccess)
			{
				return Result;
			}

			Result.Water = Prepared->Water;

			FVoxelSectionMeshResult WaterMesh;
			Result.bSuccess = FVoxelHeightfieldMesher::BuildWater(
				*Result.Water,
				*Registry,
				WaterMesh,
				Result.Error,
				&InCancel, TextureStretch);
			if (Result.bSuccess)
			{
				AppendNonEmptyBatches(*Result.SurfaceMesh, MoveTemp(WaterMesh));
			}


			return Result;
		};

	if (bDataOnly) return EnqueuePreparedData(MoveTemp(Request));
	if (!Scheduler.Enqueue(MoveTemp(Request))) return false;
	RemovePreparedData(DataKey);
	return true;
}

bool FVoxelViewManager::RequestMacro(
	const FVoxelMacroTileKey& InKey, const int32 InPriority, const bool bDataOnly)
{
	if (!Module.IsAuthority())
	{
		const int32 Side =
			FVoxelMacroTileData::
				CellSide *
			(
				FVoxelMacroTileData::
					BaseStep <<
				InKey.Level
			);

		const FIntPoint Min =
			InKey.Coordinate *
			Side;

		const FVoxelGenerationSettings& Settings =
			Module.GetManifest().
				Settings;

		TArray<FIntVector> Modified;

		Module.GetRuntime()->
			GetChangeIndex().
			Enumerate(
				{
					FIntVector(
						Min.X,
						Min.Y,
						Settings.MinZ),
					FIntVector(
						Min.X + Side,
						Min.Y + Side,
						Settings.MaxZ)
				},
				Modified);

		if (!Modified.IsEmpty())
		{
			if (bDataOnly) return false;
			const uint64* Revision =
				MacroRevisions.Find(
					InKey);

			return Module.RequestRemoteRepresentation(
				EVoxelRepresentationWireType::
					MacroTerrain,
				{
					FIntVector(
						InKey.Coordinate.X,
						InKey.Coordinate.Y,
						0),
					InKey.Level
				},
				Revision
					? *Revision
					: 0);

		}
	}

	const TSharedPtr<
		const FVoxelGenerationPipeline,
		ESPMode::ThreadSafe> Generator =
			Module.GetGenerator();

	const TSharedPtr<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> Config =
			Module.GetGenerationConfig();

	const TSharedPtr<
		const FVoxelRegistrySnapshot,
		ESPMode::ThreadSafe> Registry =
			Module.GetRegistry();

	if (!Generator ||
		!Config ||
		!Registry)
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.SourcePriority = InPriority;

	Request.Kind =
		EVoxelTaskKind::BuildMacro;

	Request.WorkClass =
		EVoxelWorkClass::Background;

	Request.Stamp.WorldEpoch =
		WorldEpoch;

	Request.Stamp.MacroKey =
		InKey;

	Request.Stamp.Revision =
		Module.GetRuntime()->
			GetChangeHierarchy().
			GetMacroRevision(InKey);

	Request.Stamp.Token =
		HashCombineFast(
			GetTypeHash(InKey),
			GetTypeHash(
				Request.Stamp.
					Revision));

	Request.ReservedBytes =
		32ull *
		1024ull *
		1024ull;
	Request.DistanceScore = MinimumObserverDistanceCells(MacroWorldCenter(InKey));
	Request.ForwardScore = 0.0;

	if (Scheduler.Has(
		Request.Stamp,
		Request.Kind))
	{
		return false;
	}

	const FVoxelTaskKey DataKey { EVoxelTaskKind::GenerateMacro, Request.Stamp };
	TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> Prepared;
	if (bDataOnly)
	{
		if (PreparedData.Contains(DataKey) || Scheduler.Has(Request.Stamp, DataKey.Kind)) return false;
		Request.Kind = DataKey.Kind;
		Request.WorkClass = EVoxelWorkClass::Prefetch;
	}
	else
	{
		Prepared = PreparedData.FindRef(DataKey);
		if (!Prepared) return false;
	}


	const FVoxelGenerationSettings Settings = Module.GetManifest().Settings;
	const int32 Step = FVoxelMacroTileData::BaseStep << InKey.Level;
	const int32 Side = FVoxelMacroTileData::CellSide * Step;
	const int32 Margin = FMath::Max(Step, Settings.Ecology.Tree.bEnabled
		? Settings.Ecology.Tree.CrownRadius : 0);
	FVoxelOverlaySnapshotSet Overlays;
	FString OverlayError;
	const FVoxelGenerationBounds Bounds {
		FIntVector(InKey.Coordinate.X * Side - Margin, InKey.Coordinate.Y * Side - Margin, Settings.MinZ),
		FIntVector((InKey.Coordinate.X + 1) * Side + Margin, (InKey.Coordinate.Y + 1) * Side + Margin, Settings.MaxZ)
	};
	const auto EnvironmentRetention = bDataOnly ? RetainPreparedEnvironment(DataKey, *Generator, Bounds) : nullptr;
	if (bDataOnly && !Module.EnsureEnvironmentBounds(Bounds, Request.WorkClass,
		InPriority, Request.DistanceScore, Request.ForwardScore).bReady) return false;
	if (bDataOnly && !Generator->EnsureCoarseOverlayBoundsReady(Bounds, EVoxelWorkClass::Prefetch, InPriority).bReady) return false;
	if (bDataOnly && !Module.CaptureOverlays(Bounds, Overlays, OverlayError))
	{
		return false;
	}
	Request.InputBytes = Prepared ? Prepared->ResultBytes() : Overlays.GetAllocatedBytes();
	Request.Execute =
		[
			bDataOnly,
			Prepared,
			Overlays = MoveTemp(Overlays),
			Settings,
			Generator,
			EnvironmentRetention,
			Config,
			Registry,
			InKey,
			TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells
		](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;

			if (bDataOnly)
			{
				Result.Macro =
					MakeShared<
						FVoxelMacroTileData>();

				const FVoxelMacroTerrainBuilder Builder(
					Generator.ToSharedRef(), Config.ToSharedRef(), Settings, Overlays, Registry.ToSharedRef());

				Result.bSuccess =
					Builder.Build(
						InKey,
						*Result.Macro,
						Result.Error,
						&InCancel);

				return Result;
			}
			Result.bSuccess = true;
			Result.Macro = Prepared->Macro;

			if (Result.bSuccess)
			{
				Result.MacroMesh =
					MakeShared<
						FVoxelSectionMeshResult>();

				Result.bSuccess =
					FVoxelHeightfieldMesher::BuildMacro(
							*Result.Macro,
							*Config,
							*Registry,
							*Result.MacroMesh,
							Result.Error,
							&InCancel, TextureStretch);
			}

			return Result;
		};

	if (bDataOnly) return EnqueuePreparedData(MoveTemp(Request));
	if (!Scheduler.Enqueue(MoveTemp(Request))) return false;
	RemovePreparedData(DataKey);
	return true;
}

bool FVoxelViewManager::HasRenderableMesh(const FVoxelSectionMeshResult& InMesh)
{
	for (const FVoxelRenderBatch& Batch : InMesh.Batches)
	{
		if (!Batch.Mesh.Vertices.IsEmpty() && !Batch.Mesh.Triangles.IsEmpty()) return true;
	}
	return false;
}

bool FVoxelViewManager::PublishFine(const FVoxelTaskResult& InResult)
{
	const FIntVector& Key = InResult.Stamp.Section;
	if (!InResult.FineMesh || !FineWanted.Contains(Key)) return false;
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(Key);
	if (!Section || Section->Stamp.Epoch != InResult.Stamp.WorldEpoch ||
		Section->Stamp.Token != InResult.Stamp.Token || Section->CommittedRevision != InResult.Stamp.Revision)
	{
		return false;
	}

	FineReady.Add(Key);
	TrackReadyTerrainNode({ Key, 0 });
	FineLastWanted.Add(Key, FPlatformTime::Seconds());
	FineRevisions.Add(Key, InResult.Stamp.Revision);
	if (!HasRenderableMesh(*InResult.FineMesh) && !FineActors.Contains(Key))
	{
		return true;
	}

	AActor* Host = FineActors.FindRef(Key);
	if (!Publisher->Stage(BeginPublishGroup({0, FIntPoint(Key.X, Key.Y), 0}),
		Host, FVector(Key * ViewSectionSide) * Module.BlockSize(),
		Module.BlockSize(), MoveTemp(*InResult.FineMesh), 0))
	{
		bReadyTerrainBranchesDirty |= FineReady.Remove(Key) > 0;
		FineRevisions.Remove(Key);
		return false;
	}
	FineActors.Add(Key, Host);
	return true;
}

bool FVoxelViewManager::PublishVoxelProxy(const FVoxelTaskResult& InResult)
{
	const FVoxelViewKey& Key = InResult.Stamp.ViewKey;
	if (!InResult.VoxelProxy || !InResult.VoxelProxyMesh || !VoxelProxyWanted.Contains(Key) ||
		InResult.Stamp.Token != HashCombineFast(GetTypeHash(Key), GetTypeHash(InResult.Stamp.Revision)) ||
		Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(Key) != InResult.Stamp.Revision)
	{
		return false;
	}

	VoxelProxyData.Add(Key, InResult.VoxelProxy);
	PreparedVolumeSignatures.Remove(Key);
	PreparedVolumeMeshes.Remove(Key);
	FVoxelBoundaryTransitionContext BaseContext;
	BaseContext.Owner = Key;
	VolumeTransitionSignatures.Add(Key,
		BaseContext.Signature(InResult.VoxelProxy->Revision));
	UpdateProxySurfaceCoverage(*InResult.VoxelProxy);
	VoxelProxyReady.Add(Key);
	TrackReadyTerrainNode(Key);
	VoxelProxyLastWanted.Add(Key, FPlatformTime::Seconds());
	VoxelProxyRevisions.Add(Key, InResult.Stamp.Revision);
	if (!HasRenderableMesh(*InResult.VoxelProxyMesh) && !VoxelProxyActors.Contains(Key))
	{
		return true;
	}

	AActor* Host = VoxelProxyActors.FindRef(Key);
	if (!Publisher->Stage(BeginPublishGroup(
		{1, FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}), Host,
		FVector(Key.GetBounds().Min) * Module.BlockSize(),
		Module.BlockSize() * Key.GetSampleStep(), MoveTemp(*InResult.VoxelProxyMesh), 1))
	{
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(Key) > 0;
		VoxelProxyData.Remove(Key);
		ProxySurfaceCoverage.Remove(Key);
		VoxelProxyRevisions.Remove(Key);
		VolumeTransitionSignatures.Remove(Key);
		return false;
	}
	VoxelProxyActors.Add(Key, Host);
	return true;
}

bool FVoxelViewManager::PublishSurface(const FVoxelTaskResult& InResult)
{
	const FVoxelSurfaceTileKey& Key = InResult.Stamp.SurfaceKey;
	if (!InResult.Surface || !InResult.SurfaceMesh || !SurfaceWanted.Contains(Key) ||
		InResult.Stamp.Token != HashCombineFast(GetTypeHash(Key), GetTypeHash(InResult.Stamp.Revision)) ||
		Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(Key) != InResult.Stamp.Revision)
	{
		return false;
	}

	AActor* Host = SurfaceActors.FindRef(Key);
	const int32 TileSide = InResult.Surface->GetTileSide();
	if (!Publisher->Stage(BeginPublishGroup({2, Key.Coordinate, Key.Level}), Host,
		FVector(Key.Coordinate.X * TileSide, Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(),
		Module.BlockSize(), MoveTemp(*InResult.SurfaceMesh), 2))
	{
		SurfaceReady.Remove(Key);
		return false;
	}
	SurfaceActors.Add(Key, Host);
	SurfaceReady.Add(Key);
	SurfaceLastWanted.Add(Key, FPlatformTime::Seconds());
	SurfaceRevisions.Add(Key, InResult.Stamp.Revision);
	return true;
}

void FVoxelViewManager::PublishWater(
	const FVoxelTaskResult& InResult)
{
	const FVoxelSurfaceTileKey& Key =
		InResult.Stamp.SurfaceKey;

	if (!InResult.Water ||
		!InResult.WaterMesh ||
		!SurfaceWanted.Contains(Key) ||
		InResult.Stamp.Token !=
			HashCombineFast(
				GetTypeHash(Key),
				GetTypeHash(
					InResult.Stamp.
						Revision)) ||
		Module.GetRuntime()->
			GetChangeHierarchy().
			GetSurfaceRevision(Key) !=
			InResult.Stamp.Revision)
	{
		return;
	}

	AActor* Host =
		WaterActors.FindRef(
			Key);

	const int32 TileSide =
		FVoxelSurfaceTileData::
			CellSide *
		InResult.Water->Step;

	if (Publisher->Stage(
		BeginPublishGroup({2, Key.Coordinate, Key.Level}), Host,
		FVector(
			Key.Coordinate.X *
				TileSide,
			Key.Coordinate.Y *
				TileSide,
			0) *
			Module.BlockSize(),
		Module.BlockSize(),
		MoveTemp(*InResult.WaterMesh), 2))
	{
		WaterActors.Add(
			Key,
			Host);
	}
}

bool FVoxelViewManager::PublishMacro(
	const FVoxelTaskResult& InResult)
{
	const FVoxelMacroTileKey& Key =
		InResult.Stamp.MacroKey;

	if (!InResult.Macro ||
		!InResult.MacroMesh ||
		!MacroWanted.Contains(Key) ||
		InResult.Stamp.Token !=
			HashCombineFast(
				GetTypeHash(Key),
				GetTypeHash(
					InResult.Stamp.
						Revision)) ||
		Module.GetRuntime()->
			GetChangeHierarchy().
			GetMacroRevision(Key) !=
			InResult.Stamp.Revision)
	{
		return false;
	}

	AActor* Host =
		MacroActors.FindRef(
			Key);

	const int32 TileSide =
		InResult.Macro->
			GetTileSide();

	if (Publisher->Stage(
		BeginPublishGroup({3, Key.Coordinate, Key.Level}), Host,
		FVector(
			Key.Coordinate.X *
				TileSide,
			Key.Coordinate.Y *
				TileSide,
			0) *
			Module.BlockSize(),
		Module.BlockSize(),
		MoveTemp(*InResult.MacroMesh), 3))
	{
		MacroActors.Add(
			Key,
			Host);

		MacroReady.Add(
			Key);
		MacroLastWanted.Add(Key, FPlatformTime::Seconds());

		MacroRevisions.Add(
			Key,
			InResult.Stamp.Revision);

		return true;
	}

	MacroReady.Remove(
		Key);

	return false;
}
