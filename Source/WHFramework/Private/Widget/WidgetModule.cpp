
// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/WidgetModule.h"

#include "WHFrameworkCoreStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Widget/EventHandle_CloseUserWidget.h"
#include "Event/Handle/Widget/EventHandle_OpenUserWidget.h"
#include "Event/Handle/Widget/EventHandle_SetWorldWidgetVisible.h"
#include "Input/InputManager.h"
#include "Slate/SlateWidgetManager.h"
#include "Widget/World/WorldWidgetContainer.h"
		
IMPLEMENTATION_MODULE(UWidgetModule)

// Sets default values
UWidgetModule::UWidgetModule()
{
	ModuleName = FName("WidgetModule");
	ModuleDisplayName = FText::FromString(TEXT("Widget Module"));

	bModuleRequired = true;

	UserWidgetClasses = TArray<TSubclassOf<UUserWidgetBase>>();
	AllUserWidget = TMap<FName, UUserWidgetBase*>();
	TemporaryUserWidget = nullptr;
	UserWidgetClassMap = TMap<FName, TSubclassOf<UUserWidgetBase>>();

	WorldWidgetClasses = TArray<TSubclassOf<UWorldWidgetBase>>();

	static ConstructorHelpers::FClassFinder<UWorldWidgetContainer> WorldWidgetContainerClassFinder(TEXT("/WHFramework/Widget/Blueprints/WBP_WorldWidgetContainer_Base.WBP_WorldWidgetContainer_Base_C"));
	if(WorldWidgetContainerClassFinder.Succeeded())
	{
		WorldWidgetContainerClass = WorldWidgetContainerClassFinder.Class;
	}
	WorldWidgetContainerZOrder = -1;
	AllWorldWidget = TMap<FName, FWorldWidgets>();
	WorldWidgetContainer = nullptr;
	WorldWidgetClassMap = TMap<FName, TSubclassOf<UWorldWidgetBase>>();
}

UWidgetModule::~UWidgetModule()
{
	TERMINATION_MODULE(UWidgetModule)
}

#if WITH_EDITOR
void UWidgetModule::OnGenerate()
{
	Super::OnGenerate();
}

void UWidgetModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UWidgetModule)
}
#endif

void UWidgetModule::OnInitialize()
{
	Super::OnInitialize();

	FInputManager::Get().AddInputManager(this);

	UEventModuleStatics::SubscribeEvent<UEventHandle_OpenUserWidget>(this, FName("OnOpenUserWidget"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_CloseUserWidget>(this, FName("OnCloseUserWidget"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_SetWorldWidgetVisible>(this, FName("OnSetWorldWidgetVisible"));

	for(auto& Iter : UserWidgetClasses)
	{
		if(!Iter) continue;
		const FName WidgetName = Iter->GetDefaultObject<UUserWidgetBase>()->GetWidgetName();
		if(!UserWidgetClassMap.Contains(WidgetName))
		{
			UserWidgetClassMap.Add(WidgetName, Iter);
		}
	}

	for(auto& Iter : WorldWidgetClasses)
	{
		if(!Iter) continue;
		const FName WidgetName = Iter->GetDefaultObject<UWorldWidgetBase>()->GetWidgetName();
		if(!WorldWidgetClassMap.Contains(WidgetName))
		{
			WorldWidgetClassMap.Add(WidgetName, Iter);
		}
	}

	WorldWidgetContainer = CreateWidget<UWorldWidgetContainer>(GetWorld(), WorldWidgetContainerClass);
	if(WorldWidgetContainer)
	{
		WorldWidgetContainer->AddToViewport(WorldWidgetContainerZOrder);
	}
}

void UWidgetModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		for(const auto& Iter : UserWidgetClasses)
		{
			if(!Iter) continue;
			const UUserWidgetBase* DefaultObject = Iter->GetDefaultObject<UUserWidgetBase>();
			if(DefaultObject->GetParentName() == NAME_None)
			{
				switch(DefaultObject->GetWidgetCreateType())
				{
					case EWidgetCreateType::AutoCreate:
					{
						CreateUserWidget<UUserWidgetBase>(nullptr, nullptr, false, Iter);
						break;
					}
					case EWidgetCreateType::AutoCreateAndOpen:
					{
						OpenUserWidget<UUserWidgetBase>(nullptr, false, false, Iter);
						break;
					}
					default: break;
				}
			}
		}
	}
}

void UWidgetModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(IsModuleInEditor()) return;

	TArray<UUserWidget*> TickAbleWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsWithInterface(this, TickAbleWidgets, UTickAbleWidgetInterface::StaticClass(), false);
	for (auto Iter : TickAbleWidgets)
	{
		if(ITickAbleWidgetInterface::Execute_IsTickAble(Iter))
		{
			ITickAbleWidgetInterface::Execute_OnTick(Iter, DeltaSeconds);
		}
	}
	
	for (auto& Iter : AllUserWidget)
	{
		UUserWidgetBase* UserWidget = Iter.Value;
		if (UserWidget && UserWidget->GetWidgetState() == EScreenWidgetState::Opened && UserWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
		{
			UserWidget->Refresh();
		}
	}
	
	for (auto& Iter1 : AllWorldWidget)
	{
		for (auto Iter2 : Iter1.Value.WorldWidgets)
		{
			UWorldWidgetBase* WorldWidget = Iter2;
			if (WorldWidget)
			{
				WorldWidget->RefreshLocationAndVisibility();
				if(WorldWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
				{
					WorldWidget->Refresh();
				}
			}
		}
	}
}

void UWidgetModule::OnPause()
{
	Super::OnPause();
}

void UWidgetModule::OnUnPause()
{
	Super::OnUnPause();
}

void UWidgetModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		ClearAllUserWidget();
		ClearAllWorldWidget();
	}
	if(PHASEC(InPhase, EPhase::Final))
	{
		FInputManager::Get().RemoveInputManager(this);
	}
}

FString UWidgetModule::GetModuleDebugMessage()
{
	return Super::GetModuleDebugMessage();
}

void UWidgetModule::OnOpenUserWidget(UObject* InSender, UEventHandle_OpenUserWidget* InEventHandle)
{
	if(!InEventHandle->WidgetName.IsNone())
	{
		OpenUserWidgetByName(InEventHandle->WidgetName, InEventHandle->WidgetParams, InEventHandle->bInstant, InEventHandle->bForce);
	}
}

void UWidgetModule::OnCloseUserWidget(UObject* InSender, UEventHandle_CloseUserWidget* InEventHandle)
{
	if(!InEventHandle->WidgetName.IsNone())
	{
		CloseUserWidgetByName(InEventHandle->WidgetName, InEventHandle->bInstant);
	}
}

void UWidgetModule::SortUserWidgetClasses()
{
	FCoreStatics::SortStringElementArray<TSubclassOf<UUserWidgetBase>>(UserWidgetClasses,
		[](const TSubclassOf<UUserWidgetBase>& InClass)
		{
			return InClass->GetName();
		});
	Modify();
}

bool UWidgetModule::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass) const
{
	return HasUserWidgetClass<UUserWidgetBase>(InClass);
}

void UWidgetModule::AddUserWidgetClassMapping(FName InName, TSubclassOf<UUserWidgetBase> InClass)
{
	AddUserWidgetClassMapping<UUserWidgetBase>(InName, InClass);
}

bool UWidgetModule::HasUserWidget(TSubclassOf<UUserWidgetBase> InClass) const
{
	return HasUserWidget<UUserWidgetBase>(InClass);
}

UUserWidgetBase* UWidgetModule::GetUserWidget(TSubclassOf<UUserWidgetBase> InClass) const
{
	return GetUserWidget<UUserWidgetBase>(InClass);
}

UUserWidgetBase* UWidgetModule::GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InClass) const
{
	if(AllUserWidget.Contains(InName))
	{
		return AllUserWidget[InName];
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModule::CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce)
{
	return CreateUserWidget<UUserWidgetBase>(InOwner, &InParams, bForce, InClass);
}

UUserWidgetBase* UWidgetModule::CreateUserWidgetByName(FName InName, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce)
{
	return CreateUserWidgetByName<UUserWidgetBase>(InName, InOwner, &InParams, bForce);
}

bool UWidgetModule::OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams,  bool bInstant, bool bForce)
{
	return OpenUserWidget<UUserWidgetBase>(&InParams, bInstant, bForce, InClass);
}

bool UWidgetModule::OpenUserWidgetByName(FName InName, const TArray<FParameter>& InParams, bool bInstant, bool bForce)
{
	return OpenUserWidgetByName(InName, &InParams, bInstant, bForce);
}

bool UWidgetModule::CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return CloseUserWidget<UUserWidgetBase>(bInstant, InClass);
}

bool UWidgetModule::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return ToggleUserWidget<UUserWidgetBase>(bInstant, InClass);
}

bool UWidgetModule::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass,  bool bRecovery)
{
	return DestroyUserWidget<UUserWidgetBase>(bRecovery, InClass);
}

void UWidgetModule::CloseAllUserWidget(bool bInstant)
{
	for (auto Iter : AllUserWidget)
	{
		if(Iter.Value)
		{
			Iter.Value->SetLastTemporary(nullptr);
			Iter.Value->Close(bInstant);
		}
	}
}

void UWidgetModule::ClearAllUserWidget(bool bRecovery)
{
	for (auto Iter : AllUserWidget)
	{
		if(Iter.Value)
		{
			Iter.Value->OnDestroy(bRecovery);
		}
	}
	AllUserWidget.Empty();
}

void UWidgetModule::SortWorldWidgetClasses()
{
	FCoreStatics::SortStringElementArray<TSubclassOf<UWorldWidgetBase>>(WorldWidgetClasses,
		[](const TSubclassOf<UWorldWidgetBase>& InClass)
		{
			return InClass->GetName();
		});
	Modify();
}

void UWidgetModule::OnSetWorldWidgetVisible(UObject* InSender, UEventHandle_SetWorldWidgetVisible* InEventHandle)
{
	SetWorldWidgetVisible(InEventHandle->bVisible, InEventHandle->WidgetClass);
}

bool UWidgetModule::GetWorldWidgetVisible(TSubclassOf<UWorldWidgetBase> InClass)
{
	if(InClass)
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		if(AllWorldWidget.Contains(WidgetName))
		{
			return AllWorldWidget[WidgetName].bVisible;
		}
	}
	else
	{
		return WorldWidgetContainer->GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
	}
	return false;
}

void UWidgetModule::SetWorldWidgetVisible(bool bVisible, TSubclassOf<UWorldWidgetBase> InClass)
{
	if(InClass)
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		if(AllWorldWidget.Contains(WidgetName))
		{
			AllWorldWidget[WidgetName].bVisible = bVisible;
		}
	}
	else
	{
		WorldWidgetContainer->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UWidgetModule::HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const
{
	return HasWorldWidget<UWorldWidgetBase>(InIndex, InClass);
}

UWorldWidgetBase* UWidgetModule::GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const
{
	return GetWorldWidget<UWorldWidgetBase>(InIndex, InClass);
}

UWorldWidgetBase* UWidgetModule::GetWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const
{
	return GetWorldWidgetByName<UWorldWidgetBase>(InName, InIndex);
}

TArray<UWorldWidgetBase*> UWidgetModule::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass) const
{
	return GetWorldWidgets<UWorldWidgetBase>(InClass);
}

TArray<UWorldWidgetBase*> UWidgetModule::GetWorldWidgetsByName(FName InName) const
{
	return GetWorldWidgetsByName<UWorldWidgetBase>(InName);
}

UWorldWidgetBase* UWidgetModule::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams)
{
	return CreateWorldWidget<UWorldWidgetBase>(InOwner, InMapping, &InParams, InClass);
}

UWorldWidgetBase* UWidgetModule::CreateWorldWidgetByName(FName InName, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams)
{
	return CreateWorldWidgetByName<UWorldWidgetBase>(InName, InOwner, InMapping, &InParams);
}

bool UWidgetModule::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery)
{
	return DestroyWorldWidget<UWorldWidgetBase>(InIndex, bRecovery, InClass);
}

bool UWidgetModule::DestroyWorldWidgetByName(FName InName, int32 InIndex, bool bRecovery)
{
	if(AllWorldWidget.Contains(InName) && AllWorldWidget[InName].WorldWidgets.IsValidIndex(InIndex))
	{
		if(UWorldWidgetBase* WorldWidget = AllWorldWidget[InName].WorldWidgets[InIndex])
		{
			AllWorldWidget[InName].WorldWidgets.RemoveAt(InIndex);

			if(AllWorldWidget[InName].WorldWidgets.Num() > 0)
			{
				for(int32 i = 0; i < AllWorldWidget[InName].WorldWidgets.Num(); i++)
				{
					AllWorldWidget[InName].WorldWidgets[i]->WidgetIndex = i;
				}
			}
			else
			{
				AllWorldWidget.Remove(InName);
			}
			WorldWidget->OnDestroy(bRecovery);
		}
		return true;
	}
	return false;
}

void UWidgetModule::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery)
{
	return DestroyWorldWidgets<UWorldWidgetBase>(bRecovery, InClass);
}

void UWidgetModule::DestroyWorldWidgetsByName(FName InName, bool bRecovery)
{
	if(AllWorldWidget.Contains(InName))
	{
		for(auto Iter : AllWorldWidget[InName].WorldWidgets)
		{
			if(Iter)
			{
				Iter->OnDestroy(bRecovery);
			}
		}
		AllWorldWidget.Remove(InName);
	}
}

void UWidgetModule::ClearAllWorldWidget(bool bRecovery)
{
	for(auto Iter1 : AllWorldWidget)
	{
		for(auto Iter2 : Iter1.Value.WorldWidgets)
		{
			if(Iter2)
			{
				Iter2->OnDestroy(bRecovery);
			}
		}
	}
	AllWorldWidget.Empty();
}

EInputMode UWidgetModule::GetNativeInputMode() const
{
	EInputMode InputMode = FSlateWidgetManager::Get().GetNativeInputMode();
    for (const auto& Iter : AllUserWidget)
    {
    	if (Iter.Value && (Iter.Value->GetWidgetState(true) == EScreenWidgetState::Opening || Iter.Value->GetWidgetState(true) == EScreenWidgetState::Opened) && (int32)Iter.Value->GetWidgetInputMode() > (int32)InputMode)
    	{
    		InputMode = Iter.Value->GetWidgetInputMode();
    	}
    }
	for (const auto& Iter1 : AllWorldWidget)
	{
		for (const auto& Iter2 : Iter1.Value.WorldWidgets)
		{
			if (Iter2 && (int32)Iter2->GetWidgetInputMode() > (int32)InputMode)
			{
				InputMode = Iter2->GetWidgetInputMode();
			}
		}
	}
	return InputMode;
}
