
// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/WidgetModule.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Kismet/GameplayStatics.h"
#include "Widget/WidgetModuleBPLibrary.h"
#include "Widget/World/WorldWidgetContainer.h"

// Sets default values
AWidgetModule::AWidgetModule()
{
	ModuleName = FName("WidgetModule");

	UserWidgetClasses = TArray<TSubclassOf<UUserWidgetBase>>();
	AllUserWidgets = TMap<FName, UUserWidgetBase*>();
	TemporaryUserWidget = nullptr;
	UserWidgetClassMap = TMap<FName, TSubclassOf<UUserWidgetBase>>();

	AllSlateWidgets = TMap<FName, TSharedPtr<SSlateWidgetBase>>();
	TemporarySlateWidget = nullptr;

	WorldWidgetClasses = TArray<TSubclassOf<UWorldWidgetBase>>();

	WorldWidgetContainerClass = LoadClass<UWorldWidgetContainer>(nullptr, TEXT("WidgetBlueprint'/WHFramework/Widget/Blueprints/WBP_WorldWidgetContainer.WBP_WorldWidgetContainer_C'"));
	WorldWidgetContainerZOrder = -1;
	AllWorldWidgets = TMap<FName, FWorldWidgets>();
	WorldWidgetContainer = nullptr;
	WorldWidgetClassMap = TMap<FName, TSubclassOf<UWorldWidgetBase>>();
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
			const UUserWidgetBase* DefaultObject = Iter->GetDefaultObject<UUserWidgetBase>();
			if(DefaultObject->GetWidgetType() != EWidgetType::Child)
			{
				switch(DefaultObject->GetWidgetCreateType())
				{
					case EWidgetCreateType::AutoCreate:
					{
						CreateUserWidget<UUserWidgetBase>(nullptr, Iter);
						break;
					}
					case EWidgetCreateType::AutoCreateAndOpen:
					{
						OpenUserWidget<UUserWidgetBase>(nullptr, false, Iter);
						break;
					}
					default: break;
				}
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

	WorldWidgetContainer = CreateWidget<UWorldWidgetContainer>(GetWorld(), WorldWidgetContainerClass);
	if(WorldWidgetContainer)
	{
		WorldWidgetContainer->AddToViewport(WorldWidgetContainerZOrder);
	}
}

void AWidgetModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AWidgetModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	TArray<UUserWidget*> InterfaceWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsWithInterface(this, InterfaceWidgets, UTickAbleWidgetInterface::StaticClass(), false);
	for (auto Iter : InterfaceWidgets)
	{
		if(ITickAbleWidgetInterface* InterfaceWidget = Cast<ITickAbleWidgetInterface>(Iter))
		{
			if(InterfaceWidget->Execute_IsTickAble(Iter))
			{
				InterfaceWidget->Execute_OnTick(Iter, DeltaSeconds);
			}
		}
	}

	for (auto Iter : AllUserWidgets)
	{
		if(!Iter.Value) continue;
		UUserWidgetBase* UserWidget = Iter.Value;
		if (UserWidget && UserWidget->GetWidgetState() == EScreenWidgetState::Opened && UserWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
		{
			UserWidget->Refresh();
		}
	}
	for (auto Iter : AllSlateWidgets)
	{
		if(!Iter.Value) continue;
		TSharedPtr<SSlateWidgetBase> SlateWidget = Iter.Value;
		if (SlateWidget && SlateWidget->GetWidgetState() == EScreenWidgetState::Opened && SlateWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
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

bool AWidgetModule::K2_HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass) const
{
	return HasUserWidgetClass<UUserWidgetBase>(InWidgetClass);
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

bool AWidgetModule::K2_OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>& InParams, bool bInstant)
{
	return OpenUserWidgetByName(InWidgetName, &InParams, bInstant);
}

bool AWidgetModule::K2_CloseUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	return CloseUserWidget<UUserWidgetBase>(bInstant, InWidgetClass);
}

bool AWidgetModule::K2_ToggleUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	return ToggleUserWidget<UUserWidgetBase>(bInstant, InWidgetClass);
}

bool AWidgetModule::K2_DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass,  bool bRecovery)
{
	return DestroyUserWidget<UUserWidgetBase>(bRecovery, InWidgetClass);
}

void AWidgetModule::CloseAllUserWidget(bool bInstant)
{
	for (auto Iter : AllUserWidgets)
	{
		if(Iter.Value)
		{
			Iter.Value->SetLastWidget(nullptr);
			Iter.Value->Close(bInstant);
		}
	}
}

void AWidgetModule::CloseAllSlateWidget(bool bInstant)
{
	for (auto Iter : AllSlateWidgets)
	{
		if(Iter.Value)
		{
			Iter.Value->SetLastWidget(nullptr);
			Iter.Value->Close(bInstant);
		}
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
	return CreateWorldWidget<UWorldWidgetBase>(InOwner, InLocation, InSceneComp, &InParams, InWidgetClass);
}

UWorldWidgetBase* AWidgetModule::K2_CreateWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	return CreateWorldWidgetByName<UWorldWidgetBase>(InWidgetName, InOwner, InLocation, InSceneComp, &InParams, InWidgetClass);
}

bool AWidgetModule::K2_DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex, bool bRecovery)
{
	return DestroyWorldWidget<UWorldWidgetBase>(InWidgetIndex, bRecovery, InWidgetClass);
}

void AWidgetModule::K2_DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass, bool bRecovery)
{
	return DestroyWorldWidgets<UWorldWidgetBase>(bRecovery, InWidgetClass);
}

EInputMode AWidgetModule::GetNativeInputMode() const
{
	EInputMode TmpInputMode = EInputMode::None;
    for (auto Iter : AllUserWidgets)
    {
    	if(!Iter.Value) continue;
        const UUserWidgetBase* UserWidget = Iter.Value;
    	if (UserWidget && UserWidget->GetWidgetState() == EScreenWidgetState::Opened && (int32)UserWidget->GetInputMode() > (int32)TmpInputMode)
    	{
    		TmpInputMode = UserWidget->GetInputMode();
    	}
    }
	for (auto Iter : AllSlateWidgets)
	{
		if(!Iter.Value) continue;
		const TSharedPtr<SSlateWidgetBase> SlateWidget = Iter.Value;
		if (SlateWidget && SlateWidget->GetWidgetState() == EScreenWidgetState::Opened && (int32)SlateWidget->GetInputMode() > (int32)TmpInputMode)
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
