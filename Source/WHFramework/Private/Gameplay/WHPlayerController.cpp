// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHPlayerController.h"

#include "Camera/Base/CameraManagerBase.h"
#include "Components/WidgetInteractionComponent.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Common/CommonStatics.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "Debug/DebugModuleTypes.h"
#include "Input/InputModuleStatics.h"
#include "Input/Components/InputComponentBase.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"

AWHPlayerController::AWHPlayerController()
{
	PlayerCameraManagerClass = ACameraManagerBase::StaticClass();

	WidgetInteractionComp = CreateDefaultSubobject<UWidgetInteractionComponent>(FName("WidgetInteractionComp"));
	WidgetInteractionComp->SetupAttachment(RootComponent);

	InteractionRaycastMode = EInteractionRaycastMode::None;
	InteractionDistance = 100000.f;
}

void AWHPlayerController::OnInitialize_Implementation()
{
	
}

void AWHPlayerController::OnPreparatory_Implementation(EPhase InPhase)
{
	for(const auto Iter : AMainModule::GetAllModule())
	{
		if(Iter->ModuleNetworkComponent)
		{
			UModuleNetworkComponentBase* NetworkComponent = NewObject<UModuleNetworkComponentBase>(this, Iter->ModuleNetworkComponent, *FString::Printf(TEXT("%sNetworkComp"), *Iter->ModuleName.ToString()));
			NetworkComponent->RegisterComponent();
		}
	}
}

void AWHPlayerController::OnRefresh_Implementation(float DeltaSeconds)
{
	OnRefreshInteraction();
}

void AWHPlayerController::OnTermination_Implementation(EPhase InPhase)
{
	
}

void AWHPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnInitialize(this);
		Execute_OnPreparatory(this, EPhase::None);
	}
}

void AWHPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::None);
	}
}

void AWHPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetPlayerPawn(InPawn);
}

void AWHPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AWHPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	ensureEditorMsgf(InputComponent->IsA<UInputComponentBase>(), FString::Printf(TEXT("Invalid InputComponent in DefaultInput.ini, must be InputComponentBase!")), EDC_Input, EDV_Error);
}

void AWHPlayerController::Tick(float DeltaSeconds) 
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void AWHPlayerController::OnRefreshInteraction_Implementation()
{
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
		default: break;
	}
	if(HitResult.bBlockingHit)
	{
		AActor* HitActor = HitResult.GetActor();
		if(HoveringInteraction.GetObject())
		{
			if(HitActor->Implements<UInteractionAgentInterface>())
			{
				if(HitActor == HoveringInteraction.GetObject())
				{
					IInteractionAgentInterface::Execute_OnHovering(HoveringInteraction.GetObject());
				}
				else
				{
					IInteractionAgentInterface::Execute_OnEndHover(HoveringInteraction.GetObject());
					HoveringInteraction = HitActor;
					IInteractionAgentInterface::Execute_OnBeginHover(HoveringInteraction.GetObject());
					IInteractionAgentInterface::Execute_OnHovering(HoveringInteraction.GetObject());
				}
			}
			else
			{
				IInteractionAgentInterface::Execute_OnEndHover(HoveringInteraction.GetObject());
				HoveringInteraction = nullptr;
			}
		}
		else if(HitActor->Implements<UInteractionAgentInterface>())
		{
			HoveringInteraction = HitActor;
			IInteractionAgentInterface::Execute_OnBeginHover(HoveringInteraction.GetObject());
			IInteractionAgentInterface::Execute_OnHovering(HoveringInteraction.GetObject());
		}
	}
	else if(HoveringInteraction.GetObject())
	{
		IInteractionAgentInterface::Execute_OnEndHover(HoveringInteraction.GetObject());
		HoveringInteraction = nullptr;
	}

	if(UInputModuleStatics::GetKeyShortcutByName(FName("InteractSelect")).IsPressed(this))
	{
		if(HoveringInteraction.GetObject())
		{
			if(!SelectedInteraction.GetObject())
			{
				SelectedInteraction = HoveringInteraction;
				IInteractionAgentInterface::Execute_OnSelected(SelectedInteraction.GetObject());
			}
			else if(SelectedInteraction != HoveringInteraction)
			{
				IInteractionAgentInterface::Execute_OnUnSelected(SelectedInteraction.GetObject());
				SelectedInteraction = HoveringInteraction;
				IInteractionAgentInterface::Execute_OnSelected(SelectedInteraction.GetObject());
			}
		}
		else if(SelectedInteraction.GetObject())
		{
			IInteractionAgentInterface::Execute_OnUnSelected(SelectedInteraction.GetObject());
			SelectedInteraction = nullptr;
		}
	}
}

bool AWHPlayerController::RaycastSingleFromScreenPosition(FVector2D InScreenPosition, float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	if(UInputModuleStatics::GetGlobalInputMode() == EInputMode::None || UInputModuleStatics::GetGlobalInputMode() == EInputMode::UIOnly) return false;

	FVector SightPos, RayDirection;
	if(DeprojectScreenPositionToWorld(InScreenPosition.X, InScreenPosition.Y, SightPos, RayDirection))
	{
		const FVector RayStart = PlayerCameraManager->GetCameraLocation();
		const FVector RayEnd = RayStart + RayDirection * InRayDistance;
		TArray<AActor*> IgnoreActors = InIgnoreActors;
		IgnoreActors.AddUnique(GetPawn());
		IgnoreActors.AddUnique(GetPlayerPawn());
		return UKismetSystemLibrary::LineTraceSingle(this, RayStart, RayEnd, UCommonStatics::GetGameTraceType(InGameTraceChannel), false, IgnoreActors, EDrawDebugTrace::None, OutHitResult, true);
	}
	return false;
}

bool AWHPlayerController::RaycastSingleFromViewportPosition(FVector2D InViewportPosition, float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	int32 ViewportSizeX, ViewportSizeY;
	GetViewportSize(ViewportSizeX, ViewportSizeY);
	return RaycastSingleFromScreenPosition(FVector2D(ViewportSizeX * InViewportPosition.X, ViewportSizeY * InViewportPosition.Y), InRayDistance, InGameTraceChannel, InIgnoreActors, OutHitResult);
}

bool AWHPlayerController::RaycastSingleFromMousePosition(float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	if(!bShowMouseCursor) return false;

	float MousePosX, MousePosY;
	GetMousePosition(MousePosX, MousePosY);
	return RaycastSingleFromScreenPosition(FVector2D(MousePosX, MousePosY), InRayDistance, InGameTraceChannel, InIgnoreActors, OutHitResult);
}

bool AWHPlayerController::RaycastSingleFromAimPosition(float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	return RaycastSingleFromViewportPosition(GetAnimPosition(), InRayDistance, InGameTraceChannel, InIgnoreActors, OutHitResult);
}

void AWHPlayerController::SetPlayerPawn(APawn* InPlayerPawn)
{
	if(InPlayerPawn && !InPlayerPawn->Implements<UWHPlayerInterface>()) return;

	if(PlayerPawn != InPlayerPawn)
	{
		PlayerPawn = InPlayerPawn;
		OnPlayerPawnChanged.Broadcast(PlayerPawn);
	}
}
