#include "Voxel/Agent/VoxelAgentComponent.h"

#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"

#include "Components/CapsuleComponent.h"

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
		UVoxelModule::Find(
			GetWorld());

	if (!Module.IsValid())
	{
		Module =
			&UVoxelModule::Get();
	}

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

	ResetInitialCollisionGate();
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

bool UVoxelAgentComponent::MakeIntent(
	const EVoxelEditAction InAction,
	FVoxelEditIntent& OutIntent) const
{
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

	OutIntent.ExpectedTarget =
		Hit.Index;

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

	if (IAbilityInventoryAgentInterface* Agent =
		Cast<IAbilityInventoryAgentInterface>(
			InventoryOwner))
	{
		if (UAbilityInventoryBase* Inventory =
			Agent->GetInventory())
		{
			if (UAbilityInventorySlotBase* Slot =
				Inventory->GetSelectedSlot(
					ESlotSplitType::Shortcut))
			{
				OutIntent.InventorySlot =
					Slot->GetSlotIndex();

				OutIntent.ExpectedItemID =
					Slot->GetItem().ID;
			}
		}
	}

	return true;
}

bool UVoxelAgentComponent::Send(
	FVoxelEditIntent InIntent)
{
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

bool UVoxelAgentComponent::TakeContainerSlot(
	const int32 InContainerSlot,
	const int32 InCount)
{
	FVoxelEditIntent Intent;

	if (InContainerSlot < 0 ||
		InContainerSlot >= 27 ||
		InCount <= 0 ||
		InCount > 100000 ||
		!MakeIntent(
			EVoxelEditAction::
				ContainerTake,
			Intent))
	{
		return false;
	}

	Intent.ContainerSlot =
		InContainerSlot;

	Intent.Count =
		InCount;

	return Send(
		Intent);
}

bool UVoxelAgentComponent::PutContainerSlot(
	const int32 InContainerSlot,
	const int32 InCount)
{
	FVoxelEditIntent Intent;

	if (InContainerSlot < 0 ||
		InContainerSlot >= 27 ||
		InCount <= 0 ||
		InCount > 100000 ||
		!MakeIntent(
			EVoxelEditAction::
				ContainerPut,
			Intent))
	{
		return false;
	}

	Intent.ContainerSlot =
		InContainerSlot;

	Intent.Count =
		InCount;

	return Send(
		Intent);
}

void UVoxelAgentComponent::OnReply(
	const FVoxelEditReply& InReply)
{
	if (InReply.RequestId ==
			LastRequest &&
		InReply.Code !=
			EVoxelEditCode::Pending)
	{
		bBreakActive =
			false;
	}
}

void UVoxelAgentComponent::RefreshSource()
{
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
		 bEnableNonPlayerSource);

	if (GetWorld()->GetNetMode() !=
		NM_Standalone)
	{
		bActive =
			bActive &&
			BoundController.IsValid() &&
			BoundController->
				IsLocalController();
	}

	if (!bActive)
	{
		if (SourceId.IsValid())
		{
			VoxelModule->
				UnregisterSource(
					SourceId);
		}

		SourceId.Invalidate();
		return;
	}

	FVoxelStreamingSource Source;

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
		return;
	}

	Source.Id =
		SourceId.IsValid()
			? SourceId
			: FGuid::NewGuid();

	Source.Direction =
		ViewDirection;

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

	Source.VerticalExactRadius =
		FMath::Max(
			0,
			VerticalExactRadiusCells);

	Source.MovementCriticalCollisionRadius =
		FMath::Clamp(
			MovementCriticalCollisionRadiusCells,
			0,
			Source.CollisionRadius);

	if (BoundController.IsValid())
	{
		Source.VerticalFovDegrees =
			BoundController->
				PlayerCameraManager
					? BoundController->
						PlayerCameraManager->
						GetFOVAngle()
					: 90.0f;

		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;

		BoundController->
			GetViewportSize(
				ViewportWidth,
				ViewportHeight);

		Source.ViewportHeightPixels =
			FMath::Max(
				1,
				ViewportHeight);
	}

	Source.bCollision = true;

	Source.bSimulation =
		SimulationRadiusCells > 0;

	Source.RenderMode =
		GetWorld()->GetNetMode() == NM_DedicatedServer
			? EVoxelStreamingRenderMode::None
			: EVoxelStreamingRenderMode::Full;

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
}

void UVoxelAgentComponent::RefreshStepHeight()
{
	ACharacter* Character =
		Cast<ACharacter>(
			GetOwner());

	UVoxelModule* VoxelModule =
		Module.Get();

	if (!Character ||
		!Character->
			GetCharacterMovement())
	{
		return;
	}

	UCharacterMovementComponent* Movement =
		Character->
			GetCharacterMovement();

	if (bUseSmallVoxelStepHeight &&
		VoxelModule &&
		VoxelModule->IsReady())
	{
		if (PreviousStepHeight <
			0.0f)
		{
			PreviousStepHeight =
				Movement->
					MaxStepHeight;
		}

		Movement->MaxStepHeight =
			FMath::Clamp(
				static_cast<float>(
					VoxelModule->
						BlockSize()) +
					5.0f,
				20.0f,
				40.0f);
	}
	else if (PreviousStepHeight >=
		0.0f)
	{
		Movement->MaxStepHeight =
			PreviousStepHeight;

		PreviousStepHeight =
			-1.0f;
	}
}

void UVoxelAgentComponent::ResetInitialCollisionGate()
{
	bInitialCollisionGatePending =
		bGateInitialSpawnUntilCollision;

	bInitialCollisionGateApplied =
		false;

	bHasLastSafeLocation =
		false;

	LastSafeLocation =
		FVector::ZeroVector;

	InitialPreviousMoveMode = 0;
	InitialPreviousCustomMode = 0;

	ObservedWorldEpoch =
		Module.IsValid() &&
		Module->IsReady()
			? Module->
				GetWorldEpoch()
			: 0;
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

	return
		UVoxelModuleStatics::
			AreCollisionsReady(
				*const_cast<UVoxelModule*>(
					VoxelModule),
				FBox(
					InLocation - Extent,
					InLocation + Extent));
}

void UVoxelAgentComponent::ApplyInitialCollisionGate(
	ACharacter& InCharacter,
	UCharacterMovementComponent& InMovement)
{
	if (bInitialCollisionGateApplied)
	{
		return;
	}

	InitialPreviousMoveMode =
		static_cast<uint8>(
			InMovement.MovementMode);

	InitialPreviousCustomMode =
		InMovement.CustomMovementMode;

	bInitialCollisionGateApplied =
		true;

	InMovement.
		StopMovementImmediately();

	InMovement.
		DisableMovement();
}

void UVoxelAgentComponent::ReleaseInitialCollisionGate(
	ACharacter& InCharacter,
	UCharacterMovementComponent& InMovement)
{
	if (bInitialCollisionGateApplied)
	{
		InMovement.SetMovementMode(
			static_cast<EMovementMode>(
				InitialPreviousMoveMode),
			InitialPreviousCustomMode);
	}

	bInitialCollisionGateApplied =
		false;

	bInitialCollisionGatePending =
		false;

	bHasLastSafeLocation =
		true;

	LastSafeLocation =
		InCharacter.
			GetActorLocation();
}

void UVoxelAgentComponent::RefreshCollisionMovementGuard()
{
	ACharacter* Character =
		Cast<ACharacter>(
			GetOwner());

	UVoxelModule* VoxelModule =
		Module.Get();

	if (!Character ||
		!Character->IsPlayerControlled() ||
		!VoxelModule ||
		!VoxelModule->IsReady())
	{
		return;
	}

	UCharacterMovementComponent* Movement =
		Character->
			GetCharacterMovement();

	if (!Movement)
	{
		return;
	}

	const uint64 CurrentEpoch =
		VoxelModule->
			GetWorldEpoch();

	if (ObservedWorldEpoch !=
		CurrentEpoch)
	{
		ResetInitialCollisionGate();

		ObservedWorldEpoch =
			CurrentEpoch;
	}

	const FVector CurrentLocation =
		Character->
			GetActorLocation();

	const bool bCurrentReady =
		IsCollisionReadyAt(
			*Character,
			CurrentLocation);

	/**
	 * Initial Gate:
	 * 只允许这一段使用 DisableMovement。
	 */
	if (bInitialCollisionGatePending)
	{
		if (!bCurrentReady)
		{
			ApplyInitialCollisionGate(
				*Character,
				*Movement);

			return;
		}

		ReleaseInitialCollisionGate(
			*Character,
			*Movement);

		return;
	}

	/**
	 * Runtime:
	 * 绝不再把 MovementMode 改成 MOVE_None。
	 */
	if (!bCurrentReady)
	{
		if (bHasLastSafeLocation)
		{
			const FVector UnsafeDirection =
				(CurrentLocation -
					LastSafeLocation).
					GetSafeNormal();

			if (!UnsafeDirection.
				IsNearlyZero())
			{
				const double OutwardSpeed =
					FVector::DotProduct(
						Movement->Velocity,
						UnsafeDirection);

				if (OutwardSpeed > 0.0)
				{
					Movement->Velocity -=
						UnsafeDirection *
						OutwardSpeed;
				}
			}

			Character->
				SetActorLocation(
					LastSafeLocation,
					false,
					nullptr,
					ETeleportType::
						TeleportPhysics);
		}

		return;
	}

	bHasLastSafeLocation =
		true;

	LastSafeLocation =
		CurrentLocation;

	/**
	 * PrePhysics 预测一步。
	 * 如果下一小步没有 Collision，就只阻止这一帧向外输入，
	 * 不改变 MovementMode。
	 */
	const FVector PendingInput =
		Character->
			GetPendingMovementInputVector();

	const FVector MoveDirection =
		PendingInput.
			GetSafeNormal();

	if (MoveDirection.IsNearlyZero())
	{
		return;
	}

	const double PredictionDistance =
		FMath::Max(
			VoxelModule->
				BlockSize() *
				0.5,
			static_cast<double>(
				Movement->
					Velocity.Size()) *
				FMath::Max(
					0.01f,
					CollisionPredictionSeconds));

	const FVector PredictedLocation =
		CurrentLocation +
		MoveDirection *
			PredictionDistance;

	if (IsCollisionReadyAt(
		*Character,
		PredictedLocation))
	{
		return;
	}

	const double OutwardSpeed =
		FVector::DotProduct(
			Movement->Velocity,
			MoveDirection);

	if (OutwardSpeed > 0.0)
	{
		Movement->Velocity -=
			MoveDirection *
				OutwardSpeed;
	}

	/**
	 * 只消费这一帧 pending input。
	 * Enhanced Input 持续 Triggered 时下一帧会重新写入，
	 * 因此用户不需要松开再按。
	 */
	Character->
		ConsumeMovementInputVector();
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
	RefreshStepHeight();
	RefreshCollisionMovementGuard();

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

void UVoxelAgentComponent::EndPlay(
	const EEndPlayReason::Type InReason)
{
	EndBreak();

	if (Module.IsValid() &&
		SourceId.IsValid())
	{
		Module->
			UnregisterSource(
				SourceId);
	}

	SourceId.Invalidate();

	if (Network.IsValid())
	{
		Network->
			OnIntentReply.
			Remove(
				ReplyHandle);
	}

	if (ACharacter* Character =
		Cast<ACharacter>(
			GetOwner()))
	{
		if (UCharacterMovementComponent* Movement =
			Character->
				GetCharacterMovement())
		{
			if (PreviousStepHeight >=
				0.0f)
			{
				Movement->
					MaxStepHeight =
						PreviousStepHeight;
			}

			if (bInitialCollisionGateApplied)
			{
				Movement->
					SetMovementMode(
						static_cast<EMovementMode>(
							InitialPreviousMoveMode),
						InitialPreviousCustomMode);
			}
		}
	}

	PreviousStepHeight =
		-1.0f;

	bInitialCollisionGateApplied =
		false;

	Super::EndPlay(
		InReason);
}

void UVoxelAgentComponent::SetAgentEnabled(
	const bool bInEnabled)
{
	if (!bInEnabled)
	{
		EndBreak();

		if (Module.IsValid() &&
			SourceId.IsValid())
		{
			Module->
				UnregisterSource(
					SourceId);
		}

		SourceId.Invalidate();

		BindController(
			nullptr);

		if (bInitialCollisionGateApplied)
		{
			if (ACharacter* Character =
				Cast<ACharacter>(
					GetOwner()))
			{
				if (UCharacterMovementComponent* Movement =
					Character->
						GetCharacterMovement())
				{
					Movement->
						SetMovementMode(
							static_cast<EMovementMode>(
								InitialPreviousMoveMode),
							InitialPreviousCustomMode);
				}
			}
		}

		bInitialCollisionGateApplied =
			false;
	}
	else
	{
		ResetInitialCollisionGate();
	}

	bEnableStreaming =
		bInEnabled;

	SetComponentTickEnabled(
		bInEnabled);

	if (bInEnabled)
	{
		RefreshSource();
	}
}
