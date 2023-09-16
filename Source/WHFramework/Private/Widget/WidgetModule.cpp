
// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/WidgetModule.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Kismet/GameplayStatics.h"
#include "Widget/WidgetModuleBPLibrary.h"
#include "Widget/World/WorldWidgetContainer.h"
		
IMPLEMENTATION_MODULE(AWidgetModule)

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

	static ConstructorHelpers::FClassFinder<UWorldWidgetContainer> WorldWidgetContainerClassFinder(TEXT("/WHFramework/Widget/Blueprints/WBP_WorldWidgetContainer.WBP_WorldWidgetContainer_C"));
	if(WorldWidgetContainerClassFinder.Succeeded())
	{
		WorldWidgetContainerClass = WorldWidgetContainerClassFinder.Class;
	}
	WorldWidgetContainerZOrder = -1;
	AllWorldWidgets = TMap<FName, FWorldWidgets>();
	WorldWidgetContainer = nullptr;
	WorldWidgetClassMap = TMap<FName, TSubclassOf<UWorldWidgetBase>>();
}

AWidgetModule::~AWidgetModule()
{
	TERMINATION_MODULE(AWidgetModule)
}

#if WITH_EDITOR
void AWidgetModule::OnGenerate()
{
	Super::OnGenerate();
}

void AWidgetModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AWidgetModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AWidgetModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
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
				if(DefaultObject->GetParentName() == NAME_None)
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

void AWidgetModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		ClearAllUserWidget();
		ClearAllSlateWidget();
		ClearAllWorldWidget();
	}
}

bool AWidgetModule::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass) const
{
	return HasUserWidgetClass<UUserWidgetBase>(InClass);
}

bool AWidgetModule::HasUserWidget(TSubclassOf<UUserWidgetBase> InClass) const
{
	return HasUserWidget<UUserWidgetBase>(InClass);
}

UUserWidgetBase* AWidgetModule::GetUserWidget(TSubclassOf<UUserWidgetBase> InClass) const
{
	return GetUserWidget<UUserWidgetBase>(InClass);
}

UUserWidgetBase* AWidgetModule::GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InClass) const
{
	if(AllUserWidgets.Contains(InName))
	{
		return AllUserWidgets[InName];
	}
	return nullptr;
}

UUserWidgetBase* AWidgetModule::CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner)
{
	return CreateUserWidget<UUserWidgetBase>(InOwner, InClass);
}

UUserWidgetBase* AWidgetModule::CreateUserWidgetByName(FName InName, UObject* InOwner)
{
	return CreateUserWidgetByName<UUserWidgetBase>(InName, InOwner);
}

bool AWidgetModule::OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams,  bool bInstant)
{
	return OpenUserWidget<UUserWidgetBase>(&InParams, bInstant, InClass);
}

bool AWidgetModule::OpenUserWidgetByName(FName InName, const TArray<FParameter>& InParams, bool bInstant)
{
	return OpenUserWidgetByName(InName, &InParams, bInstant);
}

bool AWidgetModule::CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return CloseUserWidget<UUserWidgetBase>(bInstant, InClass);
}

bool AWidgetModule::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return ToggleUserWidget<UUserWidgetBase>(bInstant, InClass);
}

bool AWidgetModule::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass,  bool bRecovery)
{
	return DestroyUserWidget<UUserWidgetBase>(bRecovery, InClass);
}

void AWidgetModule::CloseAllUserWidget(bool bInstant)
{
	for (auto Iter : AllUserWidgets)
	{
		if(Iter.Value)
		{
			Iter.Value->SetLastTemporary(nullptr);
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
			Iter.Value->OnDestroy();
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
			Iter.Value->SetLastTemporary(nullptr);
			Iter.Value->Close(bInstant);
		}
	}
}

void AWidgetModule::ClearAllSlateWidget()
{
	AllSlateWidgets.Empty();
}

bool AWidgetModule::HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const
{
	return HasWorldWidget<UWorldWidgetBase>(InIndex, InClass);
}

UWorldWidgetBase* AWidgetModule::GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const
{
	return GetWorldWidget<UWorldWidgetBase>(InIndex, InClass);
}

UWorldWidgetBase* AWidgetModule::GetWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const
{
	return GetWorldWidgetByName<UWorldWidgetBase>(InName, InIndex);
}

TArray<UWorldWidgetBase*> AWidgetModule::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass) const
{
	return GetWorldWidgets<UWorldWidgetBase>(InClass);
}

TArray<UWorldWidgetBase*> AWidgetModule::GetWorldWidgetsByName(FName InName) const
{
	return GetWorldWidgetsByName<UWorldWidgetBase>(InName);
}

UWorldWidgetBase* AWidgetModule::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	return CreateWorldWidget<UWorldWidgetBase>(InOwner, InLocation, InSceneComp, &InParams, InClass);
}

UWorldWidgetBase* AWidgetModule::CreateWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	return CreateWorldWidgetByName<UWorldWidgetBase>(InName, InOwner, InLocation, InSceneComp, &InParams, InClass);
}

bool AWidgetModule::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery)
{
	return DestroyWorldWidget<UWorldWidgetBase>(InIndex, bRecovery, InClass);
}

bool AWidgetModule::DestroyWorldWidgetByName(FName InName, int32 InIndex, bool bRecovery)
{
	if(AllWorldWidgets.Contains(InName) && AllWorldWidgets[InName].WorldWidgets.IsValidIndex(InIndex))
	{
		if(UWorldWidgetBase* WorldWidget = AllWorldWidgets[InName].WorldWidgets[InIndex])
		{
			AllWorldWidgets[InName].WorldWidgets.RemoveAt(InIndex);

			if(AllWorldWidgets[InName].WorldWidgets.Num() > 0)
			{
				for(int32 i = 0; i < AllWorldWidgets[InName].WorldWidgets.Num(); i++)
				{
					AllWorldWidgets[InName].WorldWidgets[i]->SetWidgetIndex(i);
				}
			}
			else
			{
				AllWorldWidgets.Remove(InName);
			}
			WorldWidget->OnDestroy(bRecovery);
		}
		return true;
	}
	return false;
}

void AWidgetModule::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery)
{
	return DestroyWorldWidgets<UWorldWidgetBase>(bRecovery, InClass);
}

void AWidgetModule::DestroyWorldWidgetsByName(FName InName, bool bRecovery)
{
	if(AllWorldWidgets.Contains(InName))
	{
		for(auto Iter : AllWorldWidgets[InName].WorldWidgets)
		{
			if(Iter)
			{
				Iter->OnDestroy(bRecovery);
			}
		}
		AllWorldWidgets.Remove(InName);
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
				UObjectPoolModuleBPLibrary::DespawnObject(Iter2, false);
			}
		}
		Iter1.Value.WorldWidgets.Empty();
	}
	AllWorldWidgets.Empty();
}

EInputMode AWidgetModule::GetNativeInputMode() const
{
	EInputMode TmpInputMode = EInputMode::None;
    for (const auto& Iter : AllUserWidgets)
    {
    	if(!Iter.Value) continue;
        const UUserWidgetBase* UserWidget = Iter.Value;
    	if (UserWidget && (UserWidget->GetWidgetState() == EScreenWidgetState::Opening || UserWidget->GetWidgetState() == EScreenWidgetState::Opened) && (int32)UserWidget->GetWidgetInputMode() > (int32)TmpInputMode)
    	{
    		TmpInputMode = UserWidget->GetWidgetInputMode();
    	}
    }
	for (const auto& Iter : AllSlateWidgets)
	{
		if(!Iter.Value) continue;
		const TSharedPtr<SSlateWidgetBase> SlateWidget = Iter.Value;
		if (SlateWidget && (SlateWidget->GetWidgetState() == EScreenWidgetState::Opening || SlateWidget->GetWidgetState() == EScreenWidgetState::Opened) && (int32)SlateWidget->GetWidgetInputMode() > (int32)TmpInputMode)
		{
			TmpInputMode = SlateWidget->GetWidgetInputMode();
		}
	}
	for (const auto& Iter1 : AllWorldWidgets)
	{
		for (const auto& Iter2 : Iter1.Value.WorldWidgets)
		{
			const UWorldWidgetBase* WorldWidget = Iter2;
			if (WorldWidget && (int32)WorldWidget->GetWidgetInputMode() > (int32)TmpInputMode)
			{
				TmpInputMode = WorldWidget->GetWidgetInputMode();
			}
		}
	}
	return TmpInputMode;
}
