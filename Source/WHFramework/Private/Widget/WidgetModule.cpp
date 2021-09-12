// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModule.h"

#include "Kismet/GameplayStatics.h"

AWidgetModule::AWidgetModule()
{

}

void AWidgetModule::BeginPlay()
{
	Super::BeginPlay();
}

void AWidgetModule::UpdateInputMode()
{
	EInputMode inputMode = EInputMode::GameOnly;
	for (auto iter : AllUserWidgets)
	{
		UUserWidgetBase* UserWidget = iter.Value;
		if (UserWidget && UserWidget->IsVisible() && (int32)UserWidget->GetInputMode() < (int32)inputMode)
		{
			inputMode = UserWidget->GetInputMode();
		}
	}
	SetInputMode(inputMode);
}

void AWidgetModule::SetInputMode(EInputMode InInputMode)
{
	if(APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if(InputMode != InInputMode)
		{
			InputMode = InInputMode;
			switch (InInputMode)
			{
				case EInputMode::None:
				{
					PlayerController->SetInputMode(FInputModeUIOnly());
					PlayerController->bShowMouseCursor = false;
					break;
				}
				case EInputMode::GameAndUI:
				{
					PlayerController->SetInputMode(FInputModeGameAndUI());
					PlayerController->bShowMouseCursor = true;
					break;
				}
				case EInputMode::GameOnly:
				{
					PlayerController->SetInputMode(FInputModeGameOnly());
					PlayerController->bShowMouseCursor = false;
					break;
				}
				case EInputMode::UIOnly:
				{
					PlayerController->SetInputMode(FInputModeUIOnly());
					PlayerController->bShowMouseCursor = true;
					break;
				}
			}
		}
	}
}
