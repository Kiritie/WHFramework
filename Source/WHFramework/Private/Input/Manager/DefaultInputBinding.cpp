// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/Manager/DefaultInputBinding.h"

#include "Camera/CameraModule.h"
#include "Common/CommonModuleStatics.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Input/InputModuleStatics.h"
#include "Input/Components/InputComponentBase.h"

// ParamSets default values
UDefaultInputBinding::UDefaultInputBinding()
{
	InputBindingName = FName("Default");
}

void UDefaultInputBinding::OnInitialize(int32 InPlayerIndex)
{
	Super::OnInitialize(InPlayerIndex);
}

void UDefaultInputBinding::OnReset()
{
	Super::OnReset();
}

void UDefaultInputBinding::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UDefaultInputBinding::OnBindInput(UInputComponentBase* InInputComponent)
{
	Super::OnBindInput(InInputComponent);

	InInputComponent->BindInputAction(GameplayTags::Input_SystemOperation, ETriggerEvent::Started, this, &UDefaultInputBinding::SystemOperation);

	InInputComponent->BindInputAction(GameplayTags::Input_TurnCamera, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnTurnCamera);
	InInputComponent->BindInputAction(GameplayTags::Input_LookUpCamera, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnLookUpCamera);
	InInputComponent->BindInputAction(GameplayTags::Input_PanHCamera, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnPanHCamera, false);
	InInputComponent->BindInputAction(GameplayTags::Input_PanVCamera, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnPanVCamera, false);
	InInputComponent->BindInputAction(GameplayTags::Input_ZoomCamera, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnZoomCamera);
	
	InInputComponent->BindInputAction(GameplayTags::Input_TurnPlayer, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnTurnPlayer, false);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveForwardPlayer, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnMoveForwardPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveForwardPlayer, ETriggerEvent::Started, this, &UDefaultInputBinding::OnActionForwardPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveRightPlayer, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnMoveRightPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveRightPlayer, ETriggerEvent::Started, this, &UDefaultInputBinding::OnActionRightPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveUpPlayer, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnMoveUpPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_MoveUpPlayer, ETriggerEvent::Started, this, &UDefaultInputBinding::OnActionUpPlayer);
	InInputComponent->BindInputAction(GameplayTags::Input_JumpPlayer, ETriggerEvent::Started, this, &UDefaultInputBinding::OnJumpPlayer, false);
	
	InInputComponent->BindInputAction(GameplayTags::Input_Primary, ETriggerEvent::Started, this, &UDefaultInputBinding::OnPrimaryPressed, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Primary, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnPrimaryRepeated, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Primary, ETriggerEvent::Completed, this, &UDefaultInputBinding::OnPrimaryReleased, false);
	
	InInputComponent->BindInputAction(GameplayTags::Input_Secondary, ETriggerEvent::Started, this, &UDefaultInputBinding::OnSecondaryPressed, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Secondary, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnSecondaryRepeated, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Secondary, ETriggerEvent::Completed, this, &UDefaultInputBinding::OnSecondaryReleased, false);
		
	InInputComponent->BindInputAction(GameplayTags::Input_Third, ETriggerEvent::Started, this, &UDefaultInputBinding::OnThirdPressed, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Third, ETriggerEvent::Triggered, this, &UDefaultInputBinding::OnThirdRepeated, false);
	InInputComponent->BindInputAction(GameplayTags::Input_Third, ETriggerEvent::Completed, this, &UDefaultInputBinding::OnThirdReleased, false);
}

void UDefaultInputBinding::OnTermination()
{
	Super::OnTermination();
}

void UDefaultInputBinding::SystemOperation_Implementation()
{
}

void UDefaultInputBinding::OnTurnCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	UCameraModule::Get().AddCameraRotationInput(InValue.Get<float>() / UCommonModuleStatics::GetTimeScale(), 0.f);
}

void UDefaultInputBinding::OnLookUpCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f || UCameraModule::Get().IsControllingMove()) return;

	UCameraModule::Get().AddCameraRotationInput(0.f, (UCameraModule::Get().IsReverseCameraPitch() ? -InValue.Get<float>() : InValue.Get<float>()) / UCommonModuleStatics::GetTimeScale());
}

void UDefaultInputBinding::OnPanHCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FRotator Rotation = FRotator(0.f, UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetControlRotation().Yaw, 0.f);
	const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (UCameraModule::Get().IsReverseCameraPanMove() ? -0.7f : 0.7f);
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() / UCommonModuleStatics::GetTimeScale());
}

void UDefaultInputBinding::OnPanVCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FRotator Rotation = FRotator(UCameraModule::Get().IsEnableCameraPanZMove() ? UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetControlRotation().Pitch : 0.f, UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetControlRotation().Yaw, 0.f);
	const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(UCameraModule::Get().IsEnableCameraPanZMove() ? EAxis::Z : EAxis::X) * (UCameraModule::Get().IsReverseCameraPanMove() ? -0.7f : 0.7f);
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() / UCommonModuleStatics::GetTimeScale());
}

void UDefaultInputBinding::OnZoomCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	UCameraModule::Get().AddCameraDistanceInput(-InValue.Get<float>() / UCommonModuleStatics::GetTimeScale());
}

void UDefaultInputBinding::OnMoveForwardCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = UCameraModule::Get().GetCurrentCameraRotation().Vector();
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() / UCommonModuleStatics::GetTimeScale());
}

void UDefaultInputBinding::OnMoveRightCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	const FVector Direction = FRotationMatrix(UCameraModule::Get().GetCurrentCameraRotation()).GetUnitAxis(EAxis::Y);
	UCameraModule::Get().AddCameraMovementInput(Direction, InValue.Get<float>() / UCommonModuleStatics::GetTimeScale());
}

void UDefaultInputBinding::OnMoveUpCamera_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	UCameraModule::Get().AddCameraMovementInput(FVector::UpVector, InValue.Get<float>() / UCommonModuleStatics::GetTimeScale());
}

void UDefaultInputBinding::OnTurnPlayer_Implementation(const FInputActionValue& InValue)
{
	if(InValue.Get<float>() == 0.f) return;

	if(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_Turn(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn(), InValue.Get<float>());
	}
}

void UDefaultInputBinding::OnMoveForwardPlayer_Implementation(const FInputActionValue& InValue)
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

void UDefaultInputBinding::OnActionForwardPlayer_Implementation(const FInputActionValue& InValue)
{
}

void UDefaultInputBinding::OnMoveRightPlayer_Implementation(const FInputActionValue& InValue)
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

void UDefaultInputBinding::OnActionRightPlayer_Implementation(const FInputActionValue& InValue)
{
}

void UDefaultInputBinding::OnMoveUpPlayer_Implementation(const FInputActionValue& InValue)
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

void UDefaultInputBinding::OnActionUpPlayer_Implementation(const FInputActionValue& InValue)
{
}

void UDefaultInputBinding::OnJumpPlayer_Implementation()
{
	if(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn() && UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn()->Implements<UWHPlayerInterface>())
	{
		IWHPlayerInterface::Execute_JumpN(UCommonModuleStatics::GetPlayerController(LocalPlayerIndex)->GetPawn());
	}
}
void UDefaultInputBinding::OnPrimaryPressed_Implementation()
{
}

void UDefaultInputBinding::OnPrimaryRepeated_Implementation()
{
}

void UDefaultInputBinding::OnPrimaryReleased_Implementation()
{
}

void UDefaultInputBinding::OnSecondaryPressed_Implementation()
{
}

void UDefaultInputBinding::OnSecondaryRepeated_Implementation()
{
}

void UDefaultInputBinding::OnSecondaryReleased_Implementation()
{
}

void UDefaultInputBinding::OnThirdPressed_Implementation()
{
}

void UDefaultInputBinding::OnThirdRepeated_Implementation()
{
}

void UDefaultInputBinding::OnThirdReleased_Implementation()
{
}
