// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModule.h"

#include "Kismet/GameplayStatics.h"

// Sets default values
AWidgetModule::AWidgetModule()
{
	ModuleName = FName("WidgetModule");
}

#if WITH_EDITOR
void AWidgetModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AWidgetModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AWidgetModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AWidgetModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AWidgetModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AWidgetModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

UUserWidgetBase* AWidgetModule::K2_CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	return CreateUserWidget<UUserWidgetBase>(InWidgetClass);
}

UUserWidgetBase* AWidgetModule::K2_GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	return GetUserWidget<UUserWidgetBase>(InWidgetClass);
}

bool AWidgetModule::K2_InitializeUserWidget(AActor* InOwner, TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	return InitializeUserWidget<UUserWidgetBase>(InOwner, InWidgetClass);
}

bool AWidgetModule::K2_OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	return OpenUserWidget<UUserWidgetBase>(bInstant, InWidgetClass);
}

bool AWidgetModule::K2_CloseUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	return CloseUserWidget<UUserWidgetBase>(bInstant, InWidgetClass);
}

bool AWidgetModule::K2_ToggleUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	return ToggleUserWidget<UUserWidgetBase>(bInstant, InWidgetClass);
}

bool AWidgetModule::K2_DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	return DestroyUserWidget<UUserWidgetBase>(InWidgetClass);
}

void AWidgetModule::OpenAllUserWidget(EWidgetType InWidgetType, bool bInstant)
{
	for (auto Iter : AllUserWidgets)
	{
		if (InWidgetType == EWidgetType::None || Iter.Value->GetWidgetType() == InWidgetType)
		{
			if (Iter.Value->GetWidgetType() == EWidgetType::Temporary)
			{
				if (TemporaryUserWidget)
				{
					TemporaryUserWidget->OnClose(bInstant);
				}
				TemporaryUserWidget = Iter.Value;
			}
			Iter.Value->OnOpen(bInstant);
			UpdateInputMode();
		}
	}
}

void AWidgetModule::CloseAllUserWidget(EWidgetType InWidgetType, bool bInstant)
{
	for (auto Iter : AllUserWidgets)
	{
		if (InWidgetType == EWidgetType::None || Iter.Value->GetWidgetType() == InWidgetType)
		{
			if (Iter.Value->GetWidgetType() == EWidgetType::Temporary)
			{
				TemporaryUserWidget = nullptr;
			}
			Iter.Value->OnClose(bInstant);
			UpdateInputMode();
		}
	}
}

void AWidgetModule::OpenAllSlateWidget(EWidgetType InWidgetType, bool bInstant)
{
	for (auto Iter : AllSlateWidgets)
	{
		if (InWidgetType == EWidgetType::None || Iter.Value->GetWidgetType() == InWidgetType)
		{
			if (Iter.Value->GetWidgetType() == EWidgetType::Temporary)
			{
				if (TemporarySlateWidget)
				{
					TemporarySlateWidget->OnClose(bInstant);
				}
				TemporarySlateWidget = Iter.Value;
			}
			Iter.Value->OnOpen(bInstant);
			UpdateInputMode();
		}
	}
}

void AWidgetModule::CloseAllSlateWidget(EWidgetType InWidgetType, bool bInstant)
{
	for (auto Iter : AllSlateWidgets)
	{
		if (InWidgetType == EWidgetType::None || Iter.Value->GetWidgetType() == InWidgetType)
		{
			if (Iter.Value->GetWidgetType() == EWidgetType::Temporary)
			{
				TemporarySlateWidget = nullptr;
			}
			Iter.Value->OnClose(bInstant);
			UpdateInputMode();
		}
	}
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
