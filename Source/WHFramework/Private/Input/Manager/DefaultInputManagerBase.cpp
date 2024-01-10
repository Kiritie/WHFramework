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

	InInputComponent->BindInputAction(GameplayTags::InputTag_TurnCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::TurnCamera);
	InInputComponent->BindInputAction(GameplayTags::InputTag_LookUpCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::LookUpCamera);
	InInputComponent->BindInputAction(GameplayTags::InputTag_PanHCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::PanHCamera);
	InInputComponent->BindInputAction(GameplayTags::InputTag_PanVCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::PanVCamera);
	InInputComponent->BindInputAction(GameplayTags::InputTag_ZoomCamera, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::ZoomCamera);
	
	InInputComponent->BindInputAction(GameplayTags::InputTag_TurnPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::TurnPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_MoveHPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::MoveHPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_MoveVPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::MoveVPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_MoveForwardPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::MoveForwardPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_MoveRightPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::MoveRightPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_MoveUpPlayer, ETriggerEvent::Triggered, this, &UDefaultInputManagerBase::MoveUpPlayer);
	InInputComponent->BindInputAction(GameplayTags::InputTag_JumpPlayer, ETriggerEvent::Started, this, &UDefaultInputManagerBase::JumpPlayer);

	InInputComponent->BindInputAction(GameplayTags::InputTag_SystemOperation, ETriggerEvent::Started, this, &UDefaultInputManagerBase::SystemOperation);
}

void UDefaultInputManagerBase::SystemOperation_Implementation()
{
}

void UDefaultInputManagerBase::TurnCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::InputTag_CameraRotate).IsPressing(UCommonStatics::GetPlayerController(), true))
	{
		UCameraModule::Get().AddCameraRotationInput(InValue.Get<float>(), 0.f);
	}
}

void UDefaultInputManagerBase::LookUpCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::InputTag_CameraRotate).IsPressing(UCommonStatics::GetPlayerController(), true))
	{
		UCameraModule::Get().AddCameraRotationInput(0.f, UCameraModule::Get().IsReverseCameraPitch() ? -InValue.Get<float>() : InValue.Get<float>());
	}
}

void UDefaultInputManagerBase::PanHCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::InputTag_CameraPanMove).IsPressing(UCommonStatics::GetPlayerController()))
	{
		const FRotator Rotation = UCommonStatics::GetPlayerController()->GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (UCameraModule::Get().IsReverseCameraPanMove() ? -1.f : 1.f);
		UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>());
	}
}

void UDefaultInputManagerBase::PanVCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::InputTag_CameraPanMove).IsPressing(UCommonStatics::GetPlayerController()))
	{
		const FRotator Rotation = UCommonStatics::GetPlayerController()->GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * (UCameraModule::Get().IsReverseCameraPanMove() ? -1.f : 1.f);
		UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>());
	}
}

void UDefaultInputManagerBase::ZoomCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UInputModuleStatics::GetKeyShortcut(GameplayTags::InputTag_CameraZoom).IsPressing(UCommonStatics::GetPlayerController(), true))
	{
		UCameraModule::Get().AddCameraDistanceInput(-InValue.Get<float>());
	}
}

void UDefaultInputManagerBase::MoveForwardCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = UCameraModule::Get().GetCurrentCameraRotation().Vector();
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::InputTag_CameraSprint).IsPressing(UCommonStatics::GetPlayerController()) ? 3.f : 1.5f));
}

void UDefaultInputManagerBase::MoveRightCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = FRotationMatrix(UCameraModule::Get().GetCurrentCameraRotation()).GetUnitAxis(EAxis::Y);
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::InputTag_CameraSprint).IsPressing(UCommonStatics::GetPlayerController()) ? 3.f : 1.5f));
}

void UDefaultInputManagerBase::MoveUpCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	UCameraModule::Get().AddCameraMovementInput(FVector::UpVector, InValue.Get<float>() * (UInputModuleStatics::GetKeyShortcut(GameplayTags::InputTag_CameraSprint).IsPressing(UCommonStatics::GetPlayerController()) ? 3.f : 1.5f));
}

void UDefaultInputManagerBase::TurnPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController()->GetPawn() && UCommonStatics::GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_Turn(UCommonStatics::GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void UDefaultInputManagerBase::MoveHPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController()->GetPawn() && UCommonStatics::GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveH(UCommonStatics::GetPlayerController()->GetPawn(), InValue.Get<float>());
	}
}

void UDefaultInputManagerBase::MoveVPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonStatics::GetPlayerController()->GetPawn() && UCommonStatics::GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_MoveV(UCommonStatics::GetPlayerController()->GetPawn(), InValue.Get<float>());
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

void UDefaultInputManagerBase::JumpPlayer_Implementation()
{
	if(UCommonStatics::GetPlayerController()->GetPawn() && UCommonStatics::GetPlayerController()->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_JumpN(UCommonStatics::GetPlayerController()->GetPawn());
	}
}
