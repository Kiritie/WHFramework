
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

void AWidgetModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();

	ClearAllUserWidget();
	ClearAllSlateWidget();
	ClearAllWorldWidget();
}

bool AWidgetModule::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass) const
{
	return HasUserWidgetClass<UUserWidgetBase>(InWidgetClass);
}

bool AWidgetModule::HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass) const
{
	return HasUserWidget<UUserWidgetBase>(InWidgetClass);
}

UUserWidgetBase* AWidgetModule::GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass) const
{
	return GetUserWidget<UUserWidgetBase>(InWidgetClass);
}

UUserWidgetBase* AWidgetModule::GetUserWidgetByName(FName InWidgetName, TSubclassOf<UUserWidgetBase> InWidgetClass) const
{
	if(AllUserWidgets.Contains(InWidgetName))
	{
		return AllUserWidgets[InWidgetName];
	}
	return nullptr;
}

UUserWidgetBase* AWidgetModule::CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner)
{
	return CreateUserWidget<UUserWidgetBase>(InOwner, InWidgetClass);
}

UUserWidgetBase* AWidgetModule::CreateUserWidgetByName(FName InWidgetName, AActor* InOwner)
{
	return CreateUserWidgetByName<UUserWidgetBase>(InWidgetName, InOwner);
}

bool AWidgetModule::InitializeUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner)
{
	return InitializeUserWidget<UUserWidgetBase>(InOwner, InWidgetClass);
}

bool AWidgetModule::OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, const TArray<FParameter>& InParams,  bool bInstant)
{
	return OpenUserWidget<UUserWidgetBase>(&InParams, bInstant, InWidgetClass);
}

bool AWidgetModule::OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>& InParams, bool bInstant)
{
	return OpenUserWidgetByName(InWidgetName, &InParams, bInstant);
}

bool AWidgetModule::CloseUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	return CloseUserWidget<UUserWidgetBase>(bInstant, InWidgetClass);
}

bool AWidgetModule::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	return ToggleUserWidget<UUserWidgetBase>(bInstant, InWidgetClass);
}

bool AWidgetModule::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass,  bool bRecovery)
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

void AWidgetModule::ClearAllUserWidget()
{
	for (auto Iter : AllUserWidgets)
	{
		if(Iter.Value)
		{
			if(Iter.Value->IsRooted())
			{
				Iter.Value->RemoveFromRoot();
			}
			Iter.Value->ConditionalBeginDestroy();
		}
	}
	AllUserWidgets.Empty();
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

void AWidgetModule::ClearAllSlateWidget()
{
	AllSlateWidgets.Empty();
}

bool AWidgetModule::HasWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const
{
	return HasWorldWidget<UWorldWidgetBase>(InWidgetIndex, InWidgetClass);
}

UWorldWidgetBase* AWidgetModule::GetWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const
{
	return GetWorldWidget<UWorldWidgetBase>(InWidgetIndex, InWidgetClass);
}

UWorldWidgetBase* AWidgetModule::GetWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const
{
	return GetWorldWidgetByName<UWorldWidgetBase>(InWidgetName, InWidgetIndex);
}

TArray<UWorldWidgetBase*> AWidgetModule::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass) const
{
	return GetWorldWidgets<UWorldWidgetBase>(InWidgetClass);
}

TArray<UWorldWidgetBase*> AWidgetModule::GetWorldWidgetsByName(FName InWidgetName) const
{
	return GetWorldWidgetsByName<UWorldWidgetBase>(InWidgetName);
}

UWorldWidgetBase* AWidgetModule::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	return CreateWorldWidget<UWorldWidgetBase>(InOwner, InLocation, InSceneComp, &InParams, InWidgetClass);
}

UWorldWidgetBase* AWidgetModule::CreateWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	return CreateWorldWidgetByName<UWorldWidgetBase>(InWidgetName, InOwner, InLocation, InSceneComp, &InParams, InWidgetClass);
}

bool AWidgetModule::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex, bool bRecovery)
{
	return DestroyWorldWidget<UWorldWidgetBase>(InWidgetIndex, bRecovery, InWidgetClass);
}

bool AWidgetModule::DestroyWorldWidgetByName(FName InWidgetName, int32 InWidgetIndex, bool bRecovery)
{
	if(AllWorldWidgets.Contains(InWidgetName) && AllWorldWidgets[InWidgetName].WorldWidgets.IsValidIndex(InWidgetIndex))
	{
		if(UWorldWidgetBase* WorldWidget = AllWorldWidgets[InWidgetName].WorldWidgets[InWidgetIndex])
		{
			AllWorldWidgets[InWidgetName].WorldWidgets.RemoveAt(InWidgetIndex);

			if(AllWorldWidgets[InWidgetName].WorldWidgets.Num() > 0)
			{
				for(int32 i = 0; i < AllWorldWidgets[InWidgetName].WorldWidgets.Num(); i++)
				{
					AllWorldWidgets[InWidgetName].WorldWidgets[i]->SetWidgetIndex(i);
				}
			}
			else
			{
				AllWorldWidgets.Remove(InWidgetName);
			}
			WorldWidget->OnDestroy(bRecovery);
		}
		return true;
	}
	return false;
}

void AWidgetModule::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass, bool bRecovery)
{
	return DestroyWorldWidgets<UWorldWidgetBase>(bRecovery, InWidgetClass);
}

void AWidgetModule::DestroyWorldWidgetsByName(FName InWidgetName, bool bRecovery)
{
	if(AllWorldWidgets.Contains(InWidgetName))
	{
		for(auto Iter : AllWorldWidgets[InWidgetName].WorldWidgets)
		{
			if(Iter)
			{
				Iter->OnDestroy(bRecovery);
			}
		}
		AllWorldWidgets.Remove(InWidgetName);
	}
}

void AWidgetModule::ClearAllWorldWidget()
{
	for(auto Iter1 : AllWorldWidgets)
	{
		for(auto Iter2 : Iter1.Value.WorldWidgets)
		{
			if(Iter2)
			{
				if(Iter2->IsRooted())
				{
					Iter2->RemoveFromRoot();
				}
				Iter2->ConditionalBeginDestroy();
			}
		}
		Iter1.Value.WorldWidgets.Empty();
	}
	AllWorldWidgets.Empty();
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
