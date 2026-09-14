
// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/WidgetModule.h"

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
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Widget/Event_CloseUserWidget.h"
#include "Event/Events/Widget/Event_OpenUserWidget.h"
#include "Event/Events/Widget/Event_SetWorldWidgetVisible.h"
#include "SaveGame/Module/WidgetSaveGame.h"
#include "Widget/World/WorldWidgetContainer.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonImageN.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Screen/WidgetMountSlot.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"
		
IMPLEMENTATION_MODULE(UWidgetModule)

namespace
{
	FParameter MakeWidgetSpawnParam(const FParameter& InParam)
	{
		if(!InParam.HasValue())
		{
			return FParameter(FWidgetSpawnParameter());
		}

		if(!InParam.GetPtr<FWidgetSpawnParameter>())
		{
			ensureEditorMsgf(
			false,
			FString::Printf(
				TEXT("Widget parameter %s must derive from FWidgetSpawnParameter."),
				*GetNameSafe(InParam.GetValueStruct())),
			EDC_Widget,
			EDV_Error);
			return FParameter(FWidgetSpawnParameter());
		}
		return InParam;
	}

	FParameter MakeWidgetOpenParam(const FParameter& InParam)
	{
		if(!InParam.HasValue())
		{
			return FParameter(FWidgetOpenParameter());
		}

		if(!InParam.GetPtr<FWidgetOpenParameter>())
		{
			ensureEditorMsgf(
				false,
				FString::Printf(
					TEXT("Widget parameter %s must derive from FWidgetOpenParameter."),
					*GetNameSafe(InParam.GetValueStruct())),
				EDC_Widget,
				EDV_Error);
			return FParameter(FWidgetOpenParameter());
		}
		return InParam;
	}

	UObject* GetWidgetOwnerObject(const FParameter& InParam)
	{
		if(const FWidgetSpawnParameter* Param = InParam.GetPtr<FWidgetSpawnParameter>())
		{
			return Param->OwnerObject.Get();
		}
		if(const FWidgetOpenParameter* Param = InParam.GetPtr<FWidgetOpenParameter>())
		{
			return Param->OwnerObject.Get();
		}
		return nullptr;
	}

	FParameter MakeWidgetSpawnOwnerParam(const FParameter& InParam)
	{
		return FParameter(FWidgetSpawnParameter(GetWidgetOwnerObject(InParam)));
	}

}

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
	DefaultWidgetTheme = nullptr;


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
	if(!DefaultWidgetTheme)
	{
		OutErrors.Add(NSLOCTEXT(
			"WH.WidgetModule",
			"MissingDefaultWidgetTheme",
			"Widget module has no default widget theme."));
		return false;
	}

	for(const auto& Iter : DefaultWidgetTheme->GetButtonStyles())
	{
		if(!Iter.Key.IsValid() || !Iter.Value.Style)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "InvalidButtonStyle", "Widget theme button style {0} has an invalid tag or style class."),
				FText::FromString(Iter.Key.ToString())));
		}
	}
	for(const auto& Iter : DefaultWidgetTheme->GetTextStyles())
	{
		if(!Iter.Key.IsValid() || !Iter.Value.Style)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "InvalidTextStyle", "Widget theme text style {0} has an invalid tag or style class."),
				FText::FromString(Iter.Key.ToString())));
		}
	}
	for(const auto& Iter : DefaultWidgetTheme->GetBrushStyles())
	{
		if(!Iter.Key.IsValid())
		{
			OutErrors.Add(NSLOCTEXT(
				"WH.WidgetModule",
				"InvalidBrushStyle",
				"Widget theme contains a brush style with an invalid tag."));
		}
		else if(!Iter.Value.Brush.HasUObject() && Iter.Value.Brush.DrawAs != ESlateBrushDrawType::NoDrawType)
		{
			OutWarnings.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "BrushStyleWithoutResource", "Widget theme brush style {0} has no resource object."),
				FText::FromString(Iter.Key.ToString())));
		}
	}

	const FSettingRendererClasses& RendererClasses = DefaultWidgetTheme->GetSettingRendererClasses();
	auto ValidateRendererClass = [&OutWarnings](const TCHAR* InName, TSubclassOf<UWidgetSettingEntryBase> InClass)
	{
		if(!InClass)
		{
			OutWarnings.Add(FText::Format(
				NSLOCTEXT("WH.WidgetModule", "MissingSettingRenderer", "Widget theme has no renderer class for {0} settings."),
				FText::FromString(InName)));
		}
	};
	ValidateRendererClass(TEXT("Bool"), RendererClasses.BoolClass);
	ValidateRendererClass(TEXT("Number"), RendererClasses.NumberClass);
	ValidateRendererClass(TEXT("Enum"), RendererClasses.EnumClass);
	ValidateRendererClass(TEXT("Text"), RendererClasses.TextClass);
	ValidateRendererClass(TEXT("Option"), RendererClasses.OptionClass);
	ValidateRendererClass(TEXT("Key"), RendererClasses.KeyClass);

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
			const FGameplayTag WidgetTag = Config.ResolveWidgetTag();
			if(!WidgetTag.IsValid() || Config.ResolveParentWidgetTag().IsValid())
			{
				continue;
			}
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

	for(const auto& Iter : UserWidgetByTag)
	{
		UUserWidgetBase* UserWidget = Iter.Value;
		if(UserWidget
			&& UserWidget->GetWidgetState() == EScreenWidgetState::Opened
			&& UserWidget->IsWidgetActiveInHierarchy()
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

UUserWidgetBase* UWidgetModule::GetUserWidgetByTag(FGameplayTag InWidgetTag, TSubclassOf<UUserWidgetBase> InExpectedClass) const
{
	const TObjectPtr<UUserWidgetBase>* Found = UserWidgetByTag.Find(InWidgetTag);
	UUserWidgetBase* Widget = Found ? Found->Get() : nullptr;
	return !InExpectedClass || (Widget && Widget->IsA(InExpectedClass)) ? Widget : nullptr;
}

bool UWidgetModule::MountUserWidget(UUserWidgetBase* InWidget, const FScreenWidgetConfig& InConfig)
{
	if(!InWidget)
	{
		return false;
	}

	const FGameplayTag WidgetTag = InConfig.ResolveWidgetTag();
	const FGameplayTag ParentWidgetTag = InConfig.ResolveParentWidgetTag();
	if(!ParentWidgetTag.IsValid())
	{
		InWidget->AddToViewport(InConfig.ZOrder);
		return true;
	}

	UUserWidgetBase* ParentWidget = GetUserWidgetByTag(ParentWidgetTag);
	if(!ParentWidget)
	{
		ensureEditorMsgf(false, FString::Printf(TEXT("Failed to mount widget %s: parent %s does not exist."), *WidgetTag.ToString(), *ParentWidgetTag.ToString()), EDC_Widget, EDV_Error);
		return false;
	}

	UPanelWidget* MountPanel = nullptr;
	if(InConfig.SlotTag.IsValid())
	{
		MountPanel = ParentWidget->GetWidgetMountSlot(InConfig.SlotTag);
		if(!MountPanel)
		{
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to mount widget %s: slot %s was not found in parent %s."), *WidgetTag.ToString(), *InConfig.SlotTag.ToString(), *ParentWidgetTag.ToString()), EDC_Widget, EDV_Error);
			return false;
		}
	}
	else
	{
		MountPanel = Cast<UPanelWidget>(ParentWidget->GetRootWidget());
		if(!MountPanel)
		{
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to mount widget %s: parent %s root widget is not a PanelWidget."), *WidgetTag.ToString(), *ParentWidgetTag.ToString()), EDC_Widget, EDV_Error);
			return false;
		}
	}

	if(UContentWidget* ContentWidget = Cast<UContentWidget>(MountPanel))
	{
		if(ContentWidget->GetContent() && ContentWidget->GetContent() != InWidget)
		{
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to mount widget %s: mount target in parent %s already contains %s."), *WidgetTag.ToString(), *ParentWidgetTag.ToString(), *GetNameSafe(ContentWidget->GetContent())), EDC_Widget, EDV_Error);
			return false;
		}
		ContentWidget->SetContent(InWidget);
	}
	else if(UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(MountPanel))
	{
		UCanvasPanelSlot* CanvasSlot = CanvasPanel->AddChildToCanvas(InWidget);
		if(!CanvasSlot)
		{
			return false;
		}
		CanvasSlot->SetZOrder(InConfig.ZOrder);
		CanvasSlot->SetAnchors(InConfig.Anchors);
		CanvasSlot->SetOffsets(InConfig.Offsets);
		CanvasSlot->SetAlignment(InConfig.Alignment);
	}
	else if(!MountPanel->AddChild(InWidget))
	{
		ensureEditorMsgf(false, FString::Printf(TEXT("Failed to add widget %s to parent %s panel %s."), *WidgetTag.ToString(), *ParentWidgetTag.ToString(), *GetNameSafe(MountPanel)), EDC_Widget, EDV_Error);
		return false;
	}

	InWidget->ParentWidget = ParentWidget;
	ParentWidget->AddChildWidget(InWidget);
	return true;
}

void UWidgetModule::UnMountUserWidget(UUserWidgetBase* InWidget)
{
	if(InWidget->IsInViewport() || InWidget->GetParent())
	{
		InWidget->RemoveFromParent();
	}
	if(InWidget->ParentWidget)
	{
		InWidget->ParentWidget->RemoveChildWidget(InWidget);
		InWidget->ParentWidget = nullptr;
	}
}

UUserWidgetBase* UWidgetModule::CreateUserWidgetByTag(FGameplayTag InWidgetTag, const FParameter& InParam, TSubclassOf<UUserWidgetBase> InClass)
{
	if(UUserWidgetBase* Existing = GetUserWidgetByTag(InWidgetTag))
	{
		return Existing;
	}

	const FParameter SpawnParam = MakeWidgetSpawnParam(InParam);
	const FParameter OwnerParam = MakeWidgetSpawnOwnerParam(SpawnParam);
	const FScreenWidgetConfig* Config = GetUserWidgetConfig(InWidgetTag);
	if(!Config)
	{
		return nullptr;
	}
	const FGameplayTag ParentWidgetTag = Config->ResolveParentWidgetTag();
	if(ParentWidgetTag.IsValid() && !GetUserWidgetByTag(ParentWidgetTag))
	{
		return nullptr;
	}

	const TSubclassOf<UUserWidgetBase> SpawnClass = InClass ? InClass : Config->WidgetClass;
	UUserWidgetBase* Widget = SpawnClass
		? Cast<UUserWidgetBase>(UObjectPoolModuleStatics::SpawnObject(SpawnClass.Get(), SpawnParam))
		: nullptr;
	if(!Widget)
	{
		return nullptr;
	}

	Widget->WidgetTag = Config->ResolveWidgetTag();
	UserWidgetByTag.Add(InWidgetTag, Widget);

	Widget->OnCreate(SpawnParam);
	Widget->Init(SpawnParam, false);
	Widget->SetVisibility(ESlateVisibility::Hidden);
	if(!MountUserWidget(Widget, *Config))
	{
		UserWidgetByTag.Remove(InWidgetTag);
		Widget->OnDestroy(EObjectDespawnMode::Destroy);
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
		if(ChildConfig->CreateType == EWidgetCreateType::AutoCreate
			|| ChildConfig->CreateType == EWidgetCreateType::AutoCreateAndOpen)
		{
			CreateUserWidgetByTag(ChildTag, OwnerParam);
		}
	}
	return Widget;
}

bool UWidgetModule::OpenUserWidgetByTag(FGameplayTag InWidgetTag, const FParameter& InParam, bool bInstant, bool bForce, TSubclassOf<UUserWidgetBase> InClass)
{
	const FParameter OpenParam = MakeWidgetOpenParam(InParam);
	const FParameter SpawnOwnerParam = MakeWidgetSpawnOwnerParam(OpenParam);
	const FScreenWidgetConfig* Config = GetUserWidgetConfig(InWidgetTag);
	if(!Config)
	{
		return false;
	}
	const FGameplayTag ParentWidgetTag = Config->ResolveParentWidgetTag();
	if(ParentWidgetTag.IsValid())
	{
		const UUserWidgetBase* ParentWidget = GetUserWidgetByTag(ParentWidgetTag);
		if(!ParentWidget || !ParentWidget->IsWidgetOpened())
		{
			return false;
		}
	}

	UUserWidgetBase* Widget = GetUserWidgetByTag(InWidgetTag);
	if(!Widget)
	{
		Widget = CreateUserWidgetByTag(
			InWidgetTag,
			SpawnOwnerParam,
			InClass);
	}
	if(!Widget || !Widget->CanOpen())
	{
		return false;
	}

	if(Config->WidgetType == EWidgetType::Temporary)
	{
		FWidgetMountContext Context;
		Context.ParentWidgetTag = ParentWidgetTag;
		Context.SlotTag = Config->SlotTag;
		if(UUserWidgetBase* ActiveWidget = ActiveTemporaryWidgets.FindRef(Context); ActiveWidget && ActiveWidget != Widget)
		{
			ActiveWidget->Close(true);
		}
		ActiveTemporaryWidgets.Add(Context, Widget);
	}

	if(bForce && Widget->GetWidgetState() != EScreenWidgetState::None)
	{
		Widget->OnClose(true);
	}
	const EScreenWidgetState PreviousState = Widget->GetWidgetState();
	Widget->OnOpen(OpenParam, bInstant);
	if(PreviousState != EScreenWidgetState::Opening && PreviousState != EScreenWidgetState::Opened)
	{
		TArray<FGameplayTag> ChildTags;
		UserWidgetChildrenMap.MultiFind(InWidgetTag, ChildTags);
		for(const FGameplayTag& ChildTag : ChildTags)
		{
			const FScreenWidgetConfig* ChildConfig = GetUserWidgetConfig(ChildTag);
			if(ChildConfig
				&& ChildConfig->CreateType == EWidgetCreateType::AutoCreateAndOpen)
			{
				OpenUserWidgetByTag(ChildTag, FWidgetOpenParameter(), bInstant);
			}
		}
	}
	return true;
}

bool UWidgetModule::CloseUserWidgetByTag(FGameplayTag InWidgetTag, bool bInstant)
{
	if(UUserWidgetBase* Widget = GetUserWidgetByTag(InWidgetTag))
	{
		if(const FScreenWidgetConfig* Config = GetUserWidgetConfig(InWidgetTag); Config && Config->WidgetType == EWidgetType::Temporary)
		{
			FWidgetMountContext Context;
			Context.ParentWidgetTag = Config->ResolveParentWidgetTag();
			Context.SlotTag = Config->SlotTag;
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
		return OpenUserWidgetByTag(InWidgetTag, FWidgetOpenParameter(), bInstant);
	}
	if(Widget->GetWidgetState() == EScreenWidgetState::Opened)
	{
		return CloseUserWidgetByTag(InWidgetTag, bInstant);
	}
	return OpenUserWidgetByTag(InWidgetTag, FWidgetOpenParameter(), bInstant);
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

bool UWidgetModule::CloseTemporaryWidgetInSlot(FGameplayTag InParentWidgetTag, FGameplayTag InSlotTag, bool bInstant)
{
	FWidgetMountContext Context;
	Context.ParentWidgetTag = InParentWidgetTag;
	Context.SlotTag = InSlotTag;
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

bool UWidgetModule::DestroyUserWidgetByTag(FGameplayTag InWidgetTag, EObjectDespawnMode InMode)
{
	if(UUserWidgetBase* Widget = GetUserWidgetByTag(InWidgetTag))
	{
		UserWidgetByTag.Remove(InWidgetTag);

		TArray<FGameplayTag> ChildTags;
		UserWidgetChildrenMap.MultiFind(InWidgetTag, ChildTags);
		for(const FGameplayTag& ChildTag : ChildTags)
		{
			DestroyUserWidgetByTag(ChildTag, InMode);
		}

		TArray<FWidgetMountContext> TemporaryContexts;
		ActiveTemporaryWidgets.GetKeys(TemporaryContexts);
		for(const FWidgetMountContext& Context : TemporaryContexts)
		{
			if(Context.ParentWidgetTag == InWidgetTag
				|| ActiveTemporaryWidgets.FindRef(Context) == Widget)
			{
				ActiveTemporaryWidgets.Remove(Context);
			}
		}

		Widget->OnDestroy(InMode);

		UnMountUserWidget(Widget);

		return true;
	}
	return false;
}

void UWidgetModule::OnOpenUserWidget(UObject* InSender, const FEventOpenUserWidget& InEvent)
{
	const FGameplayTag WidgetTag = InEvent.WidgetTag.IsValid()
		? InEvent.WidgetTag
		: ResolveWidgetTagForClass(InEvent.WidgetClassOverride, false);
	if(WidgetTag.IsValid())
	{
		OpenUserWidgetByTag(
			WidgetTag,
			InEvent.WidgetParam,
			InEvent.bInstant,
			InEvent.bForce,
			InEvent.WidgetClassOverride);
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

UUserWidgetBase* UWidgetModule::CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam)
{
	return CreateUserWidget<UUserWidgetBase>(InParam, InClass);
}

bool UWidgetModule::OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam, bool bInstant, bool bForce)
{
	return OpenUserWidget<UUserWidgetBase>(InParam, bInstant, bForce, InClass);
}

bool UWidgetModule::CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return CloseUserWidget<UUserWidgetBase>(bInstant, InClass);
}

bool UWidgetModule::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return ToggleUserWidget<UUserWidgetBase>(bInstant, InClass);
}

bool UWidgetModule::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, EObjectDespawnMode InMode)
{
	return DestroyUserWidget<UUserWidgetBase>(InMode, InClass);
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

void UWidgetModule::ClearAllUserWidget(EObjectDespawnMode InMode)
{
	TArray<FGameplayTag> WidgetTags;
	UserWidgetByTag.GetKeys(WidgetTags);
	for(const FGameplayTag& WidgetTag : WidgetTags)
	{
		DestroyUserWidgetByTag(WidgetTag, InMode);
	}
	UserWidgetByTag.Reset();
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
				if(Config.WidgetClass == InClass)
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
	const FParameter SpawnParam = MakeWidgetSpawnParam(InParam);
	const FWorldWidgetConfig* Config = GetWorldWidgetConfig(InWidgetTag);
	if(!Config)
	{
		return nullptr;
	}
	const TSubclassOf<UWorldWidgetBase> SpawnClass = InClass
		? InClass
		: Config->WidgetClass;
	UWorldWidgetBase* Widget = SpawnClass
		? Cast<UWorldWidgetBase>(UObjectPoolModuleStatics::SpawnObject(SpawnClass.Get(), SpawnParam))
		: nullptr;
	if(!Widget)
	{
		return nullptr;
	}

	FWorldWidgets& Widgets = WorldWidgetByTag.FindOrAdd(InWidgetTag);
	Widget->WidgetTag = Config->ResolveWidgetTag();
	Widgets.WorldWidgets.Add(Widget);
	Widget->OnCreate(InMapping, SpawnParam);
	return Widget;
}

bool UWidgetModule::DestroyWorldWidgetByTag(FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, EObjectDespawnMode InMode)
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
			Widget->OnDestroy(InMode);
		}
	}
}
