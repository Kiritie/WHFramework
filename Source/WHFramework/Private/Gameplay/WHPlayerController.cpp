// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHPlayerController.h"

#include "Net/UnrealNetwork.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Camera/CameraModule.h"
#include "Camera/CameraModuleNetworkComponent.h"
#include "Camera/Base/CameraPawnBase.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleNetworkComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Global/GlobalBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Main/MainModule.h"
#include "Math/MathBPLibrary.h"
#include "Media/MediaModuleNetworkComponent.h"
#include "Network/NetworkModuleNetworkComponent.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"

AWHPlayerController::AWHPlayerController()
{
	WidgetInteractionComp = CreateDefaultSubobject<UWidgetInteractionComponent>(FName("WidgetInteractionComp"));
	WidgetInteractionComp->SetupAttachment(RootComponent);

	ModuleNetCompMap = TMap<TSubclassOf<UModuleNetworkComponent>, UModuleNetworkComponent*>();
	
	AudioModuleNetComp = CreateDefaultSubobject<UAudioModuleNetworkComponent>(FName("AudioModuleNetComp"));
	ModuleNetCompMap.Add(UAudioModuleNetworkComponent::StaticClass(), AudioModuleNetComp);
	
	CameraModuleNetComp = CreateDefaultSubobject<UCameraModuleNetworkComponent>(FName("CameraModuleNetComp"));
	ModuleNetCompMap.Add(UCameraModuleNetworkComponent::StaticClass(), CameraModuleNetComp);
	
	CharacterModuleNetComp = CreateDefaultSubobject<UCharacterModuleNetworkComponent>(FName("CharacterModuleNetComp"));
	ModuleNetCompMap.Add(UCharacterModuleNetworkComponent::StaticClass(), CharacterModuleNetComp);
	
	EventModuleNetComp = CreateDefaultSubobject<UEventModuleNetworkComponent>(FName("EventModuleNetComp"));
	ModuleNetCompMap.Add(UEventModuleNetworkComponent::StaticClass(), EventModuleNetComp);
	
	MediaModuleNetComp = CreateDefaultSubobject<UMediaModuleNetworkComponent>(FName("MediaModuleNetComp"));
	ModuleNetCompMap.Add(UMediaModuleNetworkComponent::StaticClass(), MediaModuleNetComp);
	
	NetworkModuleNetComp = CreateDefaultSubobject<UNetworkModuleNetworkComponent>(FName("NetworkModuleNetComp"));
	ModuleNetCompMap.Add(UNetworkModuleNetworkComponent::StaticClass(), NetworkModuleNetComp);
	
	ProcedureModuleNetComp = CreateDefaultSubobject<UProcedureModuleNetworkComponent>(FName("ProcedureModuleNetComp"));
	ModuleNetCompMap.Add(UProcedureModuleNetworkComponent::StaticClass(), ProcedureModuleNetComp);

	TouchInputRate = 1.f;

	TouchPressedCount = 0;
	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;
}

void AWHPlayerController::OnInitialize_Implementation()
{
	
}

void AWHPlayerController::OnPreparatory_Implementation()
{
	
}

void AWHPlayerController::BeginPlay()
{
	Super::BeginPlay();

	OnPreparatory();
}

void AWHPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	InputComponent->BindAxis(FName("Turn"), this, &AWHPlayerController::Turn);
	InputComponent->BindAxis(FName("LookUp"), this, &AWHPlayerController::LookUp);
	InputComponent->BindAxis(FName("PanH"), this, &AWHPlayerController::PanH);
	InputComponent->BindAxis(FName("PanV"), this, &AWHPlayerController::PanV);
	InputComponent->BindAxis(FName("ZoomCam"), this, &AWHPlayerController::ZoomCam);

	InputComponent->BindAxis(FName("MoveForward"), this, &AWHPlayerController::MoveForward);
	InputComponent->BindAxis(FName("MoveRight"), this, &AWHPlayerController::MoveRight);
	InputComponent->BindAxis(FName("MoveUp"), this, &AWHPlayerController::MoveUp);

	InputComponent->BindAction(FName("Interact"), EInputEvent::IE_Pressed, this, &AWHPlayerController::StartInteract);
	InputComponent->BindAction(FName("Interact"), EInputEvent::IE_Released, this, &AWHPlayerController::EndInteract);

	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AWHPlayerController::TouchPressed);
	InputComponent->BindTouch(EInputEvent::IE_Released, this, &AWHPlayerController::TouchReleased);
	InputComponent->BindTouch(EInputEvent::IE_Repeat, this, &AWHPlayerController::TouchMoved);
}

void AWHPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if(InPawn->Implements<UWHPlayerInterface>() && !InPawn->IsA<ACameraPawnBase>())
	{
		PlayerPawn = InPawn;
	}
}

void AWHPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AWHPlayerController::Tick(float DeltaTime) 
{
	Super::Tick(DeltaTime);
}

void AWHPlayerController::Turn(float InRate)
{
	if(InRate == 0.f) return;

	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		if(!CameraModule->GetCameraRotateKey().IsValid() || IsInputKeyDown(CameraModule->GetCameraRotateKey()))
		{
			CameraModule->AddCameraRotationInput(InRate, 0.f);
		}
	}
}

void AWHPlayerController::LookUp(float InRate)
{
	if(InRate == 0.f) return;

	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		if(!CameraModule->GetCameraRotateKey().IsValid() || IsInputKeyDown(CameraModule->GetCameraRotateKey()))
		{
			CameraModule->AddCameraRotationInput(0.f, CameraModule->IsReverseCameraPitch() ? -InRate : InRate);
		}
	}
}

void AWHPlayerController::PanH(float InRate)
{
	if(InRate == 0.f) return;

	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		if(!CameraModule->GetCameraPanMoveKey().IsValid() || IsInputKeyDown(CameraModule->GetCameraPanMoveKey()))
		{
			const FRotator Rotation = GetControlRotation();
			const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (CameraModule->IsReverseCameraPanMove() ? -1.f : 1.f);
			CameraModule->AddCameraMovementInput(Direction, InRate);
		}
	}
}

void AWHPlayerController::PanV(float InRate)
{
	if(InRate == 0.f) return;

	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		if(!CameraModule->GetCameraPanMoveKey().IsValid() || IsInputKeyDown(CameraModule->GetCameraPanMoveKey()))
		{
			const FRotator Rotation = GetControlRotation();
			const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * (CameraModule->IsReverseCameraPanMove() ? -1.f : 1.f);
			CameraModule->AddCameraMovementInput(Direction, InRate);
		}
	}
}

void AWHPlayerController::ZoomCam(float InRate)
{
	if(InRate == 0.f) return;

	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		if(!CameraModule->GetCameraZoomKey().IsValid() || IsInputKeyDown(CameraModule->GetCameraZoomKey()))
		{
			CameraModule->AddCameraDistanceInput(-InRate);
		}
	}
}

void AWHPlayerController::TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			if(TouchReleaseTimerHandle1.IsValid())
			{
				TouchReleasedImpl(InTouchIndex);
			}
			TouchPressedImpl();
			break;
		}
		case ETouchIndex::Touch2:
		{
			if(TouchReleaseTimerHandle2.IsValid())
			{
				TouchReleasedImpl(InTouchIndex);
			}
			TouchPressedImpl();
			break;
		}
		case ETouchIndex::Touch3:
		{
			if(TouchReleaseTimerHandle3.IsValid())
			{
				TouchReleasedImpl(InTouchIndex);
			}
			TouchPressedImpl();
			break;
		}
		default: break;
	}
}

void AWHPlayerController::TouchPressedImpl()
{
	TouchPressedCount++;

	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;

	// GEngine->AddOnScreenDebugMessage(0, 1.5f, FColor::Cyan, FString::FromInt(TouchPressedCount));
}

void AWHPlayerController::TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AWHPlayerController::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle1, TimerDelegate, 0.15f, false);
			break;
		}
		case ETouchIndex::Touch2:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AWHPlayerController::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle2, TimerDelegate, 0.15f, false);
			break;
		}
		case ETouchIndex::Touch3:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AWHPlayerController::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle3, TimerDelegate, 0.15f, false);
			break;
		}
		default: break;
	}
}

void AWHPlayerController::TouchReleasedImpl(ETouchIndex::Type InTouchIndex)
{
	TouchPressedCount--;
	if(TouchPressedCount < 0)
	{
		TouchPressedCount = 0;
	}
	
	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;

	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			GetWorld()->GetTimerManager().ClearTimer(TouchReleaseTimerHandle1);
			break;
		}
		case ETouchIndex::Touch2:
		{
			GetWorld()->GetTimerManager().ClearTimer(TouchReleaseTimerHandle2);
			break;
		}
		case ETouchIndex::Touch3:
		{
			GetWorld()->GetTimerManager().ClearTimer(TouchReleaseTimerHandle3);
			break;
		}
		default: break;
	}

	// GEngine->AddOnScreenDebugMessage(0, 1.5f, FColor::Cyan, FString::FromInt(TouchPressedCount));
}

void AWHPlayerController::TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	if(TouchPressedCount <= 0) return;
	
	if(TouchPressedCount == 1)
	{
		float TouchLocationX = 0.f;
		float TouchLocationY = 0.f;
		bool bIsCurrentPressed = false;
		GetInputTouchState(InTouchIndex, TouchLocationX, TouchLocationY, bIsCurrentPressed);
		
		if(TouchLocationPrevious != FVector2D(-1.f, -1.f))
		{
			if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
			{
				CameraModule->AddCameraRotationInput((TouchLocationX - TouchLocationPrevious.X) * TouchInputRate, -(TouchLocationY - TouchLocationPrevious.Y) * TouchInputRate);
			}
		}
		TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
	}
	else if(TouchPressedCount == 2)
	{
		float TouchLocationX1 = 0.f;
		float TouchLocationY1 = 0.f;
		bool bIsCurrentPressed1 = false;
		GetInputTouchState(ETouchIndex::Touch1, TouchLocationX1, TouchLocationY1, bIsCurrentPressed1);
		
		float TouchLocationX2;
		float TouchLocationY2;
		bool bIsCurrentPressed2;
		GetInputTouchState(ETouchIndex::Touch2, TouchLocationX2, TouchLocationY2, bIsCurrentPressed2);
		
		const float TouchCurrentPinchValue = FVector2D::Distance(FVector2D(TouchLocationX1, TouchLocationY1), FVector2D(TouchLocationX2, TouchLocationY2));
		if(TouchPinchValuePrevious != -1.f)
		{
			if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
			{
				CameraModule->AddCameraDistanceInput(-(TouchCurrentPinchValue - TouchPinchValuePrevious) * TouchInputRate);
			}
		}
		TouchPinchValuePrevious = TouchCurrentPinchValue;
	}
	else if(TouchPressedCount == 3)
	{
		float TouchLocationX = 0.f;
		float TouchLocationY = 0.f;
		bool bIsCurrentPressed = false;
		GetInputTouchState(ETouchIndex::Touch1, TouchLocationX, TouchLocationY, bIsCurrentPressed);
		
		if(TouchLocationPrevious != FVector2D(-1.f, -1.f))
		{
			const FRotator Rotation = GetControlRotation();
			const FVector DirectionH = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (TouchLocationX - TouchLocationPrevious.X);
			const FVector DirectionV = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * -(TouchLocationY - TouchLocationPrevious.Y);
			if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
			{
				CameraModule->AddCameraMovementInput(DirectionH + DirectionV, TouchInputRate * (CameraModule->IsReverseCameraPanMove() ? -1.f : 1.f));
			}
		}
		TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
	}
}

void AWHPlayerController::StartInteract(FKey InKey)
{
	WidgetInteractionComp->PressPointerKey(InKey);
}

void AWHPlayerController::EndInteract(FKey InKey)
{
	WidgetInteractionComp->ReleasePointerKey(InKey);
}

void AWHPlayerController::MoveForward(float InValue)
{
	if(InValue == 0.f) return;

	if(GetPawn() && GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveForward(GetPawn(), InValue);
	}
}

bool AWHPlayerController::RaycastSingleFromAimPoint(float InRayDistance, ECollisionChannel InGameTraceType, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult) const
{
	int32 viewportSizeX, viewportSizeY;
	FVector sightPos, rayDirection;
	GetViewportSize(viewportSizeX, viewportSizeY);
	if(DeprojectScreenPositionToWorld(viewportSizeX * 0.5f, viewportSizeY * 0.5f, sightPos, rayDirection))
	{
		const FVector rayStart = PlayerCameraManager->GetCameraLocation();
		const FVector rayEnd = rayStart + rayDirection * InRayDistance;
		TArray<AActor*> ignoreActors = InIgnoreActors;
		ignoreActors.AddUnique(GetPawn());
		ignoreActors.AddUnique(GetPlayerPawn());
		return UKismetSystemLibrary::LineTraceSingle(this, rayStart, rayEnd, UGlobalBPLibrary::GetGameTraceChannel(InGameTraceType), false, ignoreActors, EDrawDebugTrace::None, OutHitResult, true);
	}
	return false;
}

void AWHPlayerController::MoveRight(float InValue)
{
	if(InValue == 0.f) return;

	if(GetPawn() && GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveRight(GetPawn(), InValue);
	}
}

void AWHPlayerController::MoveUp(float InValue)
{
	if(InValue == 0.f) return;

	if(GetPawn() && GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveUp(GetPawn(), InValue);
	}
}
