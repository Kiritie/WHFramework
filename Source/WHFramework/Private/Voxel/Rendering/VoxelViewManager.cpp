#include "Voxel/Rendering/VoxelViewManager.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
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
	const TConstArrayView<FVector> InObservers)
{
	UpdateFineAndVoxelProxy(
		InObservers);

	UpdateSurface(
		InObservers);

	UpdateMacro(
		InObservers);

	const double Now =
		FPlatformTime::Seconds();

	UpdateWantedTimestamps(
		Now);

	ResolveTransitionVisibility();

	CleanupRetiredRepresentations(
		Now);
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

		Pair.Value->
			SetActorHiddenInGame(
				bHidden);
	}

	for (const TPair<
		FVoxelViewKey,
		TObjectPtr<AActor>>& Pair :
		VoxelProxyActors)
	{
		if (Pair.Value)
		{
			Pair.Value->
				SetActorHiddenInGame(
					false);
		}
	}

	for (const TPair<
		FVoxelSurfaceTileKey,
		TObjectPtr<AActor>>& Pair :
		SurfaceActors)
	{
		if (Pair.Value)
		{
			Pair.Value->
				SetActorHiddenInGame(
					false);
		}
	}

	for (const TPair<
		FVoxelSurfaceTileKey,
		TObjectPtr<AActor>>& Pair :
		WaterActors)
	{
		if (Pair.Value)
		{
			Pair.Value->
				SetActorHiddenInGame(
					false);
		}
	}

	for (const TPair<
		FVoxelMacroTileKey,
		TObjectPtr<AActor>>& Pair :
		MacroActors)
	{
		if (Pair.Value)
		{
			Pair.Value->
				SetActorHiddenInGame(
					false);
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
		return true;
	case EVoxelTaskKind::BuildVoxelProxy:
		PublishVoxelProxy(InResult);
		return true;
	case EVoxelTaskKind::BuildSurface:
		if (InResult.Surface && SurfaceWanted.Contains(InResult.Stamp.SurfaceKey))
		{
			PublishSurface(InResult);
			const FVoxelSurfaceTileKey Key = InResult.Stamp.SurfaceKey;
			SurfaceData.Add(Key, InResult.Surface);
			FVoxelTaskRequest Request;
			Request.Kind = EVoxelTaskKind::BuildWater;
			Request.WorkClass = EVoxelWorkClass::Visible;
			Request.Stamp = InResult.Stamp;
			Request.ReservedBytes = 24ull * 1024ull * 1024ull;
			const TSharedPtr<const FVoxelSurfaceTileData> Surface = InResult.Surface;
			const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();
			Request.Execute = [Surface, Registry](const TAtomic<bool>& InCancel)
			{
				FVoxelTaskResult Result;
				if (InCancel.Load())
				{
					return Result;
				}
				Result.Water = MakeShared<FVoxelWaterSurfaceTileData>();
				FVoxelWaterViewBuilder Builder;
				Result.bSuccess = Builder.Build(*Surface, *Result.Water, Result.Error);
				if (Result.bSuccess && Registry)
				{
					Result.WaterMesh = MakeShared<FVoxelSectionMeshResult>();
					Result.bSuccess = FVoxelHeightfieldMesher::BuildWater(
						*Result.Water,
						*Registry,
						*Result.WaterMesh,
						Result.Error,
						&InCancel);
				}
				return Result;
			};
			Scheduler.Enqueue(MoveTemp(Request));
		}
		return true;
	case EVoxelTaskKind::BuildWater:
		if (InResult.Water && SurfaceWanted.Contains(InResult.Stamp.SurfaceKey))
		{
			PublishWater(InResult);
			WaterData.Add(InResult.Stamp.SurfaceKey, MoveTemp(InResult.Water));
		}
		return true;
	case EVoxelTaskKind::BuildMacro:
		if (InResult.Macro && MacroWanted.Contains(InResult.Stamp.MacroKey))
		{
			PublishMacro(InResult);
			MacroData.Add(InResult.Stamp.MacroKey, MoveTemp(InResult.Macro));
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
			return Result;
		};
	}
	else if (InReply.Type == EVoxelRepresentationWireType::MacroTerrain)
	{
		FVoxelMacroTileData Data;
		if (!FVoxelRepresentationSync::DecodeMacro(InReply.Data, Data, OutError)) return false;
		Request.Kind = EVoxelTaskKind::BuildMacro;
		Request.Stamp.MacroKey = Data.Key;
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
	for (const FVoxelRepresentationWireKey& WireKey : InInvalidate.Keys)
	{
		const FVoxelViewKey VoxelKey { WireKey.Coordinate, WireKey.Level };
		if (AActor* Actor = VoxelProxyActors.FindRef(VoxelKey)) Actor->Destroy();
		VoxelProxyActors.Remove(VoxelKey); VoxelProxyData.Remove(VoxelKey); VoxelProxyRevisions.Remove(VoxelKey);
		Module.GetRuntime()->GetChangeHierarchy().SetVoxelProxyRevision(WireKey.Coordinate, InInvalidate.Revision);

		const FIntPoint Coordinate(WireKey.Coordinate.X, WireKey.Coordinate.Y);
		const FVoxelSurfaceTileKey SurfaceKey { Coordinate, WireKey.Level };
		if (AActor* Actor = SurfaceActors.FindRef(SurfaceKey)) Actor->Destroy();
		if (AActor* Actor = WaterActors.FindRef(SurfaceKey)) Actor->Destroy();
		SurfaceActors.Remove(SurfaceKey); WaterActors.Remove(SurfaceKey);
		SurfaceData.Remove(SurfaceKey); WaterData.Remove(SurfaceKey); SurfaceRevisions.Remove(SurfaceKey);
		Module.GetRuntime()->GetChangeHierarchy().SetSurfaceRevision(Coordinate, InInvalidate.Revision);

		const FVoxelMacroTileKey MacroKey { Coordinate, WireKey.Level };
		if (AActor* Actor = MacroActors.FindRef(MacroKey)) Actor->Destroy();
		MacroActors.Remove(MacroKey); MacroData.Remove(MacroKey); MacroRevisions.Remove(MacroKey);
		Module.GetRuntime()->GetChangeHierarchy().SetMacroRevision(Coordinate, InInvalidate.Revision);
	}
}

void FVoxelViewManager::InvalidateSection(const FIntVector& InKey)
{
	FineRevisions.Remove(InKey);
	Scheduler.CancelSection(InKey);
	const FVoxelGenerationBounds Bounds { InKey * ViewSectionSide, (InKey + FIntVector(1)) * ViewSectionSide };
	for (auto Iterator = VoxelProxyData.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator.Key().GetBounds().Intersects(Bounds))
		{
			if (AActor* Actor = VoxelProxyActors.FindRef(Iterator.Key()))
			{
				Actor->Destroy();
			}
			VoxelProxyActors.Remove(Iterator.Key());
			VoxelProxyRevisions.Remove(Iterator.Key());
			Iterator.RemoveCurrent();
		}
	}
	for (auto Iterator = SurfaceData.CreateIterator(); Iterator; ++Iterator)
	{
		const int32 Side = FVoxelSurfaceTileData::CellSide * (8 << Iterator.Key().Level);
		const FVoxelGenerationBounds TileBounds {
			FIntVector(Iterator.Key().Coordinate.X * Side, Iterator.Key().Coordinate.Y * Side, MIN_int32),
			FIntVector((Iterator.Key().Coordinate.X + 1) * Side, (Iterator.Key().Coordinate.Y + 1) * Side, MAX_int32) };
		if (Bounds.Intersects(TileBounds))
		{
			if (AActor* Actor = SurfaceActors.FindRef(Iterator.Key())) Actor->Destroy();
			if (AActor* Actor = WaterActors.FindRef(Iterator.Key())) Actor->Destroy();
			SurfaceActors.Remove(Iterator.Key());
			WaterActors.Remove(Iterator.Key());
			SurfaceRevisions.Remove(Iterator.Key());
			WaterData.Remove(Iterator.Key());
			Iterator.RemoveCurrent();
		}
	}
	for (auto Iterator = MacroData.CreateIterator(); Iterator; ++Iterator)
	{
		const int32 Side = FVoxelMacroTileData::CellSide *
			(FVoxelMacroTileData::BaseStep << Iterator.Key().Level);
		const FVoxelGenerationBounds TileBounds {
			FIntVector(Iterator.Key().Coordinate.X * Side, Iterator.Key().Coordinate.Y * Side, MIN_int32),
			FIntVector((Iterator.Key().Coordinate.X + 1) * Side, (Iterator.Key().Coordinate.Y + 1) * Side, MAX_int32) };
		if (Bounds.Intersects(TileBounds))
		{
			if (AActor* Actor = MacroActors.FindRef(Iterator.Key())) Actor->Destroy();
			MacroActors.Remove(Iterator.Key());
			MacroRevisions.Remove(Iterator.Key());
			Iterator.RemoveCurrent();
		}
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
		const FVoxelSection* Section = Module.GetRuntime()->FindSection(Pair.Key);
		if (Section && Section->Status == EVoxelSectionStatus::DataReady)
		{
			const uint64* Revision = FineRevisions.Find(Pair.Key);
			if (!Revision || *Revision != Section->CommittedRevision)
			{
				RequestFine(Pair.Key, Section->CommittedRevision);
			}
		}
	}
	for (const FVoxelViewKey& Key : Module.GetCurrentInterest().VoxelProxy)
	{
		VoxelProxyWanted.Add(Key);
		const uint64 Revision = Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(Key.Coordinate);
		const uint64* PublishedRevision = VoxelProxyRevisions.Find(Key);
		if (!PublishedRevision || *PublishedRevision != Revision)
		{
			RequestVoxelProxy(Key);
		}
	}
}

void FVoxelViewManager::UpdateSurface(const TConstArrayView<FVector> InObservers)
{
	SurfaceWanted = Module.GetCurrentInterest().Surface;
	for (const FVoxelSurfaceTileKey& Key : SurfaceWanted)
	{
		const uint64 Revision = Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(Key.Coordinate);
		const uint64* PublishedRevision = SurfaceRevisions.Find(Key);
		if (!PublishedRevision || *PublishedRevision != Revision)
		{
			RequestSurface(Key);
		}
	}
}

void FVoxelViewManager::UpdateMacro(const TConstArrayView<FVector> InObservers)
{
	MacroWanted = Module.GetCurrentInterest().Macro;
	for (const FVoxelMacroTileKey& Key : MacroWanted)
	{
		const uint64 Revision = Module.GetRuntime()->GetChangeHierarchy().GetMacroRevision(Key.Coordinate);
		const uint64* PublishedRevision = MacroRevisions.Find(Key);
		if (!PublishedRevision || *PublishedRevision != Revision)
		{
			RequestMacro(Key);
		}
	}
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
		32ull *
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
