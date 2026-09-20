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
}

FVoxelViewManager::~FVoxelViewManager()
{
	Reset();
}

void FVoxelViewManager::Tick(
	const uint64 InInterestRevision,
	const TConstArrayView<FVector> InObservers)
{
	const double Now =
		FPlatformTime::Seconds();

	if (AppliedInterestRevision != InInterestRevision)
	{
		UpdateFineAndVoxelProxy(InObservers);
		UpdateSurface(InObservers);
		UpdateMacro(InObservers);
		UpdateWantedTimestamps(Now);
		FineAdmissions = FineWanted.Array();
		VoxelProxyAdmissions = VoxelProxyWanted.Array();
		SurfaceAdmissions = SurfaceWanted.Array();
		MacroAdmissions = MacroWanted.Array();
		if (!InObservers.IsEmpty())
		{
			const FVector Observer = InObservers[0];
			const double BlockSize = Module.BlockSize();
			FineAdmissions.Sort([Observer, BlockSize](const FIntVector& InA, const FIntVector& InB)
			{
				const FVector CenterA = FVector(InA * ViewSectionSide + FIntVector(ViewSectionSide / 2)) * BlockSize;
				const FVector CenterB = FVector(InB * ViewSectionSide + FIntVector(ViewSectionSide / 2)) * BlockSize;
				return FVector::DistSquared(CenterA, Observer) < FVector::DistSquared(CenterB, Observer);
			});
			VoxelProxyAdmissions.Sort([Observer, BlockSize](const FVoxelViewKey& InA, const FVoxelViewKey& InB)
			{
				const FVoxelGenerationBounds BoundsA = InA.GetBounds();
				const FVoxelGenerationBounds BoundsB = InB.GetBounds();
				const FVector CenterA = FVector(BoundsA.Min + BoundsA.Max) * (0.5 * BlockSize);
				const FVector CenterB = FVector(BoundsB.Min + BoundsB.Max) * (0.5 * BlockSize);
				return FVector::DistSquared(CenterA, Observer) < FVector::DistSquared(CenterB, Observer);
			});
			SurfaceAdmissions.Sort([this, Observer, BlockSize](const FVoxelSurfaceTileKey& InA, const FVoxelSurfaceTileKey& InB)
			{
				const FVoxelCoverageRect BoundsA = SurfaceCoverageRect(InA);
				const FVoxelCoverageRect BoundsB = SurfaceCoverageRect(InB);
				const FVector2D CenterA = FVector2D(BoundsA.Min + BoundsA.Max) * (0.5 * BlockSize);
				const FVector2D CenterB = FVector2D(BoundsB.Min + BoundsB.Max) * (0.5 * BlockSize);
				const FVector2D ObserverXY(Observer.X, Observer.Y);
				return FVector2D::DistSquared(CenterA, ObserverXY) < FVector2D::DistSquared(CenterB, ObserverXY);
			});
			MacroAdmissions.Sort([this, Observer, BlockSize](const FVoxelMacroTileKey& InA, const FVoxelMacroTileKey& InB)
			{
				const FVoxelCoverageRect BoundsA = MacroCoverageRect(InA);
				const FVoxelCoverageRect BoundsB = MacroCoverageRect(InB);
				const FVector2D CenterA = FVector2D(BoundsA.Min + BoundsA.Max) * (0.5 * BlockSize);
				const FVector2D CenterB = FVector2D(BoundsB.Min + BoundsB.Max) * (0.5 * BlockSize);
				const FVector2D ObserverXY(Observer.X, Observer.Y);
				return FVector2D::DistSquared(CenterA, ObserverXY) < FVector2D::DistSquared(CenterB, ObserverXY);
			});
		}
		AppliedInterestRevision = InInterestRevision;
		FineAdmissionIndex = 0;
		VoxelProxyAdmissionIndex = 0;
		SurfaceAdmissionIndex = 0;
		MacroAdmissionIndex = 0;
		bCoverageDirty = true;
	}

	ProcessAdmissions();

	if (CVarVoxelDebugRepresentation.GetValueOnGameThread() > 0 &&
		Now >= NextRepresentationDebugLog)
	{
		LogRepresentationState(InObservers);
		NextRepresentationDebugLog = Now + 5.0;
	}

	if (bCoverageDirty && Now >= NextCoverageCheck)
	{
		ResolveTransitionVisibility();
		CleanupRetiredRepresentations(Now);
		NextCoverageCheck = Now + RetireDelaySeconds;
		bCoverageDirty = false;
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
}

void FVoxelViewManager::ProcessAdmissions()
{
	constexpr int32 MaxAdmissionsPerTick = 24;
	int32 Attempts = 0;
	auto AdvanceIndex = [](int32& InOutIndex, const int32 InNum)
	{
		const int32 Result = InOutIndex;
		InOutIndex = InNum > 0 ? (InOutIndex + 1) % InNum : 0;
		return Result;
	};

	while (Attempts < MaxAdmissionsPerTick &&
		(!FineAdmissions.IsEmpty() || !VoxelProxyAdmissions.IsEmpty() || !SurfaceAdmissions.IsEmpty() || !MacroAdmissions.IsEmpty()))
	{
		const int32 Lane = Attempts % 4;
		if (Lane == 0 && !FineAdmissions.IsEmpty())
		{
			const FIntVector Key = FineAdmissions[AdvanceIndex(FineAdmissionIndex, FineAdmissions.Num())];
			if (const FVoxelSection* Section = Module.GetRuntime()->FindSection(Key);
				Section && Section->Status == EVoxelSectionStatus::DataReady)
			{
				const uint64* PublishedRevision = FineRevisions.Find(Key);
				if (!PublishedRevision || *PublishedRevision != Section->CommittedRevision)
				{
					RequestFine(Key, Section->CommittedRevision);
				}
			}
		}
		else if (Lane == 1 && !VoxelProxyAdmissions.IsEmpty())
		{
			const FVoxelViewKey Key = VoxelProxyAdmissions[AdvanceIndex(VoxelProxyAdmissionIndex, VoxelProxyAdmissions.Num())];
			const uint64 Revision = Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(Key.Coordinate);
			const uint64* PublishedRevision = VoxelProxyRevisions.Find(Key);
			if (!PublishedRevision || *PublishedRevision != Revision) RequestVoxelProxy(Key);
		}
		else if (Lane == 2 && !SurfaceAdmissions.IsEmpty())
		{
			const FVoxelSurfaceTileKey Key = SurfaceAdmissions[AdvanceIndex(SurfaceAdmissionIndex, SurfaceAdmissions.Num())];
			const uint64 Revision = Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(Key.Coordinate);
			const uint64* PublishedRevision = SurfaceRevisions.Find(Key);
			if (!PublishedRevision || *PublishedRevision != Revision) RequestSurface(Key);
		}
		else if (Lane == 3 && !MacroAdmissions.IsEmpty())
		{
			const FVoxelMacroTileKey Key = MacroAdmissions[AdvanceIndex(MacroAdmissionIndex, MacroAdmissions.Num())];
			const uint64 Revision = Module.GetRuntime()->GetChangeHierarchy().GetMacroRevision(Key.Coordinate);
			const uint64* PublishedRevision = MacroRevisions.Find(Key);
			if (!PublishedRevision || *PublishedRevision != Revision) RequestMacro(Key);
		}
		++Attempts;
	}
}

void FVoxelViewManager::SetActorHiddenCached(AActor* InActor, const bool bInHidden)
{
	if (!InActor) return;
	const TWeakObjectPtr<AActor> Key(InActor);
	if (const bool* Previous = ActorHiddenStates.Find(Key);
		Previous && *Previous == bInHidden)
	{
		return;
	}
	InActor->SetActorHiddenInGame(bInHidden);
	ActorHiddenStates.Add(Key, bInHidden);
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
			const bool* bHidden = ActorHiddenStates.Find(TWeakObjectPtr<AActor>(Pair.Value));
			Count += !bHidden || !*bHidden ? 1 : 0;
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
		const bool* bHidden = ActorHiddenStates.Find(TWeakObjectPtr<AActor>(Pair.Value));
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
			!bHidden || !*bHidden);
		++Logged;
	}
}

void FVoxelViewManager::UpdateWantedTimestamps(
	const double InNow)
{
	for (const FIntVector& Key :
		FineWanted)
	{
		FineLastWanted.Add(
			Key,
			InNow);
	}

	for (const FVoxelViewKey& Key :
		VoxelProxyWanted)
	{
		VoxelProxyLastWanted.Add(
			Key,
			InNow);
	}

	for (const FVoxelSurfaceTileKey& Key :
		SurfaceWanted)
	{
		SurfaceLastWanted.Add(
			Key,
			InNow);
	}

	for (const FVoxelMacroTileKey& Key :
		MacroWanted)
	{
		MacroLastWanted.Add(
			Key,
			InNow);
	}
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

void FVoxelViewManager::GatherReadyWantedProxySurfaceRects(TArray<FVoxelCoverageRect>& OutCoverage) const
{
	OutCoverage.Reset();
	for (const FVoxelViewKey& Key : VoxelProxyReady)
	{
		if (!VoxelProxyWanted.Contains(Key)) continue;
		const TSharedPtr<const FVoxelVoxelProxyData>* DataPtr = VoxelProxyData.Find(Key);
		if (!DataPtr || !DataPtr->IsValid()) continue;
		const FVoxelVoxelProxyData& Data = *DataPtr->Get();
		if (Data.GridSide != ViewSectionSide || Data.Cells.Num() != ViewSectionSide * ViewSectionSide * ViewSectionSide) continue;
		const int32 Step = Key.GetStep();
		const FVoxelGenerationBounds Bounds = Key.GetBounds();
		for (int32 Y = 0; Y < ViewSectionSide; ++Y)
		{
			for (int32 X = 0; X < ViewSectionSide; ++X)
			{
				if (!VoxelCoverage::HasProxyTopSurfaceInColumn(Data, X, Y)) continue;
				const FIntPoint Min(Bounds.Min.X + X * Step, Bounds.Min.Y + Y * Step);
				OutCoverage.Add({ Min, Min + FIntPoint(Step, Step) });
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
	return VoxelCoverage::IsFullyCovered3D(FineCoverageBox(InKey), Coverage);
}

bool FVoxelViewManager::IsProxyReplacementReady(const FVoxelViewKey& InKey) const
{
	TArray<FVoxelCoverageBox> FineCoverage;
	GatherReadyWantedFineBoxes(FineCoverage);
	if (VoxelCoverage::IsFullyCovered3D(VoxelProxyCoverageBox(InKey), FineCoverage)) return true;
	TArray<FVoxelCoverageRect> SurfaceCoverage;
	GatherReadyWantedSurfaceRects(SurfaceCoverage);
	return VoxelCoverage::IsFullyCovered2D(VoxelProxyCoverageRect(InKey), SurfaceCoverage);
}

bool FVoxelViewManager::IsSurfaceReplacementReady(const FVoxelSurfaceTileKey& InKey) const
{
	const FVoxelCoverageRect Target = SurfaceCoverageRect(InKey);
	TArray<FVoxelCoverageRect> ProxyCoverage;
	GatherReadyWantedProxySurfaceRects(ProxyCoverage);
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
	for (const TPair<FIntVector, TObjectPtr<AActor>>& Pair : FineActors)
	{
		if (Pair.Value) SetActorHiddenCached(Pair.Value, !FineWanted.Contains(Pair.Key) && IsFineReplacementReady(Pair.Key));
	}
	for (const TPair<FVoxelViewKey, TObjectPtr<AActor>>& Pair : VoxelProxyActors)
	{
		if (Pair.Value) SetActorHiddenCached(Pair.Value, !VoxelProxyWanted.Contains(Pair.Key) && IsProxyReplacementReady(Pair.Key));
	}
	for (const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>>& Pair : SurfaceActors)
	{
		if (!Pair.Value) continue;
		const bool bHidden = !SurfaceWanted.Contains(Pair.Key) && IsSurfaceReplacementReady(Pair.Key);
		SetActorHiddenCached(Pair.Value, bHidden);
		if (AActor* Water = WaterActors.FindRef(Pair.Key)) SetActorHiddenCached(Water, bHidden);
	}
	for (const TPair<FVoxelMacroTileKey, TObjectPtr<AActor>>& Pair : MacroActors)
	{
		if (Pair.Value) SetActorHiddenCached(Pair.Value, !MacroWanted.Contains(Pair.Key) && IsMacroReplacementReady(Pair.Key));
	}
}

void FVoxelViewManager::CleanupRetiredRepresentations(const double InNow)
{
	auto CanRetire = [InNow](const double InLastWanted, const bool bReplacementReady, const bool bInsideDomain)
	{
		const double Age = InNow - InLastWanted;
		return Age >= RetireDelaySeconds && (bReplacementReady || (!bInsideDomain && Age >= OutsideDomainRetireDelaySeconds));
	};

	for (const FIntVector Key : FineReady.Array())
	{
		if (FineWanted.Contains(Key) || !CanRetire(FineLastWanted.FindRef(Key), IsFineReplacementReady(Key), IsFineInsideRenderDomain(Key))) continue;
		if (AActor* Actor = FineActors.FindRef(Key))
		{
			ActorHiddenStates.Remove(TWeakObjectPtr<AActor>(Actor));
			Actor->Destroy();
		}
		FineActors.Remove(Key);
		FineReady.Remove(Key);
		FineRevisions.Remove(Key);
		FineLastWanted.Remove(Key);
	}
	for (const FVoxelViewKey Key : VoxelProxyReady.Array())
	{
		if (VoxelProxyWanted.Contains(Key) || !CanRetire(VoxelProxyLastWanted.FindRef(Key), IsProxyReplacementReady(Key), IsProxyInsideRenderDomain(Key))) continue;
		if (AActor* Actor = VoxelProxyActors.FindRef(Key))
		{
			ActorHiddenStates.Remove(TWeakObjectPtr<AActor>(Actor));
			Actor->Destroy();
		}
		VoxelProxyActors.Remove(Key);
		VoxelProxyReady.Remove(Key);
		VoxelProxyData.Remove(Key);
		VoxelProxyRevisions.Remove(Key);
		VoxelProxyLastWanted.Remove(Key);
	}
	for (const FVoxelSurfaceTileKey Key : SurfaceReady.Array())
	{
		if (SurfaceWanted.Contains(Key) || !CanRetire(SurfaceLastWanted.FindRef(Key), IsSurfaceReplacementReady(Key), IsSurfaceInsideRenderDomain(Key))) continue;
		if (AActor* Actor = SurfaceActors.FindRef(Key))
		{
			ActorHiddenStates.Remove(TWeakObjectPtr<AActor>(Actor));
			Actor->Destroy();
		}
		if (AActor* Water = WaterActors.FindRef(Key))
		{
			ActorHiddenStates.Remove(TWeakObjectPtr<AActor>(Water));
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
	for (const FVoxelMacroTileKey Key : MacroReady.Array())
	{
		if (MacroWanted.Contains(Key) || !CanRetire(MacroLastWanted.FindRef(Key), IsMacroReplacementReady(Key), IsMacroInsideRenderDomain(Key))) continue;
		if (AActor* Actor = MacroActors.FindRef(Key))
		{
			ActorHiddenStates.Remove(TWeakObjectPtr<AActor>(Actor));
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
			Module.GetRuntime()->GetChangeHierarchy().SetVoxelProxyRevision(InReply.Key.Coordinate, InReply.Revision);
		}
		else if (InReply.Type == EVoxelRepresentationWireType::SurfaceProxy)
		{
			Module.GetRuntime()->GetChangeHierarchy().SetSurfaceRevision(
				FIntPoint(InReply.Key.Coordinate.X, InReply.Key.Coordinate.Y), InReply.Revision);
		}
		else if (InReply.Type == EVoxelRepresentationWireType::MacroTerrain)
		{
			Module.GetRuntime()->GetChangeHierarchy().SetMacroRevision(
				FIntPoint(InReply.Key.Coordinate.X, InReply.Key.Coordinate.Y), InReply.Revision);
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
		Module.GetRuntime()->GetChangeHierarchy().SetVoxelProxyRevision(Data.Key.Coordinate, InReply.Revision);
		const TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes = Module.GetShapes();
		const uint64 RecipeHash = Module.GetManifest().RecipeHash;
		Request.Execute = [Data = MoveTemp(Data), Registry, Shapes, RecipeHash](const TAtomic<bool>& InCancel) mutable
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
				Snapshot, *Registry, *Shapes, *Result.VoxelProxyMesh, &InCancel);
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
		Module.GetRuntime()->GetChangeHierarchy().SetSurfaceRevision(Data.Key.Coordinate, InReply.Revision);
		const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();
		Request.Execute = [Data = MoveTemp(Data), Registry, Config](const TAtomic<bool>& InCancel) mutable
		{
			FVoxelTaskResult Result;
			Result.Surface = MakeShared<FVoxelSurfaceTileData>(MoveTemp(Data));
			Result.SurfaceMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.bSuccess = Config && FVoxelHeightfieldMesher::BuildTerrain(
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
				8);
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
				*Result.Water, *Registry, WaterMesh, Result.Error, &InCancel);
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
		Module.GetRuntime()->GetChangeHierarchy().SetMacroRevision(Data.Key.Coordinate, InReply.Revision);
		const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();
		Request.Execute = [Data = MoveTemp(Data), Registry, Config](const TAtomic<bool>& InCancel) mutable
		{
			FVoxelTaskResult Result;
			Result.Macro = MakeShared<FVoxelMacroTileData>(MoveTemp(Data));
			Result.MacroMesh = MakeShared<FVoxelSectionMeshResult>();
			Result.bSuccess = Config && FVoxelHeightfieldMesher::BuildTerrain(
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
				16);
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
		VoxelProxyRevisions.Remove(VoxelKey);
		Module.GetRuntime()->GetChangeHierarchy().SetVoxelProxyRevision(WireKey.Coordinate, InInvalidate.Revision);

		const FIntPoint Coordinate(WireKey.Coordinate.X, WireKey.Coordinate.Y);
		const FVoxelSurfaceTileKey SurfaceKey { Coordinate, WireKey.Level };
		SurfaceReady.Remove(SurfaceKey);
		SurfaceData.Remove(SurfaceKey);
		WaterData.Remove(SurfaceKey);
		SurfaceRevisions.Remove(SurfaceKey);
		Module.GetRuntime()->GetChangeHierarchy().SetSurfaceRevision(Coordinate, InInvalidate.Revision);

		const FVoxelMacroTileKey MacroKey { Coordinate, WireKey.Level };
		MacroReady.Remove(MacroKey);
		MacroData.Remove(MacroKey);
		MacroRevisions.Remove(MacroKey);
		Module.GetRuntime()->GetChangeHierarchy().SetMacroRevision(Coordinate, InInvalidate.Revision);
	}
}

void FVoxelViewManager::InvalidateSection(const FIntVector& InKey)
{
	AppliedInterestRevision = 0;
	bCoverageDirty = true;
	FineRevisions.Remove(InKey);
	FineReady.Remove(InKey);
	Scheduler.CancelSection(InKey);
	const FIntVector CellCenter = InKey * ViewSectionSide + FIntVector(ViewSectionSide / 2);
	for (uint8 Level = 1; Level <= 8; ++Level)
	{
		const int32 Side = ViewSectionSide << Level;
		const FVoxelViewKey Key {
			FIntVector(
				VoxelGeneration::FloorDivide(CellCenter.X, Side),
				VoxelGeneration::FloorDivide(CellCenter.Y, Side),
				VoxelGeneration::FloorDivide(CellCenter.Z, Side)),
			Level
		};
		VoxelProxyRevisions.Remove(Key);
		VoxelProxyReady.Remove(Key);
		VoxelProxyData.Remove(Key);
	}

	for (uint8 Level = 0; Level <= 8; ++Level)
	{
		const int32 SurfaceSide = FVoxelSurfaceTileData::CellSide * (1 << Level);
		const FVoxelSurfaceTileKey SurfaceKey {
			FIntPoint(
				VoxelGeneration::FloorDivide(CellCenter.X, SurfaceSide),
				VoxelGeneration::FloorDivide(CellCenter.Y, SurfaceSide)),
			Level
		};
		SurfaceRevisions.Remove(SurfaceKey);
		SurfaceReady.Remove(SurfaceKey);
		SurfaceData.Remove(SurfaceKey);
		WaterData.Remove(SurfaceKey);

		const int32 MacroSide = FVoxelMacroTileData::CellSide *
			(FVoxelMacroTileData::BaseStep << Level);
		const FVoxelMacroTileKey MacroKey {
			FIntPoint(
				VoxelGeneration::FloorDivide(CellCenter.X, MacroSide),
				VoxelGeneration::FloorDivide(CellCenter.Y, MacroSide)),
			Level
		};
		MacroRevisions.Remove(MacroKey);
		MacroReady.Remove(MacroKey);
		MacroData.Remove(MacroKey);
	}
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
	FineWanted.Reset();
	VoxelProxyWanted.Reset();
	SurfaceWanted.Reset();
	MacroWanted.Reset();
	FineReady.Reset();
	VoxelProxyReady.Reset();
	SurfaceReady.Reset();
	MacroReady.Reset();
	VoxelProxyData.Reset();
	SurfaceData.Reset();
	WaterData.Reset();
	MacroData.Reset();
	FineLastWanted.Reset();
	VoxelProxyLastWanted.Reset();
	SurfaceLastWanted.Reset();
	MacroLastWanted.Reset();
	FineAdmissions.Reset();
	VoxelProxyAdmissions.Reset();
	SurfaceAdmissions.Reset();
	MacroAdmissions.Reset();
	ActorHiddenStates.Reset();
	AppliedInterestRevision = 0;
	FineAdmissionIndex = 0;
	VoxelProxyAdmissionIndex = 0;
	SurfaceAdmissionIndex = 0;
	MacroAdmissionIndex = 0;
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
		}
		if (FineActors.Contains(Pair.Key))
		{
			++Result.Renderable;
		}
	}
	return Result;
}

bool FVoxelViewManager::EnumerateModifiedSections(
	const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutSections,
	const TAtomic<bool>* InCancel) const
{
	OutSections.Reset();
	if (const FVoxelWorldRuntime* Runtime = Module.GetRuntime())
	{
		return Runtime->GetChangeIndex().Enumerate(InBounds, OutSections, InCancel);
	}
	return true;
}

bool FVoxelViewManager::ReadOverlay(
	const FIntVector& InSection,
	FVoxelOverlaySnapshot& OutOverlay) const
{
	const FVoxelSection* Section = Module.GetRuntime() ? Module.GetRuntime()->FindSection(InSection) : nullptr;
	if (!Section || Section->Status != EVoxelSectionStatus::DataReady)
	{
		return false;
	}
	OutOverlay.Section = InSection;
	OutOverlay.Revision = Section->CommittedRevision;
	OutOverlay.Blocks = Section->Overlay;
	return true;
}

void FVoxelViewManager::UpdateFineAndVoxelProxy(const TConstArrayView<FVector> InObservers)
{
	FineWanted.Reset();
	VoxelProxyWanted.Reset();
	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : Module.GetCurrentInterest().Exact)
	{
		if (!Pair.Value.bFineRender)
		{
			continue;
		}
		FineWanted.Add(Pair.Key);
	}
	for (const FVoxelViewKey& Key : Module.GetCurrentInterest().VoxelProxy)
	{
		VoxelProxyWanted.Add(Key);
	}
}

void FVoxelViewManager::UpdateSurface(const TConstArrayView<FVector> InObservers)
{
	SurfaceWanted = Module.GetCurrentInterest().Surface;
}

void FVoxelViewManager::UpdateMacro(const TConstArrayView<FVector> InObservers)
{
	MacroWanted = Module.GetCurrentInterest().Macro;
}

void FVoxelViewManager::RequestFine(
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
		return;
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
		return;
	}

	FVoxelSectionSnapshot Snapshot;

	if (!Module.GetRuntime()->
		CaptureSnapshot(
			InSection,
			Snapshot))
	{
		return;
	}

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

	Scheduler.Enqueue(
		MoveTemp(Request));
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

void FVoxelViewManager::RequestVoxelProxy(
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

			Module.
				RequestRemoteRepresentation(
					EVoxelRepresentationWireType::
						VoxelProxy,
					{
						InKey.Coordinate,
						InKey.Level
					},
					Revision
						? *Revision
						: 0);

			return;
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
		return;
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
			GetVoxelProxyRevision(
				InKey.Coordinate);

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

	if (Scheduler.Has(
			Request.Stamp,
			Request.Kind))
	{
		return;
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

	Request.Execute =
		[
			Config,
			Cache,
			Registry,
			Shapes,
			RecipeHash,
			InKey
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
				Builder.BuildNatural(
					InKey,
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
						&InCancel);

			if (!Result.bSuccess &&
				Result.Error.IsEmpty())
			{
				Result.Error =
					TEXT(
						"Failed to mesh voxel proxy");
			}

			return Result;
		};

	Scheduler.Enqueue(
		MoveTemp(
			Request));
}

void FVoxelViewManager::RequestSurface(
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

			Module.RequestRemoteRepresentation(
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

			return;
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
		return;
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
			GetSurfaceRevision(
				InKey.Coordinate);

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

	if (Scheduler.Has(
		Request.Stamp,
		Request.Kind))
	{
		return;
	}

	const FVoxelGenerationSettings Settings =
		Module.GetManifest().
			Settings;

	Request.Execute =
		[
			this,
			Generator,
			Config,
			Registry,
			Settings,
			InKey
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
				*this);
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
						BuildTerrain(
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
							8);
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
				&InCancel);
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

	Scheduler.Enqueue(
		MoveTemp(Request));
}

void FVoxelViewManager::RequestMacro(
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

			Module.RequestRemoteRepresentation(
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

			return;
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
		return;
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
			GetMacroRevision(
				InKey.Coordinate);

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

	if (Scheduler.Has(
		Request.Stamp,
		Request.Kind))
	{
		return;
	}

	Request.Execute =
		[
			Generator,
			Config,
			Registry,
			InKey
		](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;

			Result.Macro =
				MakeShared<
					FVoxelMacroTileData>();

			const FVoxelMacroTerrainBuilder Builder(
				Generator.ToSharedRef());

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
						BuildTerrain(
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
							16);
			}

			return Result;
		};

	Scheduler.Enqueue(
		MoveTemp(Request));
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
	FineRevisions.Add(Key, InResult.Stamp.Revision);
	if (!HasRenderableMesh(*InResult.FineMesh))
	{
		if (AActor* Existing = FineActors.FindRef(Key))
		{
			ActorHiddenStates.Remove(TWeakObjectPtr<AActor>(Existing));
			Existing->Destroy();
		}
		FineActors.Remove(Key);
		return true;
	}

	AActor* Host = FineActors.FindRef(Key);
	if (!PublishMeshActor(Host, FVector(Key * ViewSectionSide) * Module.BlockSize(), Module.BlockSize(), *InResult.FineMesh))
	{
		FineReady.Remove(Key);
		FineRevisions.Remove(Key);
		return false;
	}
	FineActors.Add(Key, Host);
	SetActorHiddenCached(Host, false);
	return true;
}

bool FVoxelViewManager::PublishVoxelProxy(const FVoxelTaskResult& InResult)
{
	const FVoxelViewKey& Key = InResult.Stamp.ViewKey;
	if (!InResult.VoxelProxy || !InResult.VoxelProxyMesh || !VoxelProxyWanted.Contains(Key) ||
		InResult.Stamp.Token != HashCombineFast(GetTypeHash(Key), GetTypeHash(InResult.Stamp.Revision)) ||
		Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(Key.Coordinate) != InResult.Stamp.Revision)
	{
		return false;
	}

	VoxelProxyData.Add(Key, InResult.VoxelProxy);
	VoxelProxyReady.Add(Key);
	VoxelProxyRevisions.Add(Key, InResult.Stamp.Revision);
	if (!HasRenderableMesh(*InResult.VoxelProxyMesh))
	{
		if (AActor* Existing = VoxelProxyActors.FindRef(Key))
		{
			ActorHiddenStates.Remove(TWeakObjectPtr<AActor>(Existing));
			Existing->Destroy();
		}
		VoxelProxyActors.Remove(Key);
		return true;
	}

	AActor* Host = VoxelProxyActors.FindRef(Key);
	if (!PublishMeshActor(Host, FVector(Key.GetBounds().Min) * Module.BlockSize(),
		Module.BlockSize() * Key.GetStep(), *InResult.VoxelProxyMesh))
	{
		VoxelProxyReady.Remove(Key);
		VoxelProxyData.Remove(Key);
		VoxelProxyRevisions.Remove(Key);
		return false;
	}
	VoxelProxyActors.Add(Key, Host);
	SetActorHiddenCached(Host, false);
	return true;
}

bool FVoxelViewManager::PublishSurface(const FVoxelTaskResult& InResult)
{
	const FVoxelSurfaceTileKey& Key = InResult.Stamp.SurfaceKey;
	if (!InResult.Surface || !InResult.SurfaceMesh || !SurfaceWanted.Contains(Key) ||
		InResult.Stamp.Token != HashCombineFast(GetTypeHash(Key), GetTypeHash(InResult.Stamp.Revision)) ||
		Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(Key.Coordinate) != InResult.Stamp.Revision)
	{
		return false;
	}

	AActor* Host = SurfaceActors.FindRef(Key);
	const int32 TileSide = InResult.Surface->GetTileSide();
	if (!PublishMeshActor(Host, FVector(Key.Coordinate.X * TileSide, Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(),
		Module.BlockSize(), *InResult.SurfaceMesh))
	{
		SurfaceReady.Remove(Key);
		return false;
	}
	SurfaceActors.Add(Key, Host);
	SurfaceReady.Add(Key);
	SurfaceRevisions.Add(Key, InResult.Stamp.Revision);
	SetActorHiddenCached(Host, false);
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
			GetSurfaceRevision(
				Key.Coordinate) !=
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

	if (PublishMeshActor(
		Host,
		FVector(
			Key.Coordinate.X *
				TileSide,
			Key.Coordinate.Y *
				TileSide,
			0) *
			Module.BlockSize(),
		Module.BlockSize(),
		*InResult.WaterMesh))
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
			GetMacroRevision(
				Key.Coordinate) !=
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

	if (PublishMeshActor(
		Host,
		FVector(
			Key.Coordinate.X *
				TileSide,
			Key.Coordinate.Y *
				TileSide,
			0) *
			Module.BlockSize(),
		Module.BlockSize(),
		*InResult.MacroMesh))
	{
		MacroActors.Add(
			Key,
			Host);

		MacroReady.Add(
			Key);

		MacroRevisions.Add(
			Key,
			InResult.Stamp.Revision);

		SetActorHiddenCached(
			Host,
			false);

		return true;
	}

	MacroReady.Remove(
		Key);

	return false;
}

bool FVoxelViewManager::PublishMeshActor(
	AActor*& InOutActor,
	const FVector& InLocation,
	const double InBlockSize,
	const FVoxelSectionMeshResult& InMesh)
{
	if (!Module.GetWorld() ||
		!Module.GetMaterialSet() ||
		!FMath::IsFinite(
			InBlockSize) ||
		InBlockSize <= 0.0)
	{
		return false;
	}

	TArray<UMaterialInterface*> Materials;
	Materials.Reserve(
		InMesh.Batches.Num());

	for (const FVoxelRenderBatch& Batch :
		InMesh.Batches)
	{
		const FVoxelMaterialBank* Bank =
			Module.GetMaterialSet()->
				FindBank(
					Batch.Group,
					Batch.Bank);

		if (!Bank ||
			!Bank->Material)
		{
			return false;
		}

		Materials.Add(
			Bank->Material);
	}

	AActor* Host =
		InOutActor;

	const bool bNewHost =
		Host == nullptr;

	if (!Host)
	{
		FActorSpawnParameters Parameters;
		Parameters.ObjectFlags |=
			RF_Transient;

		Parameters.
			SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::
					AlwaysSpawn;

		Host =
			Module.GetWorld()->
				SpawnActor<AActor>(
					AActor::StaticClass(),
					FTransform::Identity,
					Parameters);

		if (!Host)
		{
			return false;
		}

		Host->SetReplicates(false);
		Host->SetActorEnableCollision(false);

		USceneComponent* Root =
			NewObject<USceneComponent>(
				Host);

		if (!Root)
		{
			Host->Destroy();
			return false;
		}

		Host->SetRootComponent(
			Root);

		Root->RegisterComponent();
	}

	Host->SetActorLocation(
		InLocation);

	TInlineComponentArray<
		UVoxelMeshComponent*>
		Components;

	Host->GetComponents(
		Components);

	for (int32 BatchIndex = 0;
		BatchIndex <
			InMesh.Batches.Num();
		++BatchIndex)
	{
		UVoxelMeshComponent* Component =
			Components.IsValidIndex(
				BatchIndex)
				? Components[
					BatchIndex]
				: nullptr;

		if (!Component)
		{
			Component =
				NewObject<
					UVoxelMeshComponent>(
						Host);

			if (!Component)
			{
				if (bNewHost)
				{
					Host->Destroy();
				}

				return false;
			}

			Component->SetupAttachment(
				Host->GetRootComponent());

			Component->
				RegisterComponent();

			Components.Add(
				Component);
		}

		if (!Component->Apply(
			InMesh.Batches[
				BatchIndex].
				Mesh,
			InBlockSize,
			Materials[
				BatchIndex]))
		{
			Host->Destroy();
			InOutActor = nullptr;
			return false;
		}
	}

	for (int32 ComponentIndex =
			Components.Num() - 1;
		ComponentIndex >=
			InMesh.Batches.Num();
		--ComponentIndex)
	{
		if (Components[
			ComponentIndex])
		{
			Components[
				ComponentIndex]->
					DestroyComponent();
		}
	}

	InOutActor =
		Host;

	return true;
}
