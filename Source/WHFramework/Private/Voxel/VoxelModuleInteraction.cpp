#include "Voxel/VoxelModule.h"
#include "Ability/Inventory/InventorySnapshotTypes.h"
#include "Ability/Inventory/InventoryCommandTypes.h"
#include "Ability/Inventory/InventoryModule.h"
#include "Ability/Inventory/InventoryItemRegistry.h"
#include "Voxel/Interaction/VoxelInventoryDomainBridge.h"
#include "Voxel/Save/InventoryVoxelEntityCodec.h"
#include "Misc/SecureHash.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Rendering/VoxelViewLod.h"

#include "Async/Async.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include <limits>
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Asset/AssetModuleStatics.h"
#include "GameFramework/PlayerController.h"
#include "Main/MainModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Authoring/VoxelViewProfile.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Collision/VoxelPhysicsRetirement.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Voxel/Navigation/VoxelNavigationPresenter.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "SaveGame/SaveGameModule.h"
#include "SaveGame/SaveGameStorage.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelBuiltinFeatures.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Agent/VoxelAgentComponent.h"
#include "Voxel/Interaction/VoxelEditTransaction.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Map/VoxelMapTileCache.h"
#include "Voxel/Rendering/VoxelTerrainFrameBudget.h"
#include "Voxel/Map/VoxelMapSurfaceResolver.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Rendering/VoxelFineSafety.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Scene/VoxelSceneRegion.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/VoxelItemBridge.h"

#include "Voxel/VoxelModuleInternal.h"

using namespace VoxelModuleInternal;

FVoxelTraceResult UVoxelModule::Trace(
	const FVector& InStart,
	const FVector& InDirection,
	const double InDistance) const
{
	if (!IsReady())
	{
		return {};
	}
	return FVoxelRaycast::Trace(
		*Runtime,
		*Registry.GetSnapshot(),
		*Shapes,
		InStart,
		InDirection,
		InDistance,
		BlockSize());
}

bool UVoxelModule::IsActorRayClear(
	const FVector& InStart,
	const FVector& InEnd,
	AActor* InIgnore) const
{
	FCollisionQueryParams Parameters(SCENE_QUERY_STAT(VoxelActorLineOfSight), false);
	if (InIgnore)
	{
		Parameters.AddIgnoredActor(InIgnore);
	}
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(
		Hit,
		InStart,
		InEnd,
		ECC_Visibility,
		Parameters);
}

bool UVoxelModule::VerifyView(
	APlayerController* InController,
	AActor* InAuthorizedObserver,
	const FVoxelEditIntent& InIntent,
	FVoxelTraceResult& OutTrace,
	FString& OutError) const
{
	if (!IsReady() ||
		!IsAuthority() ||
		!InAuthorizedObserver ||
		InAuthorizedObserver->GetWorld() != GetWorld() ||
		InIntent.Origin.ContainsNaN() ||
		InIntent.Direction.ContainsNaN() ||
		InIntent.Direction.IsNearlyZero())
	{
		OutError = TEXT("Invalid voxel edit view");
		return false;
	}
	FVector Eye;
	FRotator Rotation;
	InAuthorizedObserver->GetActorEyesViewPoint(Eye, Rotation);
	if (InController && InController->GetPawn() == InAuthorizedObserver)
	{
		Rotation = InController->GetControlRotation();
	}
	if (GetWorld()->GetNetMode() != NM_Standalone)
	{
		if (!InController ||
			!InController->HasAuthority() ||
			FVector::DistSquared(InIntent.Origin, Eye) > FMath::Square(500.0) ||
			FVector::DotProduct(Rotation.Vector(), InIntent.Direction.GetSafeNormal()) < 0.984807753)
		{
			OutError = TEXT("View is outside the authorized observer");
			return false;
		}
	}
	OutTrace = Trace(InIntent.Origin, InIntent.Direction, 600.0);
	if (OutTrace.Status != EVoxelTraceStatus::Hit ||
		OutTrace.Index != InIntent.ExpectedTarget ||
		FVector::Distance(OutTrace.Point, Eye) > 600.0 ||
		!IsActorRayClear(
			InIntent.Origin,
			OutTrace.Point - InIntent.Direction.GetSafeNormal() * 0.1,
			InAuthorizedObserver))
	{
		OutError = OutTrace.Status == EVoxelTraceStatus::NeedsData ?
			TEXT("Target data is not ready") :
			TEXT("Target is not visible or is too far");
		return false;
	}
	OutError.Reset();
	return true;
}

bool UVoxelModule::PlacementOverlapsActors(const FVoxelInteractionPlan& InPlan) const
{
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	for (const FVoxelCellEdit& Edit : InPlan.Cells)
	{
		if (Edit.Value.IsAir() || Edit.Value == Edit.Expected)
		{
			continue;
		}
		const FVoxelRuntimeDefinition* Definition = Registry.GetSnapshot()->Find(Edit.Value.TypeId);
		if (!Definition || !Definition->bSolid)
		{
			continue;
		}
		const FVoxelResolvedShape& Shape = Definition->GetShape(*Shapes, Edit.Value.State);
		for (const FBox& Box : Shape.CollisionBoxes)
		{
			const FVector Center = (FVector(Edit.Position) + Box.GetCenter()) * BlockSize();
			const FVector HalfExtent = Box.GetExtent() * BlockSize() - FVector(0.1);
			if (HalfExtent.GetMin() > 0.0 && GetWorld()->OverlapAnyTestByObjectType(
				Center,
				FQuat::Identity,
				Objects,
				FCollisionShape::MakeBox(HalfExtent)))
			{
				return true;
			}
		}
	}
	return false;
}

UAbilityInventoryBase* UVoxelModule::ResolveInventory(
	APlayerController* InController,
	AActor* InSource) const
{
	AActor* Owner = InController && InController->GetPawn() ? InController->GetPawn() : InSource;
	IAbilityInventoryAgentInterface* Agent = Cast<IAbilityInventoryAgentInterface>(Owner);
	return Agent ? Agent->GetInventory() : nullptr;
}

FVoxelEditReply UVoxelModule::ExecuteIntent(
	APlayerController* InController,
	AActor* InSource,
	const FVoxelEditIntent& InIntent,
	bool bTrustedStandaloneCreative)
{
	FVoxelEditReply Reply;
	Reply.RequestId = InIntent.RequestId;
	Reply.Code = EVoxelEditCode::Rejected;
	if (!InIntent.RequestId || !InSource || IsWorldSaveSuspended() || bMutating || !IsReady() || !IsAuthority())
	{
		Reply.Code = EVoxelEditCode::Busy;
		return Reply;
	}
	UInventoryModule* System = UInventoryModule::GetPtr(GetWorld());
	if (!System || !System->IsReady() || System->IsArchiveFrozen())
	{
		Reply.Code = EVoxelEditCode::Busy;
		Reply.Reason = TEXT("Inventory.WorldNotReadyOrSaving");
		return Reply;
	}
	if (!InIntent.InventoryStoreId.IsValid() || InIntent.InventoryStoreId != System->GetStoreId())
	{
		Reply.Code = EVoxelEditCode::Stale;
		Reply.Reason = TEXT("Inventory.StoreChanged");
		return Reply;
	}
	if (InIntent.Action == EVoxelEditAction::BreakCancel)
	{
		Breaking.Remove(InSource);
		Reply.Code = EVoxelEditCode::Accepted;
		return Reply;
	}

	FVoxelTraceResult Hit;
	if (!VerifyView(InController, InSource, InIntent, Hit, Reply.Reason))
	{
		Reply.Code = Hit.Status == EVoxelTraceStatus::NeedsData ?
			EVoxelEditCode::NeedsData :
			EVoxelEditCode::Rejected;
		Breaking.Remove(InSource);
		return Reply;
	}
	const FVoxelSection* TargetSection = Runtime->FindSection(VoxelModuleToSection(Hit.Index));
	if (!TargetSection || TargetSection->CommittedRevision != InIntent.ExpectedRevision)
	{
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	if (Manifest.TerrainType == EVoxelTerrainType::Superflat &&
		Hit.Index.Z == VoxelGeneration::SuperflatSurfaceZ &&
		(InIntent.Action == EVoxelEditAction::BreakBegin || InIntent.Action == EVoxelEditAction::BreakPulse))
	{
		Breaking.Remove(InSource);
		Reply.Reason = TEXT("Voxel.SuperflatFoundationUnbreakable");
		return Reply;
	}

	const double Now = FPlatformTime::Seconds();
	if (InIntent.Action == EVoxelEditAction::BreakBegin)
	{
		FBreak Break;
		Break.InventoryStoreId = InIntent.InventoryStoreId;
		Break.Target = Hit.Index;
		Break.Expected = Hit.State;
		Break.Began = Now;
		Break.LastPulse = Now;
		Breaking.Add(InSource, Break);
		Reply.Code = EVoxelEditCode::Pending;
		return Reply;
	}
	if (InIntent.Action == EVoxelEditAction::BreakPulse)
	{
		FBreak* Break = Breaking.Find(InSource);
		if (!Break ||
			Break->InventoryStoreId != InIntent.InventoryStoreId ||
			Break->Target != Hit.Index ||
			Break->Expected != Hit.State ||
			Now - Break->LastPulse > 0.35)
		{
			Breaking.Remove(InSource);
			Reply.Code = EVoxelEditCode::Stale;
			return Reply;
		}
		Break->LastPulse = Now;
		const FVoxelRuntimeDefinition* Definition = Registry.GetSnapshot()->Find(Hit.State.TypeId);
		if (!Definition || (Now - Break->Began) * 1000.0 < Definition->BreakMilliseconds)
		{
			Reply.Code = EVoxelEditCode::Pending;
			return Reply;
		}
	}

    UAbilityInventoryBase* Inventory = ResolveInventory(InController, InSource);
    if(!System || !System->IsReady() || !Inventory || !InventoryBridge || !InventoryContribution)
    {
        Reply.Reason = TEXT("Inventory.WorldNotReady");
        return Reply;
    }
    const FInventorySnapshot Snapshot = Inventory->MakeSnapshot();
    const UVoxelAgentComponent* Agent = InSource->FindComponentByClass<UVoxelAgentComponent>();
    bTrustedStandaloneCreative = bTrustedStandaloneCreative && GetWorld()->GetNetMode() == NM_Standalone && Agent && Agent->bCreativeInStandalone;
    uint16 PlaceType = 0;
    const FInventorySlotState* Slot = nullptr;
    if(InIntent.Action == EVoxelEditAction::Place)
    {
        FPrimaryAssetId VoxelId = InIntent.ExpectedItemID;
        if(!bTrustedStandaloneCreative)
        {
            const IAbilityInventoryAgentInterface* InventoryAgent = Cast<IAbilityInventoryAgentInterface>(InSource);
            Slot = Snapshot.FindSlot(InIntent.InventorySlot);
            if(!InventoryAgent || InIntent.InventorySlot != InventoryAgent->GetSelectedInventorySlot() ||
                Snapshot.InventoryId != InIntent.ExpectedInventoryId || Snapshot.Revision != InIntent.ExpectedInventoryRevision ||
                Snapshot.Generation != InIntent.ExpectedInventoryGeneration || !Slot || Slot->bLocked || Slot->Item.IsEmpty() ||
                Slot->Item.ItemId != InIntent.ExpectedItemID)
            {
                Reply.Code = EVoxelEditCode::Stale;
                Reply.Reason = TEXT("Inventory.SelectedSlotChanged");
                return Reply;
            }
            const FInventoryItemDefinition* ItemDefinition = System->GetRegistry().Find(Slot->Item.ItemId);
            if(!ItemDefinition || !ItemDefinition->PlaceableVoxelId.IsValid())
            {
                Reply.Reason = TEXT("Inventory.ItemNotPlaceable");
                return Reply;
            }
            VoxelId = ItemDefinition->PlaceableVoxelId;
        }
        const FVoxelRuntimeDefinition* Definition = Registry.GetSnapshot()->Find(VoxelId);
        if(!Definition)
        {
            Reply.Reason = TEXT("Inventory.PlaceableVoxelUnknown");
            return Reply;
        }
        PlaceType = Definition->TypeId;
    }
    FVoxelInteractionPlan Plan;
	if (InIntent.PlacementYaw > 3)
	{
		Reply.Reason = TEXT("Voxel.InvalidPlacementRotation");
		return Reply;
	}
    if(!BuildInteractionPlan(Hit, InIntent.Action, PlaceType, InIntent.PlacementYaw, InSource, Plan, Reply.Reason) ||
        !ValidateInteractionPlan(Plan, Reply.Reason))
    {
        return Reply;
    }
    Plan.Action = InIntent.Action;
    if (InIntent.Action == EVoxelEditAction::Place && Plan.ObjectAnchor != InIntent.ExpectedPlacementAnchor)
    {
        Reply.Code = EVoxelEditCode::Stale;
        Reply.Reason = TEXT("Voxel.PlacementAnchorChanged");
        return Reply;
    }
    Plan.RequestId = InIntent.RequestId;
    const FString EventText = FString::Printf(TEXT("Voxel/%s/%s/%s/%llu/%d"),
        *System->GetWorldEpoch().ToString(EGuidFormats::Digits), *System->GetStoreId().ToString(EGuidFormats::Digits), *Snapshot.OwnerId.ToString(EGuidFormats::Digits),
        InIntent.RequestId, static_cast<int32>(InIntent.Action));
    const FTCHARToUTF8 EventUtf8(*EventText);
    FMD5 EventHash;
    EventHash.Update(reinterpret_cast<const uint8*>(EventUtf8.Get()), EventUtf8.Length());
    uint8 EventDigest[16];
    EventHash.Final(EventDigest);
    FVoxelByteReader EventReader(MakeArrayView(EventDigest));
    Plan.EventId = EventReader.Guid();
    FInventoryBatchRequest Request;
    if(Slot && Plan.Cost > 0)
    {
        FInventoryOperation Remove;
        Remove.Type = EInventoryOperationType::Remove;
        Remove.SourceInventoryId = Snapshot.InventoryId;
        Remove.SourceSlot = Slot->Key;
        Remove.Filter.bMatchSignature = true;
        Remove.Filter.Signature = Slot->Item.Signature();
        Remove.Requested = Plan.Cost;
        Remove.AmountMode = EInventoryAmountMode::Exact;
        Request.Operations.Add(MoveTemp(Remove));
        Request.ExpectedRevisions.Add({Snapshot.InventoryId, Snapshot.Revision, Snapshot.Generation});
    }
    FInventoryCommandResult Result;
    if(!ExecuteInventoryWorldEdit(Plan, InSource, Request, System->MakeServerAuthority(Snapshot.OwnerId, TEXT("Voxel.Interaction")), Result, Reply.Reason))
    {
        Reply.Code = Result.Code == EInventoryResultCode::ConfirmationPending ? EVoxelEditCode::ConfirmationPending :
            Result.Code == EInventoryResultCode::RevisionStale ? EVoxelEditCode::Stale : EVoxelEditCode::Rejected;
        return Reply;
    }
    Breaking.Remove(InSource);
    OnInteractionCommitted(Plan, InSource);
    Reply.Code = EVoxelEditCode::Accepted;
    return Reply;
}


EVoxelEditCode UVoxelModule::PreviewPlacement(const FVoxelEditIntent& InIntent, AActor* InSource, FVoxelInteractionPlan& OutPlan, FString& OutError)
{
	OutPlan = {};
	if (!IsReady() || !InSource || InSource->GetWorld() != GetWorld() || InIntent.Action != EVoxelEditAction::Place || InIntent.PlacementYaw > 3)
	{
		return EVoxelEditCode::Rejected;
	}
	const UInventoryModule* System = UInventoryModule::GetPtr(GetWorld());
	const FInventoryItemDefinition* Item = System ? System->GetRegistry().Find(InIntent.ExpectedItemID) : nullptr;
	const FVoxelRuntimeDefinition* Definition = Item ? Registry.GetSnapshot()->Find(Item->PlaceableVoxelId) : nullptr;
	if (!Definition)
	{
		OutError = TEXT("Inventory.ItemNotPlaceable");
		return EVoxelEditCode::Rejected;
	}
	const FVoxelTraceResult Hit = Trace(InIntent.Origin, InIntent.Direction);
	if (Hit.Status == EVoxelTraceStatus::NeedsData) return EVoxelEditCode::NeedsData;
	if (Hit.Status != EVoxelTraceStatus::Hit || Hit.Index != InIntent.ExpectedTarget) return EVoxelEditCode::Stale;
	if (!BuildInteractionPlan(Hit, EVoxelEditAction::Place, Definition->TypeId, InIntent.PlacementYaw, InSource, OutPlan, OutError))
	{
		return OutPlan.bNeedsData ? EVoxelEditCode::NeedsData : EVoxelEditCode::Rejected;
	}
	for (const FVoxelCellEdit& Cell : OutPlan.Cells)
	{
		for (uint8 Face = 0; Face < 6; ++Face)
		{
			FVoxelBlockState Neighbor;
			if (!Runtime->TryGetBlock(Cell.Position + VoxelCoord::Direction(Face), Neighbor)) return EVoxelEditCode::NeedsData;
		}
	}
	if (OutPlan.ObjectAnchor != InIntent.ExpectedPlacementAnchor || !ValidateInteractionPlan(OutPlan, OutError) || PlacementOverlapsActors(OutPlan))
	{
		return EVoxelEditCode::Rejected;
	}
	return EVoxelEditCode::Accepted;
}

bool UVoxelModule::BuildInteractionPlan(const FVoxelTraceResult& InHit, EVoxelEditAction InAction, uint16 InPlaceType, uint8 InPlacementYaw, AActor* InSource, FVoxelInteractionPlan& OutPlan, FString& OutError)
{
	return FVoxelEditTransaction::Build(*Runtime, *Registry.GetSnapshot(), *Shapes, InHit, InAction, InPlaceType, InPlacementYaw, BlockSize(), OutPlan, OutError);
}

void UVoxelModule::OnInteractionCommitted(const FVoxelInteractionPlan& InPlan, AActor* InSource)
{
}

bool UVoxelModule::ValidateInteractionPlan(FVoxelInteractionPlan& InOutPlan, FString& OutError)
{
	return FVoxelEditTransaction::ValidateBatch(*Runtime, *Registry.GetSnapshot(), *Shapes, InOutPlan.Cells, OutError);
}

bool UVoxelModule::ApplyPrefab(
	const FVoxelPrefabSaveData& InPrefab,
	const FIntVector& InOrigin,
	FString& OutError)
{
	FInventoryCommandResult Result;
	return ApplyPrefab(InPrefab, InOrigin, Result, OutError);
}

bool UVoxelModule::ApplyPrefab(
	const FVoxelPrefabSaveData& InPrefab,
	const FIntVector& InOrigin,
	FInventoryCommandResult& OutResult,
	FString& OutError)
{
	OutResult = FInventoryCommandResult();
	if (!IsAuthority() || !IsReady() || IsWorldSaveSuspended() || bMutating || InPrefab.Cells.IsEmpty())
	{
		OutError = TEXT("Invalid prefab operation");
		return false;
	}
	if (!UVoxelPrefabData::ValidateCells(InPrefab, *Registry.GetSnapshot(), OutError))
	{
		return false;
	}
	TArray<FVoxelCellEdit> Cells;
	TSet<FIntVector> Seen;
	for (const FVoxelPrefabCell& Cell : InPrefab.Cells)
	{
		const int64 X = static_cast<int64>(InOrigin.X) + Cell.Offset.X;
		const int64 Y = static_cast<int64>(InOrigin.Y) + Cell.Offset.Y;
		const int64 Z = static_cast<int64>(InOrigin.Z) + Cell.Offset.Z;
		if (FMath::Abs(X) >= VoxelBlock::MaxAbsCoordinate ||
			FMath::Abs(Y) >= VoxelBlock::MaxAbsCoordinate ||
			FMath::Abs(Z) >= VoxelBlock::MaxAbsCoordinate)
		{
			OutError = TEXT("Prefab target exceeds voxel coordinate bounds");
			return false;
		}
		FVoxelCellEdit Edit;
		Edit.Position = FIntVector(static_cast<int32>(X), static_cast<int32>(Y), static_cast<int32>(Z));
		if (Seen.Contains(Edit.Position) ||
			!Runtime->TryGetBlock(Edit.Position, Edit.Expected) ||
			(!Cell.bClear && !FVoxelItemBridge::ToBlock(*Registry.GetSnapshot(), Cell.Item, Edit.Value)))
		{
			OutError = TEXT("Prefab contains duplicate, unloaded, or invalid cells");
			return false;
		}
		if (Cell.bClear)
		{
			Edit.Value = {};
		}
		Seen.Add(Edit.Position);
		Cells.Add(Edit);
	}
	FVoxelInteractionPlan Plan;
	Plan.Cells = Cells;
	if (!ValidateInteractionPlan(Plan, OutError))
	{
		return false;
	}
	if (PlacementOverlapsActors(Plan))
	{
		OutError = TEXT("Prefab overlaps live actors");
		return false;
	}

    Plan.EventId = FGuid::NewGuid();
    Plan.ObjectAction = TEXT("World.ApplyPrefab");
    UInventoryModule* System = UInventoryModule::GetPtr(GetWorld());
    if(!System)
    {
        OutError = TEXT("Inventory.WorldNotReady");
        return false;
    }
    FInventoryBatchRequest Request;
    return ExecuteInventoryWorldEdit(Plan, nullptr, Request, System->MakeServerAuthority(System->GetWorldId(), TEXT("World.ApplyPrefab")), OutResult, OutError);
}


bool UVoxelModule::ExportPrefab(
	const FIntVector& InMin,
	const FIntVector& InMax,
	FVoxelPrefabSaveData& OutPrefab,
	FString& OutError) const
{
	if (!IsReady())
	{
		OutError = TEXT("Voxel world is not running");
		return false;
	}
	const int64 SizeX = static_cast<int64>(InMax.X) - InMin.X;
	const int64 SizeY = static_cast<int64>(InMax.Y) - InMin.Y;
	const int64 SizeZ = static_cast<int64>(InMax.Z) - InMin.Z;
	if (SizeX <= 0 || SizeY <= 0 || SizeZ <= 0)
	{
		OutError = TEXT("Prefab bounds are empty");
		return false;
	}
	const FIntVector Center(
		static_cast<int32>(static_cast<int64>(InMin.X) + SizeX / 2),
		static_cast<int32>(static_cast<int64>(InMin.Y) + SizeY / 2),
		static_cast<int32>(static_cast<int64>(InMin.Z) + SizeZ / 2));
	FVoxelPrefabSaveData Prefab;
	for (int32 Z = InMin.Z; Z < InMax.Z; ++Z)
	{
		for (int32 Y = InMin.Y; Y < InMax.Y; ++Y)
		{
			for (int32 X = InMin.X; X < InMax.X; ++X)
			{
				const FIntVector Position(X, Y, Z);
				FVoxelBlockState State;
				if (!Runtime->TryGetBlock(Position, State))
				{
					OutError = TEXT("Prefab selection is not fully loaded");
					return false;
				}
				if (State.IsAir())
				{
					continue;
				}
				const FVoxelRuntimeDefinition* Definition = Registry.GetSnapshot()->Find(State.TypeId);
				if (Definition && Definition->EntityKind)
				{
					const FVoxelSection* Section = Runtime->FindSection(VoxelModuleToSection(Position));
					const FVoxelBlockEntityState* Entity = Section ? Section->Entities.Find(VoxelModuleToCellIndex(Position)) : nullptr;
					FVoxelBlockEntityState Default;
					if (!Entity ||
						!FVoxelBlockEntityCodec::MakeDefault(Definition->EntityKind, Default, Definition->EntityVariant) ||
						!(*Entity == Default))
					{
						OutError = TEXT("Prefab export requires default block entity state");
						return false;
					}
				}
				FVoxelPrefabCell Cell;
				Cell.Offset = Position - Center;
				if (!FVoxelItemBridge::ToItem(*Registry.GetSnapshot(), State, 1, Cell.Item))
				{
					return false;
				}
				Prefab.Cells.Add(MoveTemp(Cell));
			}
		}
	}
	OutPrefab = MoveTemp(Prefab);
	OutError.Reset();
	return true;
}
