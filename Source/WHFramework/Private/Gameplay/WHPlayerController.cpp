// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHPlayerController.h"

#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Mode/FollowCameraMode.h"
#include "Character/CharacterModule.h"
#include "Character/Base/CharacterBase.h"
#include "Camera/CameraModuleStatics.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Common/CommonModuleStatics.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "Debug/DebugModuleTypes.h"
#include "Gameplay/WHPlayerState.h"
#include "Input/InputModuleStatics.h"
#include "Input/InputManager.h"
#include "Input/Components/InputComponentBase.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/Base/ModuleNetworkComponentBase.h"
#include "Pawn/PawnModule.h"
#include "Voxel/Network/VoxelModuleNetworkComponent.h"

AWHPlayerController::AWHPlayerController()
{
	PlayerCameraManagerClass = ACameraManagerBase::StaticClass();

	WidgetInteractionComp = CreateDefaultSubobject<UWidgetInteractionComponent>(FName("WidgetInteractionComp"));
	WidgetInteractionComp->SetupAttachment(RootComponent);
	VoxelNetworkComponent = CreateDefaultSubobject<UVoxelModuleNetworkComponent>(TEXT("VoxelNetworkComponent"));

	InteractionRaycastMode = EInteractionRaycastMode::None;
	InteractionDistance = 100000.f;

	bInitialized = false;
}

bool AWHPlayerController::ShouldSuppressGameplayInput() const
{
	const FInputManager* Manager = FInputManager::GetPtr();
	const EInputMode Mode = Manager ? Manager->GetGlobalInputMode(const_cast<AWHPlayerController*>(this)) : EInputMode::GameOnly;
	return IsLocalController() && (Mode == EInputMode::None || Mode == EInputMode::UIOnly);
}

bool AWHPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	return ShouldSuppressGameplayInput() || Super::InputKey(Params);
}

bool AWHPlayerController::InputTouch(FTouchId TouchId, ETouchType::Type Type, const FVector2D& TouchLocation, float Force, uint64 Timestamp)
{
	return ShouldSuppressGameplayInput() || Super::InputTouch(TouchId, Type, TouchLocation, Force, Timestamp);
}

bool AWHPlayerController::InputMotion(FInputDeviceId DeviceId, const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration, uint64 Timestamp)
{
	return ShouldSuppressGameplayInput() || Super::InputMotion(DeviceId, Tilt, RotationRate, Gravity, Acceleration, Timestamp);
}

bool AWHPlayerController::InputGesture(FInputDeviceId DeviceId, EGestureEvent GestureType, EGesturePhase GesturePhase, const FVector2D& GestureDelta, bool bIsDirectionInvertedFromDevice, uint64 Timestamp)
{
	return ShouldSuppressGameplayInput() || Super::InputGesture(DeviceId, GestureType, GesturePhase, GestureDelta, bIsDirectionInvertedFromDevice, Timestamp);
}

void AWHPlayerController::OnInitialize_Implementation()
{
	bInitialized = true;
}

void AWHPlayerController::OnPreparatory_Implementation()
{
	AMainModule* MainModule = AMainModule::GetPtr(GetWorld());
	if (!MainModule) return;
	for (const auto Module : MainModule->GetModules())
	{
		if (!Module || !Module->ModuleNetworkComponent)
		{
			continue;
		}
		if (Module->ModuleNetworkComponent->IsChildOf(UVoxelModuleNetworkComponent::StaticClass()))
		{
			continue;
		}
		if (GetComponentByClass(Module->ModuleNetworkComponent))
		{
			continue;
		}
		UModuleNetworkComponentBase* Component = NewObject<UModuleNetworkComponentBase>(
		    this, Module->ModuleNetworkComponent, *FString::Printf(TEXT("%sNetworkComp"), *Module->ModuleName.ToString()));
		Component->RegisterComponent();
	}
	if (HasAuthority() && !GetPawn())
	{
		if (UPawnModule* Pawns = UPawnModule::GetPtr(GetWorld())) Pawns->SwitchToDefaultPawn(this);
	}
}

void AWHPlayerController::OnRefresh_Implementation(float DeltaSeconds)
{
	RefreshInteraction();
}

void AWHPlayerController::OnTermination_Implementation()
{
}

void AWHPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (Execute_IsUseDefaultLifecycle(this))
	{
		if (!Execute_IsInitialized(this))
		{
			Execute_OnInitialize(this);
		}
		Execute_OnPreparatory(this);
	}
}

void AWHPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this);
	}
}

void AWHPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetPlayerPawn(InPawn);

	if (IsLocalController())
	{
		if (UCameraModuleStatics::GetCameraManager(this)) SetControlRotation(UCameraModuleStatics::GetViewRotation(this));
	}
}

void AWHPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AWHPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	ensureEditorMsgf(InputComponent->IsA<UInputComponentBase>(),
	                 FString::Printf(TEXT("Invalid InputComponent in DefaultInput.ini, must be InputComponentBase!")),
	                 EDC_Input,
	                 EDV_Error);
}

void AWHPlayerController::InitPlayerState()
{
	Super::InitPlayerState();

	if (AWHPlayerState* WHPlayerState = Cast<AWHPlayerState>(PlayerState))
	{
		WHPlayerState->Execute_OnInitialize(WHPlayerState);
	}
}

void AWHPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void AWHPlayerController::RefreshInteraction_Implementation()
{
	if (!IsLocalController()) return;

	FHitResult HitResult;

	switch (InteractionRaycastMode)
	{
		case EInteractionRaycastMode::FromAimPosition:
		{
			RaycastSingleFromAimPosition(InteractionDistance, ECC_Visibility, {}, HitResult);
			break;
		}
		case EInteractionRaycastMode::FromMousePosition:
		{
			RaycastSingleFromMousePosition(InteractionDistance, ECC_Visibility, {}, HitResult);
			break;
		}
		default:
			break;
	}

	if (HitResult.bBlockingHit && HitResult.GetActor())
	{
		AActor* HitActor = HitResult.GetActor();
		if (HoveringInteraction.GetObject())
		{
			if (HitActor->Implements<UInteractionAgentInterface>())
			{
				if (HitActor == HoveringInteraction.GetObject())
				{
					IInteractionAgentInterface::Execute_OnHovering(HoveringInteraction.GetObject(), this);
				}
				else
				{
					IInteractionAgentInterface::Execute_OnEndHover(HoveringInteraction.GetObject(), this);
					HoveringInteraction = HitActor;
					IInteractionAgentInterface::Execute_OnBeginHover(HoveringInteraction.GetObject(), this);
					IInteractionAgentInterface::Execute_OnHovering(HoveringInteraction.GetObject(), this);
				}
			}
			else
			{
				IInteractionAgentInterface::Execute_OnEndHover(HoveringInteraction.GetObject(), this);
				HoveringInteraction = nullptr;
			}
		}
		else if (HitActor->Implements<UInteractionAgentInterface>())
		{
			HoveringInteraction = HitActor;
			IInteractionAgentInterface::Execute_OnBeginHover(HoveringInteraction.GetObject(), this);
			IInteractionAgentInterface::Execute_OnHovering(HoveringInteraction.GetObject(), this);
		}

		if (InteractionRaycastMode == EInteractionRaycastMode::FromMousePosition && UCommonModuleStatics::HasMouseCapture(this) && HoveringInteraction.GetObject())
		{
			IInteractionAgentInterface::Execute_OnEndHover(HoveringInteraction.GetObject(), this);
			HoveringInteraction = nullptr;
		}
	}
	else if (HoveringInteraction.GetObject())
	{
		IInteractionAgentInterface::Execute_OnEndHover(HoveringInteraction.GetObject(), this);
		HoveringInteraction = nullptr;
	}
}

void AWHPlayerController::SetSelectedInteraction(TScriptInterface<IInteractionAgentInterface> InInteractionAgent)
{
	if (InInteractionAgent.GetObject())
	{
		if (!SelectedInteraction.GetObject())
		{
			SelectedInteraction = InInteractionAgent;
			IInteractionAgentInterface::Execute_OnSelected(SelectedInteraction.GetObject(), this);
		}
		else if (SelectedInteraction != InInteractionAgent)
		{
			IInteractionAgentInterface::Execute_OnDeselected(SelectedInteraction.GetObject(), this);
			SelectedInteraction = InInteractionAgent;
			IInteractionAgentInterface::Execute_OnSelected(SelectedInteraction.GetObject(), this);
		}
	}
	else if (SelectedInteraction.GetObject())
	{
		IInteractionAgentInterface::Execute_OnDeselected(SelectedInteraction.GetObject(), this);
		SelectedInteraction = nullptr;
	}
}

bool AWHPlayerController::RaycastSingleFromScreenPosition(
    FVector2D InScreenPosition, float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	const FInputManager* Input = FInputManager::GetPtr();
	const EInputMode InputMode = Input ? Input->GetGlobalInputMode(this) : EInputMode::None;
	if (!IsLocalController() || !PlayerCameraManager || InputMode == EInputMode::None || InputMode == EInputMode::UIOnly) return false;

	FVector SightPos, RayDirection;
	if (DeprojectScreenPositionToWorld(InScreenPosition.X, InScreenPosition.Y, SightPos, RayDirection))
	{
		const FVector RayStart = PlayerCameraManager->GetCameraLocation();
		const ACameraManagerBase* CameraManager = Cast<ACameraManagerBase>(PlayerCameraManager);
		const FVector RayEnd = RayStart + RayDirection * (InRayDistance + (CameraManager ? CameraManager->GetCurrentRigDistance() : 0.f));
		TArray<AActor*> IgnoreActors = InIgnoreActors;
		IgnoreActors.AddUnique(GetPawn());
		IgnoreActors.AddUnique(GetPlayerPawn());
		return UKismetSystemLibrary::LineTraceSingle(
		    this, RayStart, RayEnd, UEngineTypes::ConvertToTraceType(InGameTraceChannel), false, IgnoreActors, EDrawDebugTrace::None, OutHitResult, true);
	}
	return false;
}

bool AWHPlayerController::RaycastSingleFromViewportPosition(
    FVector2D InViewportPosition, float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	FSceneViewProjectionData Projection;
	if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport ||
	    !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection)) return false;
	const FIntRect Rect = Projection.GetConstrainedViewRect();
	const FVector2D Position(Rect.Min.X + Rect.Width() * InViewportPosition.X, Rect.Min.Y + Rect.Height() * InViewportPosition.Y);
	return RaycastSingleFromScreenPosition(Position, InRayDistance, InGameTraceChannel, InIgnoreActors, OutHitResult);
}

bool AWHPlayerController::RaycastSingleFromMousePosition(float InRayDistance,
                                                         ECollisionChannel InGameTraceChannel,
                                                         const TArray<AActor*>& InIgnoreActors,
                                                         FHitResult& OutHitResult)
{
	if (!bShowMouseCursor)
		return false;

	float MousePosX, MousePosY;
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	FSceneViewProjectionData Projection;
	if (!GetMousePosition(MousePosX, MousePosY) || !LocalPlayer || !LocalPlayer->ViewportClient ||
	    !LocalPlayer->ViewportClient->Viewport || !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection)) return false;
	const FIntRect Rect = Projection.GetConstrainedViewRect();
	if (MousePosX < Rect.Min.X || MousePosX >= Rect.Max.X || MousePosY < Rect.Min.Y || MousePosY >= Rect.Max.Y) return false;
	return RaycastSingleFromScreenPosition(FVector2D(MousePosX, MousePosY), InRayDistance, InGameTraceChannel, InIgnoreActors, OutHitResult);
}

bool AWHPlayerController::RaycastSingleFromAimPosition(float InRayDistance,
                                                       ECollisionChannel InGameTraceChannel,
                                                       const TArray<AActor*>& InIgnoreActors,
                                                       FHitResult& OutHitResult)
{
	return RaycastSingleFromViewportPosition(GetAnimPosition(), InRayDistance, InGameTraceChannel, InIgnoreActors, OutHitResult);
}

void AWHPlayerController::SetPlayerPawn(APawn* InPlayerPawn)
{
	if (InPlayerPawn && !InPlayerPawn->Implements<UWHPlayerInterface>())
		return;

	if (PlayerPawn != InPlayerPawn)
	{
		if (PlayerPawn)
		{
			PlayerPawn->OnDestroyed.RemoveAll(this);
		}
		PlayerPawn = InPlayerPawn;
		if (PlayerPawn)
		{
			PlayerPawn->OnDestroyed.AddDynamic(this, &AWHPlayerController::OnPlayerDestroyed);
		}
		OnPlayerPawnChanged.Broadcast(PlayerPawn);
	}
}

void AWHPlayerController::OnPlayerDestroyed(AActor* InPlayerActor)
{
	SetPlayerPawn(nullptr);
}


bool AWHPlayerController::CanRequestCharacterSwitch(const ACharacterBase* InCharacter) const
{
    if (!InCharacter) return true;
    return InCharacter->GetWorld() == GetWorld() &&
        (GetPawn() == InCharacter || (GetPlayerPawn() == InCharacter && InCharacter->IsOwnedBy(this)));
}

void AWHPlayerController::ServerRequestCharacterSwitch_Implementation(ACharacterBase* InCharacter, bool bResetCamera, bool bInstant)
{
    if (!HasAuthority() || !CanRequestCharacterSwitch(InCharacter)) return;
    if (UCharacterModule* Characters = UCharacterModule::GetPtr(GetWorld()))
        Characters->SwitchCharacter(this, InCharacter, bResetCamera, bInstant);
}

void AWHPlayerController::ClientApplyCharacterView_Implementation(ACharacterBase* InCharacter, bool bInstant)
{
    if (!IsLocalController() || (InCharacter && InCharacter->GetWorld() != GetWorld())) return;
    if (ACameraManagerBase* Camera = Cast<ACameraManagerBase>(PlayerCameraManager))
    {
        if (InCharacter)
        {
            FCameraModeContext Context;
            Context.Target = InCharacter;
            Context.Transition = bInstant ? FCameraTransitionParams::Instant() : FCameraTransitionParams::Smooth();
            Camera->SetMode(UFollowCameraMode::StaticClass(), Context);
        }
        else Camera->ClearModeTarget();
    }
}
