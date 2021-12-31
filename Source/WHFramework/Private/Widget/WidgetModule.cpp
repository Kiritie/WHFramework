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

	for(auto Iter : UserWidgetClasses)
	{
		if(Iter)
		{
			const FName WidgetName = Iter->GetDefaultObject<UUserWidgetBase>()->GetWidgetName();
			if(!UserWidgetClassMap.Contains(WidgetName))
			{
				UserWidgetClassMap.Add(WidgetName, Iter);
			}
		}
	}
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

bool AWidgetModule::K2_HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass) const
{
	return HasUserWidget<UUserWidgetBase>(InWidgetClass);
}

UUserWidgetBase* AWidgetModule::K2_GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass) const
{
	return GetUserWidget<UUserWidgetBase>(InWidgetClass);
}

UUserWidgetBase* AWidgetModule::K2_CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	return CreateUserWidget<UUserWidgetBase>(InWidgetClass);
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
		}
	}
}

void AWidgetModule::CloseAllUserWidget(EWidgetType InWidgetType, bool bInstant)
{
	for (auto Iter : AllUserWidgets)
	{
		if(!Iter.Value) continue;
		if (InWidgetType == EWidgetType::None || Iter.Value->GetWidgetType() == InWidgetType)
		{
			if (Iter.Value->GetWidgetType() == EWidgetType::Temporary)
			{
				TemporaryUserWidget = nullptr;
			}
			Iter.Value->OnClose(bInstant);
		}
	}
}

void AWidgetModule::OpenAllSlateWidget(EWidgetType InWidgetType, bool bInstant)
{
	for (auto Iter : AllSlateWidgets)
	{
		if(!Iter.Value) continue;
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
		}
	}
}

void AWidgetModule::CloseAllSlateWidget(EWidgetType InWidgetType, bool bInstant)
{
	for (auto Iter : AllSlateWidgets)
	{
		if(!Iter.Value) continue;
		if (InWidgetType == EWidgetType::None || Iter.Value->GetWidgetType() == InWidgetType)
		{
			Iter.Value->SetLastWidget(nullptr);
			Iter.Value->OnClose(bInstant);
		}
		TemporarySlateWidget = nullptr;
	}
}

void AWidgetModule::UpdateInputMode()
{
	EInputMode TempInputMode = EInputMode::GameOnly;
	for (auto Iter : AllUserWidgets)
	{
		if(!Iter.Value) continue;
		UUserWidgetBase* UserWidget = Iter.Value;
		if (UserWidget && UserWidget->IsOpened() && (int32)UserWidget->GetInputMode() < (int32)TempInputMode)
		{
			TempInputMode = UserWidget->GetInputMode();
		}
	}
	SetInputMode(TempInputMode);
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
			OnChangeInputMode.Broadcast(InputMode);
		}
	}
}
