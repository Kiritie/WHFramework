#include "Voxel/Rendering/VoxelViewManager.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
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

namespace
{
	constexpr int32 ViewSectionSide = 16;

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
	Publisher->Tick();
	bCoverageDirty |= Publisher->NeedsUpdate();
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
		AppliedInterestRevision = InInterestRevision;
		bCoverageDirty = true;
	}

	if (CVarVoxelDebugRepresentation.GetValueOnGameThread() > 0 &&
		Now >= NextRepresentationDebugLog)
	{
		LogRepresentationState(InObservers);
		NextRepresentationDebugLog = Now + 5.0;
	}

	if (bCoverageDirty && !Publisher->IsBusy())
	{
		const double CoverageStart = FPlatformTime::Seconds();
		bCoverageDirty = false;
		ResolveTransitionVisibility();
		if (!Publisher->IsBusy() && Now >= NextCoverageCheck) CleanupRetiredRepresentations(Now);
		LastCoverageMilliseconds = (FPlatformTime::Seconds() - CoverageStart) * 1000.0;
		if (Now >= NextCoverageCheck) NextCoverageCheck = Now + RetireDelaySeconds;
		for (const TPair<FIntVector, TObjectPtr<AActor>>& Pair : FineActors)
		{
			bCoverageDirty |= !FineWanted.Contains(Pair.Key);
		}
		for (const TPair<FVoxelViewKey, TObjectPtr<AActor>>& Pair : VoxelProxyActors)
		{
			bCoverageDirty |= !VoxelProxyWanted.Contains(Pair.Key);
		}
		for (const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>>& Pair : SurfaceActors)
		{
			bCoverageDirty |= !SurfaceWanted.Contains(Pair.Key);
		}
		for (const TPair<FVoxelMacroTileKey, TObjectPtr<AActor>>& Pair : MacroActors)
		{
			bCoverageDirty |= !MacroWanted.Contains(Pair.Key);
		}
	}
	ProcessAdmissions();
}

void FVoxelViewManager::ProcessAdmissions()
{
	const FVoxelInterestSet& Interest = Module.GetCurrentInterest();
	const double Frontier = ResolveAdmissionFrontier();
	LastResolvedFrontier = Frontier;
	const FVoxelViewSettings& Settings = Module.GetViewSettings();
	const int32 BuildLimits[] = { Settings.FineBuildsPerFrame, Settings.VoxelProxyBuildsPerFrame,
		Settings.SurfaceBuildsPerFrame, Settings.MacroBuildsPerFrame };
	double RemainingAdmissionSeconds = Settings.BuildAdmissionMilliseconds / 1000.0;
	// 各表示共享同一个空间前沿，远景不能越过尚未完成的近景。
	for (int32 Kind = 0; Kind < 4 && RemainingAdmissionSeconds > 0.0; ++Kind)
	{
		const TArray<int32>& Lane = Interest.AdmissionLanes[Kind];
		int32 First = 0;
		while (First < Lane.Num() && (IsAdmissionSatisfied(Admissions[Lane[First]]) ||
			IsAdmissionTerminalFailure(Admissions[Lane[First]])))
		{
			++First;
		}
		if (First == Lane.Num()) continue;
		if (Admissions[Lane[First]].DistanceCells > Frontier) continue;
		int32& Scan = AdmissionScanIndices[Kind];
		if (Scan < First || Scan >= Lane.Num() || Admissions[Lane[Scan]].DistanceCells > Frontier)
		{
			Scan = First;
		}
		const double AdmissionStart = FPlatformTime::Seconds();
		const double Deadline = AdmissionStart + RemainingAdmissionSeconds;
		int32 Submitted = 0;
		const int32 MaximumAttempts = FMath::Min(Lane.Num() - Scan, FMath::Max(64, BuildLimits[Kind] * 8));
		for (int32 Attempt = 0; Attempt < MaximumAttempts && Scan < Lane.Num() &&
			Submitted < BuildLimits[Kind] && FPlatformTime::Seconds() < Deadline; ++Attempt)
		{
			const FVoxelViewAdmission& Admission = Admissions[Lane[Scan]];
			if (Admission.DistanceCells > Frontier)
			{
				Scan = First;
				break;
			}
			++Scan;
			if (!IsAdmissionSatisfied(Admission) && !IsAdmissionTerminalFailure(Admission))
			{
				Submitted += TrySubmitAdmission(Admission) ? 1 : 0;
			}
		}
		RemainingAdmissionSeconds -= FPlatformTime::Seconds() - AdmissionStart;
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
	for (int32& Scan : AdmissionScanIndices) Scan = 0;
}

void FVoxelViewManager::SortAdmissionsByPriority(TArray<FVoxelViewAdmission>& InOutAdmissions)

{
	InOutAdmissions.Sort([](const FVoxelViewAdmission& A, const FVoxelViewAdmission& B)
	{
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

bool FVoxelViewManager::IsAdmissionSatisfied(const FVoxelViewAdmission& A) const
{
	switch (A.Kind)
	{
	case EVoxelViewAdmissionKind::Fine: return FineReady.Contains(A.FineKey);
	case EVoxelViewAdmissionKind::VoxelProxy: return VoxelProxyReady.Contains(A.ProxyKey);
	case EVoxelViewAdmissionKind::Surface: return SurfaceReady.Contains(A.SurfaceKey);
	case EVoxelViewAdmissionKind::Macro: return MacroReady.Contains(A.MacroKey);
	default: return true;
	}
}

bool FVoxelViewManager::IsAdmissionTerminalFailure(const FVoxelViewAdmission& A) const
{
	if (A.Kind != EVoxelViewAdmissionKind::Fine) return false;
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(A.FineKey);
	return Section && Section->Status == EVoxelSectionStatus::Failed;
}

double FVoxelViewManager::GetDataAdmissionLimit() const
{
	return ResolveAdmissionFrontier() + ViewSectionSide * 2.0;
}

double FVoxelViewManager::ResolveAdmissionFrontier() const
{
	return ResolveAdmissionFrontier(
		Module.GetCurrentInterest().Admissions,
		[this](const FVoxelViewAdmission& Admission)
		{
			return IsAdmissionSatisfied(Admission) || IsAdmissionTerminalFailure(Admission);
		},
		AdmissionBandWidthCells);
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
			return (!Revision || *Revision != Section->CommittedRevision) && RequestFine(A.FineKey, Section->CommittedRevision);
		}
		return false;
	case EVoxelViewAdmissionKind::VoxelProxy: return RequestVoxelProxy(A.ProxyKey);
	case EVoxelViewAdmissionKind::Surface: return RequestSurface(A.SurfaceKey);
	case EVoxelViewAdmissionKind::Macro: return RequestMacro(A.MacroKey);
	default: return false;
	}
}

void FVoxelViewManager::CancelStaleViewTasks()
{
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

	const FVoxelTaskDiagnostics SchedulerStats = Scheduler.GetDiagnostics();
	const FVoxelTerrainViewPlan& Plan = Module.GetCurrentInterest().TerrainPlan;
	UE_LOG(LogTemp, Display, TEXT("Voxel terrain plan: roots=%d leaves=%d required=%d presented=%d budgetLimited=%d overBudget=%d"),
		Plan.Roots.Num(), Plan.Leaves.Num(), Plan.Required.Num(), VisibleTerrainNodes.Num(), Plan.bBudgetLimited, Plan.OverBudgetLeaves);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Voxel frontier=%.1f admissions=%d pending=%d running=%d surfaceP=%d surfaceR=%d macroP=%d macroR=%d coverageMs=%.3f"),
		LastResolvedFrontier,
		Admissions.Num(),
		SchedulerStats.Pending,
		SchedulerStats.Running,
		SchedulerStats.PendingByKind.FindRef(EVoxelTaskKind::BuildSurface),
		SchedulerStats.RunningByKind.FindRef(EVoxelTaskKind::BuildSurface),
		SchedulerStats.PendingByKind.FindRef(EVoxelTaskKind::BuildMacro),
		SchedulerStats.RunningByKind.FindRef(EVoxelTaskKind::BuildMacro),
		LastCoverageMilliseconds);

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
		if (SurfaceWanted.Contains(Key)) OutCoverage.Add(SurfaceCoverageRect(Key));
	}
}

void FVoxelViewManager::GatherReadyWantedMacroRects(TArray<FVoxelCoverageRect>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(MacroReady.Num());
	for (const FVoxelMacroTileKey& Key : MacroReady)
	{
		if (MacroWanted.Contains(Key)) OutCoverage.Add(MacroCoverageRect(Key));
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
	const int32 Step = InData.Key.GetStep();
	const FVoxelGenerationBounds Bounds = InData.Key.GetBounds();
	for (int32 Y = 0; Y < ViewSectionSide; ++Y)
	{
		int32 RunStart = INDEX_NONE;
		for (int32 X = 0; X <= ViewSectionSide; ++X)
		{
			const bool bCovered = X < ViewSectionSide && VoxelCoverage::HasProxyTopSurfaceInColumn(InData, X, Y);
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
	TArray<FVoxelCoverageRect> ProxyCoverage;
	GatherReadyWantedProxySurfaceRects(Target, ProxyCoverage);
	if (VoxelCoverage::IsFullyCovered2D(Target, ProxyCoverage)) return true;
	TArray<FVoxelCoverageRect> MacroCoverage;
	GatherReadyWantedMacroRects(MacroCoverage);
	return VoxelCoverage::IsFullyCovered2D(Target, MacroCoverage);
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

void FVoxelViewManager::ResolveTransitionVisibility()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewHandoff);
	Publisher->BeginBatch();
	const FVoxelTerrainViewPlan& Plan = Module.GetCurrentInterest().TerrainPlan;
	const bool bUsesTerrainPlan = !Plan.Roots.IsEmpty();
	TSet<FVoxelViewKey> TargetTerrainNodes;
	if (bUsesTerrainPlan)
	{
		const TSet<FVoxelViewKey>& Previous = VisibleTerrainNodes;
		TSet<FVoxelViewKey> ReadyNodes = VoxelProxyReady;
		for (const FIntVector& Key : FineReady) ReadyNodes.Add({ Key, 0 });
		Plan.ResolveVisible([this](const FVoxelViewKey& Key)
		{
			return Key.Level == 0 ? FineReady.Contains(Key.Coordinate) : VoxelProxyReady.Contains(Key);
		}, TargetTerrainNodes, &Previous, &ReadyNodes);
		const uint8 RootLevel = Plan.Roots.CreateConstIterator()->Level;
		for (const FVoxelViewKey& Key : Previous)
		{
			FVoxelViewKey Root = Key;
			while (Root.Level < RootLevel)
			{
				Root = Root.GetParent();
			}
			if (Plan.Roots.Contains(Root))
			{
				continue;
			}
			const bool bRetain = Key.Level == 0
				? IsFineInsideRenderDomain(Key.Coordinate) && !IsFineReplacementReady(Key.Coordinate)
				: IsProxyInsideRenderDomain(Key) && !IsProxyReplacementReady(Key);
			if (bRetain)
			{
				TargetTerrainNodes.Add(Key);
			}
		}
	}
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
					for (const FVoxelCoverageRect& Rect : *Rects)
					{
						ProxySurfaceBoxes.Add(Column(Rect));
					}
				}
			}
		}
	}
	else if (!bUsesTerrainPlan)
	{
		for (const FIntVector& Key : FineReady)
		{
			if (FineWanted.Contains(Key))
			{
				FineBoxes.Add(Box(FineCoverageBox(Key)));
			}
		}
		for (const auto& Pair : FineActors)
		{
			if (FineWanted.Contains(Pair.Key) && !FineReady.Contains(Pair.Key) && Publisher->IsPresented(Pair.Value))
			{
				FineBoxes.Add(Box(FineCoverageBox(Pair.Key)));
			}
		}
	}
	VoxelMeshClipper::NormalizeBoxes(FineBoxes);
	VoxelMeshClipper::NormalizeBoxes(ProxyBoxes);
	VoxelMeshClipper::NormalizeBoxes(ProxySurfaceBoxes);
	for (const TPair<FIntVector, TObjectPtr<AActor>>& Pair : FineActors)
	{
		if (!Pair.Value) continue;
		if (bUsesTerrainPlan)
		{
			const bool bVisible = TargetTerrainNodes.Contains({ Pair.Key, 0 });
			if (bVisible)
			{
				Publisher->SetCoverage(Pair.Value, {});
			}
			Publisher->SetHidden(Pair.Value, !bVisible);
			continue;
		}
		const bool bWanted = FineWanted.Contains(Pair.Key);
		const bool bHidden = !bWanted && IsFineReplacementReady(Pair.Key);
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
		if (!bHidden) Publisher->SetCoverage(Pair.Value, MoveTemp(Exclusions));
		Publisher->SetHidden(Pair.Value, bHidden);
	}
	for (const TPair<FVoxelViewKey, TObjectPtr<AActor>>& Pair : VoxelProxyActors)
	{
		if (!Pair.Value) continue;
		if (bUsesTerrainPlan)
		{
			const bool bVisible = TargetTerrainNodes.Contains(Pair.Key);
			if (bVisible)
			{
				Publisher->SetCoverage(Pair.Value, {});
			}
			Publisher->SetHidden(Pair.Value, !bVisible);
			continue;
		}
		const bool bWanted = VoxelProxyWanted.Contains(Pair.Key);
		const bool bHidden = !bWanted && IsProxyReplacementReady(Pair.Key);
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
		if (!bHidden) Publisher->SetCoverage(Pair.Value, MoveTemp(Exclusions));
		Publisher->SetHidden(Pair.Value, bHidden);
	}
	for (const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>>& Pair : SurfaceActors)
	{
		if (!Pair.Value) continue;
		const bool bWanted = SurfaceWanted.Contains(Pair.Key);
		const bool bHidden = !bWanted && IsSurfaceReplacementReady(Pair.Key);
		TArray<FBox> Exclusions = FineBoxes;
		Exclusions.Append(ProxySurfaceBoxes);
		for (const FVoxelSurfaceTileKey& Key : SurfaceReady)
		{
			if (SurfaceWanted.Contains(Key) && Key != Pair.Key && (!bWanted || Key.Level < Pair.Key.Level))
				Exclusions.Add(Column(SurfaceCoverageRect(Key)));
		}
		if (!bWanted)
		{
			for (const FVoxelMacroTileKey& Key : MacroReady)
			{
				if (MacroWanted.Contains(Key)) Exclusions.Add(Column(MacroCoverageRect(Key)));
			}
		}
		if (!bHidden)
		{
			Publisher->SetCoverage(Pair.Value, Exclusions);
			if (AActor* Water = WaterActors.FindRef(Pair.Key)) Publisher->SetCoverage(Water, MoveTemp(Exclusions));
		}
		Publisher->SetHidden(Pair.Value, bHidden);
		if (AActor* Water = WaterActors.FindRef(Pair.Key)) Publisher->SetHidden(Water, bHidden);
	}
	for (const TPair<FVoxelMacroTileKey, TObjectPtr<AActor>>& Pair : MacroActors)
	{
		if (!Pair.Value) continue;
		const bool bWanted = MacroWanted.Contains(Pair.Key);
		const bool bHidden = !bWanted && IsMacroReplacementReady(Pair.Key);
		TArray<FBox> Exclusions = FineBoxes;
		Exclusions.Append(ProxySurfaceBoxes);
		for (const FVoxelSurfaceTileKey& Key : SurfaceReady)
		{
			if (SurfaceWanted.Contains(Key)) Exclusions.Add(Column(SurfaceCoverageRect(Key)));
		}
		for (const FVoxelMacroTileKey& Key : MacroReady)
		{
			if (MacroWanted.Contains(Key) && Key != Pair.Key && (!bWanted || Key.Level < Pair.Key.Level))
				Exclusions.Add(Column(MacroCoverageRect(Key)));
		}
		if (!bHidden) Publisher->SetCoverage(Pair.Value, MoveTemp(Exclusions));
		Publisher->SetHidden(Pair.Value, bHidden);
	}
	if (!Publisher->EndBatch([this, Nodes = MoveTemp(TargetTerrainNodes)]() mutable
	{
		VisibleTerrainNodes = MoveTemp(Nodes);
	}))
	{
		bCoverageDirty = true;
	}
}

void FVoxelViewManager::CleanupRetiredRepresentations(const double InNow)
{
	auto CanRetire = [InNow](const double InLastWanted, const bool bReplacementReady, const bool bInsideDomain)
	{
		const double Age = InNow - InLastWanted;
		return Age >= RetireDelaySeconds && (bReplacementReady || (!bInsideDomain && Age >= OutsideDomainRetireDelaySeconds));
	};

	TSet<FIntVector> FineRetireKeys = FineReady;
	for (const auto& Pair : FineActors) FineRetireKeys.Add(Pair.Key);
	for (const FIntVector Key : FineRetireKeys)
	{
		if (FineWanted.Contains(Key) || VisibleTerrainNodes.Contains({ Key, 0 }) || !CanRetire(FineLastWanted.FindRef(Key), IsFineReplacementReady(Key), IsFineInsideRenderDomain(Key))) continue;
		if (AActor* Actor = FineActors.FindRef(Key))
		{
			Publisher->Forget(Actor);
			Actor->Destroy();
		}
		FineActors.Remove(Key);
		FineReady.Remove(Key);
		FineRevisions.Remove(Key);
		FineLastWanted.Remove(Key);
	}
	TSet<FVoxelViewKey> ProxyRetireKeys = VoxelProxyReady;
	for (const auto& Pair : VoxelProxyActors) ProxyRetireKeys.Add(Pair.Key);
	for (const FVoxelViewKey Key : ProxyRetireKeys)
	{
		if (VoxelProxyWanted.Contains(Key) || VisibleTerrainNodes.Contains(Key) || !CanRetire(VoxelProxyLastWanted.FindRef(Key), IsProxyReplacementReady(Key), IsProxyInsideRenderDomain(Key))) continue;
		if (AActor* Actor = VoxelProxyActors.FindRef(Key))
		{
			Publisher->Forget(Actor);
			Actor->Destroy();
		}
		VoxelProxyActors.Remove(Key);
		VoxelProxyReady.Remove(Key);
		VoxelProxyData.Remove(Key);
		ProxySurfaceCoverage.Remove(Key);
		VoxelProxyRevisions.Remove(Key);
		VoxelProxyLastWanted.Remove(Key);
	}
	TSet<FVoxelSurfaceTileKey> SurfaceRetireKeys = SurfaceReady;
	for (const auto& Pair : SurfaceActors) SurfaceRetireKeys.Add(Pair.Key);
	for (const FVoxelSurfaceTileKey Key : SurfaceRetireKeys)
	{
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
	}
	TSet<FVoxelMacroTileKey> MacroRetireKeys = MacroReady;
	for (const auto& Pair : MacroActors) MacroRetireKeys.Add(Pair.Key);
	for (const FVoxelMacroTileKey Key : MacroRetireKeys)
	{
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
		bCoverageDirty |= PublishFine(InResult);
		return true;
	case EVoxelTaskKind::BuildVoxelProxy:
		bCoverageDirty |= PublishVoxelProxy(InResult);
		return true;
	case EVoxelTaskKind::BuildSurface:
		if (InResult.Surface && SurfaceWanted.Contains(InResult.Stamp.SurfaceKey) && PublishSurface(InResult))
		{
			const FVoxelSurfaceTileKey Key = InResult.Stamp.SurfaceKey;
			SurfaceData.Add(Key, InResult.Surface);
			bCoverageDirty = true;
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
			bCoverageDirty = true;
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
				Snapshot, *Registry, *Shapes, *Result.VoxelProxyMesh, &InCancel, Result.VoxelProxy->Key.GetStep() / VoxelViewLod::TexturePeriodCells(Result.VoxelProxy->Key.GetStep(), TextureStretch));
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
				8, TextureStretch);
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
			Result.bSuccess = Config && FVoxelHeightfieldMesher::BuildBlockyTerrain(
				Result.Macro->Side,
				Result.Macro->Step,
				Result.Macro->Height,
				Result.Macro->SurfaceClass,
				*Config,
				*Registry,
				*Result.MacroMesh,
				Result.Error,
				&InCancel,
				Result.Macro->ForestCoverage,
				-0.05,
				16, TextureStretch);
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
	bCoverageDirty = true;
	for (const FVoxelRepresentationWireKey& WireKey : InInvalidate.Keys)
	{
		const FVoxelViewKey VoxelKey { WireKey.Coordinate, WireKey.Level };
		VoxelProxyReady.Remove(VoxelKey);
		VoxelProxyData.Remove(VoxelKey);
		ProxySurfaceCoverage.Remove(VoxelKey);
		VoxelProxyRevisions.Remove(VoxelKey);
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
}

void FVoxelViewManager::InvalidateSection(const FIntVector& InKey)
{
	InvalidateNeighbors(InKey);
	bCoverageDirty = true;
	FineRevisions.Remove(InKey);
	FineReady.Remove(InKey);
	Scheduler.CancelSection(InKey);
	const FVoxelChangeHierarchy& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	for (const FVoxelViewKey& Key : VoxelProxyReady.Array())
	{
		if (!Hierarchy.AffectsVoxelProxy(Key, InKey)) continue;
		VoxelProxyRevisions.Remove(Key);
		VoxelProxyReady.Remove(Key);
		VoxelProxyData.Remove(Key);
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
			FineReady.Remove(Key);
			FineRevisions.Remove(Key);
			Neighbors.Add(Key);
		}
	}
	Scheduler.CancelMatching([&Neighbors](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildFineMesh && Neighbors.Contains(Stamp.Section);
	});
	bCoverageDirty = true;
}

void FVoxelViewManager::Reset()
{
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
	FineActors.Reset();
	FineRevisions.Reset();
	VoxelProxyActors.Reset();
	VoxelProxyRevisions.Reset();
	SurfaceActors.Reset();
	WaterActors.Reset();
	MacroActors.Reset();
	SurfaceRevisions.Reset();
	MacroRevisions.Reset();
	VisibleTerrainNodes.Reset();
	FineWanted.Reset();
	VoxelProxyWanted.Reset();
	SurfaceWanted.Reset();
	MacroWanted.Reset();
	FineReady.Reset();
	VoxelProxyReady.Reset();
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
	for (int32& Scan : AdmissionScanIndices) Scan = 0;
	Publisher->Reset();
	AppliedInterestRevision = 0;
	LastResolvedFrontier = 0.0;
	bCoverageDirty = true;
	NextCoverageCheck = 0.0;
	NextRepresentationDebugLog = 0.0;
}

bool FVoxelViewManager::HasPrimaryRepresentation() const
{
	return !FineActors.IsEmpty() ||
		!VoxelProxyActors.IsEmpty() ||
		!SurfaceActors.IsEmpty() ||
		!MacroActors.IsEmpty();
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

	// 已知全空气区块没有任何面，不必复制Halo或占用后台网格任务；仍通过同一版本/发布路径交接。
	if (Section->Blocks.Num() == 4096 && !Section->Blocks.ContainsByPredicate([](const FVoxelBlockState& Block)
		{ return !Block.IsAir(); }))
	{
		FVoxelTaskResult Result;
		Result.Kind = EVoxelTaskKind::BuildFineMesh;
		Result.Stamp = Stamp;
		Result.bSuccess = true;
		Result.FineMesh = MakeShared<FVoxelSectionMeshResult>();
		const bool bPublished = PublishFine(Result);
		bCoverageDirty |= bPublished;
		return bPublished;
	}

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
		Demand && Demand->bWarmupData
			? EVoxelWorkClass::Critical
			: EVoxelWorkClass::Visible;

	Request.Stamp =
		Stamp;
	Request.DistanceScore = MinimumObserverDistanceCells(
		FVector(InSection * ViewSectionSide + FIntVector(ViewSectionSide / 2)) * Module.BlockSize());
	Request.ForwardScore = 0.0;

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
	if (InData.GridSide != ViewSectionSide ||
		InData.Cells.Num() != ViewSectionSide * ViewSectionSide * ViewSectionSide)
	{
		OutError = TEXT("Voxel proxy snapshot has invalid cell data");
		return false;
	}

	FVoxelSectionSnapshot Snapshot;
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
		if (Source.Num() != ViewSectionSide * ViewSectionSide)
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
	const FVoxelViewKey& InKey)
{
	if (!Module.IsAuthority())
	{
		TArray<FIntVector> Modified;

		Module.
			GetRuntime()->
			GetChangeIndex().
			Enumerate(
				InKey.GetBounds(),
				Modified);

		if (!Modified.IsEmpty())
		{
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
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> Config =
			Module.
				GetGenerationConfig();

	const TSharedPtr<
		FVoxelGenerationPlanCache,
		ESPMode::ThreadSafe> Cache =
			Module.
				GetGenerationCache();

	if (!Config ||
		!Cache)
	{
		return false;
	}

	FVoxelTaskRequest Request;

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
	FVoxelGenerationBounds OverlayBounds = InKey.GetBounds();
	OverlayBounds.Min -= FIntVector(InKey.GetStep());
	OverlayBounds.Max += FIntVector(InKey.GetStep());
	FString OverlayError;
	if (!Module.CaptureOverlays(OverlayBounds, Overlays, OverlayError)) return false;
	Request.InputBytes = Overlays.GetAllocatedBytes();

	Request.Execute =
		[
			Config,
			Cache,
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

			Result.VoxelProxy =
				MakeShared<
					FVoxelVoxelProxyData>();

			const FVoxelVoxelProxyBuilder Builder(
				Config.ToSharedRef(),
				Cache.ToSharedRef());

			Result.bSuccess =
				Builder.Build(
					InKey,
					Overlays,
					*Result.
						VoxelProxy,
					Result.Error,
					&InCancel);

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
						&InCancel, Result.VoxelProxy->Key.GetStep() / VoxelViewLod::TexturePeriodCells(Result.VoxelProxy->Key.GetStep(), TextureStretch));

			if (!Result.bSuccess &&
				Result.Error.IsEmpty())
			{
				Result.Error =
					TEXT(
						"Failed to mesh voxel proxy");
			}

			return Result;
		};

	return Scheduler.Enqueue(MoveTemp(Request));
}

bool FVoxelViewManager::RequestSurface(
	const FVoxelSurfaceTileKey& InKey)
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

	const FVoxelGenerationSettings Settings =
		Module.GetManifest().
			Settings;
	FVoxelOverlaySnapshotSet Overlays;
	const int32 Step = 1 << InKey.Level;
	const int32 Side = FVoxelSurfaceTileData::CellSide * Step;
	const FVoxelGenerationBounds OverlayBounds{
		FIntVector(InKey.Coordinate.X * Side - Step, InKey.Coordinate.Y * Side - Step, Settings.MinZ),
		FIntVector((InKey.Coordinate.X + 1) * Side + Step, (InKey.Coordinate.Y + 1) * Side + Step, Settings.MaxZ)
	};
	FString OverlayError;
	if (!Module.CaptureOverlays(OverlayBounds, Overlays, OverlayError)) return false;
	Request.InputBytes = Overlays.GetAllocatedBytes();

	Request.Execute =
		[
			Overlays = MoveTemp(Overlays),
			Generator,
			Config,
			Registry,
			Settings,
			InKey,
			TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells
		](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;
			const double TotalStart = FPlatformTime::Seconds();

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

			const double TerrainMeshStart = FPlatformTime::Seconds();
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
							8, TextureStretch);
			}
			const double TerrainMeshMilliseconds =
				(FPlatformTime::Seconds() - TerrainMeshStart) * 1000.0;

			if (!Result.bSuccess)
			{
				return Result;
			}

			const double WaterBuildStart = FPlatformTime::Seconds();
			Result.Water = MakeShared<FVoxelWaterSurfaceTileData>();
			FVoxelWaterViewBuilder WaterBuilder;
			Result.bSuccess = WaterBuilder.Build(*Result.Surface, *Result.Water, Result.Error);
			if (!Result.bSuccess)
			{
				return Result;
			}
			const double WaterBuildMilliseconds =
				(FPlatformTime::Seconds() - WaterBuildStart) * 1000.0;

			const double WaterMeshStart = FPlatformTime::Seconds();
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
			const double WaterMeshMilliseconds =
				(FPlatformTime::Seconds() - WaterMeshStart) * 1000.0;
			const double TotalMilliseconds =
				(FPlatformTime::Seconds() - TotalStart) * 1000.0;

#if !UE_BUILD_SHIPPING
			if (TotalMilliseconds >= 50.0)
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("Voxel Surface phase key=(%d,%d,L%d) columnsMs=%.2f overlayMs=%.2f terrainMeshMs=%.2f waterBuildMs=%.2f waterMeshMs=%.2f totalMs=%.2f"),
					InKey.Coordinate.X,
					InKey.Coordinate.Y,
					InKey.Level,
					Timing.ColumnsMilliseconds,
					Timing.OverlayMilliseconds,
					TerrainMeshMilliseconds,
					WaterBuildMilliseconds,
					WaterMeshMilliseconds,
					TotalMilliseconds);
			}
#endif

			return Result;
		};

	return Scheduler.Enqueue(MoveTemp(Request));
}

bool FVoxelViewManager::RequestMacro(
	const FVoxelMacroTileKey& InKey)
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

	const FVoxelGenerationSettings Settings = Module.GetManifest().Settings;
	const int32 Step = FVoxelMacroTileData::BaseStep << InKey.Level;
	const int32 Side = FVoxelMacroTileData::CellSide * Step;
	FVoxelOverlaySnapshotSet Overlays;
	FString OverlayError;
	const FVoxelGenerationBounds Bounds {
		FIntVector(InKey.Coordinate.X * Side - Step, InKey.Coordinate.Y * Side - Step, Settings.MinZ),
		FIntVector((InKey.Coordinate.X + 1) * Side + Step, (InKey.Coordinate.Y + 1) * Side + Step, Settings.MaxZ)
	};
	if (!Module.CaptureOverlays(Bounds, Overlays, OverlayError))
	{
		return false;
	}
	Request.InputBytes = Overlays.GetAllocatedBytes();
	Request.Execute =
		[
			Overlays = MoveTemp(Overlays),
			Settings,
			Generator,
			Config,
			Registry,
			InKey,
			TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells
		](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;

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

			if (Result.bSuccess)
			{
				Result.MacroMesh =
					MakeShared<
						FVoxelSectionMeshResult>();

				Result.bSuccess =
					FVoxelHeightfieldMesher::
						BuildBlockyTerrain(
							Result.Macro->Side,
							Result.Macro->Step,
							Result.Macro->Height,
							Result.Macro->SurfaceClass,
							*Config,
							*Registry,
							*Result.MacroMesh,
							Result.Error,
							&InCancel,
							Result.Macro->
								ForestCoverage,
							-0.05,
							16, TextureStretch);
			}

			return Result;
		};

	return Scheduler.Enqueue(MoveTemp(Request));
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
	FineLastWanted.Add(Key, FPlatformTime::Seconds());
	FineRevisions.Add(Key, InResult.Stamp.Revision);
	if (!HasRenderableMesh(*InResult.FineMesh) && !FineActors.Contains(Key))
	{
		return true;
	}

	AActor* Host = FineActors.FindRef(Key);
	if (!Publisher->Stage(Host, FVector(Key * ViewSectionSide) * Module.BlockSize(), Module.BlockSize(), MoveTemp(*InResult.FineMesh)))
	{
		FineReady.Remove(Key);
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
	UpdateProxySurfaceCoverage(*InResult.VoxelProxy);
	VoxelProxyReady.Add(Key);
	VoxelProxyLastWanted.Add(Key, FPlatformTime::Seconds());
	VoxelProxyRevisions.Add(Key, InResult.Stamp.Revision);
	if (!HasRenderableMesh(*InResult.VoxelProxyMesh) && !VoxelProxyActors.Contains(Key))
	{
		return true;
	}

	AActor* Host = VoxelProxyActors.FindRef(Key);
	if (!Publisher->Stage(Host, FVector(Key.GetBounds().Min) * Module.BlockSize(),
		Module.BlockSize() * Key.GetStep(), MoveTemp(*InResult.VoxelProxyMesh)))
	{
		VoxelProxyReady.Remove(Key);
		VoxelProxyData.Remove(Key);
		ProxySurfaceCoverage.Remove(Key);
		VoxelProxyRevisions.Remove(Key);
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
	if (!Publisher->Stage(Host, FVector(Key.Coordinate.X * TileSide, Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(),
		Module.BlockSize(), MoveTemp(*InResult.SurfaceMesh)))
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
		Host,
		FVector(
			Key.Coordinate.X *
				TileSide,
			Key.Coordinate.Y *
				TileSide,
			0) *
			Module.BlockSize(),
		Module.BlockSize(),
		MoveTemp(*InResult.WaterMesh)))
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
		Host,
		FVector(
			Key.Coordinate.X *
				TileSide,
			Key.Coordinate.Y *
				TileSide,
			0) *
			Module.BlockSize(),
		Module.BlockSize(),
		MoveTemp(*InResult.MacroMesh)))
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
