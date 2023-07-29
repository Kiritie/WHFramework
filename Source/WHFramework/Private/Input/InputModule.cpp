// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModule.h"

#include "Camera/CameraModule.h"
#include "Camera/CameraModuleBPLibrary.h"
#include "Main/Base/ModuleBase.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Input/EventHandle_ChangeInputMode.h"
#include "Gameplay/WHPlayerController.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Input/InputManager.h"
#include "Kismet/GameplayStatics.h"
#include "Global/GlobalBPLibrary.h"
#include "Input/InputModuleBPLibrary.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
		
IMPLEMENTATION_MODULE(AInputModule)

// Sets default values
AInputModule::AInputModule()
{
	ModuleName = FName("InputModule");
	
	KeyShortcuts.Add(FName("CameraPanMove"), FInputKeyShortcut(FKey("MiddleMouseButton")));
	KeyShortcuts.Add(FName("CameraRotate"), FInputKeyShortcut());
	KeyShortcuts.Add(FName("CameraZoom"), FInputKeyShortcut());

	AxisMappings.Add(FInputAxisMapping(FName("TurnCamera"), this, FName("TurnCamera")));
	AxisMappings.Add(FInputAxisMapping(FName("LookUpCamera"), this, FName("LookUpCamera")));
	AxisMappings.Add(FInputAxisMapping(FName("PanHCamera"), this, FName("PanHCamera")));
	AxisMappings.Add(FInputAxisMapping(FName("PanVCamera"), this, FName("PanVCamera")));
	AxisMappings.Add(FInputAxisMapping(FName("ZoomCamera"), this, FName("ZoomCamera")));
	AxisMappings.Add(FInputAxisMapping(FName("TurnPlayer"), this, FName("TurnPlayer")));
	AxisMappings.Add(FInputAxisMapping(FName("MoveHPlayer"), this, FName("MoveHPlayer")));
	AxisMappings.Add(FInputAxisMapping(FName("MoveVPlayer"), this, FName("MoveVPlayer")));
	AxisMappings.Add(FInputAxisMapping(FName("MoveForwardPlayer"), this, FName("MoveForwardPlayer")));
	AxisMappings.Add(FInputAxisMapping(FName("MoveRightPlayer"), this, FName("MoveRightPlayer")));
	AxisMappings.Add(FInputAxisMapping(FName("MoveUpPlayer"), this, FName("MoveUpPlayer")));

	TouchMappings.Add(FInputTouchMapping(IE_Pressed, this, FName("TouchPressed")));
	TouchMappings.Add(FInputTouchMapping(IE_Released, this, FName("TouchReleased")));
	TouchMappings.Add(FInputTouchMapping(IE_Repeat, this, FName("TouchMoved")));

	TouchInputRate = 1.f;
	TouchPressedCount = 0;
	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;
}

AInputModule::~AInputModule()
{
	TERMINATION_MODULE(AInputModule)
}

#if WITH_EDITOR
void AInputModule::OnGenerate()
{
	Super::OnGenerate();
}

void AInputModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AInputModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AInputModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
	
	for(auto Iter : KeyMappings)
	{
		FInputKeyBinding KB(FInputChord(Iter.Key, false, false, false, false), Iter.Event);
		KB.KeyDelegate.BindDelegate(Iter.TargetActor ? Iter.TargetActor : this, Iter.FuncName);
		GetPlayerController()->InputComponent->KeyBindings.Emplace(MoveTemp(KB));
	}
	for(auto Iter : ActionMappings)
	{
		FInputActionBinding AB(Iter.ActionName, Iter.Event);
		AB.ActionDelegate.BindDelegate(Iter.TargetActor ? Iter.TargetActor : this, Iter.FuncName);
		GetPlayerController()->InputComponent->AddActionBinding(AB);
	}
	for(auto Iter : AxisMappings)
	{
		FInputAxisBinding AB(Iter.AxisName);
		AB.AxisDelegate.BindDelegate(Iter.TargetActor ? Iter.TargetActor : this, Iter.FuncName);
		GetPlayerController()->InputComponent->AxisBindings.Emplace(MoveTemp(AB));
	}
	for(auto Iter : TouchMappings)
	{
		FInputTouchBinding TB(Iter.Event);
		TB.TouchDelegate.BindDelegate(Iter.TargetActor ? Iter.TargetActor : this, Iter.FuncName);
		GetPlayerController()->InputComponent->TouchBindings.Emplace(MoveTemp(TB));
	}

	if(InPhase == EPhase::Final)
	{
		UpdateInputMode();
	}
}

void AInputModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AInputModule::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	TouchPressedCount = 0;
}

void AInputModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AInputModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AInputModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();
}

FInputKeyShortcut AInputModule::GetKeyShortcutByName(const FName InName) const
{
	if(KeyShortcuts.Contains(InName))
	{
		return KeyShortcuts[InName];
	}
	return FInputKeyShortcut();
}

FEventReply AInputModule::OnWidgetInputKeyDown_Implementation(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	return FEventReply(false);
}

FEventReply AInputModule::OnWidgetInputMouseButtonDown_Implementation(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FEventReply(false);
}

void AInputModule::TurnCamera(float InRate)
{
	if(InRate == 0.f || ACameraModule::Get()->IsControllingMove()) return;

	if(!GetKeyShortcutByName(FName("CameraRotate")).IsValid() || GetPlayerController()->IsInputKeyDown(GetKeyShortcutByName(FName("CameraRotate")).Key))
	{
		ACameraModule::Get()->AddCameraRotationInput(InRate, 0.f);
	}
}

void AInputModule::LookUpCamera(float InRate)
{
	if(InRate == 0.f || GetKeyShortcutByName(FName("CameraPanMove")).IsValid() && GetPlayerController()->IsInputKeyDown(GetKeyShortcutByName(FName("CameraPanMove")).Key)) return;

	if(!GetKeyShortcutByName(FName("CameraRotate")).IsValid() || GetPlayerController()->IsInputKeyDown(GetKeyShortcutByName(FName("CameraRotate")).Key))
	{
		ACameraModule::Get()->AddCameraRotationInput(0.f, ACameraModule::Get()->IsReverseCameraPitch() ? -InRate : InRate);
	}
}

void AInputModule::PanHCamera(float InRate)
{
	if(InRate == 0.f) return;

	if(GetKeyShortcutByName(FName("CameraPanMove")).IsValid() && GetPlayerController()->IsInputKeyDown(GetKeyShortcutByName(FName("CameraPanMove")).Key))
	{
		const FRotator Rotation = GetPlayerController()->GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (ACameraModule::Get()->IsReverseCameraPanMove() ? -1.f : 1.f);
		ACameraModule::Get()->AddCameraMovementInput(Direction, InRate);
	}
}

void AInputModule::PanVCamera(float InRate)
{
	if(InRate == 0.f) return;

	if(GetKeyShortcutByName(FName("CameraPanMove")).IsValid() && GetPlayerController()->IsInputKeyDown(GetKeyShortcutByName(FName("CameraPanMove")).Key))
	{
		const FRotator Rotation = GetPlayerController()->GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * (ACameraModule::Get()->IsReverseCameraPanMove() ? -1.f : 1.f);
		ACameraModule::Get()->AddCameraMovementInput(Direction, InRate);
	}
}

void AInputModule::ZoomCamera(float InRate)
{
	if(InRate == 0.f) return;

	if(!GetKeyShortcutByName(FName("CameraZoom")).IsValid() || GetPlayerController()->IsInputKeyDown(GetKeyShortcutByName(FName("CameraZoom")).Key))
	{
		ACameraModule::Get()->AddCameraDistanceInput(-InRate);
	}
}

void AInputModule::TurnPlayer(float InValue)
{
	if(InValue == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_Turn(GetPlayerController()->GetPawn(), InValue);
	}
}

void AInputModule::MoveHPlayer(float InValue)
{
	if(InValue == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveH(GetPlayerController()->GetPawn(), InValue);
	}
}

void AInputModule::MoveVPlayer(float InValue)
{
	if(InValue == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveV(GetPlayerController()->GetPawn(), InValue);
	}
}

void AInputModule::MoveForwardPlayer(float InValue)
{
	if(InValue == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveForward(GetPlayerController()->GetPawn(), InValue);
	}
}

void AInputModule::MoveRightPlayer(float InValue)
{
	if(InValue == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveRight(GetPlayerController()->GetPawn(), InValue);
	}
}

void AInputModule::MoveUpPlayer(float InValue)
{
	if(InValue == 0.f) return;

	if(GetPlayerController()->GetPawn() && GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveUp(GetPlayerController()->GetPawn(), InValue);
	}
}

void AInputModule::TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation)
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

void AInputModule::TouchPressedImpl()
{
	TouchPressedCount++;

	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;
}

void AInputModule::TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AInputModule::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle1, TimerDelegate, 0.15f, false);
			break;
		}
		case ETouchIndex::Touch2:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AInputModule::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle2, TimerDelegate, 0.15f, false);
			break;
		}
		case ETouchIndex::Touch3:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AInputModule::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle3, TimerDelegate, 0.15f, false);
			break;
		}
		default: break;
	}
}

void AInputModule::TouchReleasedImpl(ETouchIndex::Type InTouchIndex)
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
}

void AInputModule::TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	if(TouchPressedCount <= 0) return;
	
	if(TouchPressedCount == 1)
	{
		float TouchLocationX = 0.f;
		float TouchLocationY = 0.f;
		bool bIsCurrentPressed = false;
		GetPlayerController()->GetInputTouchState(InTouchIndex, TouchLocationX, TouchLocationY, bIsCurrentPressed);
		
		if(TouchLocationPrevious != FVector2D(-1.f, -1.f))
		{
			ACameraModule::Get()->AddCameraRotationInput((TouchLocationX - TouchLocationPrevious.X) * TouchInputRate, -(TouchLocationY - TouchLocationPrevious.Y) * TouchInputRate);
		}
		TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
	}
	else if(TouchPressedCount == 2)
	{
		float TouchLocationX1 = 0.f;
		float TouchLocationY1 = 0.f;
		bool bIsCurrentPressed1 = false;
		GetPlayerController()->GetInputTouchState(ETouchIndex::Touch1, TouchLocationX1, TouchLocationY1, bIsCurrentPressed1);
		
		float TouchLocationX2;
		float TouchLocationY2;
		bool bIsCurrentPressed2;
		GetPlayerController()->GetInputTouchState(ETouchIndex::Touch2, TouchLocationX2, TouchLocationY2, bIsCurrentPressed2);
		
		const float TouchCurrentPinchValue = FVector2D::Distance(FVector2D(TouchLocationX1, TouchLocationY1), FVector2D(TouchLocationX2, TouchLocationY2));
		if(TouchPinchValuePrevious != -1.f)
		{
			UCameraModuleBPLibrary::AddCameraDistanceInput(-(TouchCurrentPinchValue - TouchPinchValuePrevious) * TouchInputRate);
		}
		TouchPinchValuePrevious = TouchCurrentPinchValue;
	}
	else if(TouchPressedCount == 3)
	{
		float TouchLocationX = 0.f;
		float TouchLocationY = 0.f;
		bool bIsCurrentPressed = false;
		GetPlayerController()->GetInputTouchState(ETouchIndex::Touch1, TouchLocationX, TouchLocationY, bIsCurrentPressed);
		
		if(TouchLocationPrevious != FVector2D(-1.f, -1.f))
		{
			const FRotator Rotation = GetPlayerController()->GetControlRotation();
			const FVector DirectionH = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (TouchLocationX - TouchLocationPrevious.X);
			const FVector DirectionV = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * -(TouchLocationY - TouchLocationPrevious.Y);
			ACameraModule::Get()->AddCameraMovementInput(DirectionH + DirectionV, TouchInputRate * (ACameraModule::Get()->IsReverseCameraPanMove() ? -1.f : 1.f));
		}
		TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
	}
}

AWHPlayerController* AInputModule::GetPlayerController()
{
	if(!PlayerController)
	{
		PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>();
	}
	return PlayerController;
}

void AInputModule::UpdateInputMode()
{
	EInputMode TmpInputMode = EInputMode::None;
	for (auto Iter : AMainModule::GetAllModule())
	{
		if(IInputManager* InputManager = Cast<IInputManager>(Iter))
		{
			if ((int32)InputManager->GetNativeInputMode() > (int32)TmpInputMode)
			{
				TmpInputMode = InputManager->GetNativeInputMode();
			}
		}
	}
	SetGlobalInputMode(TmpInputMode);
}

void AInputModule::SetGlobalInputMode(EInputMode InInputMode)
{
	if(GlobalInputMode != InInputMode)
	{
		GlobalInputMode = InInputMode;
		switch (InInputMode)
		{
			case EInputMode::None:
			{
				GetPlayerController()->SetInputMode(FInputModeNone());
				GetPlayerController()->bShowMouseCursor = false;
				break;
			}
			case EInputMode::GameOnly:
			{
				GetPlayerController()->SetInputMode(FInputModeGameOnly());
				GetPlayerController()->bShowMouseCursor = false;
				break;
			}
			case EInputMode::UIOnly:
			{
				GetPlayerController()->SetInputMode(FInputModeUIOnly());
				GetPlayerController()->bShowMouseCursor = true;
				break;
			}
			case EInputMode::GameAndUI:
			{
				GetPlayerController()->SetInputMode(FInputModeGameAndUI());
				GetPlayerController()->bShowMouseCursor = true;
				break;
			}
			case EInputMode::GameAndUI_NotHideCursor:
			{
				GetPlayerController()->SetInputMode(FInputModeGameAndUI_NotHideCursor());
				GetPlayerController()->bShowMouseCursor = true;
				break;
			}
			default: break;
		}
		UEventModuleBPLibrary::BroadcastEvent(UEventHandle_ChangeInputMode::StaticClass(), EEventNetType::Multicast, this, { &GlobalInputMode });
	}
}
