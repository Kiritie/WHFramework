// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/Manager/DefaultInputManagerBase.h"

#include "Camera/CameraModule.h"
#include "Common/CommonStatics.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Input/InputModuleStatics.h"
#include "Input/Components/InputComponentBase.h"

// ParamSets default values
UDefaultInputManagerBase::UDefaultInputManagerBase()
{
	InputManagerName = FName("Default");
}

void UDefaultInputManagerBase::OnInitialize()
{
	Super::OnInitialize();
}

void UDefaultInputManagerBase::OnReset()
{
	Super::OnReset();
}

void UDefaultInputManagerBase::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UDefaultInputManagerBase::OnBindAction(UInputComponentBase* InInputComponent)
{
	Super::OnBindAction(InInputComponent);

	InInputComponent->BindInputAction(GameplayTags::Input_SystemOperation, ETriggerEvent::Started, this, &UDefaultInputManagerBase::SystemOperation);

	InInputComponent->BindInputAction(GameplayTags::Input_TurnCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnTurnCamera);
	InInputComponent->BindInputAction(GameplayTags::Input_LookUpCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnLookUpCamera);
	InInputComponent->BindInputAction(GameplayTags::Input_PanHCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnPanHCamera, false);
	InInputComponent->BindInputAction(GameplayTags::Input_PanVCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnPanVCamera, false);
	InInputComponent->BindInputAction(GameplayTags::Input_ZoomCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnZoomCamera);
	
	InInputComponent->BindInputAction(GameplayTags::Input_TurnPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnTurnPlayer, false);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveForwardPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnMoveForwardPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveForwardPlayer, ETriggerEvent::Started, this, &UDefaultInputManagerBase::OnActionForwardPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveRightPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnMoveRightPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveRightPlayer, ETriggerEvent::Started, this, &UDefaultInputManagerBase::OnActionRightPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveUpPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnMoveUpPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveUpPlayer, ETriggerEvent::Started, this, &UDefaultInputManagerBase::OnActionUpPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_JumpPlayer, ETriggerEvent::Started, this, &UDefaultInputManagerBase::OnJumpPlayer, false);
	
	InInputComponent->BindInputAction(GameplayTags::Input_Primary, ETriggerEvent::Started, this, &UDefaultInputManagerBase::OnPrimaryPressed, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Primary, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnPrimaryRepeated, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Primary, ETriggerEvent::Completed, this, &UDefaultInputManagerBase::OnPrimaryReleased, false);
	
	InInputComponent->BindInputAction(GameplayTags::Input_Secondary, ETriggerEvent::Started, this, &UDefaultInputManagerBase::OnSecondaryPressed, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Secondary, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnSecondaryRepeated, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Secondary, ETriggerEvent::Completed, this, &UDefaultInputManagerBase::OnSecondaryReleased, false);
		
	InInputComponent->BindInputAction(GameplayTags::Input_Third, ETriggerEvent::Started, this, &UDefaultInputManagerBase::OnThirdPressed, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Third, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::OnThirdRepeated, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Third, ETriggerEvent::Completed, this, &UDefaultInputManagerBase::OnThirdReleased, false);
}

void UDefaultInputManagerBase::OnTermination()
{
	Super::OnTermination();
}

void UDefaultInputManagerBase::SystemOperation_Implementation()
{
}

void UDefaultInputManagerBase::OnTurnCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraRotate).IsPressing(UCommonStatics::GetPlayerController(LocalPlayerIndex), true))
	{
		UCameraModule::Get().AddCameraRotationInput(InValue.Get<float>() / UCommonStatics::GetTimeScale(), 0.f);
	}
}

void UDefaultInputManagerBase::OnLookUpCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraRotate).IsPressing(UCommonStatics::GetPlayerController(LocalPlayerIndex), true))
	{
		UCameraModule::Get().AddCameraRotationInput(0.f, (UCameraModule::Get().IsReverseCameraPitch() ? -InValue.Get<float>() : InValue.Get<float>()) / UCommonStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::OnPanHCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraPanMove).IsPressing(UCommonStatics::GetPlayerController(LocalPlayerIndex)))
	{
		const FRotator Rotation = FRotator(0.f, UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetControlRotation().Yaw, 0.f);
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (UCameraModule::Get().IsReverseCameraPanMove() ? -0.7f : 0.7f);
		UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() / UCommonStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::OnPanVCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraPanMove).IsPressing(UCommonStatics::GetPlayerController(LocalPlayerIndex)))
	{
		const FRotator Rotation = FRotator(UCameraModule::Get().IsEnableCameraPanZMove() ? UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetControlRotation().Pitch : 0.f, UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetControlRotation().Yaw, 0.f);
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(UCameraModule::Get().IsEnableCameraPanZMove() ? EAxis::Z : EAxis::X) * (UCameraModule::Get().IsReverseCameraPanMove() ? -0.7f : 0.7f);
		UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() / UCommonStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::OnZoomCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraZoom).IsPressing(UCommonStatics::GetPlayerController(LocalPlayerIndex), true))
	{
		UCameraModule::Get().AddCameraDistanceInput(-InValue.Get<float>() / UCommonStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::OnMoveForwardCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = UCameraModule::Get().GetCurrentCameraRotation().Vector();
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraSprint).IsPressing(UCommonStatics::GetPlayerController(LocalPlayerIndex)) ? 3.f : 1.f) / UCommonStatics::GetTimeScale());
}

void UDefaultInputManagerBase::OnMoveRightCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = FRotationMatrix(UCameraModule::Get().GetCurrentCameraRotation()).GetUnitAxis(EAxis::Y);
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraSprint).IsPressing(UCommonStatics::GetPlayerController(LocalPlayerIndex)) ? 3.f : 1.f) / UCommonStatics::GetTimeScale());
}

void UDefaultInputManagerBase::OnMoveUpCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	UCameraModule::Get().AddCameraMovementInput(FVector::UpVector, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraSprint).IsPressing(UCommonStatics::GetPlayerController(LocalPlayerIndex)) ? 3.f : 1.f) / UCommonStatics::GetTimeScale());
}

void UDefaultInputManagerBase::OnTurnPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_Turn(UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn(), InValue.Get<float>());
	}
}

void UDefaultInputManagerBase::OnMoveForwardPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveForward(UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn(), InValue.Get<float>());
	}
	else
	{
		OnMoveForwardCamera(InValue);
	}
}

void UDefaultInputManagerBase::OnActionForwardPlayer_Implementation(const FInputActionValue& InValue)
{
}

void UDefaultInputManagerBase::OnMoveRightPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveRight(UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn(), InValue.Get<float>());
	}
	else
	{
		OnMoveRightCamera(InValue);
	}
}

void UDefaultInputManagerBase::OnActionRightPlayer_Implementation(const FInputActionValue& InValue)
{
}

void UDefaultInputManagerBase::OnMoveUpPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveUp(UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn(), InValue.Get<float>());
	}
	else
	{
		OnMoveUpCamera(InValue);
	}
}

void UDefaultInputManagerBase::OnActionUpPlayer_Implementation(const FInputActionValue& InValue)
{
}

void UDefaultInputManagerBase::OnJumpPlayer_Implementation()
{
	if(UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_JumpN(UCommonStatics::GetPlayerController(LocalPlayerIndex)->GetPawn());
	}
}
void UDefaultInputManagerBase::OnPrimaryPressed_Implementation()
{
}

void UDefaultInputManagerBase::OnPrimaryRepeated_Implementation()
{
}

void UDefaultInputManagerBase::OnPrimaryReleased_Implementation()
{
}

void UDefaultInputManagerBase::OnSecondaryPressed_Implementation()
{
}

void UDefaultInputManagerBase::OnSecondaryRepeated_Implementation()
{
}

void UDefaultInputManagerBase::OnSecondaryReleased_Implementation()
{
}

void UDefaultInputManagerBase::OnThirdPressed_Implementation()
{
}

void UDefaultInputManagerBase::OnThirdRepeated_Implementation()
{
}

void UDefaultInputManagerBase::OnThirdReleased_Implementation()
{
}
