
// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/WidgetModule.h"
#include "WHFrameworkCoreMacros.h"
#include "Widget/WidgetMountTypes.h"
#include "Widget/ScreenWidgetTypes.h"
#include "Widget/WorldWidgetTypes.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Common/LifecycleTypes.h"
#include "Common/LanguageTypes.h"
#include "Setting/SettingPresentationTypes.h"

#include "WHFrameworkCoreStatics.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Common/CommonModuleStatics.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SceneComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Event/EventModuleStatics.h"
#include "Event/EventModule.h"
#include "Main/MainModuleStatics.h"
#include "Event/Events/Widget/Event_CloseUserWidget.h"
#include "Event/Events/Widget/Event_OpenUserWidget.h"
#include "Event/Events/Widget/Event_SetWorldWidgetVisible.h"
#include "Widget/World/WorldWidgetContainer.h"
#include "Widget/World/WorldWidgetComponent.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonImageN.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Screen/WidgetMountSlot.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"
		
IMPLEMENTATION_MODULE(UWidgetModule)

#include "Widget/WidgetParameterHelpers.h"

using namespace WidgetParameterHelpers;

// Sets default values
UWidgetModule::UWidgetModule()
{
	ModuleName = FName("WidgetModule");
	ModuleDisplayName = FText::FromString(TEXT("Widget Module"));
	ModuleDependencies = { FName("ObjectPoolModule"), FName("EventModule") };


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
}

UWidgetModule::~UWidgetModule()
{
}

#if WITH_EDITOR
void UWidgetModule::OnGenerate()
{
	Super::OnGenerate();

	TArray<FText> Errors;
	TArray<FText> Warnings;
	ValidateScreenWidgetConfigs(Errors, Warnings);
	ValidateWorldWidgetConfigs(Errors, Warnings);
	ValidateWidgetTheme(Errors, Warnings);
	ValidateNativeWidgetUsage(Warnings);
	for(const FText& Error : Errors)
	{
		ensureEditorMsgf(false, Error.ToString(), EDC_Widget, EDV_Error);
	}
	for(const FText& Warning : Warnings)
	{
		ensureEditorMsgf(false, Warning.ToString(), EDC_Widget, EDV_Warning);
	}
}

bool UWidgetModule::ValidateScreenWidgetConfigs(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	TMap<FGameplayTag, int32> ConfigIndexByTag;
	TMultiMap<UClass*, FGameplayTag> TagsByClass;
	TMap<FWidgetMountContext, FGameplayTag> ChildByMountContext;

	for(int32 Index = 0; Index < UserWidgetConfigs.Num(); ++Index)
	{
		const FScreenWidgetConfig& Config = UserWidgetConfigs[Index];
		if(!Config.WidgetClass)
		{
			OutErrors.Add(FText::Format(NSLOCTEXT("WH.WidgetModule", "MissingWidgetClass", "Screen widget config {0} has no widget class."), Index));
			continue;
		}

		const FGameplayTag WidgetTag = Config.ResolveWidgetTag();
		if(!WidgetTag.IsValid())
		{
			OutErrors.Add(FText::Format(NSLOCTEXT("WH.WidgetModule", "MissingWidgetTag", "Screen widget config {0} ({1}) has neither a default tag nor an override tag."), Index, FText::FromString(GetNameSafe(Config.WidgetClass))));
			continue;
		}

		if(const int32* ExistingIndex = ConfigIndexByTag.Find(WidgetTag))
		{
			OutErrors.Add(FText::Format(NSLOCTEXT("WH.WidgetModule", "DuplicateWidgetTag", "Screen widget tag {0} is used by config {1} and config {2}."), FText::FromString(WidgetTag.ToString()), *ExistingIndex, Index));
		}
		else
		{
			ConfigIndexByTag.Add(WidgetTag, Index);
		}
		TagsByClass.Add(Config.WidgetClass.Get(), WidgetTag);

		const FGameplayTag ParentWidgetTag = Config.ResolveParentWidgetTag();
		if(!ParentWidgetTag.IsValid())
		{
			if(Config.SlotTag.IsValid())
			{
				OutErrors.Add(FText::Format(NSLOCTEXT("WH.WidgetModule", "RootHasSlot", "Root screen widget {0} specifies slot {1}."), FText::FromString(WidgetTag.ToString()), FText::FromString(Config.SlotTag.ToString())));
			}
			continue;
		}

		if(!Config.SlotTag.IsValid())
		{
			continue;
		}

		FWidgetMountContext Context;
		Context.ParentWidgetTag = ParentWidgetTag;
		Context.SlotTag = Config.SlotTag;
		if(const FGameplayTag* ExistingChild = ChildByMountContext.Find(Context))
		{
			OutErrors.Add(FText::Format(NSLOCTEXT("WH.WidgetModule", "MultipleChildrenInMountSlot", "Mount slot {0}.{1} is assigned to both {2} and {3}."), FText::FromString(Context.ParentWidgetTag.ToString()), FText::FromString(Context.SlotTag.ToString()), FText::FromString(ExistingChild->ToString()), FText::FromString(WidgetTag.ToString())));
		}
		else
		{
			ChildByMountContext.Add(Context, WidgetTag);
		}
	}

	for(const FScreenWidgetConfig& Config : UserWidgetConfigs)
	{
		const FGameplayTag WidgetTag = Config.ResolveWidgetTag();
		const FGameplayTag ParentWidgetTag = Config.ResolveParentWidgetTag();
		if(!WidgetTag.IsValid() || !ParentWidgetTag.IsValid())
		{
			continue;
		}
		if(!ConfigIndexByTag.Contains(ParentWidgetTag))
		{
			OutErrors.Add(FText::Format(NSLOCTEXT("WH.WidgetModule", "MissingParentWidget", "Screen widget {0} resolves to missing parent {1}."), FText::FromString(WidgetTag.ToString()), FText::FromString(ParentWidgetTag.ToString())));
			continue;
		}

		const int32* ParentIndex = ConfigIndexByTag.Find(ParentWidgetTag);
		if(!ParentIndex)
		{
			continue;
		}
		const FScreenWidgetConfig& ParentConfig = UserWidgetConfigs[*ParentIndex];
		const UWidgetBlueprintGeneratedClass* ParentGeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(ParentConfig.WidgetClass.Get());
		const UWidgetTree* WidgetTree = ParentGeneratedClass ? ParentGeneratedClass->GetWidgetTreeArchetype() : nullptr;
		if(!Config.SlotTag.IsValid())
		{
			const UPanelWidget* RootPanel = WidgetTree
				? Cast<UPanelWidget>(WidgetTree->RootWidget)
				: nullptr;
			if(!RootPanel)
			{
				OutErrors.Add(FText::Format(
					NSLOCTEXT("WH.WidgetModule", "InvalidParentRootPanel", "Parent widget {0} root widget is not a PanelWidget, but child {1} has no mount slot."),
					FText::FromString(ParentWidgetTag.ToString()),
					FText::FromString(WidgetTag.ToString())));
			}
			continue;
		}

		TArray<UWidget*> Widgets;
		if(WidgetTree)
		{
			WidgetTree->GetAllWidgets(Widgets);
		}

		TSet<FGameplayTag> SlotTags;
		const UWidgetMountSlot* MatchingSlot = nullptr;
		for(const UWidget* Widget : Widgets)
		{
			const UWidgetMountSlot* MountSlot = Cast<UWidgetMountSlot>(Widget);
			if(!MountSlot || !MountSlot->GetSlotTag().IsValid())
			{
				continue;
			}
			if(SlotTags.Contains(MountSlot->GetSlotTag()))
			{
				OutErrors.Add(FText::Format(NSLOCTEXT("WH.WidgetModule", "DuplicateMountSlot", "Widget class {0} contains duplicate mount slot tag {1}."), FText::FromString(GetNameSafe(ParentConfig.WidgetClass)), FText::FromString(MountSlot->GetSlotTag().ToString())));
			}
			SlotTags.Add(MountSlot->GetSlotTag());
			if(MountSlot->GetSlotTag() == Config.SlotTag)
			{
				MatchingSlot = MountSlot;
			}
		}

		if(!MatchingSlot)
		{
			OutErrors.Add(FText::Format(NSLOCTEXT("WH.WidgetModule", "MissingMountSlot", "Parent class {0} does not contain mount slot {1} required by {2}."), FText::FromString(GetNameSafe(ParentConfig.WidgetClass)), FText::FromString(Config.SlotTag.ToString()), FText::FromString(WidgetTag.ToString())));
		}
		else if(MatchingSlot->GetContent())
		{
			OutErrors.Add(FText::Format(NSLOCTEXT("WH.WidgetModule", "StaticMountSlotContent", "Mount slot {0}.{1} already has designer content and cannot receive dynamic widget {2}."), FText::FromString(ParentWidgetTag.ToString()), FText::FromString(Config.SlotTag.ToString()), FText::FromString(WidgetTag.ToString())));
		}
	}

	TArray<UClass*> WidgetClasses;
	TagsByClass.GetKeys(WidgetClasses);
	for(UClass* WidgetClass : WidgetClasses)
	{
		TArray<FGameplayTag> Tags;
		TagsByClass.MultiFind(WidgetClass, Tags);
		if(Tags.Num() > 1)
		{
			OutWarnings.Add(FText::Format(NSLOCTEXT("WH.WidgetModule", "AmbiguousWidgetClass", "Widget class {0} maps to {1} tags; class convenience APIs are disabled for this class."), FText::FromString(GetNameSafe(WidgetClass)), Tags.Num()));
		}
	}

	return OutErrors.IsEmpty();
}

bool UWidgetModule::ValidateWorldWidgetConfigs(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	TMap<FGameplayTag, int32> ConfigIndexByTag;
	TMultiMap<UClass*, FGameplayTag> TagsByClass;
	for(int32 Index = 0; Index < WorldWidgetConfigs.Num(); ++Index)
	{
		const FWorldWidgetConfig& Config = WorldWidgetConfigs[Index];
		if(!Config.WidgetClass)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "MissingWorldWidgetClass", "World widget config {0} has no widget class."),
				Index));
			continue;
		}

		const FGameplayTag WidgetTag = Config.ResolveWidgetTag();
		if(!WidgetTag.IsValid())
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "MissingWorldWidgetTag", "World widget config {0} ({1}) has neither a default tag nor an override tag."),
				Index,
				FText::FromString(GetNameSafe(Config.WidgetClass))));
			continue;
		}

		if(const int32* ExistingIndex = ConfigIndexByTag.Find(WidgetTag))
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "DuplicateWorldWidgetTag", "World widget tag {0} is used by config {1} and config {2}."),
				FText::FromString(WidgetTag.ToString()),
				*ExistingIndex,
				Index));
		}
		else
		{
			ConfigIndexByTag.Add(WidgetTag, Index);
		}
		TagsByClass.Add(Config.WidgetClass.Get(), WidgetTag);
	}

	TArray<UClass*> WidgetClasses;
	TagsByClass.GetKeys(WidgetClasses);
	for(UClass* WidgetClass : WidgetClasses)
	{
		TArray<FGameplayTag> Tags;
		TagsByClass.MultiFind(WidgetClass, Tags);
		if(Tags.Num() > 1)
		{
			OutWarnings.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "AmbiguousWorldWidgetClass", "World widget class {0} maps to {1} tags; class convenience APIs are disabled for this class."),
				FText::FromString(GetNameSafe(WidgetClass)),
				Tags.Num()));
		}
	}

	return OutErrors.IsEmpty();
}

bool UWidgetModule::ValidateWidgetTheme(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	const UWidgetTheme* ResolvedTheme = FWidgetThemeResolver::ResolveTheme(this);
	if(!ResolvedTheme)
	{
		OutErrors.Add(NSLOCTEXT(
			"WH.WidgetModule",
			"MissingResolvedTheme",
			"Widget module has no default widget theme."));
		return false;
	}

	for(const auto& Iter : ResolvedTheme->GetButtonStyles())
	{
		if(!Iter.Key.IsValid() || !Iter.Value.Style)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "InvalidButtonStyle", "Widget theme button style {0} has an invalid tag or style class."),
				FText::FromString(Iter.Key.ToString())));
		}
	}
	for(const auto& Iter : ResolvedTheme->GetTextStyles())
	{
		if(!Iter.Key.IsValid() || !Iter.Value.Style)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "InvalidTextStyle", "Widget theme text style {0} has an invalid tag or style class."),
				FText::FromString(Iter.Key.ToString())));
		}
	}
	for(const auto& Iter : ResolvedTheme->GetBrushStyles())
	{
		if(!Iter.Key.IsValid())
		{
			OutErrors.Add(NSLOCTEXT(
				"WH.WidgetModule",
				"InvalidBrushStyle",
				"Widget theme contains a brush style with an invalid tag."));
		}
		else if(!Iter.Value.Style)
		{
			OutWarnings.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "BrushStyleWithoutResource", "Widget theme brush style {0} requires migration to a brush style class."),
				FText::FromString(Iter.Key.ToString())));
		}
	}

	auto ValidateRendererClass = [&OutWarnings, ResolvedTheme](const TCHAR* InName, ESettingRendererType InType)
	{
		if(!ResolvedTheme->FindSettingRendererClass(InType))
		{
			OutWarnings.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "MissingSettingRenderer", "Widget theme has no renderer class for {0} settings."),
				FText::FromString(InName)));
		}
	};
	ValidateRendererClass(TEXT("Bool"), ESettingRendererType::Bool);
	ValidateRendererClass(TEXT("Number"), ESettingRendererType::Number);
	ValidateRendererClass(TEXT("Enum"), ESettingRendererType::Enum);
	ValidateRendererClass(TEXT("Text"), ESettingRendererType::Text);
	ValidateRendererClass(TEXT("Option"), ESettingRendererType::Option);
	ValidateRendererClass(TEXT("Key"), ESettingRendererType::Key);

	return OutErrors.IsEmpty();
}

void UWidgetModule::ValidateNativeWidgetUsage(TArray<FText>& OutWarnings) const
{
	TSet<UClass*> WidgetClasses;
	for(const FScreenWidgetConfig& Config : UserWidgetConfigs)
	{
		if(Config.WidgetClass)
		{
			WidgetClasses.Add(Config.WidgetClass.Get());
		}
	}
	for(const FWorldWidgetConfig& Config : WorldWidgetConfigs)
	{
		if(Config.WidgetClass)
		{
			WidgetClasses.Add(Config.WidgetClass.Get());
		}
	}

	for(UClass* WidgetClass : WidgetClasses)
	{
		const UWidgetBlueprintGeneratedClass* GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
		const UWidgetTree* WidgetTree = GeneratedClass ? GeneratedClass->GetWidgetTreeArchetype() : nullptr;
		if(!WidgetTree)
		{
			continue;
		}

		TMap<UClass*, int32> NativeControlCounts;
		TArray<UWidget*> Widgets;
		WidgetTree->GetAllWidgets(Widgets);
		for(const UWidget* Widget : Widgets)
		{
			if(Widget && (
				Widget->GetClass() == UButton::StaticClass()
				|| Widget->GetClass() == UTextBlock::StaticClass()
				|| Widget->GetClass() == UImage::StaticClass()
				|| Widget->GetClass() == UWidgetSwitcher::StaticClass()))
			{
				NativeControlCounts.FindOrAdd(Widget->GetClass())++;
			}
		}

		for(const auto& Iter : NativeControlCounts)
		{
			FString Replacement;
			if(Iter.Key == UButton::StaticClass())
			{
				Replacement = TEXT("UCommonButton");
			}
			else if(Iter.Key == UTextBlock::StaticClass())
			{
				Replacement = TEXT("UCommonTextBlockN");
			}
			else if(Iter.Key == UImage::StaticClass())
			{
				Replacement = TEXT("UCommonImageN");
			}
			else
			{
				Replacement = TEXT("UCommonAnimatedSwitcher");
			}

			OutWarnings.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "NativeWidgetControl", "Widget class {0} contains {1} native {2} control(s); use {3} when framework styling or CommonUI behavior is required."),
				FText::FromString(GetNameSafe(WidgetClass)),
				Iter.Value,
				FText::FromString(GetNameSafe(Iter.Key)),
				FText::FromString(Replacement)));
		}
	}
}

void UWidgetModule::OnDestroy()
{
	Super::OnDestroy();

}
#endif

void UWidgetModule::OnInitialize()
{
	Super::OnInitialize();

	UEventModuleStatics::SubscribeEvent<FEventOpenUserWidget>(this, &ThisClass::OnOpenUserWidget);
	UEventModuleStatics::SubscribeEvent<FEventCloseUserWidget>(this, &ThisClass::OnCloseUserWidget);
	UEventModuleStatics::SubscribeEvent<FEventSetWorldWidgetVisible>(this, &ThisClass::OnSetWorldWidgetVisible);

	BuildRuntimeCaches();

}

void UWidgetModule::OnPreparatory(EPhase InPhase)
{
    if (PHASEC(InPhase, EPhase::Final))
    {
        SetLanguageType(LanguageType);
        SetGlobalScale(GlobalScale);
        bPlayerWidgetsReady = true;
        RefreshPlayerWidgets();
    }
    Super::OnPreparatory(InPhase);
}

void UWidgetModule::OnControlRefresh(float RealDeltaSeconds)
{
	if (!IsBusinessAdmissionOpen()) return;
    Super::OnControlRefresh(RealDeltaSeconds);
    if (bPlayerWidgetsReady)
    {
        RefreshPlayerWidgets();
		if (!bPlayerWidgetsReady) return;
		TArray<TWeakObjectPtr<UWorldWidgetComponent>> Components = WorldWidgetComponents.Array();
		for (const TWeakObjectPtr<UWorldWidgetComponent>& Component : Components)
		{
			if (Component.IsValid()) Component->RefreshPlayerWorldWidgets();
			else WorldWidgetComponents.Remove(Component);
			if (!bPlayerWidgetsReady) return;
		}
		TArray<TWeakObjectPtr<UWorldWidgetBase>> WorldWidgets;
		for (const auto& Entry : WorldWidgetByTag)
			for (UWorldWidgetBase* Widget : Entry.Value.WorldWidgets) if (Widget) WorldWidgets.Add(Widget);
		for (const TWeakObjectPtr<UWorldWidgetBase>& Widget : WorldWidgets)
			if (Widget.IsValid() && Widget->GetWidgetTag().IsValid()) Widget->RefreshLocationAndVisibility();
    }
}

void UWidgetModule::RegisterWorldWidgetComponent(UWorldWidgetComponent* InComponent)
{
	if (!IsBusinessAdmissionOpen()) return;
	if (InComponent) WorldWidgetComponents.Add(InComponent);
}

void UWidgetModule::UnregisterWorldWidgetComponent(UWorldWidgetComponent* InComponent)
{
	WorldWidgetComponents.Remove(InComponent);
}

void UWidgetModule::RefreshPlayerWidgets()
{
	if (!IsBusinessAdmissionOpen()) return;
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    if (!World || !GameInstance || World->GetNetMode() == NM_DedicatedServer) return;
    const TArray<ULocalPlayer*> LocalPlayers = GameInstance->GetLocalPlayers();
	TArray<TWeakObjectPtr<ULocalPlayer>> PreparedKeys;
	PreparedPlayers.GetKeys(PreparedKeys);
    for (const TWeakObjectPtr<ULocalPlayer>& Key : PreparedKeys)
    {
        ULocalPlayer* LocalPlayer = Key.Get();
        APlayerController* Current = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
        if (!LocalPlayer || !LocalPlayers.Contains(LocalPlayer) || !Current || Current != PreparedPlayers.FindRef(Key).Get())
        {
			PreparedPlayers.Remove(Key);
            if (LocalPlayer) ClearLocalPlayerWidgets(LocalPlayer, EObjectDespawnMode::Destroy);
			if (UWorldWidgetContainer* Container = WorldWidgetContainers.FindRef(LocalPlayer)) Container->RemoveFromParent();
			WorldWidgetContainers.Remove(LocalPlayer);
			if (!bPlayerWidgetsReady) return;
        }
    }
    for (ULocalPlayer* LocalPlayer : LocalPlayers)
    {
        APlayerController* Player = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
        if (!ResolveLocalPlayer(Player)) continue;
		if (WorldWidgetContainerClass && !WorldWidgetContainers.Contains(LocalPlayer))
		{
			if (UWorldWidgetContainer* Container = CreateWidget<UWorldWidgetContainer>(Player, WorldWidgetContainerClass))
			{
				Container->SetVisibility(bWorldWidgetsVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
				if (Container->AddToPlayerScreen(WorldWidgetContainerZOrder)) WorldWidgetContainers.Add(LocalPlayer, Container);
			}
		}
		if (PreparedPlayers.Contains(LocalPlayer)) continue;
        PreparedPlayers.Add(LocalPlayer, Player);
        for (const FScreenWidgetConfig& Config : UserWidgetConfigs)
        {
            const FGameplayTag Tag = Config.ResolveWidgetTag();
            if (!Tag.IsValid() || Config.ResolveParentWidgetTag().IsValid()) continue;
            if (Config.CreateType == EWidgetCreateType::AutoCreate) CreateUserWidgetByTag(Player, Tag);
            else if (Config.CreateType == EWidgetCreateType::AutoCreateAndOpen) OpenUserWidgetByTag(Player, Tag, FParameter());
			if (!bPlayerWidgetsReady) return;
        }
    }
}

void UWidgetModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	if (!IsBusinessAdmissionOpen()) return;
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(bInEditor)
	{
		return;
	}

	TArray<TWeakObjectPtr<UUserWidget>> TickSnapshot;
	for (auto It = TickableWidgets.CreateIterator(); It; ++It)
	{
		if (!::IsValid(*It)) It.RemoveCurrent();
		else TickSnapshot.Add(*It);
	}
	for (const TWeakObjectPtr<UUserWidget>& WeakWidget : TickSnapshot)
	{
		UUserWidget* Widget = WeakWidget.Get();
		if(!::IsValid(Widget))
		{
			continue;
		}
		if (!TickableWidgets.Contains(Widget)) continue;
		if((Widget->IsInViewport() || Widget->GetParent())
			&& ITickAbleWidgetInterface::Execute_IsTickAble(Widget))
		{
			if(const UUserWidgetBase* UserWidget = Cast<UUserWidgetBase>(Widget); UserWidget && !UserWidget->IsWidgetActiveInHierarchy())
			{
				continue;
			}
			if(const ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(Widget))
			{
				const UUserWidgetBase* OwnerWidget = Cast<UUserWidgetBase>(SubWidget->GetOwnerWidget());
				if(OwnerWidget && !OwnerWidget->IsWidgetActiveInHierarchy())
				{
					continue;
				}
			}
			ITickAbleWidgetInterface::Execute_OnTick(Widget, DeltaSeconds);
		}
	}

	TArray<TWeakObjectPtr<UUserWidgetBase>> ScreenSnapshot;
	for (const auto& Iter : UserWidgetByTag) if (Iter.Value) ScreenSnapshot.Add(Iter.Value.Get());
	for (const TWeakObjectPtr<UUserWidgetBase>& WeakWidget : ScreenSnapshot)
	{
		UUserWidgetBase* UserWidget = WeakWidget.Get();
		if(UserWidget
			&& UserWidget->GetWidgetTag().IsValid()
			&& UserWidget->GetWidgetState() == EScreenWidgetState::Opened
			&& UserWidget->IsWidgetActiveInHierarchy()
			&& UserWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
		{
			UserWidget->Refresh();
		}
	}

	TArray<TWeakObjectPtr<UWorldWidgetBase>> WorldSnapshot;
	for (const auto& Iter : WorldWidgetByTag)
		for (UWorldWidgetBase* Widget : Iter.Value.WorldWidgets) if (Widget) WorldSnapshot.Add(Widget);
	for (const TWeakObjectPtr<UWorldWidgetBase>& WeakWidget : WorldSnapshot)
		if (UWorldWidgetBase* Widget = WeakWidget.Get(); Widget && Widget->GetWidgetTag().IsValid() && Widget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
			Widget->Refresh();
}

void UWidgetModule::RegisterTickableWidget(UUserWidget* InWidget)
{
	if (!IsBusinessAdmissionOpen()) return;
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
		bPlayerWidgetsReady = false;
		if (UEventModule* Events = UMainModuleStatics::GetModuleForCleanup<UEventModule>(GetWorld()))
		{
			Events->UnsubscribeEvent<FEventOpenUserWidget>(this);
			Events->UnsubscribeEvent<FEventCloseUserWidget>(this);
			Events->UnsubscribeEvent<FEventSetWorldWidgetVisible>(this);
		}
		ClearAllUserWidget();
		ClearAllWorldWidgetForCleanup(EObjectDespawnMode::Destroy);
		for (const auto& Pair : WorldWidgetContainers) if (Pair.Value) Pair.Value->RemoveFromParent();
		WorldWidgetContainers.Reset();
		WorldWidgetComponents.Reset();
		TickableWidgets.Reset();
	}
}

void UWidgetModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData.GetRef<FWidgetModuleSaveData>();

	SetLanguageType(SaveData.LanguageType);
	SetGlobalScale(SaveData.GlobalScale);
}

void UWidgetModule::UnloadData(EPhase InPhase)
{
}

FParameter UWidgetModule::ToData()
{
	FWidgetModuleSaveData SaveData;

	SaveData.LanguageType = LanguageType;
	SaveData.GlobalScale = GlobalScale;
	
	return FParameter(MoveTemp(SaveData));
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
		if(const FGameplayTag ParentWidgetTag = Config.ResolveParentWidgetTag(); ParentWidgetTag.IsValid())
		{
			UserWidgetChildrenMap.Add(ParentWidgetTag, WidgetTag);
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
	for(const FScreenWidgetConfig& Config : UserWidgetConfigs)
	{
		if(Config.ResolveWidgetTag() == InWidgetTag)
		{
			return &Config;
		}
	}
	return nullptr;
}

FGameplayTag UWidgetModule::ResolveWidgetTagForClass(TSubclassOf<UUserWidgetBase> InClass, bool bEnsured) const
{
	TArray<FGameplayTag> Tags;
	if(InClass)
	{
		UserWidgetClassTagMap.MultiFind(InClass.Get(), Tags);
		if(Tags.IsEmpty())
		{
			for(const FScreenWidgetConfig& Config : UserWidgetConfigs)
			{
				if(Config.WidgetClass && Config.WidgetClass->IsChildOf(InClass))
				{
					Tags.Add(Config.ResolveWidgetTag());
				}
			}
		}
	}

	ensureEditorMsgf(
		!bEnsured || Tags.Num() == 1,
		FString::Printf(TEXT("Screen widget class must resolve to exactly one tag, but %s resolved to %d."), *GetNameSafe(InClass), Tags.Num()),
		EDC_Widget,
		EDV_Error);
	return Tags.Num() == 1 ? Tags[0] : FGameplayTag();
}

ULocalPlayer* UWidgetModule::ResolveLocalPlayer(APlayerController* InPlayer) const
{
    return ::IsValid(InPlayer) && InPlayer->GetWorld() == GetWorld() && InPlayer->IsLocalController() ? InPlayer->GetLocalPlayer() : nullptr;
}

UUserWidgetBase* UWidgetModule::GetUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, TSubclassOf<UUserWidgetBase> InExpectedClass) const
{
    ULocalPlayer* LocalPlayer = ResolveLocalPlayer(InPlayer);
    if (!LocalPlayer) return nullptr;
    FScreenWidgetKey Key; Key.LocalPlayer = LocalPlayer; Key.WidgetTag = InWidgetTag;
    const TObjectPtr<UUserWidgetBase>* Found = UserWidgetByTag.Find(Key);
    UUserWidgetBase* Widget = Found ? Found->Get() : nullptr;
    return !InExpectedClass || (Widget && Widget->IsA(InExpectedClass)) ? Widget : nullptr;
}

void UWidgetModule::OnOpenUserWidget(UObject* InSender, const FEventOpenUserWidget& InEvent)
{
    const FGameplayTag Tag = InEvent.WidgetTag.IsValid() ? InEvent.WidgetTag : ResolveWidgetTagForClass(InEvent.WidgetClassOverride, false);
    if (Tag.IsValid()) OpenUserWidgetByTag(InEvent.Player, Tag, InEvent.WidgetParam, InEvent.bInstant, InEvent.bForce, InEvent.WidgetClassOverride);
}

void UWidgetModule::OnCloseUserWidget(UObject* InSender, const FEventCloseUserWidget& InEvent)
{
    if (InEvent.WidgetTag.IsValid()) CloseUserWidgetByTag(InEvent.Player, InEvent.WidgetTag, InEvent.bInstant);
}

bool UWidgetModule::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass) const { return HasUserWidgetClass<UUserWidgetBase>(InClass); }
TSubclassOf<UUserWidgetBase> UWidgetModule::GetUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass) const { return GetUserWidgetClass<UUserWidgetBase>(InClass); }
bool UWidgetModule::HasUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass) const { return HasUserWidget<UUserWidgetBase>(InPlayer, InClass); }
UUserWidgetBase* UWidgetModule::GetUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass) const { return GetUserWidget<UUserWidgetBase>(InPlayer, InClass); }
UUserWidgetBase* UWidgetModule::CreateUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam) { return CreateUserWidget<UUserWidgetBase>(InPlayer, InParam, InClass); }
bool UWidgetModule::OpenUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam, bool bInstant, bool bForce) { return OpenUserWidget<UUserWidgetBase>(InPlayer, InParam, bInstant, bForce, InClass); }
bool UWidgetModule::CloseUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, bool bInstant) { return CloseUserWidget<UUserWidgetBase>(InPlayer, bInstant, InClass); }
bool UWidgetModule::ToggleUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, bool bInstant) { return ToggleUserWidget<UUserWidgetBase>(InPlayer, bInstant, InClass); }
bool UWidgetModule::DestroyUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, EObjectDespawnMode InMode) { return DestroyUserWidget<UUserWidgetBase>(InPlayer, InMode, InClass); }

void UWidgetModule::CloseAllUserWidget(APlayerController* InPlayer, bool bInstant)
{
    if (!IsBusinessAdmissionOpen()) return;
    ULocalPlayer* LocalPlayer = ResolveLocalPlayer(InPlayer);
    if (!LocalPlayer) return;
    for (auto It = TemporaryWidgetHistories.CreateIterator(); It; ++It) if (It.Key().LocalPlayer == LocalPlayer) It.RemoveCurrent();
    for (auto It = ActiveTemporaryWidgets.CreateIterator(); It; ++It) if (It.Key().LocalPlayer == LocalPlayer) It.RemoveCurrent();
    TArray<TObjectPtr<UUserWidgetBase>> Widgets;
    for (const auto& Entry : UserWidgetByTag) if (Entry.Key.LocalPlayer == LocalPlayer) Widgets.Add(Entry.Value);
    for (UUserWidgetBase* Widget : Widgets) if (::IsValid(Widget)) Widget->OnClose(bInstant);
}

void UWidgetModule::ClearPlayerWidgets(APlayerController* InPlayer, EObjectDespawnMode InMode)
{
    if (!IsBusinessAdmissionOpen()) return;
    if (ULocalPlayer* LocalPlayer = ResolveLocalPlayer(InPlayer)) ClearLocalPlayerWidgets(LocalPlayer, InMode);
}

void UWidgetModule::ClearLocalPlayerWidgets(ULocalPlayer* InLocalPlayer, EObjectDespawnMode InMode)
{
    if (!InLocalPlayer) return;
	TArray<UWorldWidgetBase*> PlayerWorldWidgets;
	for (const auto& Entry : WorldWidgetByTag)
		for (UWorldWidgetBase* Widget : Entry.Value.WorldWidgets)
			if (Widget && Widget->GetOwningLocalPlayer() == InLocalPlayer) PlayerWorldWidgets.Add(Widget);
	for (UWorldWidgetBase* Widget : PlayerWorldWidgets)
	{
		if (Widget)
			DestroyWorldWidgetByTagForCleanup(Widget->GetWidgetTag(), Widget, InMode);
	}
    for (auto It = TemporaryWidgetHistories.CreateIterator(); It; ++It) if (It.Key().LocalPlayer == InLocalPlayer) It.RemoveCurrent();
    for (auto It = ActiveTemporaryWidgets.CreateIterator(); It; ++It) if (It.Key().LocalPlayer == InLocalPlayer) It.RemoveCurrent();
    TArray<FScreenWidgetKey> Keys;
    for (const auto& Entry : UserWidgetByTag)
    {
        if (Entry.Key.LocalPlayer == InLocalPlayer) Keys.Add(Entry.Key);
    }
    for (const FScreenWidgetKey& Key : Keys)
    {
        UUserWidgetBase* Widget = UserWidgetByTag.FindRef(Key);
        if (!Widget) { UserWidgetByTag.Remove(Key); continue; }
        UserWidgetByTag.Remove(Key);
        Widget->OnClosed.RemoveAll(this);
        UnMountUserWidget(Widget);
        Widget->OnDestroy(InMode);
    }
}

void UWidgetModule::ClearAllUserWidget(EObjectDespawnMode InMode)
{
    TSet<ULocalPlayer*> Players;
    for (const auto& Entry : UserWidgetByTag) if (Entry.Key.LocalPlayer) Players.Add(Entry.Key.LocalPlayer);
    for (ULocalPlayer* LocalPlayer : Players) ClearLocalPlayerWidgets(LocalPlayer, InMode);
    UserWidgetByTag.Reset();
    ActiveTemporaryWidgets.Reset();
    TemporaryWidgetHistories.Reset();
    PreparedPlayers.Reset();
}

const FWorldWidgetConfig* UWidgetModule::GetWorldWidgetConfig(FGameplayTag InWidgetTag) const
{
	if(const int32* ConfigIndex = WorldWidgetConfigIndexMap.Find(InWidgetTag))
	{
		return WorldWidgetConfigs.IsValidIndex(*ConfigIndex)
			? &WorldWidgetConfigs[*ConfigIndex]
			: nullptr;
	}
	for(const FWorldWidgetConfig& Config : WorldWidgetConfigs)
	{
		if(Config.ResolveWidgetTag() == InWidgetTag)
		{
			return &Config;
		}
	}
	return nullptr;
}

FGameplayTag UWidgetModule::ResolveWorldWidgetTagForClass(TSubclassOf<UWorldWidgetBase> InClass, bool bEnsured) const
{
	TArray<FGameplayTag> Tags;
	if(InClass)
	{
		WorldWidgetClassTagMap.MultiFind(InClass.Get(), Tags);
		if(Tags.IsEmpty())
		{
			for(const FWorldWidgetConfig& Config : WorldWidgetConfigs)
			{
				if(Config.WidgetClass && Config.WidgetClass->IsChildOf(InClass))
				{
					Tags.Add(Config.ResolveWidgetTag());
				}
			}
		}
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

UWorldWidgetBase* UWidgetModule::CreateWorldWidgetByTag(FGameplayTag InWidgetTag, FWorldWidgetMapping InMapping, const FParameter& InParam, TSubclassOf<UWorldWidgetBase> InClass)
{
	if (!IsBusinessAdmissionOpen()) return nullptr;
	if (!bPlayerWidgetsReady) return nullptr;
	const FParameter SpawnParam = MakeWidgetSpawnParam(InParam);
	const FWorldWidgetConfig* Config = GetWorldWidgetConfig(InWidgetTag);
	if(!Config)
	{
		return nullptr;
	}
	const FWorldWidgetConfig ConfigSnapshot = *Config;
	const FWidgetSpawnParameter* Spawn = SpawnParam.GetPtr<FWidgetSpawnParameter>();
	if (Config->Space == EWorldWidgetSpace::Screen &&
		(!Spawn || !ResolveLocalPlayer(Spawn->OwningPlayer.Get()) || !GetWorldWidgetContainer(Spawn->OwningPlayer.Get()))) return nullptr;
	const TSubclassOf<UWorldWidgetBase> SpawnClass = InClass && InClass->IsChildOf(Config->WidgetClass)
		? InClass
		: Config->WidgetClass;
	UObjectPoolModule* Pool = UObjectPoolModule::GetPtr(GetWorld());
	UWorldWidgetBase* Widget = SpawnClass && Pool
		? Cast<UWorldWidgetBase>(Pool->SpawnObject(SpawnClass.Get(), SpawnParam))
		: nullptr;
	if(!Widget)
	{
		return nullptr;
	}

	FWorldWidgets& Widgets = WorldWidgetByTag.FindOrAdd(InWidgetTag);
	Widget->WidgetTag = ConfigSnapshot.ResolveWidgetTag();
	Widgets.WorldWidgets.Add(Widget);
	Widget->OnCreate(InMapping, SpawnParam);
	return Widget;
}

bool UWidgetModule::DestroyWorldWidgetByTag(FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, EObjectDespawnMode InMode)
{
	if (!IsBusinessAdmissionOpen()) return false;
	return DestroyWorldWidgetByTagForCleanup(InWidgetTag, InWidget, InMode);
}

bool UWidgetModule::DestroyWorldWidgetByTagForCleanup(FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, EObjectDespawnMode InMode)
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
	if(Widgets->WorldWidgets.IsEmpty())
	{
		WorldWidgetByTag.Remove(InWidgetTag);
	}
	InWidget->OnDestroy(InMode);
	return true;
}

bool UWidgetModule::GetWorldWidgetVisibleByTag(FGameplayTag InWidgetTag) const
{
	if(const FWorldWidgets* Widgets = WorldWidgetByTag.Find(InWidgetTag))
	{
		return bWorldWidgetsVisible && Widgets->bVisible;
	}
	return false;
}

void UWidgetModule::SetWorldWidgetVisibleByTag(FGameplayTag InWidgetTag, bool bVisible)
{
	if (!IsBusinessAdmissionOpen()) return;
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
	return bWorldWidgetsVisible;
}

void UWidgetModule::SetWorldWidgetVisible(bool bVisible, TSubclassOf<UWorldWidgetBase> InClass)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(InClass)
	{
		SetWorldWidgetVisibleByTag(ResolveWorldWidgetTagForClass(InClass, false), bVisible);
	}
	else
	{
		bWorldWidgetsVisible = bVisible;
		for (const auto& Pair : WorldWidgetContainers) if (Pair.Value)
			Pair.Value->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

UWorldWidgetContainer* UWidgetModule::GetWorldWidgetContainer(APlayerController* InPlayer) const
{
	ULocalPlayer* LocalPlayer = ResolveLocalPlayer(InPlayer);
	return LocalPlayer ? WorldWidgetContainers.FindRef(LocalPlayer) : nullptr;
}

TArray<UWorldWidgetBase*> UWidgetModule::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass) const
{
	return GetWorldWidgets<UWorldWidgetBase>(InClass);
}

UWorldWidgetBase* UWidgetModule::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, FWorldWidgetMapping InMapping, const FParameter& InParam)
{
	return CreateWorldWidget<UWorldWidgetBase>(InMapping, InParam, InClass);
}

bool UWidgetModule::DestroyWorldWidget(UWorldWidgetBase* InWidget, EObjectDespawnMode InMode)
{
	return InWidget
		&& DestroyWorldWidgetByTag(InWidget->GetWidgetTag(), InWidget, InMode);
}

void UWidgetModule::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, EObjectDespawnMode InMode)
{
	DestroyWorldWidgets<UWorldWidgetBase>(InMode, InClass);
}

void UWidgetModule::ClearAllWorldWidget(EObjectDespawnMode InMode)
{
	if (!IsBusinessAdmissionOpen()) return;
	ClearAllWorldWidgetForCleanup(InMode);
}

void UWidgetModule::ClearAllWorldWidgetForCleanup(EObjectDespawnMode InMode)
{
	TArray<UWorldWidgetBase*> WidgetsToDestroy;
	for(const auto& Iter : WorldWidgetByTag)
	{
		for (UWorldWidgetBase* Widget : Iter.Value.WorldWidgets)
			if (Widget) WidgetsToDestroy.Add(Widget);
	}
	for(UWorldWidgetBase* Widget : WidgetsToDestroy)
	{
		DestroyWorldWidgetByTagForCleanup(Widget->GetWidgetTag(), Widget, InMode);
	}
}
