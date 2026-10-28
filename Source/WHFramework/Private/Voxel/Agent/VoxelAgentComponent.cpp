#include "Voxel/Agent/VoxelAgentComponent.h"

#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/InventoryNetworkComponent.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Ability/Inventory/InventoryItemRegistry.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"

#include "Components/CapsuleComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Voxels/VoxelItemBridge.h"

UVoxelAgentComponent::UVoxelAgentComponent()
{
	PrimaryComponentTick.bCanEverTick =
		true;

	PrimaryComponentTick.TickGroup =
		TG_PrePhysics;
}

void UVoxelAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	Module =
		UVoxelModule::GetPtr(
			GetWorld());

	if (APawn* Pawn =
		Cast<APawn>(
			GetOwner()))
	{
		BindController(
			Cast<APlayerController>(
				Pawn->GetController()));
	}

	if (ACharacter* Character =
		Cast<ACharacter>(
			GetOwner()))
	{
		if (UCharacterMovementComponent* Movement =
			Character->
				GetCharacterMovement())
		{
			Movement->
				AddTickPrerequisiteComponent(
					this);
		}
	}

	ResetBirthClaim();
}

void UVoxelAgentComponent::BindController(
	APlayerController* InController)
{
	if (InController &&
		InController->GetWorld() !=
			GetWorld())
	{
		return;
	}

	if (Network.IsValid())
	{
		Network->
			OnIntentReply.
			Remove(
				ReplyHandle);
	}

	BoundController =
		InController;

	Network =
		InController
			? InController->
				FindComponentByClass<
					UVoxelModuleNetworkComponent>()
			: nullptr;

	if (Network.IsValid())
	{
		ReplyHandle =
			Network->
				OnIntentReply.
				AddUObject(
					this,
					&UVoxelAgentComponent::
						OnReply);
	}
}

bool UVoxelAgentComponent::View(
	FVector& OutOrigin,
	FVector& OutDirection) const
{
	if (!GetOwner())
	{
		return false;
	}

	FRotator Rotation;

	if (bUseControllerView &&
		BoundController.IsValid() &&
		BoundController->
			IsLocalController())
	{
		BoundController->
			GetPlayerViewPoint(
				OutOrigin,
				Rotation);
	}
	else if (UCameraComponent* Camera =
		GetOwner()->
			FindComponentByClass<
				UCameraComponent>())
	{
		OutOrigin =
			Camera->
				GetComponentLocation();

		Rotation =
			Camera->
				GetComponentRotation();
	}
	else
	{
		GetOwner()->
			GetActorEyesViewPoint(
				OutOrigin,
				Rotation);
	}

	OutDirection =
		Rotation.Vector();

	return
		!OutOrigin.ContainsNaN() &&
		!OutDirection.ContainsNaN();
}

bool UVoxelAgentComponent::TraceVoxel(
	FVoxelHitResult& OutHit) const
{
	OutHit =
		FVoxelHitResult();

	UVoxelModule* VoxelModule =
		Module.Get();

	FVector Origin;
	FVector Direction;

	if (!VoxelModule ||
		!VoxelModule->IsReady() ||
		!View(
			Origin,
			Direction))
	{
		return false;
	}

	const FVoxelTraceResult Hit =
		VoxelModule->Trace(
			Origin,
			Direction);

	OutHit.bNeedsData =
		Hit.Status ==
			EVoxelTraceStatus::NeedsData;

	if (Hit.Status !=
		EVoxelTraceStatus::Hit)
	{
		return false;
	}

	OutHit.bHit = true;
	OutHit.Index = Hit.Index;
	OutHit.PlacementIndex =
		Hit.PlacementIndex;
	OutHit.Point = Hit.Point;
	OutHit.Normal = Hit.Normal;

	return FVoxelItemBridge::ToItem(
		*VoxelModule->
			GetRegistry(),
		Hit.State,
		1,
		OutHit.VoxelItem);
}

FGuid UVoxelAgentComponent::GetInventoryStoreId() const
{
    if(GetOwner() && GetOwner()->HasAuthority())
    {
        const UInventoryWorldSubsystem* System = GetWorld() ? GetWorld()->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
        return System && System->IsReady() ? System->GetStoreId() : FGuid();
    }
    const UInventoryNetworkComponent* InventoryNetwork = BoundController.IsValid() ?
        BoundController->FindComponentByClass<UInventoryNetworkComponent>() : nullptr;
    return InventoryNetwork ? InventoryNetwork->GetStoreId() : FGuid();
}

bool UVoxelAgentComponent::MakeIntent(
	const EVoxelEditAction InAction,
	FVoxelEditIntent& OutIntent) const
{
	OutIntent.InventoryStoreId = GetInventoryStoreId();
	if (!OutIntent.InventoryStoreId.IsValid()) return false;
	UVoxelModule* VoxelModule =
		Module.Get();

	if (!VoxelModule ||
		!VoxelModule->IsReady() ||
		!View(
			OutIntent.Origin,
			OutIntent.Direction))
	{
		return false;
	}

	const FVoxelTraceResult Hit =
		VoxelModule->Trace(
			OutIntent.Origin,
			OutIntent.Direction);

	if (Hit.Status !=
		EVoxelTraceStatus::Hit)
	{
		return false;
	}

	OutIntent.Action =
		InAction;
	OutIntent.PlacementYaw = InAction == EVoxelEditAction::Place ? PlacementYaw : 0;

	OutIntent.ExpectedTarget =
		Hit.Index;
	const FVoxelRuntimeDefinition* HitDefinition = VoxelModule->GetRegistry()->Find(Hit.State.TypeId);
	OutIntent.ExpectedPlacementAnchor = HitDefinition && HitDefinition->bReplaceable ? Hit.Index : Hit.PlacementIndex;

	const FVoxelSectionKey SectionKey =
		VoxelCoord::Section(
			Hit.Index);

	const FVoxelSection* Section =
		VoxelModule->
			GetRuntime()->
			FindSection({
				SectionKey.X,
				SectionKey.Y,
				SectionKey.Z
			});

	if (!Section)
	{
		return false;
	}

	OutIntent.ExpectedRevision =
		Section->
			CommittedRevision;

	AActor* InventoryOwner =
		BoundController.IsValid() &&
		BoundController->GetPawn()
			? BoundController->GetPawn()
			: GetOwner();


    if(IAbilityInventoryAgentInterface* InventoryAgent = Cast<IAbilityInventoryAgentInterface>(InventoryOwner))
    {
        OutIntent.InventorySlot = InventoryAgent->GetSelectedInventorySlot();
        const FInventorySnapshot* View = nullptr;
        FInventorySnapshot ServerSnapshot;
        if(InventoryOwner->HasAuthority())
        {
            if(const UAbilityInventoryBase* Inventory = InventoryAgent->GetInventory())
            {
                ServerSnapshot = Inventory->MakeSnapshot();
                View = &ServerSnapshot;
            }
        }
        else if(BoundController.IsValid())
        {
            if(const UInventoryNetworkComponent* InventoryNetwork = BoundController->FindComponentByClass<UInventoryNetworkComponent>())
            {
                View = InventoryNetwork->FindView(InventoryNetwork->GetOwnerInventoryId());
            }
        }
        if(View)
        {
            OutIntent.ExpectedInventoryId = View->InventoryId;
            OutIntent.ExpectedInventoryRevision = View->Revision;
            OutIntent.ExpectedInventoryGeneration = View->Generation;
            if(const FInventorySlotState* Slot = View->FindSlot(OutIntent.InventorySlot))
            {
                OutIntent.ExpectedItemID = Slot->Item.ItemId;
                const UInventoryWorldSubsystem* System = GetWorld()->GetSubsystem<UInventoryWorldSubsystem>();
                const FInventoryItemDefinition* Item = System ? System->GetRegistry().Find(Slot->Item.ItemId) : nullptr;
                const FVoxelRuntimeDefinition* Placed = Item ? VoxelModule->GetRegistry()->Find(Item->PlaceableVoxelId) : nullptr;
                if (HitDefinition && Placed && HitDefinition->Shape == EVoxelShapeKind::Slab && Placed->Shape == EVoxelShapeKind::Slab &&
                    Hit.State.TypeId == Placed->TypeId && !(Hit.State.State & 0x800) &&
                    ((!(Hit.State.State & 8) && Hit.Normal.Z > .5) || ((Hit.State.State & 8) && Hit.Normal.Z < -.5)))
                {
                    OutIntent.ExpectedPlacementAnchor = Hit.Index;
                }
            }
        }
    }

	return true;
}

bool UVoxelAgentComponent::Send(
	FVoxelEditIntent InIntent)
{
	// BreakCancel 没有射线捕获步骤；其他请求保留 MakeIntent 时捕获的分支身份。
	if (InIntent.Action == EVoxelEditAction::BreakCancel && !InIntent.InventoryStoreId.IsValid())
		InIntent.InventoryStoreId = GetInventoryStoreId();
	if (!InIntent.InventoryStoreId.IsValid()) return false;
	UVoxelModule* VoxelModule =
		Module.Get();

	if (!VoxelModule ||
		!VoxelModule->IsReady())
	{
		return false;
	}

	if (GetWorld()->GetNetMode() ==
		NM_Standalone)
	{
		InIntent.RequestId =
			NextLocalID ==
				MAX_uint64
				? 0
				: NextLocalID++;

		if (!InIntent.RequestId)
		{
			return false;
		}

		LastRequest =
			InIntent.RequestId;
		if (InIntent.Action == EVoxelEditAction::Place) LastPlacementRequest = InIntent.RequestId;

		const FVoxelEditReply Reply =
			VoxelModule->
				ExecuteIntent(
					BoundController.Get(),
					GetOwner(),
					InIntent,
					bCreativeInStandalone);

		OnReply(
			Reply);

		return
			Reply.Code !=
			EVoxelEditCode::Rejected;
	}

	if (!Network.IsValid() ||
		!Network->IsSessionReady())
	{
		return false;
	}

	InIntent.RequestId =
		Network->
			AllocateRequestId();

	if (!InIntent.RequestId)
	{
		return false;
	}

	LastRequest =
		InIntent.RequestId;
	if (InIntent.Action == EVoxelEditAction::Place) LastPlacementRequest = InIntent.RequestId;

	return Network->
		SubmitIntent(
			InIntent);
}

void UVoxelAgentComponent::BeginBreak()
{
	bHeld = true;
	LastPulse = -10.0;
}

void UVoxelAgentComponent::EndBreak()
{
	bHeld = false;

	if (bBreakActive)
	{
		FVoxelEditIntent Intent;

		Intent.Action =
			EVoxelEditAction::
				BreakCancel;

		Intent.ExpectedTarget =
			BreakTarget;

		View(
			Intent.Origin,
			Intent.Direction);

		Send(
			Intent);
	}

	bBreakActive = false;
}

bool UVoxelAgentComponent::PlaceSelected()
{
	FVoxelEditIntent Intent;

	return
		MakeIntent(
			EVoxelEditAction::Place,
			Intent) &&
		Send(
			Intent);
}

bool UVoxelAgentComponent::CanRotateSelected() const
{
	const IAbilityInventoryAgentInterface* InventoryAgent = Cast<IAbilityInventoryAgentInterface>(GetOwner());
	const UVoxelModule* VoxelModule = Module.Get();
	if (!InventoryAgent || !VoxelModule || !VoxelModule->IsReady())
	{
		return false;
	}
	FInventorySnapshot Snapshot;
	if (GetOwner()->HasAuthority())
	{
		const UAbilityInventoryBase* Inventory = InventoryAgent->GetInventory();
		if (!Inventory)
		{
			return false;
		}
		Snapshot = Inventory->MakeSnapshot();
	}
	else
	{
		const UInventoryNetworkComponent* InventoryNetwork = BoundController.IsValid() ? BoundController->FindComponentByClass<UInventoryNetworkComponent>() : nullptr;
		const FInventorySnapshot* View = InventoryNetwork ? InventoryNetwork->FindView(InventoryNetwork->GetOwnerInventoryId()) : nullptr;
		if (!View)
		{
			return false;
		}
		Snapshot = *View;
	}
	const FInventorySlotState* Slot = Snapshot.FindSlot(InventoryAgent->GetSelectedInventorySlot());
	const UInventoryWorldSubsystem* System = GetWorld()->GetSubsystem<UInventoryWorldSubsystem>();
	const FInventoryItemDefinition* Item = Slot && !Slot->bLocked && !Slot->Item.IsEmpty() && System ? System->GetRegistry().Find(Slot->Item.ItemId) : nullptr;
	const FVoxelRuntimeDefinition* Definition = Item && VoxelModule->GetRegistry() ? VoxelModule->GetRegistry()->Find(Item->PlaceableVoxelId) : nullptr;
	return Definition && (Definition->Shape == EVoxelShapeKind::FullCube || Definition->Shape == EVoxelShapeKind::Stair ||
		Definition->Shape == EVoxelShapeKind::Door || Definition->Shape == EVoxelShapeKind::Trapdoor || Definition->Shape == EVoxelShapeKind::ObjectPart);
}

bool UVoxelAgentComponent::CapturePlacement(FVoxelEditIntent& OutIntent) const
{
	OutIntent = {};
	return MakeIntent(EVoxelEditAction::Place, OutIntent) && OutIntent.ExpectedItemID.IsValid();
}

bool UVoxelAgentComponent::SubmitPlacement(const FVoxelEditIntent& InIntent)
{
	return InIntent.Action == EVoxelEditAction::Place && Send(InIntent);
}

bool UVoxelAgentComponent::RotateSelected(int32 InQuarterTurns)
{
	if (!CanRotateSelected() || InQuarterTurns == 0)
	{
		return false;
	}
	PlacementYaw = uint8((int32(PlacementYaw) + (InQuarterTurns > 0 ? 1 : 3)) & 3);
	OnPlacementChanged.Broadcast();
	return true;
}

void UVoxelAgentComponent::ResetPlacementRotation()
{
	FVector Origin;
	FVector Direction;
	const uint8 Yaw = View(Origin, Direction) ? uint8((FMath::RoundToInt(FMath::Atan2(Direction.Y, Direction.X) / (PI * .5)) + 4) & 3) : 0;
	if (PlacementYaw != Yaw)
	{
		PlacementYaw = Yaw;
		OnPlacementChanged.Broadcast();
	}
}

bool UVoxelAgentComponent::PlaceItem(
	const FVoxelItem& InItem)
{
	FVoxelEditIntent Intent;

	if (!InItem.IsValid() ||
		!MakeIntent(
			EVoxelEditAction::Place,
			Intent))
	{
		return false;
	}

	Intent.ExpectedItemID =
		InItem.VoxelAssetID;

	return Send(
		Intent);
}

bool UVoxelAgentComponent::UseTarget()
{
	FVoxelEditIntent Intent;

	return
		MakeIntent(
			EVoxelEditAction::Use,
			Intent) &&
		Send(
			Intent);
}





void UVoxelAgentComponent::OnReply(
	const FVoxelEditReply& InReply)
{
	if (InReply.RequestId == LastPlacementRequest)
	{
		OnPlacementReply.Broadcast(InReply);
	}
	if (InReply.RequestId ==
			LastRequest &&
		InReply.Code !=
			EVoxelEditCode::Pending)
	{
		bBreakActive =
			false;
	}
}

bool UVoxelAgentComponent::BuildStreamingSource(const bool bInLocalView, FVoxelStreamingSource& OutSource) const
{
	UVoxelModule* VoxelModule = Module.Get();
	if (!VoxelModule || !GetOwner()) return false;
	FVoxelStreamingSource& Source = OutSource;
	Source = {};
	// 出生凭证只由出生事务拥有；此组件不持有移动模式。
	Source.Purpose = BirthClaim.IsValid() && !IsBirthSourceCommitted()
		? EVoxelStreamingSourcePurpose::InitialSpawn : EVoxelStreamingSourcePurpose::Observer;
	Source.bInheritWorldView = bInheritWorldView;
	Source.View = StreamingView;
	Source.bLocalView = bInLocalView;
	Source.Priority = StreamingPriority;
	const ENetMode NetMode = GetWorld()->GetNetMode();
	if (NetMode == NM_DedicatedServer)
	{
		Source.Capabilities = EVoxelStreamingCapability::Data |
			EVoxelStreamingCapability::Collision |
			EVoxelStreamingCapability::Readiness |
			EVoxelStreamingCapability::RetainGenerationCache;
	}
	else if (Source.bLocalView)
	{
		Source.Capabilities = EVoxelStreamingCapability::Data |
			EVoxelStreamingCapability::Collision |
			EVoxelStreamingCapability::FineVisual |
			EVoxelStreamingCapability::Readiness |
			EVoxelStreamingCapability::RetainGenerationCache;
		if (bProvideWorldView)
		{
			Source.Capabilities |= EVoxelStreamingCapability::WorldVisual;
		}
	}
	else if (GetOwner()->HasAuthority())
	{
		Source.Capabilities = EVoxelStreamingCapability::Data |
			EVoxelStreamingCapability::Collision |
			EVoxelStreamingCapability::RetainGenerationCache;
	}
	else
	{
		return false;
	}
	if (SimulationRadiusCells > 0 && GetOwner()->HasAuthority())
	{
		Source.Capabilities |= EVoxelStreamingCapability::Simulation;
	}
	if (Source.Purpose == EVoxelStreamingSourcePurpose::InitialSpawn || BoundController.IsValid())
		Source.Capabilities |= EVoxelStreamingCapability::Readiness;
	Source.Normalize();

	FVector ViewOrigin;
	FVector ViewDirection;

	if (!View(
			ViewOrigin,
			ViewDirection) ||
		!VoxelCoord::FromWorld(
			GetOwner()->
				GetActorLocation(),
			VoxelModule->BlockSize(),
			Source.Center))
	{
		return false;
	}

	Source.Id =
		SourceId.IsValid()
			? SourceId
			: FGuid::NewGuid();

	Source.Direction =
		ViewDirection;
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Source.VelocityCellsPerSecond = Character->GetVelocity() /
			FMath::Max(VoxelModule->BlockSize(), UE_DOUBLE_SMALL_NUMBER);
	}

	Source.ExactRadius =
		FMath::Max(
			0,
			ExactRadiusCells);

	Source.CollisionRadius =
		FMath::Max(
			0,
			CollisionRadiusCells);

	Source.SimulationRadius =
		FMath::Max(
			0,
			SimulationRadiusCells);

	Source.MovementCriticalCollisionRadius =
		FMath::Clamp(
			MovementCriticalCollisionRadiusCells,
			0,
			Source.CollisionRadius);

	if (BoundController.IsValid())
	{
		const ULocalPlayer* LocalPlayer = BoundController->GetLocalPlayer();
		FSceneViewProjectionData Projection;
		if (LocalPlayer && LocalPlayer->ViewportClient &&
			LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection) &&
			Projection.IsPerspectiveProjection() && Projection.IsValidViewRectangle())
		{
			const double VerticalScale = FMath::Abs(Projection.ProjectionMatrix.M[1][1]);
			Source.VerticalFovDegrees = FMath::RadiansToDegrees(2.0 * FMath::Atan(1.0 / VerticalScale));
			Source.ViewportHeightPixels = Projection.GetConstrainedViewRect().Height();
		}
	}

	return true;
}

void UVoxelAgentComponent::RefreshSource()
{
	if (!Module.IsValid())
	{
		Module = UVoxelModule::GetPtr(GetWorld());
	}
	if (Module.IsValid() && Module->IsReady() && ObservedWorldEpoch != Module->GetWorldEpoch())
	{
		ResetBirthClaim();
	}
	UVoxelModule* VoxelModule =
		Module.Get();

	if (!VoxelModule)
	{
		return;
	}

	if (APawn* Pawn =
		Cast<APawn>(
			GetOwner()))
	{
		APlayerController* PlayerController =
			Cast<APlayerController>(
				Pawn->GetController());
		if (!PlayerController && BirthClaim.IsValid()) PlayerController = BoundController.Get();

		if (BoundController.Get() !=
			PlayerController)
		{
			BindController(
				PlayerController);
		}
	}

	APawn* Pawn =
		Cast<APawn>(
			GetOwner());

	bool bActive =
		bEnableStreaming &&
		(!Pawn ||
		 Pawn->IsPlayerControlled() ||
		 BirthClaim.IsValid() ||
		 bEnableNonPlayerSource);

	if (GetWorld()->GetNetMode() !=
		NM_Standalone)
	{
		bActive =
			bActive &&
			(GetOwner()->HasAuthority() ||
				(BoundController.IsValid() && BoundController->IsLocalController()));
	}

	if (!bActive)
	{
		if (SourceId.IsValid() && (VoxelModule->IsSourceOwnedBy(SourceId, this) ||
			VoxelModule->IsSourceOwnedBy(SourceId, GetOwner())))
		{
			VoxelModule->
				UnregisterSource(
					SourceId);
		}

		SourceId.Invalidate();
		return;
	}

	if (BirthClaim.IsValid() && !IsBirthSourceCommitted())
	{
		TryCommitBirthSource();
		return;
	}

	FVoxelStreamingSource Source;
	if (!BuildStreamingSource(BoundController.IsValid() && BoundController->IsLocalController(), Source)) return;

	if (!SourceId.IsValid())
	{
		SourceId =
			VoxelModule->
				RegisterSource(
					this,
					Source);
	}
	else if (!VoxelModule->
		UpdateSource(
			SourceId,
			Source))
	{
		SourceId.Invalidate();
	}
	if (BirthClaim.IsValid() && !IsBirthSourceCommitted()) TryCommitBirthSource();
}

bool UVoxelAgentComponent::PrepareBirthSource(const FGuid& InSource)
{
	UVoxelModule* VoxelModule = Module.Get();
	FIntVector Cell;
	if (!VoxelModule || !VoxelModule->IsReady() || !GetOwner() ||
		!VoxelCoord::FromWorld(GetOwner()->GetActorLocation(), VoxelModule->BlockSize(), Cell)) return false;
	if (BirthClaim.IsValid() && BirthClaim.SourceId != InSource)
	{
		VoxelModule->AbortPlayerClaim(BirthClaim);
		BirthClaim = {};
	}
	FVoxelPlayerClaimToken Prepared;
	if (!VoxelModule->PreparePlayerClaim(InSource, GetOwner(), BirthPawnIncarnation, Cell, Prepared)) return false;
	const FGuid Previous = SourceId;
	BirthClaim = Prepared;
	SourceId = InSource;
	ObservedWorldEpoch = VoxelModule->GetWorldEpoch();
	// Bootstrap 仍持有同一条记录；直到 Commit 成功都不能撤销它。
	if (Previous.IsValid() && Previous != SourceId &&
		(VoxelModule->IsSourceOwnedBy(Previous, this) || VoxelModule->IsSourceOwnedBy(Previous, GetOwner())))
		VoxelModule->UnregisterSource(Previous);
	bEnableStreaming = true;
	SetComponentTickEnabled(true);
	return true;
}

bool UVoxelAgentComponent::TryCommitBirthSource()
{
	if (IsBirthSourceCommitted()) return true;
	if (!bBirthCandidateCommitted) return false;
	if (!SourceId.IsValid() || !PrepareBirthSource(SourceId) || !Module.IsValid()) return false;
	return Module->CommitPlayerClaim(BirthClaim, GetOwner());
}

bool UVoxelAgentComponent::IsBirthSourceCommitted() const
{
	return Module.IsValid() && Module->IsPlayerClaimCommitted(BirthClaim, GetOwner());
}



void UVoxelAgentComponent::NotifyTravelTeleported()
{
	RefreshSource();
	if (Module.IsValid())
	{
		Module->ForceVoxelStreamingRefresh();
	}
}

bool UVoxelAgentComponent::IsTravelObserverReady() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character && Module.IsValid() && Module->IsSourceAdmitted(SourceId) &&
		IsCollisionReadyAt(*Character, Character->GetActorLocation());
}

void UVoxelAgentComponent::ResetBirthClaim()
{
	if (Module.IsValid())
	{
		Module->AbortPlayerClaim(BirthClaim);
	}
	BirthClaim = {};
	BirthPawnIncarnation = FGuid::NewGuid();
	ObservedWorldEpoch = Module.IsValid() && Module->IsReady() ? Module->GetWorldEpoch() : 0;
}

bool UVoxelAgentComponent::IsCollisionReadyAt(
	const ACharacter& InCharacter,
	const FVector& InLocation) const
{
	const UVoxelModule* VoxelModule =
		Module.Get();

	const UCapsuleComponent* Capsule =
		InCharacter.
			GetCapsuleComponent();

	if (!VoxelModule ||
		!VoxelModule->IsReady() ||
		!Capsule)
	{
		return false;
	}

	const double Radius =
		Capsule->
			GetScaledCapsuleRadius();

	const double HalfHeight =
		Capsule->
			GetScaledCapsuleHalfHeight();

	const FVector Extent(
		Radius + 1.0,
		Radius + 1.0,
		HalfHeight + 1.0);

	return VoxelModule->IsTerrainSafeAt(SourceId,
		FBox(InLocation - Extent, InLocation + Extent));
}







void UVoxelAgentComponent::TickComponent(
	const float InDeltaSeconds,
	const ELevelTick InTickType,
	FActorComponentTickFunction* InTickFunction)
{
	Super::TickComponent(
		InDeltaSeconds,
		InTickType,
		InTickFunction);

	RefreshSource();

	const double Now =
		FPlatformTime::Seconds();

	if (bHeld &&
		Now - LastPulse >=
			0.1)
	{
		LastPulse =
			Now;

		FVoxelEditIntent Intent;

		if (!MakeIntent(
			EVoxelEditAction::BreakPulse,
			Intent))
		{
			if (bBreakActive)
			{
				EndBreak();
			}

			return;
		}

		if (!bBreakActive ||
			BreakTarget !=
				Intent.ExpectedTarget)
		{
			Intent.Action =
				EVoxelEditAction::
					BreakBegin;

			BreakTarget =
				Intent.ExpectedTarget;

			bBreakActive =
				true;
		}

		if (!Send(
			Intent))
		{
			bBreakActive =
				false;
		}
	}
}

void UVoxelAgentComponent::EndPlay(const EEndPlayReason::Type InReason)
{
	EndBreak();
	if (Module.IsValid())
	{
		Module->AbortPlayerClaim(BirthClaim);
		if (SourceId.IsValid() && (Module->IsSourceOwnedBy(SourceId, this) || Module->IsSourceOwnedBy(SourceId, GetOwner())))
		{
			Module->UnregisterSource(SourceId);
		}
	}
	SourceId.Invalidate();
	BindController(nullptr);
	Super::EndPlay(InReason);
}

void UVoxelAgentComponent::SetAgentEnabled(const bool bInEnabled)
{
	if (bInEnabled == bEnableStreaming)
	{
		if (bInEnabled)
		{
			RefreshSource();
		}
		return;
	}
	if (!bInEnabled)
	{
		EndBreak();
		if (Module.IsValid())
		{
			Module->AbortPlayerClaim(BirthClaim);
			if (SourceId.IsValid() && (Module->IsSourceOwnedBy(SourceId, this) || Module->IsSourceOwnedBy(SourceId, GetOwner())))
			{
				Module->UnregisterSource(SourceId);
			}
		}
		BirthClaim = {};
		SourceId.Invalidate();
		BindController(nullptr);
	}
	else
	{
		ResetBirthClaim();
	}
	bEnableStreaming = bInEnabled;
	SetComponentTickEnabled(bInEnabled);
	if (bInEnabled)
	{
		RefreshSource();
	}
}

bool UVoxelAgentComponent::IsStreamingLocationAdmitted(const FIntVector& InCell, const uint64 InEpoch) const
{
	return Module.IsValid() && Module->IsSourceAdmittedAt(SourceId, InCell, InEpoch);
}
