
// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/WidgetModule.h"

#include "WHFrameworkCoreStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Common/CommonModuleStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Widget/Event_CloseUserWidget.h"
#include "Event/Events/Widget/Event_OpenUserWidget.h"
#include "Event/Events/Widget/Event_SetWorldWidgetVisible.h"
#include "SaveGame/Module/WidgetSaveGame.h"
#include "Widget/World/WorldWidgetContainer.h"
#include "Widget/Screen/WidgetMountSlot.h"
		
IMPLEMENTATION_MODULE(UWidgetModule)

// Sets default values
UWidgetModule::UWidgetModule()
{
	ModuleName = FName("WidgetModule");
	ModuleDisplayName = FText::FromString(TEXT("Widget Module"));

	ModuleSaveGame = UWidgetSaveGame::StaticClass();

	bModuleRequired = true;

	LanguageTypes = TArray<FLanguageType>();
	LanguageType = 0;
	GlobalScale = 1.f;


	WorldWidgetConfigs = TArray<FWorldWidgetConfig>();

	static ConstructorHelpers::FClassFinder<UWorldWidgetContainer> WorldWidgetContainerClassFinder(TEXT("/WHFramework/Widget/Blueprints/WBP_WorldWidgetContainer_Base.WBP_WorldWidgetContainer_Base_C"));
	if(WorldWidgetContainerClassFinder.Succeeded())
	{
		WorldWidgetContainerClass = WorldWidgetContainerClassFinder.Class;
	}
	WorldWidgetContainerZOrder = -1;
	WorldWidgetTraceType = UEngineTypes::ConvertToTraceType(ECC_Visibility);
	WorldWidgetContainer = nullptr;
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

	UEventModuleStatics::SubscribeEvent<FEventOpenUserWidget>(this, &ThisClass::OnOpenUserWidget);
	UEventModuleStatics::SubscribeEvent<FEventCloseUserWidget>(this, &ThisClass::OnCloseUserWidget);
	UEventModuleStatics::SubscribeEvent<FEventSetWorldWidgetVisible>(this, &ThisClass::OnSetWorldWidgetVisible);

	BuildRuntimeCaches();

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
		SetLanguageType(LanguageType);
		SetGlobalScale(GlobalScale);
		for(const FScreenWidgetConfig& Config : UserWidgetConfigs)
		{
			if(Config.ParentWidgetTag.IsValid())
			{
				continue;
			}

			const FGameplayTag WidgetTag = Config.ResolveWidgetTag();
			switch(Config.CreateType)
			{
				case EWidgetCreateType::AutoCreate:
				{
					CreateUserWidgetByTag(WidgetTag);
					break;
				}
				case EWidgetCreateType::AutoCreateAndOpen:
				{
					OpenUserWidgetByTag(WidgetTag, FParameter());
					break;
				}
				default:
					break;
			}
		}
	}
}

void UWidgetModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(bInEditor)
	{
		return;
	}

	for(auto Iter = TickableWidgets.CreateIterator(); Iter; ++Iter)
	{
		UUserWidget* Widget = *Iter;
		if(!::IsValid(Widget))
		{
			Iter.RemoveCurrent();
			continue;
		}
		if((Widget->IsInViewport() || Widget->GetParent())
			&& ITickAbleWidgetInterface::Execute_IsTickAble(Widget))
		{
			ITickAbleWidgetInterface::Execute_OnTick(Widget, DeltaSeconds);
		}
	}

	for(const auto& Iter : UserWidgetByTag)
	{
		UUserWidgetBase* UserWidget = Iter.Value;
		if(UserWidget
			&& UserWidget->GetWidgetState() == EScreenWidgetState::Opened
			&& UserWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
		{
			UserWidget->Refresh();
		}
	}

	for(const auto& Iter : WorldWidgetByTag)
	{
		for(UWorldWidgetBase* WorldWidget : Iter.Value.WorldWidgets)
		{
			if(WorldWidget)
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

void UWidgetModule::RegisterTickableWidget(UUserWidget* InWidget)
{
	if(InWidget && InWidget->Implements<UTickAbleWidgetInterface>())
	{
		TickableWidgets.Add(InWidget);
	}
}

void UWidgetModule::UnregisterTickableWidget(UUserWidget* InWidget)
{
	TickableWidgets.Remove(InWidget);
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
		TickableWidgets.Reset();
	}
}

void UWidgetModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FWidgetModuleSaveData>();

	SetLanguageType(SaveData.LanguageType);
	SetGlobalScale(SaveData.GlobalScale);
}

void UWidgetModule::UnloadData(EPhase InPhase)
{
}

FSaveData* UWidgetModule::ToData()
{
	FWidgetModuleSaveData& SaveData = GetMutableSaveData<FWidgetModuleSaveData>();
	SaveData = FWidgetModuleSaveData();

	SaveData.LanguageType = LanguageType;
	SaveData.GlobalScale = GlobalScale;
	
	return &SaveData;
}

FString UWidgetModule::GetModuleDebugMessage()
{
	return Super::GetModuleDebugMessage();
}

void UWidgetModule::BuildRuntimeCaches()
{
	UserWidgetConfigIndexMap.Reset();
	UserWidgetClassTagMap.Reset();
	UserWidgetChildrenMap.Reset();
	WorldWidgetConfigIndexMap.Reset();
	WorldWidgetClassTagMap.Reset();

	for(int32 Index = 0; Index < UserWidgetConfigs.Num(); ++Index)
	{
		const FScreenWidgetConfig& Config = UserWidgetConfigs[Index];
		const FGameplayTag WidgetTag = Config.ResolveWidgetTag();
		if(!WidgetTag.IsValid() || !Config.WidgetClass)
		{
			continue;
		}

		ensureEditorMsgf(
			!UserWidgetConfigIndexMap.Contains(WidgetTag),
			FString::Printf(TEXT("Duplicate screen widget tag: %s"), *WidgetTag.ToString()),
			EDC_Widget,
			EDV_Error);
		UserWidgetConfigIndexMap.Add(WidgetTag, Index);
		UserWidgetClassTagMap.Add(Config.WidgetClass.Get(), WidgetTag);
		if(Config.ParentWidgetTag.IsValid())
		{
			UserWidgetChildrenMap.Add(Config.ParentWidgetTag, WidgetTag);
		}
	}

	for(int32 Index = 0; Index < WorldWidgetConfigs.Num(); ++Index)
	{
		const FWorldWidgetConfig& Config = WorldWidgetConfigs[Index];
		const FGameplayTag WidgetTag = Config.ResolveWidgetTag();
		if(WidgetTag.IsValid() && Config.WidgetClass)
		{
			ensureEditorMsgf(!WorldWidgetConfigIndexMap.Contains(WidgetTag), FString::Printf(TEXT("Duplicate world widget tag: %s"), *WidgetTag.ToString()), EDC_Widget, EDV_Error);
			WorldWidgetConfigIndexMap.Add(WidgetTag, Index);
			WorldWidgetClassTagMap.Add(Config.WidgetClass.Get(), WidgetTag);
		}
	}
}

const FScreenWidgetConfig* UWidgetModule::GetUserWidgetConfig(FGameplayTag InWidgetTag) const
{
	if(const int32* ConfigIndex = UserWidgetConfigIndexMap.Find(InWidgetTag))
	{
		return UserWidgetConfigs.IsValidIndex(*ConfigIndex) ? &UserWidgetConfigs[*ConfigIndex] : nullptr;
	}
	return nullptr;
}

FGameplayTag UWidgetModule::ResolveWidgetTagForClass(TSubclassOf<UUserWidgetBase> InClass, bool bEnsured) const
{
	TArray<FGameplayTag> Tags;
	if(InClass)
	{
		UserWidgetClassTagMap.MultiFind(InClass.Get(), Tags);
	}

	ensureEditorMsgf(
		!bEnsured || Tags.Num() == 1,
		FString::Printf(TEXT("Screen widget class must resolve to exactly one tag, but %s resolved to %d."), *GetNameSafe(InClass), Tags.Num()),
		EDC_Widget,
		EDV_Error);
	return Tags.Num() == 1 ? Tags[0] : FGameplayTag();
}

UUserWidgetBase* UWidgetModule::GetUserWidgetByTag(FGameplayTag InWidgetTag, TSubclassOf<UUserWidgetBase> InExpectedClass) const
{
	const TObjectPtr<UUserWidgetBase>* Found = UserWidgetByTag.Find(InWidgetTag);
	UUserWidgetBase* Widget = Found ? Found->Get() : nullptr;
	return !InExpectedClass || (Widget && Widget->IsA(InExpectedClass)) ? Widget : nullptr;
}

bool UWidgetModule::EnsureParentCreated(const FScreenWidgetConfig& InConfig, UObject* InOwner)
{
	return !InConfig.ParentWidgetTag.IsValid() || CreateUserWidgetByTag(InConfig.ParentWidgetTag, InOwner) != nullptr;
}

void UWidgetModule::ApplyWidgetConfig(UUserWidgetBase* InWidget, const FScreenWidgetConfig& InConfig) const
{
	InWidget->WidgetTag = InConfig.ResolveWidgetTag();
	InWidget->WidgetType = InConfig.WidgetType;
	InWidget->WidgetZOrder = InConfig.ZOrder;
	InWidget->WidgetAnchors = InConfig.Anchors;
	InWidget->bWidgetAutoSize = InConfig.bAutoSize;
	InWidget->WidgetOffsets = InConfig.Offsets;
	InWidget->WidgetAlignment = InConfig.Alignment;
}

bool UWidgetModule::AttachWidgetToConfiguredParent(UUserWidgetBase* InWidget, const FScreenWidgetConfig& InConfig)
{
	if(!InConfig.ParentWidgetTag.IsValid())
	{
		return true;
	}

	UUserWidgetBase* ParentWidget = GetUserWidgetByTag(InConfig.ParentWidgetTag);
	if(!ParentWidget)
	{
		ensureEditorMsgf(false, FString::Printf(TEXT("Failed to attach %s to parent %s slot %s."), *InConfig.ResolveWidgetTag().ToString(), *InConfig.ParentWidgetTag.ToString(), *InConfig.ParentSlotTag.ToString()), EDC_Widget, EDV_Error);
		return false;
	}

	UWidgetMountSlot* MountSlot = ParentWidget->GetWidgetMountSlot(InConfig.ParentSlotTag);
	if(!MountSlot)
	{
		ensureEditorMsgf(false, FString::Printf(TEXT("Failed to find widget mount slot %s in parent %s."), *InConfig.ParentSlotTag.ToString(), *InConfig.ParentWidgetTag.ToString()), EDC_Widget, EDV_Error);
		return false;
	}

	if(MountSlot->GetContent() && MountSlot->GetContent() != InWidget)
	{
		ensureEditorMsgf(false, FString::Printf(TEXT("Widget mount slot %s already has content."), *InConfig.ParentSlotTag.ToString()), EDC_Widget, EDV_Error);
		return false;
	}

	InWidget->ParentWidget = ParentWidget;
	ParentWidget->AddChildWidget(InWidget);
	MountSlot->SetContent(InWidget);
	return true;
}

UUserWidgetBase* UWidgetModule::CreateUserWidgetByTag(FGameplayTag InWidgetTag, UObject* InOwner, const FParameter* InInitParameter, TSubclassOf<UUserWidgetBase> InClassOverride)
{
	if(UUserWidgetBase* Existing = GetUserWidgetByTag(InWidgetTag))
	{
		return Existing;
	}

	const FScreenWidgetConfig* Config = GetUserWidgetConfig(InWidgetTag);
	if(!Config || !EnsureParentCreated(*Config, InOwner))
	{
		return nullptr;
	}

	const TSubclassOf<UUserWidgetBase> SpawnClass = InClassOverride ? InClassOverride : Config->WidgetClass;
	UUserWidgetBase* Widget = SpawnClass ? UObjectPoolModuleStatics::SpawnObject<UUserWidgetBase>(SpawnClass) : nullptr;
	if(!Widget)
	{
		return nullptr;
	}

	ApplyWidgetConfig(Widget, *Config);
	UserWidgetByTag.Add(InWidgetTag, Widget);

	TArray<FParameter> Parameters;
	if(InInitParameter)
	{
		Parameters.Add(*InInitParameter);
	}
	Widget->OnCreate(InOwner, Parameters);
	Widget->Init(InOwner, &Parameters, false);
	if(!AttachWidgetToConfiguredParent(Widget, *Config))
	{
		UserWidgetByTag.Remove(InWidgetTag);
		Widget->OnDestroy(false);
		return nullptr;
	}

	TArray<FGameplayTag> ChildTags;
	UserWidgetChildrenMap.MultiFind(InWidgetTag, ChildTags);
	for(const FGameplayTag& ChildTag : ChildTags)
	{
		const FScreenWidgetConfig* ChildConfig = GetUserWidgetConfig(ChildTag);
		if(!ChildConfig)
		{
			continue;
		}
		switch(ChildConfig->CreateType)
		{
			case EWidgetCreateType::AutoCreate:
				CreateUserWidgetByTag(ChildTag, InOwner);
				break;
			case EWidgetCreateType::AutoCreateAndOpen:
				OpenUserWidgetByTag(ChildTag, FParameter());
				break;
			default:
				break;
		}
	}
	return Widget;
}

UUserWidgetBase* UWidgetModule::K2_CreateUserWidgetByTag(FGameplayTag InWidgetTag, UObject* InOwner, const FParameter& InInitParameter, TSubclassOf<UUserWidgetBase> InClassOverride)
{
	return CreateUserWidgetByTag(InWidgetTag, InOwner, &InInitParameter, InClassOverride);
}

bool UWidgetModule::OpenUserWidgetByTag(FGameplayTag InWidgetTag, const FParameter& InOpenParameter, bool bInstant, bool bForce, TSubclassOf<UUserWidgetBase> InClassOverride)
{
	UUserWidgetBase* Widget = CreateUserWidgetByTag(InWidgetTag, nullptr, nullptr, InClassOverride);
	if(!Widget || !Widget->CanOpen())
	{
		return false;
	}

	const FScreenWidgetConfig* Config = GetUserWidgetConfig(InWidgetTag);
	if(Config && Config->ParentWidgetTag.IsValid())
	{
		OpenUserWidgetByTag(Config->ParentWidgetTag, FParameter(), bInstant, false);
	}
	if(Config && Config->WidgetType == EWidgetType::Temporary)
	{
		FWidgetMountContext Context;
		Context.ParentWidgetTag = Config->ParentWidgetTag;
		Context.ParentSlotTag = Config->ParentSlotTag;
		if(UUserWidgetBase* ActiveWidget = ActiveTemporaryWidgets.FindRef(Context); ActiveWidget && ActiveWidget != Widget)
		{
			ActiveWidget->Close(true);
		}
		ActiveTemporaryWidgets.Add(Context, Widget);
	}

	TArray<FParameter> Parameters;
	Parameters.Add(InOpenParameter);
	if(bForce && Widget->GetWidgetState() != EScreenWidgetState::None)
	{
		Widget->OnClose(true);
	}
	Widget->OnOpen(Parameters, bInstant);
	return true;
}

bool UWidgetModule::CloseUserWidgetByTag(FGameplayTag InWidgetTag, bool bInstant)
{
	if(UUserWidgetBase* Widget = GetUserWidgetByTag(InWidgetTag))
	{
		if(const FScreenWidgetConfig* Config = GetUserWidgetConfig(InWidgetTag); Config && Config->WidgetType == EWidgetType::Temporary)
		{
			FWidgetMountContext Context;
			Context.ParentWidgetTag = Config->ParentWidgetTag;
			Context.ParentSlotTag = Config->ParentSlotTag;
			if(ActiveTemporaryWidgets.FindRef(Context) == Widget)
			{
				ActiveTemporaryWidgets.Remove(Context);
			}
		}
		Widget->OnClose(bInstant);
		return true;
	}
	return false;
}

bool UWidgetModule::ToggleUserWidgetByTag(FGameplayTag InWidgetTag, bool bInstant)
{
	UUserWidgetBase* Widget = GetUserWidgetByTag(InWidgetTag);
	if(!Widget)
	{
		return OpenUserWidgetByTag(InWidgetTag, FParameter(), bInstant);
	}
	if(Widget->GetWidgetState() == EScreenWidgetState::Opened)
	{
		return CloseUserWidgetByTag(InWidgetTag, bInstant);
	}
	return OpenUserWidgetByTag(InWidgetTag, FParameter(), bInstant);
}

bool UWidgetModule::CloseActiveTemporaryWidget(const FWidgetMountContext& InContext, bool bInstant)
{
	if(UUserWidgetBase* Widget = ActiveTemporaryWidgets.FindRef(InContext))
	{
		ActiveTemporaryWidgets.Remove(InContext);
		Widget->OnClose(bInstant);
		return true;
	}
	return false;
}

bool UWidgetModule::CloseTemporaryWidgetInSlot(FGameplayTag InParentWidgetTag, FGameplayTag InParentSlotTag, bool bInstant)
{
	FWidgetMountContext Context;
	Context.ParentWidgetTag = InParentWidgetTag;
	Context.ParentSlotTag = InParentSlotTag;
	return CloseActiveTemporaryWidget(Context, bInstant);
}

void UWidgetModule::CloseTemporaryWidgetsForParent(FGameplayTag InParentWidgetTag, bool bInstant)
{
	TArray<FWidgetMountContext> Contexts;
	ActiveTemporaryWidgets.GetKeys(Contexts);
	for(const FWidgetMountContext& Context : Contexts)
	{
		if(Context.ParentWidgetTag == InParentWidgetTag)
		{
			CloseActiveTemporaryWidget(Context, bInstant);
		}
	}
}

bool UWidgetModule::DestroyUserWidgetByTag(FGameplayTag InWidgetTag, bool bRecovery)
{
	if(UUserWidgetBase* Widget = GetUserWidgetByTag(InWidgetTag))
	{
		UserWidgetByTag.Remove(InWidgetTag);
		Widget->OnDestroy(bRecovery);
		return true;
	}
	return false;
}

void UWidgetModule::OnOpenUserWidget(UObject* InSender, const FEventOpenUserWidget& InEvent)
{
	if(InEvent.WidgetTag.IsValid())
	{
		OpenUserWidgetByTag(InEvent.WidgetTag, InEvent.WidgetParam, InEvent.bInstant, InEvent.bForce, InEvent.WidgetClassOverride);
	}
}

void UWidgetModule::OnCloseUserWidget(UObject* InSender, const FEventCloseUserWidget& InEvent)
{
	if(InEvent.WidgetTag.IsValid())
	{
		CloseUserWidgetByTag(InEvent.WidgetTag, InEvent.bInstant);
	}
}

bool UWidgetModule::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass) const
{
	return HasUserWidgetClass<UUserWidgetBase>(InClass);
}

TSubclassOf<UUserWidgetBase> UWidgetModule::GetUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass) const
{
	return GetUserWidgetClass<UUserWidgetBase>(InClass);
}

bool UWidgetModule::HasUserWidget(TSubclassOf<UUserWidgetBase> InClass) const
{
	return HasUserWidget<UUserWidgetBase>(InClass);
}

UUserWidgetBase* UWidgetModule::GetUserWidget(TSubclassOf<UUserWidgetBase> InClass) const
{
	return GetUserWidget<UUserWidgetBase>(InClass);
}

UUserWidgetBase* UWidgetModule::CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce)
{
	return CreateUserWidget<UUserWidgetBase>(InOwner, &InParams, bForce, InClass);
}

bool UWidgetModule::OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant, bool bForce)
{
	return OpenUserWidget<UUserWidgetBase>(&InParams, bInstant, bForce, InClass);
}

bool UWidgetModule::CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return CloseUserWidget<UUserWidgetBase>(bInstant, InClass);
}

bool UWidgetModule::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return ToggleUserWidget<UUserWidgetBase>(bInstant, InClass);
}

bool UWidgetModule::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bRecovery)
{
	return DestroyUserWidget<UUserWidgetBase>(bRecovery, InClass);
}

void UWidgetModule::CloseAllUserWidget(bool bInstant)
{
	for(const auto& Iter : UserWidgetByTag)
	{
		if(Iter.Value)
		{
			Iter.Value->OnClose(bInstant);
		}
	}
	ActiveTemporaryWidgets.Empty();
}

void UWidgetModule::ClearAllUserWidget(bool bRecovery)
{
	for(const auto& Iter : UserWidgetByTag)
	{
		if(Iter.Value)
		{
			Iter.Value->OnDestroy(bRecovery);
		}
	}
	UserWidgetByTag.Empty();
	ActiveTemporaryWidgets.Empty();
}

const FWorldWidgetConfig* UWidgetModule::GetWorldWidgetConfig(FGameplayTag InWidgetTag) const
{
	if(const int32* ConfigIndex = WorldWidgetConfigIndexMap.Find(InWidgetTag))
	{
		return WorldWidgetConfigs.IsValidIndex(*ConfigIndex)
			? &WorldWidgetConfigs[*ConfigIndex]
			: nullptr;
	}
	return nullptr;
}

FGameplayTag UWidgetModule::ResolveWorldWidgetTagForClass(TSubclassOf<UWorldWidgetBase> InClass, bool bEnsured) const
{
	TArray<FGameplayTag> Tags;
	if(InClass)
	{
		WorldWidgetClassTagMap.MultiFind(InClass.Get(), Tags);
	}
	ensureEditorMsgf(
		!bEnsured || Tags.Num() == 1,
		FString::Printf(
			TEXT("World widget class must resolve to exactly one tag, but %s resolved to %d."),
			*GetNameSafe(InClass),
			Tags.Num()),
		EDC_Widget,
		EDV_Error);
	return Tags.Num() == 1 ? Tags[0] : FGameplayTag();
}

TArray<UWorldWidgetBase*> UWidgetModule::GetWorldWidgetsByTag(FGameplayTag InWidgetTag) const
{
	if(const FWorldWidgets* Widgets = WorldWidgetByTag.Find(InWidgetTag))
	{
		return Widgets->WorldWidgets;
	}
	return TArray<UWorldWidgetBase*>();
}

UWorldWidgetBase* UWidgetModule::GetWorldWidgetByTag(FGameplayTag InWidgetTag, int32 InIndex, TSubclassOf<UWorldWidgetBase> InExpectedClass) const
{
	const FWorldWidgets* Widgets = WorldWidgetByTag.Find(InWidgetTag);
	UWorldWidgetBase* Widget = Widgets && Widgets->WorldWidgets.IsValidIndex(InIndex)
		? Widgets->WorldWidgets[InIndex]
		: nullptr;
	return !InExpectedClass || (Widget && Widget->IsA(InExpectedClass))
		? Widget
		: nullptr;
}

UWorldWidgetBase* UWidgetModule::CreateWorldWidgetByTag(FGameplayTag InWidgetTag, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams, TSubclassOf<UWorldWidgetBase> InClassOverride)
{
	const FWorldWidgetConfig* Config = GetWorldWidgetConfig(InWidgetTag);
	if(!Config)
	{
		return nullptr;
	}
	const TSubclassOf<UWorldWidgetBase> SpawnClass = InClassOverride
		? InClassOverride
		: Config->WidgetClass;
	UWorldWidgetBase* Widget = SpawnClass
		? UObjectPoolModuleStatics::SpawnObject<UWorldWidgetBase>(SpawnClass)
		: nullptr;
	if(!Widget)
	{
		return nullptr;
	}

	FWorldWidgets& Widgets = WorldWidgetByTag.FindOrAdd(InWidgetTag);
	Widget->WidgetTag = InWidgetTag;
	Widget->WidgetIndex = Widgets.WorldWidgets.Add(Widget);
	Widget->OnCreate(InOwner, InMapping, InParams);
	return Widget;
}

bool UWidgetModule::DestroyWorldWidgetByTag(FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, bool bRecovery)
{
	FWorldWidgets* Widgets = WorldWidgetByTag.Find(InWidgetTag);
	if(!Widgets || !InWidget)
	{
		return false;
	}
	const int32 RemovedIndex = Widgets->WorldWidgets.IndexOfByKey(InWidget);
	if(RemovedIndex == INDEX_NONE)
	{
		return false;
	}
	Widgets->WorldWidgets.RemoveAt(RemovedIndex);
	for(int32 Index = RemovedIndex; Index < Widgets->WorldWidgets.Num(); ++Index)
	{
		if(Widgets->WorldWidgets[Index])
		{
			Widgets->WorldWidgets[Index]->WidgetIndex = Index;
		}
	}
	if(Widgets->WorldWidgets.IsEmpty())
	{
		WorldWidgetByTag.Remove(InWidgetTag);
	}
	InWidget->OnDestroy(bRecovery);
	return true;
}

bool UWidgetModule::GetWorldWidgetVisibleByTag(FGameplayTag InWidgetTag) const
{
	if(const FWorldWidgets* Widgets = WorldWidgetByTag.Find(InWidgetTag))
	{
		return Widgets->bVisible;
	}
	return false;
}

void UWidgetModule::SetWorldWidgetVisibleByTag(FGameplayTag InWidgetTag, bool bVisible)
{
	if(FWorldWidgets* Widgets = WorldWidgetByTag.Find(InWidgetTag))
	{
		Widgets->bVisible = bVisible;
	}
}

void UWidgetModule::OnSetWorldWidgetVisible(UObject* InSender, const FEventSetWorldWidgetVisible& InEvent)
{
	SetWorldWidgetVisible(InEvent.bVisible, InEvent.WidgetClass);
}

bool UWidgetModule::GetWorldWidgetVisible(TSubclassOf<UWorldWidgetBase> InClass)
{
	if(InClass)
	{
		return GetWorldWidgetVisibleByTag(ResolveWorldWidgetTagForClass(InClass, false));
	}
	return WorldWidgetContainer
		&& WorldWidgetContainer->GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
}

void UWidgetModule::SetWorldWidgetVisible(bool bVisible, TSubclassOf<UWorldWidgetBase> InClass)
{
	if(InClass)
	{
		SetWorldWidgetVisibleByTag(ResolveWorldWidgetTagForClass(InClass, false), bVisible);
	}
	else if(WorldWidgetContainer)
	{
		WorldWidgetContainer->SetVisibility(
			bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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

TArray<UWorldWidgetBase*> UWidgetModule::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass) const
{
	return GetWorldWidgets<UWorldWidgetBase>(InClass);
}

UWorldWidgetBase* UWidgetModule::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams)
{
	return CreateWorldWidget<UWorldWidgetBase>(InOwner, InMapping, &InParams, InClass);
}

bool UWidgetModule::DestroyWorldWidget(UWorldWidgetBase* InWidget, bool bRecovery)
{
	return InWidget
		&& DestroyWorldWidgetByTag(InWidget->GetWidgetTag(), InWidget, bRecovery);
}

bool UWidgetModule::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery)
{
	return DestroyWorldWidget<UWorldWidgetBase>(InIndex, bRecovery, InClass);
}

void UWidgetModule::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery)
{
	DestroyWorldWidgets<UWorldWidgetBase>(bRecovery, InClass);
}

void UWidgetModule::ClearAllWorldWidget(bool bRecovery)
{
	TArray<UWorldWidgetBase*> WidgetsToDestroy;
	for(const auto& Iter : WorldWidgetByTag)
	{
		WidgetsToDestroy.Append(Iter.Value.WorldWidgets);
	}
	WorldWidgetByTag.Empty();
	for(UWorldWidgetBase* Widget : WidgetsToDestroy)
	{
		if(Widget)
		{
			Widget->OnDestroy(bRecovery);
		}
	}
}
