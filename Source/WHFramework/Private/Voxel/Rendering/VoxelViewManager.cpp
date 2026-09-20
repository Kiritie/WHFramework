#include "Voxel/Rendering/VoxelViewManager.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
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
		AppliedInterestRevision = InInterestRevision;
		FineAdmissionIndex = 0;
		VoxelProxyAdmissionIndex = 0;
		SurfaceAdmissionIndex = 0;
		MacroAdmissionIndex = 0;
		bCoverageDirty = true;
	}

	ProcessAdmissions();

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

void FVoxelViewManager::ResolveTransitionVisibility()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewCoverage);

	for (const TPair<
		FIntVector,
		TObjectPtr<AActor>>& Pair :
		FineActors)
	{
		if (!Pair.Value)
		{
			continue;
		}

		const bool bHidden =
			IsFineCoveredByRetainedProxy(
				Pair.Key);

		SetActorHiddenCached(Pair.Value, bHidden);
	}

	for (const TPair<
		FVoxelViewKey,
		TObjectPtr<AActor>>& Pair :
		VoxelProxyActors)
	{
		if (Pair.Value)
		{
			SetActorHiddenCached(Pair.Value, false);
		}
	}

	for (const TPair<
		FVoxelSurfaceTileKey,
		TObjectPtr<AActor>>& Pair :
		SurfaceActors)
	{
		if (Pair.Value)
		{
			SetActorHiddenCached(Pair.Value, false);
		}
	}

	for (const TPair<
		FVoxelSurfaceTileKey,
		TObjectPtr<AActor>>& Pair :
		WaterActors)
	{
		if (Pair.Value)
		{
			SetActorHiddenCached(Pair.Value, false);
		}
	}

	for (const TPair<
		FVoxelMacroTileKey,
		TObjectPtr<AActor>>& Pair :
		MacroActors)
	{
		if (Pair.Value)
		{
			SetActorHiddenCached(Pair.Value, false);
		}
	}
}

bool FVoxelViewManager::IsFineCoveredByRetainedProxy(
	const FIntVector& InSection) const
{
	if (!FineWanted.Contains(
		InSection))
	{
		return false;
	}

	const FIntVector SectionCenter =
		InSection *
			16 +
		FIntVector(8);

	for (const TPair<
		FVoxelViewKey,
		TObjectPtr<AActor>>& Pair :
		VoxelProxyActors)
	{
		if (!Pair.Value ||
			VoxelProxyWanted.Contains(
				Pair.Key))
		{
			continue;
		}

		if (Pair.Key.
			GetBounds().
			Contains(
				SectionCenter) &&
			!HasFineReplacementForProxy(
				Pair.Key))
		{
			return true;
		}
	}

	return false;
}

bool FVoxelViewManager::HasReplacementForFine(
	const FIntVector& InSection) const
{
	const FIntVector Center =
		InSection *
			16 +
		FIntVector(8);

	for (const TPair<
		FVoxelViewKey,
		TObjectPtr<AActor>>& Pair :
		VoxelProxyActors)
	{
		if (Pair.Value &&
			VoxelProxyWanted.Contains(
				Pair.Key) &&
			Pair.Key.GetBounds().
				Contains(
					Center))
		{
			return true;
		}
	}

	for (const TPair<
		FVoxelSurfaceTileKey,
		TObjectPtr<AActor>>& Pair :
		SurfaceActors)
	{
		if (!Pair.Value ||
			!SurfaceWanted.Contains(
				Pair.Key))
		{
			continue;
		}

		const int32 Step =
			1 <<
			Pair.Key.Level;

		const int32 Side =
			FVoxelSurfaceTileData::
				CellSide *
			Step;

		const FIntPoint Min =
			Pair.Key.Coordinate *
			Side;

		if (Center.X >= Min.X &&
			Center.Y >= Min.Y &&
			Center.X <
				Min.X + Side &&
			Center.Y <
				Min.Y + Side)
		{
			return true;
		}
	}

	for (const TPair<
		FVoxelMacroTileKey,
		TObjectPtr<AActor>>& Pair :
		MacroActors)
	{
		if (!Pair.Value ||
			!MacroWanted.Contains(
				Pair.Key))
		{
			continue;
		}

		const int32 Step =
			FVoxelMacroTileData::
				BaseStep <<
			Pair.Key.Level;

		const int32 Side =
			FVoxelMacroTileData::
				CellSide *
			Step;

		const FIntPoint Min =
			Pair.Key.Coordinate *
			Side;

		if (Center.X >= Min.X &&
			Center.Y >= Min.Y &&
			Center.X <
				Min.X + Side &&
			Center.Y <
				Min.Y + Side)
		{
			return true;
		}
	}

	return false;
}

bool FVoxelViewManager::HasFineReplacementForProxy(
	const FVoxelViewKey& InKey) const
{
	const FVoxelGenerationBounds Bounds =
		InKey.GetBounds();

	bool bHasFineDemand = false;

	for (const FIntVector& FineKey :
		FineWanted)
	{
		const FIntVector FineCenter =
			FineKey *
				16 +
			FIntVector(8);

		if (!Bounds.Contains(
			FineCenter))
		{
			continue;
		}

		bHasFineDemand = true;

		const TObjectPtr<AActor>* Actor =
			FineActors.Find(
				FineKey);

		if (!Actor ||
			!*Actor)
		{
			return false;
		}
	}

	return bHasFineDemand;
}

bool FVoxelViewManager::HasReplacementForSurface(
	const FVoxelSurfaceTileKey& InKey) const
{
	const int32 Step =
		1 <<
		InKey.Level;

	const int32 Side =
		FVoxelSurfaceTileData::
			CellSide *
		Step;

	const FIntPoint Center =
		InKey.Coordinate *
			Side +
		FIntPoint(
			Side / 2,
			Side / 2);

	for (const TPair<
		FVoxelViewKey,
		TObjectPtr<AActor>>& Pair :
		VoxelProxyActors)
	{
		if (!Pair.Value ||
			!VoxelProxyWanted.Contains(
				Pair.Key))
		{
			continue;
		}

		const FVoxelGenerationBounds Bounds =
			Pair.Key.GetBounds();

		if (Center.X >=
				Bounds.Min.X &&
			Center.Y >=
				Bounds.Min.Y &&
			Center.X <
				Bounds.Max.X &&
			Center.Y <
				Bounds.Max.Y)
		{
			return true;
		}
	}

	for (const TPair<
		FIntVector,
		TObjectPtr<AActor>>& Pair :
		FineActors)
	{
		if (!Pair.Value ||
			!FineWanted.Contains(
				Pair.Key))
		{
			continue;
		}

		const FIntVector FineCenter =
			Pair.Key *
				16 +
			FIntVector(8);

		if (FMath::Abs(
				FineCenter.X -
					Center.X) <=
				16 &&
			FMath::Abs(
				FineCenter.Y -
					Center.Y) <=
				16)
		{
			return true;
		}
	}

	return false;
}

bool FVoxelViewManager::HasReplacementForMacro(
	const FVoxelMacroTileKey& InKey) const
{
	const int32 Step =
		FVoxelMacroTileData::
			BaseStep <<
		InKey.Level;

	const int32 Side =
		FVoxelMacroTileData::
			CellSide *
		Step;

	const FIntPoint Center =
		InKey.Coordinate *
			Side +
		FIntPoint(
			Side / 2,
			Side / 2);

	for (const TPair<
		FVoxelSurfaceTileKey,
		TObjectPtr<AActor>>& Pair :
		SurfaceActors)
	{
		if (!Pair.Value ||
			!SurfaceWanted.Contains(
				Pair.Key))
		{
			continue;
		}

		const int32 SurfaceStep =
			1 <<
			Pair.Key.Level;

		const int32 SurfaceSide =
			FVoxelSurfaceTileData::
				CellSide *
			SurfaceStep;

		const FIntPoint Min =
			Pair.Key.Coordinate *
			SurfaceSide;

		if (Center.X >= Min.X &&
			Center.Y >= Min.Y &&
			Center.X <
				Min.X +
					SurfaceSide &&
			Center.Y <
				Min.Y +
					SurfaceSide)
		{
			return true;
		}
	}

	return false;
}

void FVoxelViewManager::CleanupRetiredRepresentations(
	const double InNow)
{
	for (auto Iterator =
		FineActors.CreateIterator();
		Iterator;
		++Iterator)
	{
		if (FineWanted.Contains(
			Iterator.Key()))
		{
			continue;
		}

		const double LastWanted =
			FineLastWanted.FindRef(
				Iterator.Key());

		if (InNow -
				LastWanted <
			RetireDelaySeconds ||
			!HasReplacementForFine(
				Iterator.Key()))
		{
			continue;
		}

		if (Iterator.Value())
		{
			Iterator.Value()->
				Destroy();
		}

		FineRevisions.Remove(
			Iterator.Key());

		FineLastWanted.Remove(
			Iterator.Key());

		Iterator.RemoveCurrent();
	}

	for (auto Iterator =
		VoxelProxyActors.
			CreateIterator();
		Iterator;
		++Iterator)
	{
		if (VoxelProxyWanted.Contains(
			Iterator.Key()))
		{
			continue;
		}

		const double LastWanted =
			VoxelProxyLastWanted.
				FindRef(
					Iterator.Key());

		bool bReplacementReady =
			HasFineReplacementForProxy(
				Iterator.Key());

		if (!bReplacementReady)
		{
			const FVoxelGenerationBounds Bounds =
				Iterator.Key().
					GetBounds();

			const FIntPoint Center(
				(Bounds.Min.X +
				 Bounds.Max.X) /
					2,
				(Bounds.Min.Y +
				 Bounds.Max.Y) /
					2);

			for (const TPair<
				FVoxelSurfaceTileKey,
				TObjectPtr<AActor>>& Surface :
				SurfaceActors)
			{
				if (!Surface.Value ||
					!SurfaceWanted.Contains(
						Surface.Key))
				{
					continue;
				}

				const int32 Step =
					1 <<
					Surface.Key.Level;

				const int32 Side =
					FVoxelSurfaceTileData::
						CellSide *
					Step;

				const FIntPoint Min =
					Surface.Key.Coordinate *
					Side;

				if (Center.X >= Min.X &&
					Center.Y >= Min.Y &&
					Center.X <
						Min.X + Side &&
					Center.Y <
						Min.Y + Side)
				{
					bReplacementReady =
						true;

					break;
				}
			}
		}

		if (InNow -
				LastWanted <
			RetireDelaySeconds ||
			!bReplacementReady)
		{
			continue;
		}

		if (Iterator.Value())
		{
			Iterator.Value()->
				Destroy();
		}

		VoxelProxyData.Remove(
			Iterator.Key());

		VoxelProxyRevisions.Remove(
			Iterator.Key());

		VoxelProxyLastWanted.Remove(
			Iterator.Key());

		Iterator.RemoveCurrent();
	}

	for (auto Iterator =
		SurfaceActors.CreateIterator();
		Iterator;
		++Iterator)
	{
		if (SurfaceWanted.Contains(
			Iterator.Key()))
		{
			continue;
		}

		const double LastWanted =
			SurfaceLastWanted.FindRef(
				Iterator.Key());

		if (InNow -
				LastWanted <
			RetireDelaySeconds ||
			!HasReplacementForSurface(
				Iterator.Key()))
		{
			continue;
		}

		if (Iterator.Value())
		{
			Iterator.Value()->
				Destroy();
		}

		if (AActor* Water =
			WaterActors.FindRef(
				Iterator.Key()))
		{
			Water->Destroy();
		}

		WaterActors.Remove(
			Iterator.Key());

		SurfaceData.Remove(
			Iterator.Key());

		WaterData.Remove(
			Iterator.Key());

		SurfaceRevisions.Remove(
			Iterator.Key());

		SurfaceLastWanted.Remove(
			Iterator.Key());

		Iterator.RemoveCurrent();
	}

	for (auto Iterator =
		MacroActors.CreateIterator();
		Iterator;
		++Iterator)
	{
		if (MacroWanted.Contains(
			Iterator.Key()))
		{
			continue;
		}

		const double LastWanted =
			MacroLastWanted.FindRef(
				Iterator.Key());

		if (InNow -
				LastWanted <
			RetireDelaySeconds ||
			!HasReplacementForMacro(
				Iterator.Key()))
		{
			continue;
		}

		if (Iterator.Value())
		{
			Iterator.Value()->
				Destroy();
		}

		MacroData.Remove(
			Iterator.Key());

		MacroRevisions.Remove(
			Iterator.Key());

		MacroLastWanted.Remove(
			Iterator.Key());

		Iterator.RemoveCurrent();
	}
}

bool FVoxelViewManager::OnTask(FVoxelTaskResult&& InResult)
{
	if (InResult.Stamp.WorldEpoch != WorldEpoch || InResult.bCanceled || !InResult.bSuccess)
	{
		return InResult.Kind == EVoxelTaskKind::BuildFineMesh ||
			InResult.Kind == EVoxelTaskKind::BuildVoxelProxy ||
			InResult.Kind == EVoxelTaskKind::BuildSurface ||
			InResult.Kind == EVoxelTaskKind::BuildWater ||
			InResult.Kind == EVoxelTaskKind::BuildMacro;
	}

	switch (InResult.Kind)
	{
	case EVoxelTaskKind::BuildFineMesh:
		PublishFine(InResult);
		bCoverageDirty = true;
		return true;
	case EVoxelTaskKind::BuildVoxelProxy:
		PublishVoxelProxy(InResult);
		bCoverageDirty = true;
		return true;
	case EVoxelTaskKind::BuildSurface:
		if (InResult.Surface && SurfaceWanted.Contains(InResult.Stamp.SurfaceKey))
		{
			PublishSurface(InResult);
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
		if (InResult.Macro && MacroWanted.Contains(InResult.Stamp.MacroKey))
		{
			PublishMacro(InResult);
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
		Request.Execute = [Data = MoveTemp(Data), Registry, Shapes](const TAtomic<bool>& InCancel) mutable
		{
			FVoxelTaskResult Result;
			Result.VoxelProxy = MakeShared<FVoxelVoxelProxyData>(MoveTemp(Data));
			FVoxelSectionSnapshot Snapshot;
			Snapshot.Section = Result.VoxelProxy->Key.Coordinate;
			Snapshot.Blocks.Reserve(Result.VoxelProxy->Cells.Num());
			for (const FVoxelBlockState State : Result.VoxelProxy->Cells) Snapshot.Blocks.Add(State.Pack());
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
		VoxelProxyData.Remove(VoxelKey);
		VoxelProxyRevisions.Remove(VoxelKey);
		Module.GetRuntime()->GetChangeHierarchy().SetVoxelProxyRevision(WireKey.Coordinate, InInvalidate.Revision);

		const FIntPoint Coordinate(WireKey.Coordinate.X, WireKey.Coordinate.Y);
		const FVoxelSurfaceTileKey SurfaceKey { Coordinate, WireKey.Level };
		SurfaceData.Remove(SurfaceKey);
		WaterData.Remove(SurfaceKey);
		SurfaceRevisions.Remove(SurfaceKey);
		Module.GetRuntime()->GetChangeHierarchy().SetSurfaceRevision(Coordinate, InInvalidate.Revision);

		const FVoxelMacroTileKey MacroKey { Coordinate, WireKey.Level };
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
}

bool FVoxelViewManager::HasPrimaryRepresentation() const
{
	return !FineActors.IsEmpty() ||
		!VoxelProxyActors.IsEmpty() ||
		!SurfaceActors.IsEmpty() ||
		!MacroActors.IsEmpty();
}

void FVoxelViewManager::EnumerateModifiedSections(
	const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutSections) const
{
	OutSections.Reset();
	if (const FVoxelWorldRuntime* Runtime = Module.GetRuntime())
	{
		Runtime->GetChangeIndex().Enumerate(InBounds, OutSections);
	}
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

	Request.WorkClass =
		EVoxelWorkClass::Visible;

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

void FVoxelViewManager::RequestVoxelProxy(const FVoxelViewKey& InKey)
{
	if (!Module.IsAuthority())
	{
		TArray<FIntVector> Modified;
		Module.GetRuntime()->GetChangeIndex().Enumerate(InKey.GetBounds(), Modified);
		if (!Modified.IsEmpty())
		{
			const uint64* Revision = VoxelProxyRevisions.Find(InKey);
			Module.RequestRemoteRepresentation(
				EVoxelRepresentationWireType::VoxelProxy,
				{ InKey.Coordinate, InKey.Level },
				Revision ? *Revision : 0);
			return;
		}
	}
	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();
	const TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache = Module.GetGenerationCache();
	if (!Config || !Cache)
	{
		return;
	}
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildVoxelProxy;
	Request.WorkClass = EVoxelWorkClass::Boundary;
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.ViewKey = InKey;
	Request.Stamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(InKey.Coordinate);
	Request.Stamp.Token = HashCombineFast(GetTypeHash(InKey), GetTypeHash(Request.Stamp.Revision));
	Request.ReservedBytes = 48ull * 1024ull * 1024ull;
	if (Scheduler.Has(Request.Stamp, Request.Kind))
	{
		return;
	}
	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();
	const TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes = Module.GetShapes();
	const uint64 RecipeHash = Module.GetManifest().RecipeHash;
	Request.Execute = [Config, Cache, Registry, Shapes, RecipeHash, InKey](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		Result.VoxelProxy = MakeShared<FVoxelVoxelProxyData>();
		const FVoxelVoxelProxyBuilder Builder(Config.ToSharedRef(), Cache.ToSharedRef());
		Result.bSuccess = Builder.BuildNatural(InKey, *Result.VoxelProxy, Result.Error, &InCancel);
		if (!Result.bSuccess || !Registry || !Shapes)
		{
			return Result;
		}
		FVoxelSectionSnapshot Snapshot;
		Snapshot.Section = InKey.Coordinate;
		Snapshot.Stamp.RecipeHash = RecipeHash;
		Snapshot.Blocks.Reserve(Result.VoxelProxy->Cells.Num());
		for (const FVoxelBlockState State : Result.VoxelProxy->Cells)
		{
			Snapshot.Blocks.Add(State.Pack());
		}
		Result.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
		Result.bSuccess = FVoxelSectionMesher::Build(
			Snapshot,
			*Registry,
			*Shapes,
			*Result.VoxelProxyMesh,
			&InCancel);
		if (!Result.bSuccess)
		{
			Result.Error = TEXT("Failed to mesh voxel proxy");
		}
		return Result;
	};
	Scheduler.Enqueue(MoveTemp(Request));
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

			Result.Surface =
				MakeShared<
					FVoxelSurfaceTileData>();

			const FVoxelSurfaceProxyBuilder Builder(
				Generator.ToSharedRef(),
				Settings,
				*this);

			Result.bSuccess =
				Builder.Build(
					InKey,
					*Result.Surface,
					Result.Error,
					&InCancel);

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
				*Result.Water,
				*Registry,
				WaterMesh,
				Result.Error,
				&InCancel);
			if (Result.bSuccess)
			{
				AppendNonEmptyBatches(*Result.SurfaceMesh, MoveTemp(WaterMesh));
			}

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

void FVoxelViewManager::PublishFine(const FVoxelTaskResult& InResult)
{
	if (!InResult.FineMesh || !FineWanted.Contains(InResult.Stamp.Section))
	{
		return;
	}
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(InResult.Stamp.Section);
	if (!Section ||
		Section->Stamp.Epoch != InResult.Stamp.WorldEpoch ||
		Section->Stamp.Token != InResult.Stamp.Token ||
		Section->CommittedRevision != InResult.Stamp.Revision)
	{
		return;
	}
	AActor* Host = FineActors.FindRef(InResult.Stamp.Section);
	if (!PublishMeshActor(
		Host,
		FVector(InResult.Stamp.Section * ViewSectionSide) * Module.BlockSize(),
		Module.BlockSize(),
		*InResult.FineMesh))
	{
		return;
	}
	FineActors.Add(InResult.Stamp.Section, Host);
	FineRevisions.Add(InResult.Stamp.Section, InResult.Stamp.Revision);
}

void FVoxelViewManager::PublishVoxelProxy(const FVoxelTaskResult& InResult)
{
	const FVoxelViewKey& Key = InResult.Stamp.ViewKey;
	if (!InResult.VoxelProxy ||
		!InResult.VoxelProxyMesh ||
		!VoxelProxyWanted.Contains(Key) ||
		InResult.Stamp.Token != HashCombineFast(GetTypeHash(Key), GetTypeHash(InResult.Stamp.Revision)) ||
		Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(Key.Coordinate) != InResult.Stamp.Revision)
	{
		return;
	}
	if (!InResult.VoxelProxy->bHasVisibleSurfaceEvidence)
	{
		if (AActor* Existing = VoxelProxyActors.FindRef(Key))
		{
			Existing->Destroy();
		}
		VoxelProxyActors.Remove(Key);
		VoxelProxyRevisions.Add(Key, InResult.Stamp.Revision);
		VoxelProxyData.Add(Key, InResult.VoxelProxy);
		return;
	}
	AActor* Host = VoxelProxyActors.FindRef(Key);
	if (!PublishMeshActor(
		Host,
		FVector(Key.GetBounds().Min) * Module.BlockSize(),
		Module.BlockSize() * Key.GetStep(),
		*InResult.VoxelProxyMesh))
	{
		return;
	}
	VoxelProxyActors.Add(Key, Host);
	VoxelProxyRevisions.Add(Key, InResult.Stamp.Revision);
	VoxelProxyData.Add(Key, InResult.VoxelProxy);
}

void FVoxelViewManager::PublishSurface(
	const FVoxelTaskResult& InResult)
{
	const FVoxelSurfaceTileKey& Key =
		InResult.Stamp.SurfaceKey;

	if (!InResult.Surface ||
		!InResult.SurfaceMesh ||
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
		SurfaceActors.FindRef(
			Key);

	const int32 TileSide =
		InResult.Surface->
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
		*InResult.SurfaceMesh))
	{
		SurfaceActors.Add(
			Key,
			Host);

		SurfaceRevisions.Add(
			Key,
			InResult.Stamp.Revision);
	}
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

void FVoxelViewManager::PublishMacro(
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
		return;
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

		MacroRevisions.Add(
			Key,
			InResult.Stamp.Revision);
	}
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
