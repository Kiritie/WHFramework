// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModule.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Kismet/GameplayStatics.h"
#include "Widget/WidgetModuleBPLibrary.h"

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

	for(auto Iter : WorldWidgetClasses)
	{
		if(Iter)
		{
			const FName WidgetName = Iter->GetDefaultObject<UWorldWidgetBase>()->GetWidgetName();
			if(!WorldWidgetClassMap.Contains(WidgetName))
			{
				WorldWidgetClassMap.Add(WidgetName, Iter);
			}
		}
	}

	WorldWidgetParent = CreateWidget(GetWorld(), WorldWidgetParentClass);
	if(WorldWidgetParent)
	{
		WorldWidgetParent->AddToViewport(-1);
		// if(CanvasPanelWorldWidgetParent->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	}
}

void AWidgetModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AWidgetModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	for (auto Iter : AllUserWidgets)
	{
		if(!Iter.Value) continue;
		UUserWidgetBase* UserWidget = Iter.Value;
		if (UserWidget && UserWidget->GetWidgetState() == EWidgetState::Opened && UserWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
		{
			UserWidget->Refresh();
		}
	}
	for (auto Iter : AllSlateWidgets)
	{
		if(!Iter.Value) continue;
		TSharedPtr<SSlateWidgetBase> SlateWidget = Iter.Value;
		if (SlateWidget && SlateWidget->GetWidgetState() == EWidgetState::Opened && SlateWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
		{
			SlateWidget->Refresh();
		}
	}
	for (auto Iter1 : AllWorldWidgets)
	{
		for (auto Iter2 : Iter1.Value.WorldWidgets)
		{
			UWorldWidgetBase* WorldWidget = Iter2;
			if (WorldWidget && WorldWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
			{
				WorldWidget->Refresh();
			}
		}
	}
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

UUserWidgetBase* AWidgetModule::K2_GetUserWidgetByName(FName InWidgetName, TSubclassOf<UUserWidgetBase> InWidgetClass) const
{
	if(AllUserWidgets.Contains(InWidgetName))
	{
		return AllUserWidgets[InWidgetName];
	}
	return nullptr;
}

UUserWidgetBase* AWidgetModule::K2_CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner)
{
	return CreateUserWidget<UUserWidgetBase>(InOwner, InWidgetClass);
}

UUserWidgetBase* AWidgetModule::K2_CreateUserWidgetByName(FName InWidgetName, AActor* InOwner)
{
	return CreateUserWidgetByName<UUserWidgetBase>(InWidgetName, InOwner);
}

bool AWidgetModule::K2_InitializeUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner)
{
	return InitializeUserWidget<UUserWidgetBase>(InOwner, InWidgetClass);
}

bool AWidgetModule::K2_OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, const TArray<FParameter>& InParams,  bool bInstant)
{
	return OpenUserWidget<UUserWidgetBase>(&InParams, bInstant, InWidgetClass);
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

bool AWidgetModule::K2_HasWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const
{
	return HasWorldWidget<UWorldWidgetBase>(InWidgetIndex, InWidgetClass);
}

UWorldWidgetBase* AWidgetModule::K2_GetWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const
{
	return GetWorldWidget<UWorldWidgetBase>(InWidgetIndex, InWidgetClass);
}

UWorldWidgetBase* AWidgetModule::K2_GetWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const
{
	return GetWorldWidgetByName<UWorldWidgetBase>(InWidgetName, InWidgetIndex);
}

TArray<UWorldWidgetBase*> AWidgetModule::K2_GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass) const
{
	return GetWorldWidgets<UWorldWidgetBase>(InWidgetClass);
}

TArray<UWorldWidgetBase*> AWidgetModule::K2_GetWorldWidgetsByName(FName InWidgetName) const
{
	return GetWorldWidgetsByName<UWorldWidgetBase>(InWidgetName);
}

UWorldWidgetBase* AWidgetModule::K2_CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	return CreateWorldWidget<UWorldWidgetBase>(InOwner, InLocation, InSceneComp, const_cast<TArray<FParameter>*>(&InParams), InWidgetClass);
}

bool AWidgetModule::K2_DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	return DestroyWorldWidget<UWorldWidgetBase>(InWidgetIndex, InWidgetClass);
}

void AWidgetModule::K2_DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass)
{
	return DestroyWorldWidgets<UWorldWidgetBase>(InWidgetClass);
}

EInputMode AWidgetModule::GetNativeInputMode() const
{
	EInputMode TmpInputMode = EInputMode::None;
    for (auto Iter : AllUserWidgets)
    {
    	if(!Iter.Value) continue;
        const UUserWidgetBase* UserWidget = Iter.Value;
    	if (UserWidget && UserWidget->GetWidgetState() == EWidgetState::Opened && (int32)UserWidget->GetInputMode() > (int32)TmpInputMode)
    	{
    		TmpInputMode = UserWidget->GetInputMode();
    	}
    }
	for (auto Iter : AllSlateWidgets)
	{
		if(!Iter.Value) continue;
		const TSharedPtr<SSlateWidgetBase> SlateWidget = Iter.Value;
		if (SlateWidget && SlateWidget->GetWidgetState() == EWidgetState::Opened && (int32)SlateWidget->GetInputMode() > (int32)TmpInputMode)
		{
			TmpInputMode = SlateWidget->GetInputMode();
		}
	}
	for (auto Iter1 : AllWorldWidgets)
	{
		for (auto Iter2 : Iter1.Value.WorldWidgets)
		{
			const UWorldWidgetBase* WorldWidget = Iter2;
			if (WorldWidget && (int32)WorldWidget->GetInputMode() > (int32)TmpInputMode)
			{
				TmpInputMode = WorldWidget->GetInputMode();
			}
		}
	}
	return TmpInputMode;
}
