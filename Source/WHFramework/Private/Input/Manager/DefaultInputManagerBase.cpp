// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/Manager/DefaultInputManagerBase.h"

#include "Camera/CameraModule.h"
#include "Common/CommonModuleStatics.h"
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

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraRotate).IsPressing(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex), true))
	{
		UCameraModule::Get().AddCameraRotationInput(InValue.Get<float>() / UCommonModuleStatics::GetTimeScale(), 0.f);
	}
}

void UDefaultInputManagerBase::OnLookUpCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraRotate).IsPressing(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex), true))
	{
		UCameraModule::Get().AddCameraRotationInput(0.f, (UCameraModule::Get().IsReverseCameraPitch() ? -InValue.Get<float>() : InValue.Get<float>()) / UCommonModuleStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::OnPanHCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraPanMove).IsPressing(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)))
	{
		const FRotator Rotation = FRotator(0.f, UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetControlRotation().Yaw, 0.f);
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (UCameraModule::Get().IsReverseCameraPanMove() ? -0.7f : 0.7f);
		UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() / UCommonModuleStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::OnPanVCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraPanMove).IsPressing(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)))
	{
		const FRotator Rotation = FRotator(UCameraModule::Get().IsEnableCameraPanZMove() ? UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetControlRotation().Pitch : 0.f, UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetControlRotation().Yaw, 0.f);
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(UCameraModule::Get().IsEnableCameraPanZMove() ? EAxis::Z : EAxis::X) * (UCameraModule::Get().IsReverseCameraPanMove() ? -0.7f : 0.7f);
		UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() / UCommonModuleStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::OnZoomCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraZoom).IsPressing(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex), true))
	{
		UCameraModule::Get().AddCameraDistanceInput(-InValue.Get<float>() / UCommonModuleStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::OnMoveForwardCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = UCameraModule::Get().GetCurrentCameraRotation().Vector();
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraSprint).IsPressing(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)) ? 3.f : 1.f) / UCommonModuleStatics::GetTimeScale());
}

void UDefaultInputManagerBase::OnMoveRightCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = FRotationMatrix(UCameraModule::Get().GetCurrentCameraRotation()).GetUnitAxis(EAxis::Y);
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraSprint).IsPressing(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)) ? 3.f : 1.f) / UCommonModuleStatics::GetTimeScale());
}

void UDefaultInputManagerBase::OnMoveUpCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	UCameraModule::Get().AddCameraMovementInput(FVector::UpVector, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraSprint).IsPressing(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)) ? 3.f : 1.f) / UCommonModuleStatics::GetTimeScale());
}

void UDefaultInputManagerBase::OnTurnPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_Turn(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn(), InValue.Get<float>());
	}
}

void UDefaultInputManagerBase::OnMoveForwardPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveForward(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn(), InValue.Get<float>());
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

	if(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveRight(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn(), InValue.Get<float>());
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

	if(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveUp(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn(), InValue.Get<float>());
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
	if(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_JumpN(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn());
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
