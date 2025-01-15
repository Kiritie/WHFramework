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

void UDefaultInputManagerBase::OnBindAction(UInputComponentBase* InInputComponent)
{
	Super::OnBindAction(InInputComponent);

	InInputComponent->BindInputAction(GameplayTags::Input_TurnCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::TurnCamera);
	InInputComponent->BindInputAction(GameplayTags::Input_LookUpCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::LookUpCamera);
	InInputComponent->BindInputAction(GameplayTags::Input_PanHCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::PanHCamera, false);
	InInputComponent->BindInputAction(GameplayTags::Input_PanVCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::PanVCamera, false);
	InInputComponent->BindInputAction(GameplayTags::Input_ZoomCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::ZoomCamera);
	
	InInputComponent->BindInputAction(GameplayTags::Input_TurnPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::TurnPlayer, false);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveForwardPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::MoveForwardPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveForwardPlayer, ETriggerEvent::Started, this, &UDefaultInputManagerBase::ActionForwardPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveRightPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::MoveRightPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveRightPlayer, ETriggerEvent::Started, this, &UDefaultInputManagerBase::ActionRightPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveUpPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::MoveUpPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveUpPlayer, ETriggerEvent::Started, this, &UDefaultInputManagerBase::ActionUpPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_JumpPlayer, ETriggerEvent::Started, this, &UDefaultInputManagerBase::JumpPlayer, false);

	InInputComponent->BindInputAction(GameplayTags::Input_SystemOperation, ETriggerEvent::Started, this, &UDefaultInputManagerBase::SystemOperation);
}

void UDefaultInputManagerBase::SystemOperation_Implementation()
{
}

void UDefaultInputManagerBase::TurnCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraRotate).IsPressing(UCommonStatics::GetPlayerController(), true))
	{
		UCameraModule::Get().AddCameraRotationInput(InValue.Get<float>() / UCommonStatics::GetTimeScale(), 0.f);
	}
}

void UDefaultInputManagerBase::LookUpCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraRotate).IsPressing(UCommonStatics::GetPlayerController(), true))
	{
		UCameraModule::Get().AddCameraRotationInput(0.f, (UCameraModule::Get().IsReverseCameraPitch() ? -InValue.Get<float>() : InValue.Get<float>()) / UCommonStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::PanHCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraPanMove).IsPressing(UCommonStatics::GetPlayerController()))
	{
		const FRotator Rotation = FRotator(0.f, UCommonStatics::GetPlayerController()->GetControlRotation().Yaw, 0.f);
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (UCameraModule::Get().IsReverseCameraPanMove() ? -0.7f : 0.7f);
		UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() / UCommonStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::PanVCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraPanMove).IsPressing(UCommonStatics::GetPlayerController()))
	{
		const FRotator Rotation = FRotator(UCameraModule::Get().IsEnableCameraPanZMove() ? UCommonStatics::GetPlayerController()->GetControlRotation().Pitch : 0.f, UCommonStatics::GetPlayerController()->GetControlRotation().Yaw, 0.f);
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(UCameraModule::Get().IsEnableCameraPanZMove() ? EAxis::Z : EAxis::X) * (UCameraModule::Get().IsReverseCameraPanMove() ? -0.7f : 0.7f);
		UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() / UCommonStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::ZoomCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraZoom).IsPressing(UCommonStatics::GetPlayerController(), true))
	{
		UCameraModule::Get().AddCameraDistanceInput(-InValue.Get<float>() / UCommonStatics::GetTimeScale());
	}
}

void UDefaultInputManagerBase::MoveForwardCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = UCameraModule::Get().GetCurrentCameraRotation().Vector();
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraSprint).IsPressing(UCommonStatics::GetPlayerController()) ? 3.f : 1.f) / UCommonStatics::GetTimeScale());
}

void UDefaultInputManagerBase::MoveRightCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = FRotationMatrix(UCameraModule::Get().GetCurrentCameraRotation()).GetUnitAxis(EAxis::Y);
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraSprint).IsPressing(UCommonStatics::GetPlayerController()) ? 3.f : 1.f) / UCommonStatics::GetTimeScale());
}

void UDefaultInputManagerBase::MoveUpCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	UCameraModule::Get().AddCameraMovementInput(FVector::UpVector, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraSprint).IsPressing(UCommonStatics::GetPlayerController()) ? 3.f : 1.f) / UCommonStatics::GetTimeScale());
}

void UDefaultInputManagerBase::TurnPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController()->GetPawn() && UCommonStatics::GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_Turn(UCommonStatics::GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void UDefaultInputManagerBase::MoveForwardPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController()->GetPawn() && UCommonStatics::GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveForward(UCommonStatics::GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
	else
	{
		MoveForwardCamera(InValue);
	}
}

void UDefaultInputManagerBase::ActionForwardPlayer_Implementation(const FInputActionValue& InValue)
{
}

void UDefaultInputManagerBase::MoveRightPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController()->GetPawn() && UCommonStatics::GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveRight(UCommonStatics::GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
	else
	{
		MoveRightCamera(InValue);
	}
}

void UDefaultInputManagerBase::ActionRightPlayer_Implementation(const FInputActionValue& InValue)
{
}

void UDefaultInputManagerBase::MoveUpPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController()->GetPawn() && UCommonStatics::GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveUp(UCommonStatics::GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
	else
	{
		MoveUpCamera(InValue);
	}
}

void UDefaultInputManagerBase::ActionUpPlayer_Implementation(const FInputActionValue& InValue)
{
}

void UDefaultInputManagerBase::JumpPlayer_Implementation()
{
	if(UCommonStatics::GetPlayerController()->GetPawn() && UCommonStatics::GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_JumpN(UCommonStatics::GetPlayerController()->GetPawn());
	}
}
